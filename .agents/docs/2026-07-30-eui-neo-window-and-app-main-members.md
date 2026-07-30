# Design doc: window + app-main test members for compat.eui-neo

Date: 2026-07-30

Follows `.agents/docs/2026-07-29-add-eui-neo-plan.md`, which landed `compat.eui-neo`
0.5.3 with four workspace members (`eui-neo`, `eui-neo-markdown`, `eui-neo-vulkan`,
`eui-neo-sdl2`).

## Gap

All four existing members are **headless by construction**. They assert on
`eui::json::Document`, `core::platform::` frame flags, `components::detail::`
markdown blocks and — structurally, via `nm` — which backend the dispatch TUs
reference. None of them:

* creates a window,
* initialises the OpenGL render backend,
* enters a render loop, or
* renders text (the freetype path).

Two consequences:

1. `compat.eui-neo` shipped an `app-main` feature that **nothing built**. Upstream's
   `core/app/glfw_app_main.cpp` is 584 lines of platform code and was compiled by
   zero members on zero platforms.
2. A consumer that drives EUI itself — `core::window::createWindow()` +
   `core::render::createRenderBackend()` + `app::update`/`app::render` — was never
   exercised, so it was unknown whether that surface is even reachable through the
   descriptor's `include_dirs`.

This adds one member per entry-point shape. They are the two shapes an EUI
application can have, and they are mutually exclusive per project (features resolve
per consuming project).

## Members

| member | `app-main` | entry point | new coverage |
|---|---|---|---|
| `eui-neo-window` | off | hand-written `main()` in the test TU | `core::window`, OpenGL backend, `ScopedRenderBackend`, paced loop, freetype text |
| `eui-neo-app-main` | **on** | upstream `glfw_app_main.cpp` | the feature's source compiles; `main` really comes from the package |

Both build on **all three platforms**, deliberately not `cfg`-gated. `imgui-window`
and `gui-stack` are linux-only because they pull the X11 packages directly;
`compat.eui-neo` and `compat.glfw` are not, and the four existing eui-neo members
already build on every runner. Audited before un-gating: every `core/` header these
TUs reach (`window_types.h`, `window_backend.h`, `render_backend.h`,
`primitive_geometry.h`, `render_surface.h`, `platform.h`, `input_state.h`,
`input_types.h`, `ime_bridge.h`) is platform-clean — no `windows.h`, no Cocoa, no GL
— and every GLFW entry point used is portable. `core::releaseInputQueue` reaches
`eui_ime_uninstall_message_filter`, which `ime_bridge.c` defines on all three
platforms.

Both add `GLFW_INCLUDE_NONE` before `<GLFW/glfw3.h>`, as `core/input/input_state.h`
does, so GLFW does not pull a GL header of its own choosing (`GL/gl.h` on Windows,
the deprecated `OpenGL/gl.h` on macOS). EUI reaches GL through `compat.glad`.

Neither re-declares `compat.glfw`, even though both call GLFW directly (EUI's
`core::window` facade exposes no event-pump entry point). mcpp propagates a
transitive dependency's `include_dirs` and link inputs to the consumer, and
`compat.eui-neo` already depends on `compat.glfw` on every platform. Verified by
dropping the line and rebuilding both members. This matches the existing
convention: `eui-neo-sdl2` includes `<SDL.h>` and `<curl/curl.h>`, `eui-neo-vulkan`
calls `vkEnumerateInstanceVersion`, and neither declares those packages.

## Why the run is opt-in

A window needs a display; CI runners have none. Same answer as `imgui-window`:
build + link is the headless test, and `MCPP_RUN_WINDOW=1` opts into the real run.

```
$ mcpp test -p tests/examples/eui-neo-window            # headless CI
compat.eui-neo[own main]: linked; window run is opt-in (MCPP_RUN_WINDOW=1)

$ MCPP_RUN_WINDOW=1 mcpp test -p tests/examples/eui-neo-window
compat.eui-neo[own main]: ok (60 frames in 0.98s, 120 rect / 120 text draws)
```

`eui-neo-app-main` cannot put that gate in `main()` — it does not have one. It goes in
a **namespace-scope constructor**, which runs before the package's `main()`:

