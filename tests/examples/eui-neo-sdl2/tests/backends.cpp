// Verify compat.eui-neo's `sdl2` window backend and `network` feature.
//
// Both are asserted at COMPILE time first, because both fail silently at
// runtime otherwise: a window backend that did not switch just uses GLFW, and
// `network` that did not switch leaves core/platform/network.cpp compiled as
// stubs that return failure. Neither shows up without a display or a network.
#include <eui_neo.h>
// SDL_MAIN_HANDLED before <SDL.h>: on Windows SDL_main.h does
// `#define main SDL_main` and expects the real entry point to come from
// SDL2main (src/main/windows/SDL_windows_main.c). This package does not ship
// that — a library consumer should not be handed an entry point — so the test
// takes SDL's documented alternative and keeps its own main, pairing it with
// SDL_SetMainReady() below. Without this the link fails with
// "LNK1561: entry point must be defined".
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <curl/curl.h>
import std;

#if !defined(EUI_WINDOW_BACKEND_SDL2)
#error "sdl2 feature requested but EUI_WINDOW_BACKEND_SDL2 is not defined"
#endif
#if !defined(EUI_HAS_CURL)
#error "network feature requested but EUI_HAS_CURL is not defined"
#endif

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}.title("sdl2 + network test");
    return config;
}

void compose(eui::Ui&, const eui::Screen&) {}

} // namespace app

int main() {
    // SDL's dummy driver is a real driver, so unlike the GL/Vulkan backends
    // this actually runs: init, create, query, tear down.
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::println("SDL_Init failed: {}", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("eui-neo", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 200, 100,
                                          SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::println("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    const char* driver = SDL_GetCurrentVideoDriver();
    SDL_DestroyWindow(window);
    SDL_Quit();

    // libcurl came in through the feature's dependency, and it has TLS — the
    // thing EUI-NEO's downloader needs and the thing a misconfigured curl
    // silently lacks. No connection is made.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::println("curl_global_init failed");
        return 3;
    }
    const curl_version_info_data* curlInfo = curl_version_info(CURLVERSION_NOW);
    const bool haveTls = curlInfo != nullptr &&
                         (curlInfo->features & CURL_VERSION_SSL) != 0 &&
                         curlInfo->ssl_version != nullptr;
    curl_global_cleanup();
    if (!haveTls) {
        std::println("libcurl reached us without TLS");
        return 4;
    }

    // Still eui-neo underneath — a headless facade from the base source set.
    eui::json::Document doc;
    if (!doc.parse(R"({"window": "sdl2"})")) {
        std::println("parse failed: {}", doc.error().message);
        return 5;
    }
    std::string windowBackend;
    if (!doc.stringAt("/window", windowBackend) || windowBackend != "sdl2") {
        std::println("unexpected /window: '{}'", windowBackend);
        return 6;
    }

    std::println("compat.eui-neo[sdl2,network]: ok (SDL driver={}, curl {} ssl={})",
                 driver, curlInfo->version, curlInfo->ssl_version);
    return 0;
}
