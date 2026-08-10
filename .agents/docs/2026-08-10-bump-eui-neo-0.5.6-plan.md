# Design doc: bump `compat.eui-neo` to 0.5.6

Date: 2026-08-10

Follow-up to `.agents/docs/2026-08-05-add-eui-neo-0.5.5-plan.md` (0.5.3 → 0.5.5).
Upstream released 0.5.6; this bumps the index from 0.5.5 to 0.5.6.

## Source and version

| | |
|---|---|
| Upstream | `https://github.com/sudoevolve/EUI-NEO` |
| Version | `0.5.6` (latest release) |
| Tarball | `archive/refs/tags/v0.5.6.tar.gz` |
| sha256 | `0df8d79897a480566b0989060f206431d12c4a83eb7aef50b8e5d21f1676abf8` (computed twice, stable; 14,794,160 bytes) |
| Wrap dir | `EUI-NEO-0.5.6/` — absorbed by the standard `*/` glob prefix, no `install()` hook |
| CN mirror | **pending** — no `mcpp-res` write access on this machine (same as 0.5.5); plain-string url fallback |
| License | Apache-2.0 (unchanged) |

0.5.5 tarball re-downloaded during this work and its sha256 re-derived — matches the
descriptor's `cf0da91d…`, which validates the download/verify pipeline.

## What changed upstream (0.5.5 → 0.5.6)

The release is substantial (32 commits, 86 files changed, 5 contributors), but almost all
of it is either build-system plumbing, app/example wiring, or the new `modules/` tree
(keyboard/serial modules) — none of which the compat descriptor touches. The only thing
that matters for this index is the **core library source list**:

- `CORE_SOURCES` gains exactly one file: `core/window/window_input_backend.cpp`. Upstream
  moved the input/IME event pumping (mouse buttons, scroll, text composition, key queueing)
  out of `window_backend.cpp` into its own TU.
- Everything else the descriptor names is unchanged in upstream's `CORE_SOURCES` /
  OpenGL block / glfw `ime_bridge.c` / vulkan feature list — verified by diffing the
  0.5.5 and 0.5.6 `CMakeLists.txt` source blocks.
- `3rd/dependencies.cmake` and the `3rd/` directory are **byte-identical** across the two
  tags — all eight vendored dependencies stay at the versions the descriptor pins
  (freetype 2.13.3, libpng 1.6.43, zlib 1.3.1, glfw 3.4, glad 651a425, yyjson 0.12.0, tray
  8dd1358, opengl).

### The new TU compiles on both backends with no new deps

`window_input_backend.cpp` has two `#if`-branches that match the descriptor's generated
backend header exactly:

- **GLFW branch** (default): reaches `core/platform/ime_bridge.h` (`eui_ime_*` — supplied by
  `ime_bridge.c`, already in the base source list) and `<GLFW/glfw3.h>` (`compat.glfw`, already
  a dep). The `core::detail::inputQueue` / `core::queueKeyInput` / `core::queueScrollInput`
  helpers it calls live in the header-only `core/input/input_state.h` — no new TU needed.
