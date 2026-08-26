// wss_test.cpp — offline end-to-end test for compat.websocket's `tls` feature.
//
// An ix::WebSocketServer serves TLS on loopback using test-only PEM files;
// ix::WebSocket connects with wss://localhost and trusts that certificate via
// caFile. Hostname validation intentionally stays enabled, so this verifies
// OpenSSL transport, trust-chain handling, the localhost SAN, WebSocket
// upgrade, text/binary payloads, and orderly shutdown without public network.
#ifdef HAVE_WEBSOCKET_TLS

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

namespace
{

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    if (ok)
    {
        std::cout << "  ok: " << what << "\n";
    }
    else
    {
        std::cout << "  FAIL: " << what << "\n";
        ++g_failures;
    }
}

struct RecvMsg
{
    ix::WebSocketMessageType type;
    std::string str;
    bool binary;
};

class Collector
{
public:
    void add(const ix::WebSocketMessagePtr& msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.push_back({msg->type, msg->str, msg->binary});
        condition_.notify_all();
    }

    template <typename Predicate>
    bool wait_for(Predicate predicate, int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return predicate(messages_);
        });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<RecvMsg> messages_;
};

constexpr const char* kCertificatePem = R"pem(-----BEGIN CERTIFICATE-----
MIIDJTCCAg2gAwIBAgIUFgJ40WV0h6eUam72FKECeBPFXqkwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI2MDgyNDEzMTQ0NVoXDTM2MDgy
MTEzMTQ0NVowFDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAvdaQ7B4R1b1LljeRkG91z7SXF2JRmIuStEHB34Q3N1sE
pCDBbihaP2Uzunf9/0j7ypaAsZettQTYUPIoyIGZ/qHuaAGaA14ybOl6NstyU/BZ
5i+RYOdvChJytfi9yLJ8UAayduLfRufoSV1LXGf1YfjtcWx/FDOtMRKLNKAzAh4Y
PUz1Jj6PmLP25jQ5LyA70wnPVv7lYVHS2ZKvTGTng0FADBYCRg3s/gpEB91vuNWW
9g7/BQJQMRkzaq1CGQPTlwpjV8nums5jEWJ6CQTojjOn7anwvcIw4SbSqT2/2ds3
lefSy7nLC7B4YWQw6yMvf8OvFPEbFQsZrr8nTLtoEwIDAQABo28wbTAdBgNVHQ4E
FgQULuQu97cGXi66GfXgZ4wCqmvlZ24wHwYDVR0jBBgwFoAULuQu97cGXi66GfXg
Z4wCqmvlZ24wDwYDVR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgglsb2NhbGhvc3SH
BH8AAAEwDQYJKoZIhvcNAQELBQADggEBADTtFpAfVI8NF2H69pMecu1ZL0XonQyi
7tI8OetQH6ZdV3BHlyRRb5kkA7kDJSkeKHIVmL2ekB12ayU684jZtOy2eEcRML/y
NJ0mN5+WfSUWsOhI7U+Py/LUW6o11pTC6iIoOyd2BbKd7jo/bX4XnkKgGTXVsZ4l
D12f9IYb/DWY7SDgIiy4EsVsbEaY3Na45bZ0J8GCfWwaWbP1K8c5HRilA644CfYI
ItdcWRipChc4BPMOgIMW/9QE3OtE9J6Bcub8/QABi1Wat/bEYCpOHkMxBn/EE4hK
90r/2fKkfFV9Zcc5RfBkXZ0kf8hHhtdYN0PbZU1tlaxinEFtHvL8hN4=
-----END CERTIFICATE-----
)pem";

