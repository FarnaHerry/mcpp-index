// redis_test.cpp — offline behavioral test for the `async` feature of
// compat.redis-plus-plus (libuv-backed AsyncRedis).
//
// No redis-server binary and no network access: a minimal RESP server runs
// inside the process (raw loopback sockets, same platform abstraction as the
// websocket member) and the sw::redis::AsyncRedis client is driven against it.
// The async client can pipeline PING + SET into a single TCP segment, so the
// server parses complete RESP arrays instead of scanning for keywords.
#include <sw/redis++/async_redis++.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

// ── Platform socket abstraction (same as tests/examples/websocket) ─────────

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SockType = SOCKET;
constexpr SockType kInvalidSocket = INVALID_SOCKET;
using AddrLenType = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using SockType = int;
constexpr SockType kInvalidSocket = -1;
using AddrLenType = socklen_t;
#endif

void init_sockets()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

void close_socket(SockType s)
{
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

void set_recv_timeout(SockType s, int ms)
{
#ifdef _WIN32
    DWORD t = static_cast<DWORD>(ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&t), sizeof(t));
#else
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

// ── Minimal RESP server: PING -> +PONG, SET -> +OK ─────────────────────────
// Parses complete RESP arrays so pipelined commands (async client) are served
// one reply each instead of one reply per recv().
class RespServer
{
public:
    ~RespServer()
    {
        if (client != kInvalidSocket) close_socket(client);
        if (listener != kInvalidSocket) close_socket(listener);
        if (thread.joinable()) thread.join();
    }

    bool start()
    {
        init_sockets();
        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener == kInvalidSocket) return false;
        int one = 1;
#ifdef _WIN32
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&one), sizeof(one));
#else
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // ephemeral
        if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        AddrLenType alen = sizeof(addr);
        getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &alen);
        port = ntohs(addr.sin_port);
        if (listen(listener, 1) != 0) return false;
        thread = std::thread([this] { run(); });
        return true;
    }

    int port = 0;

private:
    void run()
    {
        SockType s = accept(listener, nullptr, nullptr);
        if (s == kInvalidSocket) return;
        client = s;
        set_recv_timeout(client, 5000);

        std::string buf;
        int served = 0;
        while (served < 16) {
            char tmp[1024];
            const int n = static_cast<int>(recv(client, tmp, sizeof(tmp), 0));
            if (n <= 0) break;
            buf.append(tmp, static_cast<size_t>(n));

            size_t off = 0;
            while (served < 16) {
                // `*<count>\r\n` then `<count>` bulk strings `$<len>\r\n<data>\r\n`.
                if (buf.size() < off + 3 || buf[off] != '*') break;
                const size_t nl = buf.find("\r\n", off);
                if (nl == std::string::npos) break;
                const int count = std::atoi(buf.substr(off + 1, nl - off - 1).c_str());
                off = nl + 2;
                std::string cmd;
                bool complete = true;
                for (int i = 0; i < count; ++i) {
                    if (buf.size() < off + 1 || buf[off] != '$') { complete = false; break; }
                    const size_t nl2 = buf.find("\r\n", off);
                    if (nl2 == std::string::npos) { complete = false; break; }
                    const int len = std::atoi(buf.substr(off + 1, nl2 - off - 1).c_str());
                    off = nl2 + 2;
                    if (buf.size() < off + static_cast<size_t>(len) + 2) { complete = false; break; }
                    if (i == 0) cmd.assign(buf, off, static_cast<size_t>(len));
                    off += static_cast<size_t>(len) + 2;
                }
                if (!complete) break;
                buf.erase(0, off);
                off = 0;

                if (cmd == "PING") {
                    static constexpr char kPong[] = "+PONG\r\n";
                    send(client, kPong, static_cast<int>(sizeof(kPong) - 1), 0);
                } else if (cmd == "SET") {
                    static constexpr char kOk[] = "+OK\r\n";
                    send(client, kOk, static_cast<int>(sizeof(kOk) - 1), 0);
                }
                ++served;
            }
        }
    }

    SockType listener = kInvalidSocket;
    SockType client = kInvalidSocket;
    std::thread thread;
};

int main()
{
    RespServer server;
    if (!server.start()) {
        std::cerr << "failed to start RESP server\n";
        return 1;
    }

    sw::redis::ConnectionOptions opts;
    opts.host = "127.0.0.1";
    opts.port = server.port;
    opts.connect_timeout = std::chrono::milliseconds(3000);
    opts.socket_timeout = std::chrono::milliseconds(3000);

    sw::redis::ConnectionPoolOptions pool_opts;
    pool_opts.size = 1;

    try {
        sw::redis::AsyncRedis redis(opts, pool_opts);

        // Fire both commands back-to-back; the libuv loop runs on a background
        // thread and the server may see them pipelined in one segment.
        auto ping_res = redis.ping();
        auto set_res = redis.set("mcpp", "async");

        const std::string pong = ping_res.get();
        if (pong != "PONG") {
            std::cerr << "unexpected async PING reply: '" << pong << "'\n";
            return 2;
        }
        if (!set_res.get()) {
            std::cerr << "async SET did not return OK\n";
            return 3;
        }

        std::cout << "OK: async PING -> " << pong << ", SET -> OK\n";
        return 0;
    } catch (const sw::redis::Error &e) {
        std::cerr << "redis error: " << e.what() << "\n";
        return 4;
    }
}
