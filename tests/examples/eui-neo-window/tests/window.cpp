// compat.eui-neo with a HAND-WRITTEN main(): the own-main shape, and the only
// place in this index that builds an eui-neo window.
//
// tests/examples/eui-neo covers the headless surface (json, platform flags) and
// tests/examples/eui-neo-app-main covers the framework-main shape. What is left,
// and what this member pins, is everything between window creation and a
// presented frame: core::window::createWindow(), the OpenGL render backend,
// app::initialize/update/render, ScopedRenderBackend, and the freetype text path.
//
// Without `app-main` the package contributes no entry point, so the five things
// upstream's glfw_app_main.cpp does have to be done by hand — see runWindow().
// That is the point: a consumer must be able to drive EUI itself, and the public
// surface it needs (core::window, core::render, app::*) must actually be reachable
// through the descriptor's include_dirs.
//
// The window run needs a display, so it is opt-in via MCPP_RUN_WINDOW=1, exactly
// as in tests/examples/imgui-window. Headless CI compiles and links the whole
// loop — which is where a descriptor regression would surface — and returns 0.
//
// Builds on all three platforms. Every core/ header below is platform-clean (no
// windows.h, no Cocoa, no GL), and every GLFW entry point used here is portable,
// so there is no reason to cfg-gate this the way the X11-consuming members are.
#include <eui_neo.h>

#include "core/input/input_state.h"
#include "core/platform/platform.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

// GLFW would otherwise pull a GL header of its own choosing (GL/gl.h on Windows,
// the deprecated OpenGL/gl.h on macOS). EUI reaches GL through compat.glad, so
// this TU wants the GLFW API and nothing else — the same thing
// core/input/input_state.h does before its own include.
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

import std;

namespace {

// Rendered frames to require before exiting, so the opt-in run terminates on its
// own. Also the assertion: fewer than this means the loop stalled.
constexpr int kFrameBudget = 60;

struct Loop {
    float elapsed = 0.0f;
    int frames = 0;
    // Accumulated from core::render::lastRenderFrameStats(). A frame counter only
    // proves app::update() ran — these prove the OpenGL backend actually issued
    // geometry, i.e. that the render half is wired too. Without them this member
    // would pass just as happily against a null backend.
    long long rectDraws = 0;
    long long textDraws = 0;
};

Loop g_loop;

} // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("compat.eui-neo — own main")
        .pageId("eui_neo_own_main")
        .clearColor({0.06f, 0.07f, 0.09f, 1.0f})
        .windowSize(420, 240)
        .fps(60.0)
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    // A node carrying .onFrame() marks the runtime as animating, which is what
    // turns a redraw-on-demand retained-mode UI into a continuously running loop.
    ui.stack("loop.driver")
        .size(1.0f, 1.0f)
        .onFrame([](float deltaSeconds) {
            // app::update() runs the runtime twice per frame — the second pass
            // with deltaSeconds == 0, to settle the re-compose that onFrame
            // itself requested — so this fires twice per rendered frame.
            if (deltaSeconds <= 0.0f) {
                return;
            }
            g_loop.elapsed += deltaSeconds;
            ++g_loop.frames;
        })
        .build();

    // Text exercises the freetype + font-fallback path, rects the OpenGL
    // primitive path.
    const float trackWidth = std::max(120.0f, screen.width - 64.0f);

    ui.text("title")
        .position(32.0f, 40.0f)
        .size(trackWidth, 30.0f)
        .text("own main")
        .fontSize(22.0f)
        .lineHeight(28.0f)
        .color({0.94f, 0.96f, 1.0f, 1.0f})
        .build();

    ui.text("stats")
        .position(32.0f, 76.0f)
        .size(trackWidth, 24.0f)
        .text(std::format("frame {} / {}   t = {:.2f}s", g_loop.frames, kFrameBudget, g_loop.elapsed))
        .fontSize(15.0f)
        .lineHeight(20.0f)
        .color({0.58f, 0.68f, 0.80f, 1.0f})
        .build();

    const float progress = static_cast<float>(g_loop.frames) / static_cast<float>(kFrameBudget);

    ui.rect("track")
        .position(32.0f, 150.0f)
        .size(trackWidth, 6.0f)
        .radius(3.0f)
        .color({0.16f, 0.19f, 0.24f, 1.0f})
        .build();

    ui.rect("fill")
        .position(32.0f, 150.0f)
        .size(std::min(1.0f, progress) * trackWidth, 6.0f)
        .radius(3.0f)
        .color({0.98f, 0.55f, 0.36f, 1.0f})
        .build();
}

} // namespace app

namespace {

float dpiScaleOf(GLFWwindow* window) {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    return (scaleX + scaleY) * 0.5f;
}

// Framebuffer pixels per window point; EUI needs it to map cursor coordinates
// onto the framebuffer.
float pointerScaleOf(GLFWwindow* window) {
    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return 1.0f;
    }
    return 0.5f * (static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth) +
                   static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight));
}