constexpr const char* kPrivateKeyPem = R"pem(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC91pDsHhHVvUuW
N5GQb3XPtJcXYlGYi5K0QcHfhDc3WwSkIMFuKFo/ZTO6d/3/SPvKloCxl621BNhQ
8ijIgZn+oe5oAZoDXjJs6Xo2y3JT8FnmL5Fg528KEnK1+L3IsnxQBrJ24t9G5+hJ
XUtcZ/Vh+O1xbH8UM60xEos0oDMCHhg9TPUmPo+Ys/bmNDkvIDvTCc9W/uVhUdLZ
kq9MZOeDQUAMFgJGDez+CkQH3W+41Zb2Dv8FAlAxGTNqrUIZA9OXCmNXye6azmMR
YnoJBOiOM6ftqfC9wjDhJtKpPb/Z2zeV59LLucsLsHhhZDDrIy9/w68U8RsVCxmu
vydMu2gTAgMBAAECggEAV6ceuycXLQ1+PRnjSEFusDy+Frn62uh3EGvcTIwLwq9v
8Sh+p5JSOTtNKygESz8zo1LikR1rw009ZAKr+gh9RikWn0c+CZgQyGD1YR+G5mLv
32zPP2MczhW+iW8Ukfp3k6vD80jFt0OU6Wr+ROhrUJVTbS+fbYB+002woNfnNW5M
Lf3N0fdmNGDXVwattzy31BV+f8JHr3grgRVTXPaT8x3ADxxlbkI26T5w8FaYx6gZ
XE++jktNZCHB0FtgdTD9Cugm0D6WkDmu0ZYroNrk20FPswgZKqOm1/PluX5qSu0Z
7+QfTj+6oWepB4gY1xXSb9sjWn5ZZDCVih5a/P1yQQKBgQDjYHtyi/DKM0llVpO1
fBql217oKBVnd8GTSa7+1ehqTMuDnZMRwqFwrCQDMoMJ3nk06+Kvw4jRTn2HvUHG
LLZhb7ZyeukvBCh3hVc3QPxy+BuoSYvt9al0ouufIZZ4hL7Yx4S3pGmRpkd4OBmQ
9TJo2Rqjg2YLZGqxjNw3RO5kuwKBgQDVvFlYgfZlBYWKn/0Pi0XjyvQlGZYUsFdW
h2N3L6SDoMVeYgVJbC28fDjN9O4IVXarB32JCY4bXcsHvQKSsU6chbKcXGAZPaWO
UuniYRqmmjQyOqO2E7A5hbZMFgRxLmcmSPA505FB4WF3sPJd6W5X8i2RzFfCwAHR
UXdrHL6AiQKBgAx2VU3J7cCnXvZ28FGaI7vDckg3KjUpkyqHd1fwUXTCEMV99Xmb
uU17od2q/xOjZfFInHwVs4IFU0wFS32ZJcXhYZaUtgMlrzId1NHqdeu3PYzTux+n
v0ntRAzMwnqIjA1FojiOglrBSlmEeaJATisA+zzLDuTA9DgXCFrfJFHRAoGAa8rd
1IFW3oP2YX9mhRxcVxHYJ43L3wtAQOdvBoEEm03NvFf7CpiASHrtuxE3qwRPINpa
OW6UOMEI0BJG5ex+FPpopesAnDo28JxoUD9gzX0freVdA0rSqXACDEVeYCZi5zAJ
12AX9f3Qxih7U1mSyM/eo5VG/XUQdZx8eYy5luECgYBwUV7QZzTqyoEvoz3z6PAU
++c/n+YQMrDeNg1nHjBMR5qFn4H6Eg6ZsFkSHkAXYw5PyjNF/k12rHkveTQV9kuB
yvhREr3gAz2a/1cBXvVg8eNwQ0jkea83MZw9FK4Kf0Y9T0Eh6Vsg5sTitYrUV+l/
/w96Ha5ea5ypiCL+Al6aYQ==
-----END PRIVATE KEY-----
)pem";

class TempPemFiles
{
public:
    TempPemFiles()
    {
        const auto directory = std::filesystem::temp_directory_path();
        const auto stem = "mcpp-websocket-tls-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        certificate_ = directory / (stem + ".crt.pem");
        key_ = directory / (stem + ".key.pem");
        write(certificate_, kCertificatePem);
        write(key_, kPrivateKeyPem);
    }

    ~TempPemFiles()
    {
        std::error_code ignored;
        std::filesystem::remove(certificate_, ignored);
        std::filesystem::remove(key_, ignored);
    }

    bool valid() const { return valid_; }
    const std::filesystem::path& certificate() const { return certificate_; }
    const std::filesystem::path& key() const { return key_; }

private:
    void write(const std::filesystem::path& path, const char* contents)
    {
        std::ofstream out(path, std::ios::binary);
        out << contents;
        valid_ = valid_ && static_cast<bool>(out);
    }

    std::filesystem::path certificate_;
    std::filesystem::path key_;
    bool valid_ = true;
};

