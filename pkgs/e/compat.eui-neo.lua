-- compat.eui-neo — EUI-NEO, a declarative retained-mode C++17 UI framework.
--
-- Header-compat shape (Form B, `import_std = false`): the ~20 core TUs are
-- compiled into one lib and the public headers are exposed through
-- `include_dirs`, so a consumer writes `#include <eui_neo.h>`. The C++23
-- module surface (`import eui;`) is deliberately NOT modelled here — upstream
-- ships no module interface units, and wrapping 40+ component headers is a
-- separate piece of work.
--
-- Upstream vendors its third-party libraries under `3rd/` (freetype, glfw,
-- libpng, zlib, glad, tray, yyjson, md4c). NONE of those are built here: each
-- one already exists in this index as its own `compat.*` package at the same
-- upstream version, and building them once for the whole ecosystem is the
-- point of having them. `3rd/` is still on the include path because three
-- genuinely vendored single-file headers live at its root (stb_image,
-- nanosvg, nanosvgrast) and the sources include them as `"3rd/stb_image.h"`.
--
-- The build recipe below tracks upstream `CMakeLists.txt` (v0.5.7): CORE_SOURCES
-- plus the OpenGL backend and, for the glfw window backend, `ime_bridge.c`.
-- 0.5.5 grew a Shadertoy subsystem: render_backend.h and include/eui/types.h now
-- include core/render/shadertoy.h unconditionally, and opengl_backend.cpp calls
-- releaseShaderToys(), so shadertoy.cpp / shadertoy_json.cpp / shadertoy_primitive.cpp
-- and opengl_shadertoy.cpp are part of the lib, not optional (vulkan_shadertoy.cpp
-- joins the `vulkan` feature the same way). 0.5.6 adds `core/window/window_input_backend.cpp`
-- to CORE_SOURCES (upstream moved the input/IME event pumping into its own TU) —
-- the ONLY lib source-list change between the two versions; everything else the
-- descriptor names is byte-identical in upstream's CORE_SOURCES.
--
-- 0.5.7: CORE_SOURCES is byte-identical to 0.5.6, so the lib's shape does not
-- change; the version's real moves are the linux tray backend (SNI over GDBus
-- via glib/gio, replacing the dead GTK3+libappindicator path — tray_bridge.c
-- speaks freedesktop StatusNotifierItem when EUI_TRAY_SNI is set, which this
-- descriptor now does on linux, wiring glib through `xim:glib` and the
-- staging install() hook below) and SDL2-only input fixes (SDL_GetMouseFocus,
-- pointer/button mapping) plus X11 resource handling in the SDL2 window
-- backend. The new `EUI_ENABLE_TRAY` option does not affect this package:
-- tray_bridge.c is in CORE_SOURCES unconditionally and the define selects
-- which backend its body compiles to.
--
-- All `mcpp` paths are GLOBS relative to the verdir; the leading `*/` absorbs
-- the GitHub tarball's `EUI-NEO-0.5.7/` wrap layer.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "eui-neo",
    description = "EUI-NEO — declarative retained-mode C++17 UI framework (GLFW + OpenGL)",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/sudoevolve/EUI-NEO",
    type        = "package",

    xpm = {
        linux = {
            -- 0.5.7: tray_bridge.c (:207) speaks freedesktop StatusNotifierItem
            -- over GDBus when EUI_TRAY_SNI is set — no GTK3, no libappindicator.
            -- xim:glib publishes glib-2.0 / gio-2.0 / gobject-2.0 into the SubOS
            -- view; the install() hook at the bottom of this file stages them
            -- into the payload (headers for the compile, sonames for the link
            -- and the runtime closure — the same mcpp#352 boundary as GL).
            deps = { runtime = { "xim:glib@2.80.0" } },
            ["0.5.3"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.3.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.3/eui-neo-0.5.3.tar.gz" },
                sha256 = "6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1",
            },
            -- 0.5.5 has no CN mirror (never published to mcpp-res); a plain-string
            -- url keeps lint green and lets CN users fall back to upstream, per
            -- docs/cn-mirror.md. Flip it to { GLOBAL, CN } if that release ever
            -- gets mirrored — sha256 stays the same.
            ["0.5.5"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.5.tar.gz",
                sha256 = "cf0da91d7544fe406b704922137fd4d55ed080b3e647501e0ca5303abb00eb98",
            },
            ["0.5.6"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.6.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.6/eui-neo-0.5.6.tar.gz" },
                sha256 = "0df8d79897a480566b0989060f206431d12c4a83eb7aef50b8e5d21f1676abf8",
            },
            -- 0.5.7 has no CN mirror yet (never published to mcpp-res); same
            -- plain-string fallback as 0.5.5. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["0.5.7"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.7.tar.gz",
                sha256 = "2d3ec0a36e34b98d13dbdaf67afa4fe178cb4b52841eb17529517cb48be43551",
            },
        },
        macosx = {
            ["0.5.3"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.3.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.3/eui-neo-0.5.3.tar.gz" },
                sha256 = "6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1",
            },
            ["0.5.5"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.5.tar.gz",
                sha256 = "cf0da91d7544fe406b704922137fd4d55ed080b3e647501e0ca5303abb00eb98",
            },
            ["0.5.6"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.6.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.6/eui-neo-0.5.6.tar.gz" },
                sha256 = "0df8d79897a480566b0989060f206431d12c4a83eb7aef50b8e5d21f1676abf8",
            },
            -- 0.5.7 has no CN mirror yet (never published to mcpp-res); same
            -- plain-string fallback as 0.5.5. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["0.5.7"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.7.tar.gz",
                sha256 = "2d3ec0a36e34b98d13dbdaf67afa4fe178cb4b52841eb17529517cb48be43551",
            },
        },
        windows = {
            ["0.5.3"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.3.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.3/eui-neo-0.5.3.tar.gz" },
                sha256 = "6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1",
            },
            ["0.5.5"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.5.tar.gz",
                sha256 = "cf0da91d7544fe406b704922137fd4d55ed080b3e647501e0ca5303abb00eb98",
            },
            ["0.5.6"] = {
                url    = { GLOBAL = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.6.tar.gz",
                           CN     = "https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.6/eui-neo-0.5.6.tar.gz" },
                sha256 = "0df8d79897a480566b0989060f206431d12c4a83eb7aef50b8e5d21f1676abf8",
            },
            -- 0.5.7 has no CN mirror yet (never published to mcpp-res); same
            -- plain-string fallback as 0.5.5. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["0.5.7"] = {
                url    = "https://github.com/sudoevolve/EUI-NEO/archive/refs/tags/v0.5.7.tar.gz",
                sha256 = "2d3ec0a36e34b98d13dbdaf67afa4fe178cb4b52841eb17529517cb48be43551",
            },
        },
    },

    mcpp = {
        language   = "c++23",
        import_std = false,
        c_standard = "c99",

        -- `*/include` carries the umbrella `eui_neo.h` and `eui/*.h`; `*` is the
        -- verdir root, which is what makes the `"components/…"`, `"core/…"` and
        -- `"3rd/stb_image.h"` quoted includes resolve. Upstream marks both PUBLIC.
        -- `mcpp_generated/glib/include/glib-2.0` is the staged glib header tree
        -- (linux SNI tray; see install()); off linux it is an empty directory.
        include_dirs = { "*/include", "*", "mcpp_generated", "mcpp_generated/glib/include/glib-2.0" },

        -- mcpp#233/#240: every package in a link emits its objects into ONE
        -- flat obj/ dir keyed by source basename. Upstream's
        -- `core/platform/platform.cpp` and `compat.glfw`'s `src/platform.c`
        -- both want `platform.o`, and the collision drops BOTH — verified on a
        -- cold 646-object link where neither `core::platform::` nor
        -- `_glfwSelectPlatform` reached the binary. Nothing in the minimal test
        -- referenced them, so it linked green anyway; a real application would
        -- not. Route the TU through a uniquely named stub, the same technique
        -- `compat.opencv5` uses for its `modules/*/src` collisions. Renaming
        -- only this side is enough: with `platform.o` no longer contested,
        -- glfw's own object survives too.
        generated_files = {
            -- Resolves the two exclusive backend choices from the feature flags
            -- mcpp hands us. Force-included into every TU of this package via
            -- the `cflags` below, so it runs before any upstream header looks
            -- at EUI_RENDER_BACKEND_* / EUI_WINDOW_BACKEND_SDL2.
            ["mcpp_generated/mcpp_eui_backends.h"] = [==[
/* Backend selection for compat.eui-neo — see the descriptor's note. */
#pragma once

/* Render backend: vulkan when asked for, OpenGL otherwise. Exactly one. */
#if defined(MCPP_FEATURE_VULKAN)
#  define EUI_RENDER_BACKEND_VULKAN 1
#else
#  define EUI_RENDER_BACKEND_OPENGL 1
#endif

/* Window backend: SDL2 when asked for, GLFW otherwise (GLFW is the absence of
 * the SDL2 define, which is how upstream spells it too). */
#if defined(MCPP_FEATURE_SDL2)
#  define EUI_WINDOW_BACKEND_SDL2 1
#endif
]==],
            ["mcpp_generated/eui_neo_platform_tu.cpp"] = [==[
/* Uniquely named forwarding TU — see the mcpp#233 note in the descriptor. */
#include "core/platform/platform.cpp"
]==],
        },

        -- CMake CORE_SOURCES + the OpenGL render backend + glfw's ime_bridge.
        sources = {
            -- Platform layer
            "*/core/platform/async.cpp",
            -- ime_bridge.c is glfw-specific and rides with the glfw feature.
            "*/core/platform/json.cpp",
            "*/core/platform/native_bridge.c",
            "*/core/platform/network.cpp",
            "*/core/platform/performance_stats.cpp",
            -- core/platform/platform.cpp enters through the generated stub above.
            "mcpp_generated/eui_neo_platform_tu.cpp",
            "*/core/platform/tray_bridge.c",
            -- Render layer (backend-agnostic)
            "*/core/render/image.cpp",
            "*/core/render/image_facade.cpp",
            "*/core/render/image_source.cpp",
            "*/core/render/primitive.cpp",
            "*/core/render/render_backend.cpp",
            "*/core/render/shadertoy.cpp",
            "*/core/render/shadertoy_json.cpp",
            "*/core/render/shadertoy_primitive.cpp",
            "*/core/render/stb_image_impl.cpp",
            "*/core/render/text.cpp",
            -- OpenGL backend and the GLFW IME bridge are UNCONDITIONAL sources.
            -- Which of them the preprocessor keeps is decided by the generated
            -- backend header below, not by whether they were compiled.
            "*/core/render/opengl/opengl_backend.cpp",
            "*/core/render/opengl/opengl_image.cpp",
            "*/core/render/opengl/opengl_primitives.cpp",
            "*/core/render/opengl/opengl_shadertoy.cpp",
            "*/core/render/opengl/opengl_text.cpp",
            "*/core/platform/ime_bridge.c",
            -- Window layer
            "*/core/window/window_backend.cpp",
            -- 0.5.6: input/IME event pumping moved out of window_backend.cpp into
            -- its own TU (upstream CORE_SOURCES). GLFW branch rides on ime_bridge.h
            -- (ime_bridge.c, already compiled) + glfw; SDL2 branch needs only SDL.
            "*/core/window/window_input_backend.cpp",
        },

        targets = { ["eui-neo"] = { kind = "lib" } },

        -- Every entry replaces a directory upstream vendors under `3rd/`, at the
        -- same version upstream pins:
        --   freetype 2.13.3, libpng 1.6.43, zlib (3rd/zlib-1.3.1), glfw 3.4,
        --   glad 651a425 (the exact commit 3rd/dependencies.cmake fetches),
        --   yyjson 0.12.0, tray 8dd1358.
        -- `tray` is a dep on all three platforms for uniformity even though
        -- `tray_bridge.c` only reaches `tray.h` under EUI_TRAY_WINAPI (see below).
        deps = {
            ["compat.freetype"] = "2.13.3",
            ["compat.libpng"]   = "1.6.43",
            ["compat.zlib"]     = "1.3.2",
            ["compat.yyjson"]   = "0.12.0",
            -- The DEFAULT backends' packages live in the base dep set, not in
            -- the `default` feature: mcpp applies a default feature's `defines`
            -- and `sources` but IGNORES its `deps` (verified — a default member
            -- resolved only freetype/libpng/tray/yyjson and then failed on
            -- <GLFW/glfw3.h>). Non-default features' `deps` do work, which is
            -- why `vulkan` and `sdl2` can carry theirs.
            --
            -- Consequence: a consumer picking `vulkan` or `sdl2` still builds
            -- these. They are cheap — compat.opengl is header-only plus an
            -- anchor, compat.glad is one TU — and correctness beats saving
            -- compat.glfw's 23 TUs.
            ["compat.opengl"]   = "2026.05.31",
            ["compat.glad"]     = "0.0.0-651a425",
            ["compat.glfw"]     = "3.4",
            ["compat.tray"]     = "0.0.0-8dd1358",
        },

        -- ── Backend selection ──────────────────────────────────────────────
        --
        -- The render and window backends are mutually exclusive build-time
        -- choices: core/render/render_backend.cpp is
        -- `#if defined(EUI_RENDER_BACKEND_OPENGL) … #elif defined(…VULKAN)`,
        -- and core/window/window_backend.cpp is `#if EUI_WINDOW_BACKEND_SDL2`
        -- / else-GLFW. Define both halves of either pair and the first one
        -- silently wins, ignoring what the consumer asked for.
        --
        -- mcpp features are purely additive here. `default-features = false`
        -- does exist (mcpp#242, since 0.0.98) — but its `seedDefault` gate lives
        -- on the MANIFEST side, and a `default` feature declared in an xpkg
        -- DESCRIPTOR is never seeded to begin with, so there is nothing for the
        -- consumer to switch off. Re-probed on 0.0.109 by giving this package a
        -- `default = { defines = {...} }` and checking the macro from a plain
        -- consumer: absent. Unchanged in the newest mcpp (2026.7.29.2) — nothing
        -- has touched the feature system since 0.0.109, so a version bump buys
        -- no simplification of the encoding below.
        --
        -- The obvious encodings all fail on 0.0.109, each in its own way — all
        -- three verified with probes, because each failure is silent:
        --
        --   * `default = { defines/sources/deps = … }` is INERT. Not
        --     "suppressed when features are named" — never applied at all. A
        --     member depending on the package plainly built with no render
        --     backend and still passed its smoke test, because nothing in the
        --     test reached one.
        --   * `default = { implies = { … } }` is the opposite: ALWAYS applied,
        --     including when the consumer names a different feature. Routed
        --     this way, asking for `vulkan` keeps OpenGL enabled too.
        --   * a plain package-level define cannot be turned off by a feature,
        --     since features only add.
        --
        -- What does work is that mcpp passes `-DMCPP_FEATURE_<NAME>` for every
        -- enabled feature, to the package's own translation units. So the
        -- exclusivity is resolved in the preprocessor, by a force-included
        -- header, and the features themselves only need to carry sources and
        -- dependencies. Consumers get the same answer through the features'
        -- interface `defines`.
        --
        --   eui-neo = "0.5.3"                                -> opengl + glfw
        --   eui-neo = { …, features = ["vulkan"] }           -> vulkan + glfw
        --   eui-neo = { …, features = ["sdl2"] }             -> opengl + SDL2
        --   eui-neo = { …, features = ["vulkan", "sdl2"] }   -> vulkan + SDL2
        --   eui-neo = { …, features = ["markdown"] }         -> opengl + glfw
        --
        -- Note the last line: unlike an encoding built on `default`, naming an
        -- unrelated feature no longer silently drops the backends.
        -- BOTH lists, and that is not redundant: mcpp routes `cflags` to C
        -- translation units and `cxxflags` to C++ ones. A define placed only in
        -- `cflags` reaches ime_bridge.c / native_bridge.c / tray_bridge.c and
        -- NOTHING else — which is exactly how an earlier revision of this
        -- descriptor shipped `-DEUI_RENDER_BACKEND_OPENGL=1` that
        -- render_backend.cpp never saw, leaving createRenderBackend() on its
        -- `#else` branch returning a null backend. Verified by symbol
        -- inspection, since it links and runs cleanly either way.
        cflags   = { "-include", "mcpp_eui_backends.h" },
        -- `-fno-char8_t` is package-wide since 0.5.5: the Windows-only char8_t
        -- break of 0.5.3 (parseWindowsSelection) is no longer the only one —
        -- resolveResourcePath() (platform.cpp:616) and the new Shadertoy TUs
        -- return path::u8string() as std::string on EVERY platform. Root cause
        -- is char8_t, not the standard level; everything else stays at c++23.
        cxxflags = { "-include", "mcpp_eui_backends.h", "-fno-char8_t" },

        features = {
            ["vulkan"] = {
                defines = { "EUI_RENDER_BACKEND_VULKAN=1" },
                sources = {
                    "*/core/render/vulkan/vulkan_backend.cpp",
                    "*/core/render/vulkan/vulkan_cache.cpp",
                    "*/core/render/vulkan/vulkan_image.cpp",
                    "*/core/render/vulkan/vulkan_polygon.cpp",
                    "*/core/render/vulkan/vulkan_primitives.cpp",
                    "*/core/render/vulkan/vulkan_shadertoy.cpp",
                    "*/core/render/vulkan/vulkan_text.cpp",
                },
                deps = { ["compat.vulkan"] = "1.4.357.0" },
            },
            -- ── Window backend ────────────────────────────────────────────
            -- Exclusive in the same way and for the same reason as the render
            -- backend: core/window/window_backend.cpp is
            -- `#if defined(EUI_WINDOW_BACKEND_SDL2)` / else-GLFW, and
            -- ime_bridge.c is GLFW-only (upstream adds it to CORE_SOURCES only
            -- when EUI_WINDOW_BACKEND is glfw).

            ["sdl2"] = {
                -- The define is for the CONSUMER's translation units; this
                -- package's own get it from mcpp_eui_backends.h.
                defines = { "EUI_WINDOW_BACKEND_SDL2=1" },
                deps    = { ["compat.sdl2"] = "2.32.10" },
            },

            -- ── Optional capabilities ─────────────────────────────────────

            -- core/platform/network.cpp is already in the base source list and
            -- compiles to stubs without this define, so the feature costs a
            -- dependency and a define rather than a translation unit.
            ["network"] = {
                defines = { "EUI_HAS_CURL=1" },
                deps    = { ["compat.curl"] = "8.21.0" },
            },

            -- Upstream's GLFW entry point, which owns `int main()` and drives
            -- the render loop. CMake adds it per-APP (EUI_APP_MAIN_SOURCE), not
            -- to the lib, so it is opt-in here for the same reason
            -- `compat.gtest`'s `main` feature is: a consumer that has its own
            -- main() must not get a second one. A real EUI application enables
            -- this and supplies only app::dslAppConfig() + app::compose().
            --
            -- Sharper than "must not get a second one": mcpp links a
            -- dependency's objects EAGERLY, not as lazily-selected archive
            -- members, so `glfw_app_main.o` is always in the link rather than
            -- only when `main` is still undefined. Enabling this feature is
            -- therefore incompatible with ANY translation unit of the consumer
            -- that defines main() — including every `mcpp test` TU, which means
            -- an app-main project cannot carry its own tests/. Verified: adding
            -- one yields `multiple definition of 'main'` from
            -- glfw_app_main.cpp:398. tests/examples/eui-neo-app-main is
            -- structured around that constraint (no main() at all, and its
            -- opt-in window run is gated in a namespace-scope constructor
            -- because there is no main() of ours to gate it in);
            -- tests/examples/eui-neo-window is the same UI with the feature OFF
            -- and a hand-written loop.
            ["app-main"] = { sources = { "*/core/app/glfw_app_main.cpp" } },
            -- Same gate for the SDL2 window backend. Upstream picks between the
            -- two by EUI_APP_MAIN_SOURCE; here the consumer picks by name, and
            -- must pick the one matching its window backend.
            ["app-main-sdl2"] = { sources = { "*/core/app/sdl2_app_main.cpp" } },
            -- `components/markdown.h` is header-only and guards its body on
            -- EUI_HAS_MD4C, so markdown lives entirely on the CONSUMER side —
            -- the lib itself gains no translation unit from it. That is why
            -- the define goes in `defines` (an INTERFACE define, propagated to
            -- the consumer's TUs) rather than `cflags` (package-private):
            -- without it reaching the consumer, md4c would link but the
            -- component would still compile out.
            ["markdown"] = {
                defines = { "EUI_HAS_MD4C=1" },
                deps    = { ["compat.md4c"] = "0.5.3" },
            },
        },

        -- ── Platform-specific ──────────────────────────────────────────────

        windows = {
            -- Upstream: EUI_TRAY_WINAPI + NOMINMAX, winmm/urlmon/shell32/
            -- user32/imm32/pdh. ole32 comes with urlmon's COM entry points.
            -- NOMINMAX is needed by the C++ TUs too (windows.h reaches them
            -- through eui_neo.h), hence both lists; EUI_TRAY_WINAPI only gates
            -- tray_bridge.c, but keeping the pair symmetrical is cheaper than
            -- re-deriving which is which.
            --
            -- `_WIN32_WINNT` is for the `app-main` feature's TU, which nothing
            -- compiled until tests/examples/eui-neo-app-main existed:
            -- core/app/frame_pacing.h calls CreateWaitableTimerExW, and both
            -- mingw-w64's winbase.h and the Windows SDK guard that declaration
            -- behind `#if _WIN32_WINNT >= 0x0600`. The header sets no floor of
            -- its own, so it inherits the toolchain default — mingw-w64 has
            -- historically defaulted as low as 0x502. Pin the floor rather than
            -- depend on which default the runner's llvm ships; 0x0A00 is what
            -- upstream effectively builds against via the MSVC SDK, and a
            -- command-line define is respected by _mingw.h's `#ifndef` guard.
            -- The rest of the package only reaches pre-Vista APIs, which is why
            -- this never came up before.
            cflags  = { "-DEUI_TRAY_WINAPI=1", "-DNOMINMAX", "-D_WIN32_WINNT=0x0A00" },
            -- Upstream builds at CMAKE_CXX_STANDARD 17; this index's floor is
            -- c++23. `-fno-char8_t` is applied PACKAGE-WIDE (base cxxflags)
            -- since 0.5.5, not here: 0.5.3 only tripped on char8_t inside the
            -- Windows-only `parseWindowsSelection()`, but 0.5.5's
            -- resolveResourcePath() and the Shadertoy TUs return
            -- path::u8string() as std::string on every platform. Worth fixing
            -- upstream; until then this keeps us on a real upstream release
            -- tag rather than a fork carrying the patch.
            cxxflags = { "-DEUI_TRAY_WINAPI=1", "-DNOMINMAX", "-D_WIN32_WINNT=0x0A00" },
            -- Upstream lists winmm/urlmon/shell32/user32/imm32/pdh and stops
            -- there, because CMake's MSVC default `CMAKE_C_STANDARD_LIBRARIES`
            -- already drags in kernel32/user32/gdi32/shell32/ole32/comdlg32/…
            -- mcpp links only what the descriptor names, so the ones
            -- platform.cpp actually reaches have to be spelled out:
            -- comdlg32 for GetOpenFileNameW + CommDlgExtendedError, ole32 for
            -- urlmon's COM entry points. (Pdh*, Imm*, timeBeginPeriod,
            -- URLDownloadToFileA and ShellExecuteA are covered by the upstream
            -- list.) Like the char8_t break above, this only showed up once the
            -- mcpp#233 collision stopped dropping the TU.
            --
            -- kernel32 for the `app-main` TU: core/app/frame_pacing.h reaches
            -- CreateWaitableTimerExW / SetWaitableTimer / WaitForSingleObject /
            -- CloseHandle, and glfw_app_main.cpp reaches timeBeginPeriod (winmm,
            -- already listed) plus MonitorFromWindow / GetMonitorInfoW /
            -- EnumDisplaySettingsW (user32, already listed). kernel32 is part of
            -- every sane Windows default lib set, so this is belt-and-braces for
            -- the one TU in this package that had never been compiled — naming
            -- it costs nothing and the comment above is precisely about mcpp not
            -- inheriting CMake's defaults.
            ldflags = {
                "-lwinmm", "-lurlmon", "-lshell32",
                "-luser32", "-limm32", "-lpdh", "-lole32",
                "-lcomdlg32", "-lkernel32",
            },
        },

        macosx = {
            -- Upstream `enable_language(OBJC)` + LANGUAGE OBJC on the three
            -- bridge files; the AppKit tray path is Cocoa-native and never
            -- includes tray.h.
            cflags   = { "-DEUI_TRAY_APPKIT=1" },
            ldflags  = { "-framework", "Cocoa", "-lobjc" },
            flags = {
                { glob = "*/core/platform/native_bridge.c", cflags = { "-x", "objective-c" } },
                { glob = "*/core/platform/tray_bridge.c",   cflags = { "-x", "objective-c" } },
                { glob = "*/core/platform/ime_bridge.c",    cflags = { "-x", "objective-c" } },
            },
        },

        linux = {
            -- 0.5.7: tray_bridge.c (:207) speaks freedesktop StatusNotifierItem
            -- over GDBus when EUI_TRAY_SNI is set — no GTK3, no libappindicator.
            -- xim:glib (declared at the xpm→linux level) lands the libraries in
            -- the SubOS view; the install() hook below stages them plus the
            -- headers into mcpp_generated/glib/ so the compile and the runtime
            -- closure both resolve inside the hermetic boundary (the same
            -- mcpp#352 problem as GL on the host, the same staging shape as
            -- compat.glx-runtime).
            -- The -L is RELATIVE to the payload: mcpp resolves it against
            -- the install dir (same convention as compat.openblas's
            -- "-Llib"). It is load-bearing, not decorative: with only
            -- runtime.library_dirs the staged dir reaches links as
            -- -Wl,-rpath, which lld happens to search for -l but GNU ld
            -- does not — on the GNU-ld toolchain -lglib then falls through
            -- to the host's /lib64 (wrong glib, or none at all).
            cflags  = { "-DEUI_TRAY_SNI=1", "-pthread" },
            ldflags = { "-Lmcpp_generated/glib/lib",
                        "-lpthread", "-ldl",
                        "-lglib-2.0", "-lgio-2.0", "-lgobject-2.0" },
            runtime = {
                library_dirs = { "mcpp_generated/glib/lib" },
            },
        },
    },
}

import("xim.libxpkg.pkginfo")
import("xim.libxpkg.system")
import("xim.libxpkg.log")

-- Libraries the SNI tray backend needs at runtime, COPIED into the payload
-- so the link and the runtime closure both resolve inside one directory on
-- the consumer's RPATH. Copies, not symlinks: the subos view is
-- project-local while this payload is shared across projects, and a staged
-- symlink dangles the moment the project that provided the view is not the
-- one building (observed: `gio/gio.h file not found` off a shared payload
-- pointing into a wiped project .mcpp/). The payload dir is
-- version-immutable and already on the RPATH, so copies lose nothing.
-- Two origins, in preference order:
--
--   * the glib sonames and the xim-provided transitives (libffi / pcre2 /
--     zlib — xim:glib's own deps) come from the subos view when published,
--     else straight from the xim store. The view alone is NOT enough at
--     hook time: a package already cached in the store skips the config()
--     run that publishes it, so on a fresh project view the transitives
--     can be missing even though the store holds them (observed).
--   * libmount / libselinux / libblkid have NO xim provider yet: the
--     xim:glib build links them, no xim package ships them. They are
--     staged from the HOST with a warning, the same host-plane fallback
--     compat.glx-runtime used before xim:graphics existed. The day xim
--     gains util-linux/libselinux packages this branch is dead code;
--     the day a host ships a glibc newer than the payload's, this is
--     the mcpp#352 configuration again and the warn below says so.
local subos_sonames = {
    "libglib-2.0.so", "libglib-2.0.so.0",
    "libgio-2.0.so", "libgio-2.0.so.0",
    "libgobject-2.0.so", "libgobject-2.0.so.0",
    "libgmodule-2.0.so", "libgmodule-2.0.so.0",
    "libgthread-2.0.so", "libgthread-2.0.so.0",
    "libffi.so.8",
    "libpcre2-8.so", "libpcre2-8.so.0",
    "libz.so", "libz.so.1",
}
local host_fallback_sonames = {
    "libmount.so.1",
    "libselinux.so.1",
    "libblkid.so.1",
}
local host_lib_dirs = {
    "/usr/lib/x86_64-linux-gnu", "/lib/x86_64-linux-gnu",
    "/usr/lib64", "/lib64", "/usr/lib", "/lib",
}

function install()
    -- Default extraction, which this package previously got by having no
    -- hook at all: move the unpacked tarball into place (same idiom as
    -- compat.glfw).
    local srcdir = pkginfo.install_file():replace(".tar.gz", "")
    if not os.isdir(srcdir) then
        srcdir = "EUI-NEO-" .. pkginfo.version()
    end
    local idir = pkginfo.install_dir()
    os.tryrm(idir)
    os.mv(srcdir, idir)

    -- Normalize to the single wrap layer the descriptor's `*/` globs are
    -- written against. Whether the tree arrives wrapped depends on the
    -- fetch path: the GitHub tarball keeps its `EUI-NEO-<v>/` top level,
    -- the CN mirror's does not — observed on one machine, one version,
    -- one day apart. Without this, which layout a consumer compiles is a
    -- function of their mirror, and the losing side fails with
    -- `"core/…" file not found` on the quoted includes.
    if os.isdir(path.join(idir, "core")) then
        local tmp = idir .. ".wrap-tmp"
        os.tryrm(tmp)
        os.mv(idir, tmp)
        os.mkdir(idir)
        os.mv(tmp, path.join(idir, "EUI-NEO-" .. pkginfo.version()))
    end

    -- The staging dir exists on EVERY platform so the include_dirs entry
    -- `mcpp_generated/glib/include/glib-2.0` never names a missing path;
    -- only linux populates it. Off linux it stays an empty directory,
    -- which as an -I is inert.
    local gendir = path.join(pkginfo.install_dir(), "mcpp_generated", "glib")
    local geninc = path.join(gendir, "include")
    os.mkdir(path.join(geninc, "glib-2.0"))

    if os.host() ~= "linux" then
        return true
    end

    -- Resolver, in tier order. The subos VIEW (project-local) is consulted
    -- first but is NOT reliable at hook time: entries appear there only
    -- when the providing package's config() runs, and a package already
    -- cached in the store skips that run — observed on a wiped .mcpp/
    -- where the view had the glib sonames but not pcre2/zlib/libffi yet.
    -- The STORE (the xpkgs dir the payloads live in) is the stable source:
    -- a package's files exist there from the moment it is fetched, which
    -- for every xpm-level dep is strictly before this hook runs.
    local subos = system.subos_sysrootdir()
    local subos_lib = path.join(subos, "lib")

    local store_roots = {}
    local function add_root(root)
        if root and root ~= "" and os.isdir(root) then
            for _, r in ipairs(store_roots) do
                if r == root then return end
            end
            table.insert(store_roots, root)
        end
    end
    add_root(path.directory(path.directory(idir)))
    local xlh = os.getenv("XLINGS_HOME")
    if xlh and xlh ~= "" then
        add_root(path.join(xlh, "data", "xpkgs"))
    end

    local from_store = {}
    -- os.files/os.dirs are NOT in the xpm sandbox (install hook died on
    -- exactly that); glob through the shell instead. One match per line,
    -- no-match expands to an ls error that stderr's redirect swallows.
    local function glob(pattern)
        local out = {}
        for line in os.iorun("sh -c 'ls -d " .. pattern .. " 2>/dev/null'"):gmatch("[^\n]+") do
            table.insert(out, line)
        end
        return out
    end
    local function resolve(soname)
        local in_view = path.join(subos_lib, soname)
        if os.isfile(in_view) then
            return in_view, false
        end
        for _, root in ipairs(store_roots) do
            for _, hit in ipairs(glob(root .. "/*/*/lib/" .. soname)) do
                if os.isfile(hit) then
                    return hit, true
                end
            end
        end
        return nil, false
    end

    -- Headers, view first then the store (same race as the sonames).
    local subos_glib_inc = path.join(subos, "usr", "include", "glib-2.0")
    local glib_inc = nil
    if os.isdir(subos_glib_inc) then
        glib_inc = subos_glib_inc
    else
        for _, root in ipairs(store_roots) do
            local hits = glob(root .. "/xim-x-glib/*/include/glib-2.0")
            if #hits > 0 then
                glib_inc = hits[1]
                break
            end
        end
    end
    if not glib_inc then
        log.error("glib-2.0 headers found neither in this subos nor in the "
                  .. "xim store. They come from `xim:glib` (declared as a "
                  .. "runtime dep at the xpm level); if it is declared and "
                  .. "this still fires, the stack did not finish installing")
        return false
    end

    -- COPY, not symlink. The subos view is project-local while this payload
    -- is shared across projects: a symlink staged from the view dangles the
    -- moment another project (or a wiped .mcpp/) is the one that builds —
    -- observed as `gio/gio.h file not found` on a shared payload whose
    -- staged link pointed into a deleted project view. The payload dir is
    -- version-immutable and already on the consumer's RPATH, so copies are
    -- also the stable indirection the glx-runtime note wants; a glib bump
    -- simply rides the next eui-neo version.
    os.tryrm(path.join(geninc, "glib-2.0"))
    os.exec("cp -rL '" .. glib_inc .. "' '" .. geninc .. "'")

    local genlib = path.join(gendir, "lib")
    os.mkdir(genlib)
    local function stage(soname, src)
        os.exec("cp -L '" .. src .. "' '" .. path.join(genlib, soname) .. "'")
    end

    local missing = {}
    for _, soname in ipairs(subos_sonames) do
        local src, store_side = resolve(soname)
        if src then
            stage(soname, src)
            if store_side then
                table.insert(from_store, soname)
            end
        else
            table.insert(missing, soname)
        end
    end
    if #from_store > 0 then
        log.warn("%s came from the xim store, bypassing a subos view that "
                 .. "had not published them yet (publish lists lag payloads — "
                 .. "libffi.so.8: openxlings/xim-pkgindex#676)",
                 table.concat(from_store, ", "))
    end
    if #missing > 0 then
        log.error("%s is in neither this subos nor the xim store — see the "
                  .. "header note about xim:glib", table.concat(missing, ", "))
        return false
    end

    -- The host-plane fallback. Prefer the view/store for these too (a future
    -- xim provider lands there without a descriptor change); only when both
    -- have nothing do the host dirs get searched, loudly.
    local from_host = {}
    for _, soname in ipairs(host_fallback_sonames) do
        local src = resolve(soname)
        if not src then
            for _, dir in ipairs(host_lib_dirs) do
                local cand = path.join(dir, soname)
                if os.isfile(cand) then
                    src = cand
                    break
                end
            end
        end
        if src then
            stage(soname, src)
            local host_origin = false
            for _, dir in ipairs(host_lib_dirs) do
                if src:sub(1, #dir) == dir then
                    host_origin = true
                    break
                end
            end
            if host_origin then
                table.insert(from_host, soname)
            end
        else
            table.insert(missing, soname)
        end
    end
    if #from_host > 0 then
        log.warn("%s staged from the HOST: no xim package provides them yet "
                 .. "(xim:glib links libmount/libselinux; util-linux and "
                 .. "libselinux are not packaged). Built against the host's "
                 .. "glibc, they are the mcpp#352 configuration the day that "
                 .. "glibc overtakes the payload's", table.concat(from_host, ", "))
    end

    -- Versioned-symbol coherence: a HOST libselinux references pcre2
    -- symbols WITH version tags (PCRE2_10.xx), and a versioned reference
    -- cannot bind to the xim pcre2's unversioned definitions — GNU ld
    -- hard-errors at link time, lld lets it through to a runtime warning.
    -- The reverse direction is fine (unversioned refs bind to versioned
    -- defs), so when libselinux came from the host, pcre2 must too:
    -- overwrite the xim-staged copy with the host's, which satisfies
    -- both sides. Ubuntu and Fedora both ship libpcre2-8-0 by default.
    local selinux_from_host = false
    for _, soname in ipairs(from_host) do
        if soname == "libselinux.so.1" then
            selinux_from_host = true
            break
        end
    end
    if selinux_from_host then
        local host_pcre = nil
        for _, dir in ipairs(host_lib_dirs) do
            local cand = path.join(dir, "libpcre2-8.so.0")
            if os.isfile(cand) then
                host_pcre = cand
                break
            end
        end
        if host_pcre then
            stage("libpcre2-8.so.0", host_pcre)
        else
            log.warn("libselinux came from the host but libpcre2-8.so.0 did "
                     .. "not: the xim pcre2's unversioned symbols cannot "
                     .. "satisfy the host libselinux, and GNU ld will fail "
                     .. "the final link")
        end
    end
    if #missing > 0 then
        log.error("%s found neither in this subos, the xim store, nor on "
                  .. "the host", table.concat(missing, ", "))
        return false
    end
    return true
end
