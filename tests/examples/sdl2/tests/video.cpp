// Behavioral test — verify compat.sdl2 builds an SDL that can actually run a
// video subsystem, create a window and pump events.
//
// SDL is the one backend in this family that CAN be exercised headlessly:
// its `dummy` video driver is a real, fully functional driver that just never
// touches a display. So unlike the OpenGL and Vulkan backends, this is not a
// link-only smoke test — SDL_Init, SDL_CreateWindow, SDL_GetWindowSize and the
// event pump all execute.
//
// The driver is selected through SDL_HINT_VIDEODRIVER rather than the
// SDL_VIDEODRIVER environment variable so the test is self-contained and does
// not depend on how CI invokes it.
// SDL_MAIN_HANDLED before <SDL.h>: on Windows SDL_main.h does
// `#define main SDL_main` and expects the real entry point to come from
// SDL2main (src/main/windows/SDL_windows_main.c). This package does not ship
// that — a library consumer should not be handed an entry point — so the test
// takes SDL's documented alternative and keeps its own main, pairing it with
// SDL_SetMainReady() below. Without this the link fails with
// "LNK1561: entry point must be defined".
#define SDL_MAIN_HANDLED
#include <SDL.h>
import std;

int main() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::println("SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    // A build whose video subsystem never registered a driver fails here, which
    // is the failure mode a bad SDL_config.h produces — upstream's Linux
    // fallback config (SDL_config_minimal.h) builds exactly that.
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr) {
        std::println("no video driver active: {}", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Window* window = SDL_CreateWindow(
        "compat.sdl2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        320, 240, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::println("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 3;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    if (width != 320 || height != 240) {
        std::println("unexpected window size: {}x{}", width, height);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }

    // Pumping events exercises the event subsystem end to end; SDL_QUIT is
    // pushed and read back rather than waited for, so nothing here can hang.
    SDL_Event pushed{};
    pushed.type = SDL_QUIT;
    if (SDL_PushEvent(&pushed) < 0) {
        std::println("SDL_PushEvent failed: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 5;
    }
    SDL_PumpEvents();

    bool sawQuit = false;
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) sawQuit = true;
    }
    if (!sawQuit) {
        std::println("pushed SDL_QUIT never came back out of the event queue");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 6;
    }

    SDL_version linked{};
    SDL_GetVersion(&linked);

    std::println("compat.sdl2: ok (SDL {}.{}.{}, driver={}, {}x{} window, events round-tripped)",
                 linked.major, linked.minor, linked.patch, driver, width, height);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