int runWindow() {
    core::platform::repairCurrentWorkingDirectory();
    core::render::initializeRenderBackendLoader();

    glfwSetErrorCallback([](int code, const char* description) {
        std::println("glfw error {}: {}", code, description != nullptr ? description : "");
    });

    if (!glfwInit()) {
        std::println("glfwInit() failed");
        return 10;
    }

    const app::DslAppConfig& config = app::dslAppConfig();

    core::window::WindowCreateRequest request;
    request.width = config.windowWidthValue;
    request.height = config.windowHeightValue;
    request.title = config.titleValue.c_str();
    request.renderApi = core::render::windowRenderApi();

    auto* window = static_cast<GLFWwindow*>(core::window::createWindow(request));
    if (window == nullptr) {
        std::println("core::window::createWindow() failed");
        glfwTerminate();
        return 11;
    }

    auto backend = core::render::createRenderBackend(window);
    if (!backend || !backend->initialize()) {
        std::println("core::render::createRenderBackend() failed");
        core::window::destroyWindow(window);
        glfwTerminate();
        return 12;
    }

    if (!app::initialize(window)) {
        std::println("app::initialize() failed");
        backend.reset();
        core::window::destroyWindow(window);
        glfwTerminate();
        return 13;
    }

    // Frame pacing is NOT optional in a hand-written loop: the OpenGL backend
    // calls glfwSwapInterval(0) in initialize(), so present() never blocks on
    // vsync and an unpaced loop runs at thousands of fps and pins a core.
    // Upstream's main does the same thing with monitor refresh-rate detection on
    // top.
    const double frameInterval = config.fpsValue > 0.0 ? 1.0 / config.fpsValue : 0.0;
    double lastFrameTime = core::window::timeSeconds();
    double nextFrameTime = lastFrameTime;

    while (!glfwWindowShouldClose(window) && g_loop.frames < kFrameBudget) {
        glfwPollEvents();

        const double now = core::window::timeSeconds();
        const float deltaSeconds = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            glfwWaitEvents();
            lastFrameTime = core::window::timeSeconds();
            nextFrameTime = lastFrameTime;
            continue;
        }

        const float dpiScale = dpiScaleOf(window);
        const float pointerScale = pointerScaleOf(window);

        backend->makeCurrent();

        // Layout + compose() + input dispatch; also fires the .onFrame()
        // callback that advances g_loop.
        app::update(window, deltaSeconds, framebufferWidth, framebufferHeight, dpiScale, pointerScale);

        backend->beginFrame({
            window,
            core::window::nativeWindowInfo(window),
            framebufferWidth,
            framebufferHeight,
            dpiScale
        });
        {
            // app::render() draws through core::render::activeRenderBackend();
            // this guard is what installs ours as the active one. Without it the
            // frame is silently dropped.
            core::render::ScopedRenderBackend scopedBackend(*backend);
            app::render(framebufferWidth, framebufferHeight, dpiScale);
        }
        backend->present();

        // app::render() -> Runtime::render() calls beginRenderFrameStats(); this
        // moves the counters into lastRenderFrameStats() so they can be read.
        core::render::publishRenderFrameStats();
        const core::render::RenderFrameStats& stats = core::render::lastRenderFrameStats();
        g_loop.rectDraws += stats.rectDraws;
        g_loop.textDraws += stats.textDraws;

        nextFrameTime += frameInterval;
        const double slack = nextFrameTime - core::window::timeSeconds();
        if (slack > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(slack));
        } else {
            // Fell behind: resynchronise instead of accumulating debt.
            nextFrameTime = core::window::timeSeconds();
        }
    }

    core::releaseInputQueue(window);
    backend->makeCurrent();
    backend->releaseRenderCache();
    {
        core::render::ScopedRenderBackend scopedBackend(*backend);
        app::shutdown();
    }
    backend.reset();
    core::window::destroyWindow(window);
    glfwTerminate();

    if (g_loop.frames < kFrameBudget) {
        std::println("loop stalled: {} of {} frames in {:.2f}s",
                     g_loop.frames, kFrameBudget, g_loop.elapsed);
        return 14;
    }
    if (g_loop.rectDraws <= 0) {
        std::println("no rect geometry reached the backend over {} frames", g_loop.frames);
        return 15;
    }
    if (g_loop.textDraws <= 0) {
        // Text needs the freetype path AND a usable font. On a box with no
        // system font at any of upstream's Linux fallback paths this is the
        // failure you get, and it is worth distinguishing from a dead backend.
        std::println("no text geometry reached the backend over {} frames "
                     "(freetype path or system font missing)", g_loop.frames);
        return 16;
    }

    std::println("compat.eui-neo[own main]: ok ({} frames in {:.2f}s, {} rect / {} text draws)",
                 g_loop.frames, g_loop.elapsed, g_loop.rectDraws, g_loop.textDraws);
    return 0;
}

} // namespace

int main() {
    // Compiling + linking runWindow() is the headless test: it is the only thing
    // in this index that instantiates core::window / core::render / app::* from a
    // consumer TU. The run itself needs a display (opt-in).
    if (std::getenv("MCPP_RUN_WINDOW") != nullptr) {
        return runWindow();
    }
    std::println("compat.eui-neo[own main]: linked; window run is opt-in (MCPP_RUN_WINDOW=1)");
    return 0;
}