```cpp
struct WindowRunGate { WindowRunGate() { if (!getenv("MCPP_RUN_WINDOW")) exit(0); } };
const WindowRunGate g_gate;
```

Both runs are bounded by a 60-frame budget so the opt-in path terminates on its own
instead of waiting for a human to close the window.

## Why `app-main` cannot carry tests — and what that pins

mcpp links a dependency's objects **eagerly**, not as lazily-selected archive members.
`glfw_app_main.o` is therefore always in the link, not only when `main` is still
undefined. Verified by adding a `main()` to an `app-main` project:

```
ld: obj/glfw_app_main.o: in function `main':
    core/app/glfw_app_main.cpp:398: multiple definition of `main';
    obj/probe.o: first defined here
```

So an `app-main` project may not contain **any** TU of its own that defines `main` —
including every `mcpp test` TU. `eui-neo-app-main` is built around that: no `main()`,
per `tests/examples/catch2-main`. This makes the member a two-sided assertion:

* if `app-main` stops contributing its source → `undefined reference to 'main'` here;
* if `app-main` stops being opt-in → `multiple definition of 'main'` in *every other*
  member.

The descriptor comment on the feature is updated to say this; the previous wording
("a consumer that has its own main() must not get a second one") describes the intent
but understates the consequence.

## A green test is not a rendered frame

Per the `eui-neo` plan's "the library really is built" section: a frame counter only
proves `app::update()` ran. `app::update()` reaches `compose()` and the `.onFrame()`
hook without touching the render backend at all, so a member counting frames would
pass against a null backend — which is exactly the failure mode the first revision of
this descriptor shipped (`createRenderBackend()` returning null behind a green CI).

Both members therefore assert on `core::render::lastRenderFrameStats()` and require
`rectDraws > 0` **and** `textDraws > 0` over the frame budget. `textDraws` is split out
from `rectDraws` on purpose: text is the only path that needs freetype plus a usable
system font, and "no font at any of upstream's Linux fallback paths" is a different
failure from "dead backend".

## Loop mechanics worth recording

Two non-obvious things the loop in `eui-neo-window` has to get right, both discovered
by running it:

* **Pacing is mandatory.** `OpenGLRenderBackend::initialize()` calls
  `glfwSwapInterval(0)`, so `present()` never blocks on vsync. An unpaced loop ran at
  ~8300 fps and pinned a core; the member paces off `DslAppConfig::fpsValue`.
* **`onFrame` fires twice per rendered frame.** `app::update()` runs the runtime a
  second time with `deltaSeconds == 0` to settle the re-compose that `onFrame` itself
  requested (`dsl_app_impl.h`, the `composeRequested()` branch). Counting every
  invocation reports 2x the real frame rate — gate on the delta.

Neither is a descriptor bug; both are upstream behaviour a consumer must know, and
neither was discoverable from the headless members.

## Why the mcpp pin does NOT move

Checked before writing any of this, because the encoding in this descriptor is ugly
enough to be worth re-testing against a newer client. `MCPP_VERSION` stays at
**0.0.109**.