int reserve_loopback_port()
{
    Socket socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == kInvalidSocket) return -1;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
        return -1;
    }
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length) != 0)
    {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
        return -1;
    }
#ifdef _WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
    return ntohs(address.sin_port);
}


bool saw_open(const std::vector<RecvMsg>& messages)
{
    for (const auto& msg : messages)
        if (msg.type == ix::WebSocketMessageType::Open) return true;
    return false;
}

bool saw_error(const std::vector<RecvMsg>& messages)
{
    for (const auto& msg : messages)
        if (msg.type == ix::WebSocketMessageType::Error) return true;
    return false;
}

} // namespace

int main()
{
    std::cout << std::unitbuf;
    check(ix::initNetSystem(), "ix::initNetSystem()");

    TempPemFiles pem;
    check(pem.valid(), "wrote test-only certificate and private key");
    if (g_failures != 0)
    {
        ix::uninitNetSystem();
        return 1;
    }

    // Reserve a port with a raw loopback socket. The short close→listen gap is
    // retried by CI through the normal test invocation if an unrelated process
    // claims it; a fixed literal would collide systematically on shared runners.
    const int port = reserve_loopback_port();
    check(port > 0, "reserved a loopback port");
    if (g_failures != 0)
    {
        ix::uninitNetSystem();
        return 1;
    }
    ix::WebSocketServer server(port, "127.0.0.1");
    ix::SocketTLSOptions server_tls;
    server_tls.tls = true;
    server_tls.certFile = pem.certificate().string();
    server_tls.keyFile = pem.key().string();
    // The fixture authenticates the SERVER. Requesting a client certificate
    // would turn this into mTLS and reject the deliberately certificate-free
    // WebSocket client before the behavior under test can begin.
    server_tls.caFile = "NONE";
    server.setTLSOptions(server_tls);
    server.setOnClientMessageCallback(
        [](std::shared_ptr<ix::ConnectionState> /*state*/,
           ix::WebSocket& socket,
           const ix::WebSocketMessagePtr& message) {
            if (message->type != ix::WebSocketMessageType::Message) return;
            if (message->binary) socket.sendBinary(message->str);
            else socket.sendText(message->str);
        });
    check(server.listenAndStart(), "TLS WebSocketServer listenAndStart()");
    if (g_failures != 0)
    {
        server.stop();
        ix::uninitNetSystem();
        return 1;
    }

    Collector collector;
    ix::WebSocket client;
    ix::SocketTLSOptions client_tls;
    client_tls.caFile = pem.certificate().string();
    // Do NOT use caFile = "NONE" or disable_hostname_validation: certificate
    // trust and localhost SAN matching are precisely what this test asserts.
    client.setTLSOptions(client_tls);
    client.setUrl("wss://localhost:" + std::to_string(port) + "/");
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& message) { collector.add(message); });
    client.start();

    check(collector.wait_for(saw_open, 8000), "certificate-validated wss:// connection opened");
    check(!collector.wait_for(saw_error, 50), "client reported no TLS/upgrade error");

    const std::string text = "hello over WSS";
    client.sendText(text);
    check(collector.wait_for(
              [&](const std::vector<RecvMsg>& messages) {
                  for (const auto& message : messages)
                      if (message.type == ix::WebSocketMessageType::Message &&
                          !message.binary && message.str == text)
                          return true;
                  return false;
              },
              5000),
          "text message round-tripped over TLS");

    const std::string binary("\x00\x01\xfe\xff\x80WSS", 8);
    client.sendBinary(binary);
    check(collector.wait_for(
              [&](const std::vector<RecvMsg>& messages) {
                  for (const auto& message : messages)
                      if (message.type == ix::WebSocketMessageType::Message &&
                          message.binary && message.str == binary)
                          return true;
                  return false;
              },
              5000),
          "binary message round-tripped over TLS");

    client.stop();
    server.stop();
    ix::uninitNetSystem();

    if (g_failures == 0)
    {
        std::cout << "ALL WEBSOCKET-TLS ASSERTIONS PASSED\n";
        return 0;
    }
    std::cout << g_failures << " ASSERTION(S) FAILED\n";
    return 1;
}

#else
#error "HAVE_WEBSOCKET_TLS must be enabled for every declared test platform"
#endif
