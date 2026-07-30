// compat.eui-neo `app-main` FEATURE: there is no main() in this file on purpose.
// The entry point is upstream's core/app/glfw_app_main.cpp, which the feature
// adds to the library — it owns the window, the GL backend and the render loop,
// and calls back into the two symbols an EUI application must supply:
// app::dslAppConfig() and app::compose(). That is the whole shape being pinned
// here, and it is the shape a real EUI-NEO application uses.
//
// tests/examples/eui-neo-window asserts the OTHER half of the same feature pair:
// the same UI driven by a hand-written main(), with `app-main` NOT enabled.
//
// Two things make this member work under `mcpp test`:
//
//  1. The link IS the primary assertion. `main` has to come from the package;
//     if the feature ever stops gating glfw_app_main.cpp in, this member fails
//     with `undefined reference to 'main'`. Conversely, if the feature ever
//     stopped being opt-in, every OTHER member would fail with
//     `multiple definition of 'main'` — mcpp links a dependency's objects
//     eagerly, so there is no lazy-archive escape hatch.
//
//  2. Running a window needs a display, so the run is opt-in via
//     MCPP_RUN_WINDOW=1, as in tests/examples/imgui-window. Because the entry
//     point belongs to the package, the gate cannot live in main() — it lives in
//     a namespace-scope constructor, which runs before it. Headless CI links the
//     real binary, executes the guard, and exits 0.
//
// Builds on all three platforms: GLFW is upstream's default window backend
// everywhere, and nothing below is platform-specific.
#include <eui_neo.h>

#include "core/render/render_backend.h"

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

// Frames to render before asking the framework's loop to exit, so the opt-in run
// terminates on its own instead of waiting for a human to close the window.
constexpr int kFrameBudget = 60;

int g_frames = 0;

// Accumulated from the PREVIOUS frame's render stats — the framework's loop
// publishes them after each present(), and compose() is the only place an
// app-main consumer gets to look. A frame counter alone would pass against a
// null backend; these prove the OpenGL backend issued real geometry.
long long g_rectDraws = 0;
long long g_textDraws = 0;

// Runs before the package's main(). See point 2 above.
struct WindowRunGate {
    WindowRunGate() {
        if (std::getenv("MCPP_RUN_WINDOW") != nullptr) {
            return;
        }
        std::println("compat.eui-neo[app-main]: linked, main() supplied by the package; "
                     "window run is opt-in (MCPP_RUN_WINDOW=1)");
        std::exit(0);
    }
};

const WindowRunGate g_gate;

} // namespace

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("compat.eui-neo — app-main")
        .pageId("eui_neo_app_main")
        .clearColor({0.06f, 0.07f, 0.09f, 1.0f})
        .windowSize(420, 240)
        .fps(60.0)
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    // A node carrying .onFrame() marks the runtime as animating, which is what
    // keeps the framework's loop rendering continuously instead of parking in
    // glfwWaitEvents(). It is also the only per-frame hook an app-main consumer
    // has, so the frame budget is enforced from here.
    ui.stack("loop.driver")
        .size(1.0f, 1.0f)
        .onFrame([](float deltaSeconds) {
            // app::update() runs the runtime twice per frame — the second pass
            // with deltaSeconds == 0, to settle the re-compose that onFrame
            // itself requested — so this fires twice per rendered frame.
            if (deltaSeconds <= 0.0f) {
                return;
            }

            const core::render::RenderFrameStats& stats = core::render::lastRenderFrameStats();
            g_rectDraws += stats.rectDraws;
            g_textDraws += stats.textDraws;

            if (++g_frames < kFrameBudget) {
                return;
            }

            // The entry point belongs to the package, so there is no main() of
            // ours to return a code from — exit() is how this member fails.
            if (g_rectDraws <= 0) {
                std::println("no rect geometry reached the backend over {} frames", g_frames);
                std::exit(2);
            }
            if (g_textDraws <= 0) {
                // Text needs the freetype path AND a usable font; worth
                // distinguishing from a dead backend.
                std::println("no text geometry reached the backend over {} frames "
                             "(freetype path or system font missing)", g_frames);
                std::exit(3);
            }

            std::println("compat.eui-neo[app-main]: ok ({} frames, {} rect / {} text draws)",
                         g_frames, g_rectDraws, g_textDraws);

            // The framework's loop keeps our window's context current across
            // update and render, so this is the window it is driving.
            if (GLFWwindow* window = glfwGetCurrentContext()) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        })
        .build();

    // Text exercises the freetype + font-fallback path, rects the OpenGL
    // primitive path — i.e. the parts a headless smoke test cannot reach.
    ui.text("title")
        .position(32.0f, 40.0f)
        .size(std::max(120.0f, screen.width - 64.0f), 30.0f)
        .text("app-main")
        .fontSize(22.0f)
        .lineHeight(28.0f)
        .color({0.94f, 0.96f, 1.0f, 1.0f})
        .build();

    ui.text("stats")
        .position(32.0f, 76.0f)
        .size(std::max(120.0f, screen.width - 64.0f), 24.0f)
        .text(std::format("frame {} / {}", g_frames, kFrameBudget))
        .fontSize(15.0f)
        .lineHeight(20.0f)
        .color({0.58f, 0.68f, 0.80f, 1.0f})
        .build();

    const float progress = static_cast<float>(g_frames) / static_cast<float>(kFrameBudget);
    const float trackWidth = std::max(120.0f, screen.width - 64.0f);

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
        .color({0.36f, 0.62f, 0.98f, 1.0f})
        .build();
}

} // namespace app
