// Behavioral test — verify compat.curl builds a usable libcurl with working
// TLS, without touching the network.
//
// CI runners have no reliable outbound network and the index's other members
// never assume one, so nothing here opens a connection. What is asserted
// instead is everything that a misconfigured build actually gets wrong:
//
//   * the library initialises at all (curl_global_init walks the same setup
//     path a real transfer would),
//   * the TLS backend is really compiled in — a curl built with no SSL still
//     links and still runs, it just silently cannot do https, which is exactly
//     the failure EUI-NEO's `network` feature would hit at runtime,
//   * https and http are among the supported protocols,
//   * a handle can be configured with the options EUI-NEO actually sets.
#include <curl/curl.h>
import std;

// CURL_STATICLIB must arrive from the PACKAGE, not from this project. Without
// it <curl/curl.h> declares everything __declspec(dllimport) and the Windows
// link fails against a static archive; on other platforms it is inert, so this
// compile-time check is the only thing that catches a regression early.
static_assert([] {
#if defined(CURL_STATICLIB)
    return true;
#else
    return false;
#endif
}(), "CURL_STATICLIB did not reach the consumer — compat.curl stopped publishing it");

int main() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::println("curl_global_init failed");
        return 1;
    }

    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    if (info == nullptr) {
        std::println("curl_version_info returned null");
        curl_global_cleanup();
        return 2;
    }

    // The whole point of wiring a TLS backend. `ssl_version` is null when curl
    // was built without one.
    if ((info->features & CURL_VERSION_SSL) == 0 || info->ssl_version == nullptr) {
        std::println("libcurl built WITHOUT TLS — https would fail at runtime");
        curl_global_cleanup();
        return 3;
    }

    bool haveHttps = false;
    bool haveHttp = false;
    for (const char* const* p = info->protocols; p != nullptr && *p != nullptr; ++p) {
        const std::string_view proto{*p};
        if (proto == "https") haveHttps = true;
        if (proto == "http") haveHttp = true;
    }
    if (!haveHttp || !haveHttps) {
        std::println("missing protocol support (http={}, https={})", haveHttp, haveHttps);
        curl_global_cleanup();
        return 4;
    }

    // The exact option set core/platform/network.cpp configures. A handle that
    // rejects any of these would break the `network` feature.
    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        std::println("curl_easy_init failed");
        curl_global_cleanup();
        return 5;
    }
    const bool optionsOk =
        curl_easy_setopt(handle, CURLOPT_URL, "https://example.invalid/") == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 15L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L) == CURLE_OK;
    curl_easy_cleanup(handle);

    if (!optionsOk) {
        std::println("curl_easy_setopt rejected an option EUI-NEO relies on");
        curl_global_cleanup();
        return 6;
    }

    std::println("compat.curl: ok ({}, ssl={}, http+https present)",
                 info->version, info->ssl_version);
    curl_global_cleanup();
    return 0;
}