The three capabilities that could simplify `compat.eui-neo` all landed **at or before
0.0.109**, which is already the pin: `default-features = false` (#242, 0.0.98),
feature→feature forwarding (#243, 0.0.99), obj-path disambiguation (#233/#240,
0.0.97/0.0.98). The four releases after it — `2026.7.27.1`, `.28.2`, `.29.1`,
`.29.2` — carry the date-based version scheme, the private-glibc `LD_LIBRARY_PATH`
fix (#291), 4-segment version parsing, xlings pin hygiene, `[build].defines` before
the P1689 scan (#297), the musl/MinGW `build.mcpp` host helper (#298), `mcpp add`
index validation (#307) and SemVer routing (#309). `git log v0.0.109..HEAD` over the
feature/resolver/plan sources returns nothing that touches feature semantics.

One correction to the descriptor's own comment while re-probing: it claimed "there is
no `default-features = false` (mcpp#242)". That is wrong — #242 shipped in 0.0.98.
The accurate statement is that its `seedDefault` gate is **manifest**-side, and a
`default` feature declared in an xpkg **descriptor** is never seeded at all, so the
consumer has nothing to switch off. Re-probed by giving the package
`default = { defines = { "MCPP_PROBE_DEFAULT_APPLIED=1" } }` and checking the macro
from a plain consumer TU: absent on 0.0.109. The generated-header + forced-`-include`
encoding therefore stays as it is. Worth filing upstream as a descriptor-side gap in
#242's coverage.

## Verification

Local, mcpp **0.0.109** (matching `validate.yml` `env.MCPP_VERSION`), linux-x86_64,
gcc 16.1.0, X11 display with NVIDIA GL 4.6.

```
                              headless        MCPP_RUN_WINDOW=1
eui-neo-window                ok (0.08s)      ok (60 frames in 0.98s, 120 rect / 120 text draws)
eui-neo-app-main              ok (0.08s)      ok (60 frames, 118 rect / 118 text draws)
eui-neo            (existing) ok
eui-neo-markdown   (existing) ok
```

`mcpp xpkg parse`, `check_mirror_urls.lua`, `check_package_name.lua` and
`check_cross_package_refs.lua` all pass on the touched descriptor. The descriptor
change is comment-only.

**macOS and Windows are CI-verified, not locally verified** — there is no runner for
either here. PR #139, run 30502516910: all three `workspace` legs selected the same
six members and passed.

```
selected members: eui-neo-app-main eui-neo-window eui-neo-markdown \
                  eui-neo-sdl2 eui-neo-vulkan eui-neo

workspace (linux)    pass  12m26s
workspace (macos)    pass   6m15s
workspace (windows)  pass   7m50s
```

The `app-main` line on each leg is the assertion that matters, and it is
self-proving:

```
compat.eui-neo[app-main]: linked, main() supplied by the package; window run is opt-in
```

The test TU defines no `main`, so the binary can only run if `glfw_app_main.o`
supplied one. Printing that line at all therefore proves the feature's source
compiled and linked — on Windows and macOS for the first time ever, which is what
exercises the two additions below.

Two things were new there and were the reason to watch the first run:

* `glfw_app_main.cpp` has never been compiled on any platform, so its Windows half
  is newly exercised. Two descriptor additions came out of auditing it, both
  Windows-only and both for that TU alone:

  * **`-D_WIN32_WINNT=0x0A00`** (cflags + cxxflags). `core/app/frame_pacing.h`
    calls `CreateWaitableTimerExW`, which mingw-w64's `winbase.h` and the Windows
    SDK both guard behind `#if _WIN32_WINNT >= 0x0600`. The header sets no floor of
    its own and mingw-w64 has historically defaulted as low as `0x502`, so the
    build would depend on which default the runner's llvm ships. Everything else in
    the package only reaches pre-Vista APIs, which is why it never came up.
  * **`-lkernel32`**. `frame_pacing.h` reaches `CreateWaitableTimerExW` /
    `SetWaitableTimer` / `WaitForSingleObject` / `CloseHandle`. kernel32 is in
    every sane default lib set, so this is belt-and-braces — but the surrounding
    comment in the descriptor is precisely about mcpp not inheriting CMake's
    `CMAKE_C_STANDARD_LIBRARIES`, so naming it is consistent and free.

  `timeBeginPeriod` (winmm) and `MonitorFromWindow` / `GetMonitorInfoW` /
  `EnumDisplaySettingsW` (user32) were already covered by the existing list;
  `glfw3native.h` ships in compat.glfw's `include/GLFW/`.
* these are the first members to link `compat.glfw` off Linux from their **own** code
  rather than through `compat.eui-neo`, which exercises the macOS
  `Cocoa`/`IOKit`/`CoreFoundation` frameworks and the Windows `-lgdi32` from a
  consumer link.

## Not done

* `app-main-sdl2` stays uncovered. It is the same shape against `compat.sdl2`, and
  covering it means a fifth window member for one `sources` line; worth doing if the
  SDL2 window backend gains real users.
* The **opt-in windowed run** has been exercised on linux-x86_64 only. macOS and
  Windows runners are headless, so they get build+link only — same as
  `imgui-window`.