- **SDL2 branch** (`sdl2` feature): needs only `<SDL.h>` (`compat.sdl2`, already the feature's dep).

`core/input/` is headers-only in both tags (`input_state.h`, `input_types.h`), so no new
compiled input sources are introduced by the refactor.

## Descriptor changes (`pkgs/e/compat.eui-neo.lua`)

1. `xpm.{linux,macosx,windows}` each gain a `["0.5.6"]` entry (0.5.3/0.5.5 retained).
2. Base `sources` goes 24 → 25: `*/core/window/window_input_backend.cpp` added to the
   Window layer group.
3. Header comment updated to v0.5.6 + the `window_input_backend.cpp` note; wrap-layer
   mention `EUI-NEO-0.5.5/` → `EUI-NEO-0.5.6/`.
4. No feature/deps/cflags changes: the `-fno-char8_t` package-wide flag is still required —
   re-verified that 0.5.6 still returns `path::u8string()` as `std::string` in
   `core/platform/platform.cpp:616`, `core/render/shadertoy_json.cpp:40/260`,
   `core/render/image_source.cpp:216/220/574`. Backend-exclusivity encoding unchanged
   (`mcpp_eui_backends.h` reads `MCPP_FEATURE_VULKAN`/`MCPP_FEATURE_SDL2`, which 0.5.6's
   `window_input_backend.cpp` also keys off).

## CN mirror: pending (fallback form used)

`gtc` still not installed and no `~/.config/gitcode-tool/config.json` on this machine, so the
0.5.6 mirror cannot be published from here. Per `docs/cn-mirror.md`'s no-write-access
fallback, the 0.5.6 entries use a **plain-string url** (GLOBAL upstream release);
`check_mirror_urls.lua` exempts plain strings, so lint stays green and CN users fall back to
upstream. The 0.5.3 entries keep their `{ GLOBAL, CN }` tables.

Once `mcpp-res/eui-neo` has a `0.5.6` release (same tarball as GLOBAL for byte-identical
sha), flip the 0.5.6 `url` to `{ GLOBAL = …, CN =
"https://gitcode.com/mcpp-res/eui-neo/releases/download/0.5.6/eui-neo-0.5.6.tar.gz" }`.
sha256 does not change.

## Test members

All six `tests/examples/eui-neo*` members bumped their `compat.eui-neo` dep from `0.5.5` to
`0.5.6` so the whole feature surface (markdown / vulkan / sdl2+network / app-main / window)
exercises the new version. CI's selective-member logic already maps the descriptor edit to
these members (`pkgs/e/compat.eui-neo.lua` → `$lib = eui-neo` → all six match).

Note: `tests/examples/eui-neo-window` includes `core/input/input_state.h` directly, so the
0.5.6 input refactor is exercised from a consumer TU too, not just through the lib's own
sources.

## API break caught by local verification: `app::*` impl moved out of the umbrella

`eui-neo-window`(the own-main member) failed the first local run at LINK time:

```
undefined symbol: app::initialize(void*) / app::update(...) / app::render(...) / app::shutdown()
```

Root cause is a 0.5.6 umbrella change: `include/eui_neo.h` **dropped
`#include "eui/detail/dsl_app_impl.h"`** (and the Windows `<windows.h>` block it carried).
That header is what emits `app::initialize/update/render/shutdown` + `openWindow()` into a
consumer TU. In 0.5.6 upstream scoped it to the app-main entry points — only
`core/app/glfw_app_main.cpp` and `core/app/sdl2_app_main.cpp` include it directly — so
`eui-neo-app-main` (whose main() IS glfw_app_main.cpp) still links, while the hand-written
`eui-neo-window` main() lost the impl. The other members never call the `app::*` driver
functions, so they pass regardless.

Fix: `tests/examples/eui-neo-window/tests/window.cpp` now includes
`"eui/detail/dsl_app_impl.h"` explicitly — exactly what upstream's own app-main TU does. It
compiles cleanly in a plain consumer TU: no `u8string()`/char8_t in the header or its core
includes, no `EUI_RENDER_BACKEND_*` macro dependency (verified by grep), and `3rd/stb_image.h`
resolves through the descriptor's existing `"*"` include dir. The `eui-neo` member's
`header.cpp` comment was also updated to stop claiming the umbrella emits `app::update/render`.

(No descriptor change was needed for this — the library build is unaffected; it is a
consumer-surface change upstream made.)

## Verification

- `mcpp xpkg parse` (mcpp 2026.8.10.1) → `parse OK`: versions 0.5.3/0.5.5/0.5.6 on all three
  platforms, sources 25, features 6.
- Repo lint loop (lua 5.4): syntax / required fields / no-leading-v / `check_mirror_urls.lua` /
  `check_package_name.lua` all pass.
- Local `mcpp test -p <member>` (mcpp 2026.8.10.1, linux, `MCPP_INDEX_MIRROR=GLOBAL`, cold
  member dirs) — all six members pass:

| member | result |
|---|---|
| eui-neo (default OpenGL+GLFW) | `test result ok. 1 passed; 0 failed` |
| eui-neo-markdown | exit=0 |
| eui-neo-app-main | exit=0 |
| eui-neo-window | `test result ok` (after the dsl_app_impl.h fix) |
| eui-neo-sdl2 (SDL2+network) | `test result ok` — `SDL driver=dummy, curl 8.21.0-DEV ssl=OpenSSL/3.5.1` |
| eui-neo-vulkan | `test result ok` — `backend=vulkan, loader api 1.4.357` |

Local network was intermittently hung on the mcpp registry downloads during this run; the
EUI-NEO / curl / sdl2 / Vulkan-Loader tarballs were verified by sha256 and pre-seeded into
the member package caches (then extracted, matching what mcpp's install does) to keep the
cold builds moving. This is an environment workaround, not a descriptor concern.

> NOTE: local mcpp is 2026.8.10.1; the CI `MCPP_VERSION` pin was ALSO bumped 2026.8.8.2 →
> 2026.8.10.1 as part of this PR (see below). The user opts to verify locally with the
> newest mcpp; its rapid-release policy makes newer preferable.
>
> macosx/windows cannot be exercised on this linux box; the windows `app-main` TU
> (`_WIN32_WINNT`, winmm/user32/pdh) and the mac Cocoa tray link are re-exercised by CI's
> other two runners.

## CI MCPP_VERSION bump 2026.8.8.2 → 2026.8.10.1 (pre-existing linux failure fix)

First CI run of this PR: all six eui-neo members FAILED on **linux** (both default/gcc and
llvm legs, fast ~35s install-phase failures) while macos and windows PASSED. Reproduced
locally with a fresh mcpp 2026.8.8.2 (MCPP_HOME pointed at the tarball root):

```
error: xlings install_packages failed (exit 1) for 'compat.glx-runtime@2026.08.08'
  xlings reported: E_INVALID_INPUT: package 'xim:libglvnd@>=1.7.0.1' not found
```

Root cause is a **pre-existing pin/registry mismatch, not the eui-neo bump**:
`compat.eui-neo` → `compat.glfw` (linux profile) → `compat.glx-runtime@2026.08.08` →
`xim:libglvnd@>=1.7.0.1`. mcpp 2026.8.8.2's registry cannot resolve `xim:libglvnd`; macos
(no X11 profile) and windows are unaffected, and mcpp 2026.8.10.1's registry carries it.
The same failure is why main's `graphics install` check has been red on every recent main
commit (b4e28f2 / d3909f7 / 1e0c71b).

Fix: bump `MCPP_VERSION` in `.github/workflows/validate.yml` to 2026.8.10.1 (kept the 2026.8.8.2
glibc-runtime-binding note, added the libglvnd reason). `index.toml` `min_mcpp` is left at
2026.8.3.3 — no descriptor uses new grammar, and repo history shows the pin moves
independently of the floor. `tests/check_graphics_install_side_effects.sh` only mentions
2026.8.8.2 in comments (the "2026.8.8.2+" floor), which 2026.8.10.1 satisfies.
