# Design doc: add `compat.eui-neo` (EUI-NEO 0.5.3)

Date: 2026-07-29

Follow-up to `.agents/docs/2026-07-28-add-eui-compat-deps-plan.md`, which landed the six
dependency packages as PR #131 and deferred the framework itself. This is that framework.

**Depends on the five packages in `2026-07-29-add-gui-backend-packages-plan.md`**
(compat.vulkan / vulkan-headers / curl / sdl2 / glx-headers) — the `vulkan`, `sdl2` and
`network` features resolve against them.

Rebuilt from upstream rather than derived from the first revision of this PR — see
"What the first revision got wrong" below for why.

## Motivation

[EUI-NEO](https://github.com/sudoevolve/EUI-NEO) is a declarative retained-mode C++17 UI
framework. This adds it in **header-compat shape only**: a consumer writes `#include <eui_neo.h>`. The C++23 module surface
(`import eui;`) is explicitly out of scope — upstream ships no module interface units, so
`import` would mean hand-authoring wrappers over 40+ component headers. Header compat is a
prerequisite for that work, not an alternative to it.

## Source and version

| | |
|---|---|
| Upstream | `https://github.com/sudoevolve/EUI-NEO` |
| Version | `0.5.3` (latest release, published 2026-07-27) |
| Tarball | `archive/refs/tags/v0.5.3.tar.gz` |
| sha256 | `6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1` (computed twice, stable) |
| Wrap dir | `EUI-NEO-0.5.3/` — absorbed by the standard `*/` glob prefix, no `install()` hook |
| CN mirror | `gitcode.com/mcpp-res/eui-neo` @ `0.5.3`, byte-identical (see below) |
| License | Apache-2.0 |

## Shape decision: C++ source compat (Form B), deps reused from the index

Upstream vendors its whole dependency set under `3rd/` — freetype, glfw, libpng, zlib,
glad, tray, yyjson, md4c, all fully checked in (no submodules). **None of them are built
here.** Each already exists in this index at the same version upstream pins, and building
them once for the ecosystem is the entire point of having them:

| upstream `3rd/` | index package | version match |
|---|---|---|
| `3rd/freetype` | `compat.freetype` | 2.13.3 |
| `3rd/libpng-1.6.43` | `compat.libpng` | 1.6.43 |
| `3rd/zlib-1.3.1` | `compat.zlib` | 1.3.2 |
| `3rd/glfw` | `compat.glfw` | 3.4 |
| `3rd/glad` | `compat.glad` | 0.0.0-651a425 — the exact commit `3rd/dependencies.cmake` fetches |
| `3rd/tray` | `compat.tray` | 0.0.0-8dd1358 |
| `3rd/yyjson-0.12.0` | `compat.yyjson` | 0.12.0 |
| `3rd/md4c` | `compat.md4c` | 0.5.3 (feature-gated) |

`3rd/` still sits on the include path: `stb_image.h`, `nanosvg.h` and `nanosvgrast.h` are
genuinely vendored single-file headers at its root, and the sources include them as
`"3rd/stb_image.h"`.

The build recipe tracks upstream `CMakeLists.txt` v0.5.3: `CORE_SOURCES` + the OpenGL
backend + glfw's `ime_bridge.c` = 20 translation units.

### Backend selection is a build-time constant, not a feature

Upstream picks the render and window backend at configure time and compiles exactly one
in. Only `opengl` + `glfw` are modelled. `vulkan`, `sdl2` and `network` would need
`compat.vulkan`, `compat.sdl2` and `compat.curl`, none of which exist in this index —
declaring features whose deps cannot resolve only moves the failure downstream, so they
are omitted rather than stubbed.

### One TU goes through a generated stub (mcpp#233/#240)

`core/platform/platform.cpp` is **not** declared directly. mcpp emits every package's
objects into a single flat per-link `obj/` directory keyed by source basename, so upstream's
`core/platform/platform.cpp` and `compat.glfw`'s `src/platform.c` both want `platform.o`.

This is not theoretical. On a cold 646-object link with the naive declaration, `platform.o`
was absent entirely and **both** packages lost their TU — neither `core::platform::*` nor
`_glfwSelectPlatform` reached the binary. It still linked green, because the minimal test
happened to reference neither. A real EUI application would not be so lucky, and neither
would any consumer of `compat.glfw` that links a sibling package with a `platform.*`.

The fix is the technique `compat.opencv5` established for its `modules/*/src` collisions: a
uniquely named `generated_files` stub that `#include`s the real source.

```lua
generated_files = {
    ["mcpp_generated/eui_neo_platform_tu.cpp"] = "#include \"core/platform/platform.cpp\"\n",
},
sources = { …, "mcpp_generated/eui_neo_platform_tu.cpp" },
```

Renaming one side is enough: with `platform.o` no longer contested, glfw's object survives
too. `tests/examples/eui-neo` now calls `core::platform::consumeFrameRequest()` so a
regression becomes an undefined reference rather than a silent pass.

A scan of eui-neo's 20 sources against every transitively linked package
(freetype/libpng/zlib/glfw/glad/tray/yyjson/opengl + the X11 stack) found `platform` to be
the **only** collision.

### Linux tray is deliberately a no-op

Upstream sets `EUI_TRAY_APPINDICATOR=1` only when pkg-config finds **both** GTK3 and
libappindicator. This index carries neither, so the Linux profile sets no tray define at
all and `tray_bridge.c` compiles its `EUI_TRAY_HAS_BACKEND 0` stub — which is exactly what
upstream produces on a machine without those dev packages. Windows (`EUI_TRAY_WINAPI`) and
macOS (`EUI_TRAY_APPKIT`, Cocoa-native) get real tray backends.

## Backend selection — and why `default` cannot express it

Upstream selects both backends at configure time and compiles exactly one of each:
`core/render/render_backend.cpp` dispatches on
`#if defined(EUI_RENDER_BACKEND_OPENGL) … #elif defined(…VULKAN)`, and
`core/window/window_backend.cpp` on `#if defined(EUI_WINDOW_BACKEND_SDL2)` / else-GLFW.
Define both halves of either pair and the first silently wins — the caller's choice is
ignored, and since neither backend runs headless, CI would never notice.

mcpp features are additive and there is no `default-features = false` (mcpp#242 — the same
wall that stopped the ffmpeg trimmed profiles, see
`2026-07-19-compat-ffmpeg-trimmed-profiles-decision.md`). Three encodings were tried and
**each failed silently**, so each is written down:

| encoding | behaviour |
|---|---|
| `default = { defines/sources/deps = … }` | **inert** — never applied at all |
| `default = { implies = { … } }` | **always** applied, even when the consumer names a different feature |
| package-level define + additive feature | a feature cannot unset a define |

Verified on both mcpp 0.0.109 (the CI pin) and 2026.7.29.1 (latest at time of writing) —
the behaviour is identical, so this is not something a version bump fixes.

The middle row is what makes the first row dangerous: an early revision read it as Cargo's
rule ("suppressed when features are named"), which looks identical from one observation.
What disproved it was the plain `eui-neo` member passing its smoke test while
`createRenderBackend()` compiled to its `#else` branch and returned a null backend, because
nothing in a headless test ever asks for one.

### What works

mcpp passes `-DMCPP_FEATURE_<NAME>` for every enabled feature into the package's own
translation units. The exclusivity is therefore resolved in the preprocessor, by a
force-included generated header, and the features carry only sources and dependencies:

```c
#if defined(MCPP_FEATURE_VULKAN)
#  define EUI_RENDER_BACKEND_VULKAN 1
#else
#  define EUI_RENDER_BACKEND_OPENGL 1
#endif
#if defined(MCPP_FEATURE_SDL2)
#  define EUI_WINDOW_BACKEND_SDL2 1
#endif
```

Strictly better than anything built on `default` would have been: naming an unrelated
feature no longer drops the backends.

```toml
eui-neo = "0.5.3"                              # opengl + glfw
eui-neo = { …, features = ["vulkan"] }         # vulkan + glfw
eui-neo = { …, features = ["sdl2"] }           # opengl + SDL2
eui-neo = { …, features = ["vulkan","sdl2"] }  # vulkan + SDL2
eui-neo = { …, features = ["markdown"] }       # opengl + glfw, markdown on
```

### `cflags` is C-only

Force-including that header exposed a second problem: **mcpp routes `cflags` to C
translation units and `cxxflags` to C++ ones.** With `-include` in `cflags` alone, exactly
three objects received it — `ime_bridge.c`, `native_bridge.c`, `tray_bridge.c` — and every
`.cpp` compiled without it.

An earlier revision of this descriptor carried
`cflags = { "-DEUI_RENDER_BACKEND_OPENGL=1" }` and nothing else, so `render_backend.cpp`
never saw it: the package built, linked, passed its tests, and had no render backend at
all. Both lists now carry the flag; `NOMINMAX` on Windows got the same treatment.

### Verified structurally

A passing test proves nothing here. What is checked is which backend each member's dispatch
translation unit actually references:

| member | `render_backend.o` → | `window_backend.o` → |
|---|---|---|
| `eui-neo` (no features) | `OpenGLRenderBackend` | `glfwCreateWindow` |
| `eui-neo-vulkan` | `VulkanRenderBackend` | `glfwCreateWindow` |
| `eui-neo-sdl2` | `OpenGLRenderBackend` | `SDL_CreateWindow` |
| `eui-neo-markdown` | `OpenGLRenderBackend` | `glfwCreateWindow` |

The last row is the one that catches a `default`-based regression.

> Superseded in part by `.agents/docs/2026-07-30-eui-neo-window-and-app-main-members.md`:
> every member above is headless, so the backends were verified by symbol reference
> rather than by rendering. `eui-neo-window` and `eui-neo-app-main` now assert
> `rectDraws`/`textDraws` from a real frame under `MCPP_RUN_WINDOW=1`.

## Features

| feature | gates | default |
|---|---|---|
| `vulkan` | Vulkan render backend + `compat.vulkan` | off (OpenGL) |
| `sdl2` | SDL2 window backend + `compat.sdl2` | off (GLFW) |
| `network` | `compat.curl` + `EUI_HAS_CURL=1` | off |
| `app-main` | `core/app/glfw_app_main.cpp` — upstream's `int main()` and render loop | off |
| `app-main-sdl2` | `core/app/sdl2_app_main.cpp`, the SDL2 counterpart | off |
| `markdown` | `compat.md4c` dep + `EUI_HAS_MD4C=1` interface define | off |

**`app-main`** is a sources-only gate, the direct analogue of `compat.gtest`'s `main`
(gtest_main.cc). CMake adds this file per-application (`EUI_APP_MAIN_SOURCE`), never to
the library, for the same reason it is opt-in here: a consumer with its own `main()` must
not be handed a second one. A real EUI application enables it and supplies only
`app::dslAppConfig()` and `app::compose()`.

> Sharpened later: mcpp links a dependency's objects *eagerly*, so `glfw_app_main.o` is
> always in the link rather than only when `main` is undefined. An `app-main` project may
> therefore contain no `main()` of its own **at all** — including every `mcpp test` TU.
> `tests/examples/eui-neo-app-main` is built around that constraint; see
> `.agents/docs/2026-07-30-eui-neo-window-and-app-main-members.md`.

**`markdown`** is the more interesting one. `components/markdown.h` is header-only and
compiles one of *two* definitions of `detail::parseMarkdownBlocks` depending on
`EUI_HAS_MD4C` — the md4c parser, or a fallback that wraps the entire source in one
Paragraph. The library itself gains no translation unit either way, so the whole feature
lives on the consumer side. That is why the define goes in `defines` (an INTERFACE define,
propagated to the consumer's TUs) rather than `cflags` (package-private): with `cflags`,
md4c would link and the component would still silently compile out.

Note the skill doc's "features 仅能门控 sources" reflects mcpp 0.0.68. On the pinned
0.0.109, `defines` / `deps` / `implies` / `requires` / `provides` are all accepted — see
`compat.eigen`, `chriskohlhoff.asio`, `compat.spdlog`.

## Consumer contract (worth knowing before using this package)

`eui_neo.h` pulls in `eui/detail/dsl_app_impl.h`, which emits `app::update()` /
`app::render()` into the *consumer's* translation unit and leaves two symbols for the
application to define:

```cpp
namespace app {
const DslAppConfig& dslAppConfig();
void compose(eui::Ui& ui, const eui::Screen& screen);
}
```

Omitting them is a link error, not a compile error. This mirrors upstream's
`examples/*.cpp`, all of which define exactly these two. Both test members do the same.

## Verification

Local, mcpp **0.0.109** (matching `validate.yml` `env.MCPP_VERSION`), linux-x86_64, gcc 16.1.0.

All four workspace members pass, cold, on all three platforms in CI:

```
$ mcpp test -p eui-neo
compat.eui-neo smoke test: ok (parsed eui-neo v3, markdown gated off)

$ mcpp test -p eui-neo-markdown
compat.eui-neo[markdown]: ok (2 blocks, h1 = 'Heading')

$ mcpp test -p eui-neo-vulkan     # windows asserts the default OpenGL build
compat.eui-neo[vulkan]: ok (backend=vulkan, loader api 1.4.357)

$ mcpp test -p eui-neo-sdl2
compat.eui-neo[sdl2,network]: ok (SDL driver=dummy, curl 8.21.0 ssl=OpenSSL/3.5.1)
```

### The library really is built

The first revision passed CI while compiling **zero** translation units (see below), so
this is checked against the objects rather than inferred from a green test. Per-package
counts from the build cache:

```
compat.eui-neo@0.5.3     20 objs   <- exactly the 20 declared sources
compat.freetype@2.13.3   29 objs
compat.glfw@3.4          23 objs
compat.libpng@1.6.43     15 objs
compat.zlib@1.3.2        15 objs
compat.x11@1.8.13       406 objs
compat.yyjson@0.12.0      1 obj
compat.glad@…             1 obj
compat.tray@…             1 obj
```

The default member's assertions run on `eui::json::Document` (`core/platform/json.cpp`)
and `core::platform::consumeFrameRequest()` (`core/platform/platform.cpp`) — an empty or
partial library fails at **link** time instead of silently passing.

### The mcpp#233 collision fix, measured

Cold link of `tests/examples/eui-neo`, before vs after routing platform.cpp through the
generated stub:

| | objects in the link | `core::platform::*` | `_glfwSelectPlatform` |
|---|---|---|---|
| before | 646 | absent | absent |
| after | 648 | `eui_neo_platform_tu.o` | `platform.o` |

Two objects recovered: eui-neo's TU, and `compat.glfw`'s `platform.o` that the contested
name had been taking down with it.

### Real GUI verification, on a machine with a display

CI is headless, so the graphics path was unverified. Run locally on a workstation with an
X display and an RTX 4080 (OpenGL 4.6, Vulkan 1.3, plus lavapipe): a harness that opens a
real window, creates the render backend through `core::render::createRenderBackend`, and
drives three full frames (`beginFrame` → `ensureRenderCache` → `beginRenderCacheFrame` →
`blitRenderCache` → `present`).

**All four backend combinations render, with no environment variables set:**

| combination | result |
|---|---|
| OpenGL + GLFW | ok — 3 frames presented at 320x240 |
| OpenGL + SDL2 | ok — 3 frames presented |
| Vulkan + GLFW | ok — 3 frames presented |
| Vulkan + SDL2 | ok — 3 frames presented |

Getting the Vulkan half there took two fixes, both of which are in this PR and neither of
which was visible from a headless test.

#### 1. `compat.vulkan-runtime` — host ICDs were unreachable

The loader found every ICD manifest on the system and then failed to `dlopen` a single
driver:

```
DRIVER: Found the following files: /usr/share/vulkan/icd.d/lvp_icd.json …  (9 of them)
ERROR: libvulkan_lvp.so: cannot open shared object file: No such file or directory
```

The libraries are in `/usr/lib/x86_64-linux-gnu`. What cannot reach them is the process:
an mcpp binary runs under mcpp's **own** glibc (`interp: …/xim-x-glibc/2.39/…`, rpath
covering only mcpp's own trees), so a bare-soname `dlopen` never searches the host's path.
That is deliberate — it is what makes builds reproducible — and it is exactly the problem
`compat.glx-runtime` already solves for OpenGL, which is *why* the OpenGL rows passed from
the start.

`compat.vulkan-runtime` is the Vulkan counterpart: a symlink farm plus
`runtime.library_dirs`, no vendored driver. Instance extensions went 4 → 22.

Two details worth keeping:

- **Versioned sonames only.** mcpp puts `runtime.library_dirs` on the LINK line too, so a
  bare `libxcb.so` in the farm shadows this index's own `compat.xcb` and the link fails on
  `XauDisposeAuth`. Versioned names are invisible to the linker and are exactly what
  `dlopen` asks for.
- **The closure must be complete.** A farm with `libxcb.so.1` but not `libXau.so.6`
  shadows a host copy that *would* have resolved, and the executable then fails to start.

#### 2. `compat.vulkan` had to be a shared library

With the ICDs reachable, Vulkan + GLFW rendered but Vulkan + SDL2 still failed —
`createWindow`, then `createSurface` once a shared loader was on the path. The cause is
structural: `SDL_CreateWindow(SDL_WINDOW_VULKAN)` calls `SDL_Vulkan_LoadLibrary(NULL)`,
which **dlopens `libvulkan.so.1`** and resolves surface creation through whatever it finds.
Against a statically linked loader the application ends up with two of them — its own for
`vkCreateInstance`, SDL's for `vkCreateXlibSurfaceKHR` — and the surface call gets an
instance its loader never saw.

So `compat.vulkan` builds `kind = "shared", soname = "libvulkan.so.1"` on Linux, the same
shape the X11 family in this index already uses. The declaration is **inside the linux
block**, not at the top: a shared target propagates `-fPIC` to consumers, and clang rejects
that outright for the msvc target. A platform block's `targets` does override the base one
(`compat.ffmpeg` declares one per platform), so Windows keeps a plain lib around its import
library. Everything converges on one object: the
application links it, GLFW is handed its `vkGetInstanceProcAddr` through
`glfwInitVulkanLoader`, and SDL's `dlopen` lands on it by soname. That is also simply what
the Vulkan loader is designed to be.

#### One upstream-shaped bug found

`glfwInitVulkanLoader` must be called **before** `glfwInit`. Upstream's
`glfw_app_main.cpp` gets this right (line 400, one above its `glfwInit`); a consumer
writing its own entry point has to as well, or GLFW reports
`GLFW_API_UNAVAILABLE "Vulkan: Loader not found"`.

### `app-main`, and what is NOT covered### `app-main`, and what is NOT covered

`app-main` has no workspace member, because a member that enables it cannot run on a CI
runner: the feature's whole point is that `main()` comes from upstream's render loop, which
calls `glfwInit()` and opens a window. Verified out-of-tree on linux-x86_64 instead, with a
throwaway member whose only source defines `dslAppConfig()` + `compose()` and no `main`:

```
Compiling compat.eui-neo v0.5.3
Compiling probe (test)
  Running bin/probe
probe ... FAIL (exit 255)          <- headless; glfwInit() has no display

obj/glfw_app_main.o present, 648 objects in the link
nm: 0000000000001897 T main        <- main comes from the feature, not the consumer
```

So the gate compiles, supplies `main()`, and links against a consumer that has none. It is
**not** verified on macOS or Windows, and the render loop is never executed anywhere.

More generally, nothing in this package's CI test surface draws a frame. Every member is
headless by construction, so what CI proves is: the library builds on three platforms, its
umbrella header is consumable, and the non-graphical facades (JSON, platform frame flags,
markdown parsing) behave. Window creation, GL context setup, text rasterization, image
decode, input and IME are all **unexercised**, as are the Windows WinAPI and macOS AppKit
tray paths — they compile, they have never run.

Not modelled at all, and therefore not usable through this package: the `vulkan` render
backend, the `sdl2` window backend, and `network` / `EUI_HAS_CURL`. Linux tray is a
compiled no-op. See the sections above for why.

### Feature verification (both directions)

- **negative** — `tests/examples/eui-neo` does not request `markdown`, and asserts
  `parseMarkdownBlocks("# Heading\n\nBody text.\n")` returns the degenerate single
  Paragraph. Feature on by accident ⇒ this member fails.
- **positive** — `tests/examples/eui-neo-markdown` requests it long-form and asserts an h1
  block with text `Heading` plus ≥2 blocks. Interface define failing to propagate ⇒ this
  member fails (it `#if !defined(EUI_HAS_MD4C)`s to an explicit failure first).

## CN mirror

Published to gitcode `mcpp-res` per `docs/cn-mirror.md`, so the `url` is a
`{ GLOBAL, CN }` table on all three platforms:

```
repo   https://gitcode.com/mcpp-res/eui-neo
CN     https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.3/eui-neo-0.5.3.tar.gz
```

Closed-loop verified — the asset is the byte-identical GLOBAL tarball, not a repack:

```
CN http=200
GLOBAL=6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1
CN    =6951ac330d0307c633bafe720b7888bf32785103eb16973adb4ee05ef06e64d1
BYTE-IDENTICAL
```

## What the first revision got wrong

Recorded because the failure mode is subtle and CI did not catch it. "It" here is the
descriptor this PR originally proposed, before the rewrite.

1. **The package compiled nothing, and CI was green.** Its `install()` hook guessed the
   tarball's wrap directory as `main` / `EUI-NEO-<version>` / `EUI-NEO-main`; the actual
   name was `EUI-NEO-M-main` (it pointed at a personal fork's branch archive). All three
   guesses missed, `os.tryrm(install_dir())` then removed the install dir, `os.mv` failed,
   and the hook `return true`d anyway — so mcpp recorded a successful install over a
   directory that did not exist. Every source glob then matched zero files. The smoke test
   was `import std; println(...)` referencing no EUI symbol, so even the link succeeded.
   Measured: `0 objs` for `compat.eui-neo`, against 29/15/23 for freetype/libpng/glfw.
2. **The `install()` hook should not exist.** House style (`docs/package-types.md`,
   `compat.md4c`, `compat.libpng`) absorbs the wrap layer with a `*/` glob prefix; the first
   revision removed those prefixes to compensate for its own hook.
3. **"Form B → Form A include propagation not supported" was a misdiagnosis.**
   `tests/examples/freetype` does `#include <ft2build.h>` against a Form B package and
   passes on main. Headers were unreachable because the verdir did not exist.
4. **`sha256 = ""`** disabled integrity verification on all three platforms, against a
   **moving branch head** (`refs/heads/main`) of a personal fork, while `repo` pointed at
   upstream. Upstream tags `v0.5.3`; this descriptor pins it with a real digest.
5. **Dead paths.** `3rd/yyjson-0.12.0/src/yyjson.c` and `3rd/tray` do not exist in that
   fork's archive, and `compat.yyjson` / `compat.tray` / `compat.glad` — three of the six
   packages #131 added *for this framework* — were not declared as deps at all.
6. **Unconditional `-DEUI_TRAY_APPINDICATOR=1` on Linux**, which upstream only sets when
   GTK3 + libappindicator are present. It would have required GTK3 headers this index does
   not carry.
7. **Features with no resolvable deps** (`vulkan`, `sdl2`, `network`) — defines only, no
   packages behind them.

## Windows: `-fno-char8_t`

`parseWindowsSelection()` in `core/platform/platform.cpp` pushes `path::u8string()` into a
`std::vector<std::string>`. C++20 changed that return type to `std::u8string`, so the line
does not compile at this index's c++23 floor — upstream builds at `CMAKE_CXX_STANDARD 17`
and never sees it. It sits inside `#if defined(_WIN32)`, so Linux and macOS do not either.

`language = "c++17"` is not an option (mcpp accepts c++23 and up). The root cause is
`char8_t` rather than the standard level — every STL selects the `u8string()` return type
on `__cpp_char8_t` — so the Windows profile carries `cxxflags = { "-fno-char8_t" }` and the
package stays at c++23 everywhere.

Worth fixing upstream: `wideToUtf8()` already sits eight lines above and does the right
thing. Until then this is what keeps the descriptor on a real upstream release tag instead
of a fork carrying the patch (which is what the first revision did).

Note this only became visible once `platform.cpp` was actually being compiled — the
mcpp#233 collision above had been silently dropping the TU, so no compiler ever saw the
line.

## Follow-up

- Upstream PR for the `u8string()` line, after which `-fno-char8_t` can go.
- A GUI smoke test that CI can actually run — Xvfb plus Mesa llvmpipe would make the
  harness above reproducible on a runner.
- `compat.vulkan` / `compat.sdl2` / `compat.curl` would unlock the corresponding backends.
- The C++23 module layer (`import eui;`) remains open, and now has a working header-compat
  base to build on.
