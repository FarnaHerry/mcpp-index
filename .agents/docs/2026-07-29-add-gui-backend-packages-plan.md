# Design doc: five packages behind the GUI/network backends

Date: 2026-07-29

| package | version | what it is |
|---|---|---|
| `compat.vulkan-headers` | 1.4.357.0 | Khronos Vulkan headers |
| `compat.vulkan` | 1.4.357.0 | Khronos Vulkan loader, built from source |
| `compat.curl` | 8.21.0 | libcurl, OpenSSL on unix / Schannel on Windows |
| `compat.sdl2` | 2.32.10 | SDL2 |
| `compat.glx-headers` | 1.7.0 | libglvnd's `GL/glx.h` |

They land together because they are what an alternate-backend GUI stack needs, and because
each one exercised the same class of problem: an upstream that generates its build
configuration, where this index wants a plain source list. They are independently useful —
nothing here depends on any consumer.

All five build from plain source lists: no CMake, no autotools, no `install()` hook.

## `compat.vulkan-headers` + `compat.vulkan`

Split along upstream's own repository split. Headers are header-only (the `compat.opengl`
shape: include root plus an anchor TU). The loader is what a Vulkan program actually
links — `vkCreateInstance` and friends are loader trampolines dispatching into whatever ICD
the system advertises.

**A plain source list works because two things happen to be true:**

- `loader/generated/` (`vk_loader_extensions.c`, `vk_object_types.h`, …) is checked in
  upstream, so CMake's codegen step is unnecessary.
- The assembly path is optional. Upstream compiles `dev_ext_trampoline.c` +
  `phys_dev_ext.c` against hand-written GAS/MASM plus a `gen_defines.asm` that requires
  building *and running* `asm_offset`, then scraping its output with Python. That chain is
  gated on `UNKNOWN_FUNCTIONS_SUPPORTED`; upstream itself degrades when no assembler is
  found and `unknown_function_handling.c` compiles a pure-C fallback. We take the fallback
  deliberately — the cost is that unknown *device* extension entry points get no
  trampoline, which nothing in this index uses.

Two requirements only a real build surfaces:

- `VK_ENABLE_BETA_EXTENSIONS` is mandatory despite the name: the checked-in
  `vk_object_types.h` references `VK_OBJECT_TYPE_CUDA_MODULE_NV`, which the headers only
  declare under it. Without it the loader does not compile at all.
- `SYSCONFDIR` / `FALLBACK_*_DIRS` must arrive as string literals, and
  `-DSYSCONFDIR="/etc"` does not survive mcpp's flag splitting (mcpp#234): `loader.c` sees
  a bare `/etc` and fails with *expected expression before '/' token*. They ride in a
  force-included generated header instead.

WSI is enabled per platform — Xlib + XCB on Linux (which makes `compat.x11` / `compat.xcb`
/ `compat.xorgproto` a **compile** dependency, since `vulkan_xlib.h` includes
`<X11/Xlib.h>`), Metal + MVK on macOS. Search paths are the FHS/XDG defaults because the
package must find the *host's* ICDs.

### Windows is deferred

A statically linked loader is not something upstream supports there: the only static option
in its CMake is `APPLE_STATIC_LOADER`, gated to macOS with the warning that it "will only
work on MacOS and is not supported" elsewhere. Built anyway, the Windows loader links and
then faults at the first entry point (0xC0000005 out of `vkEnumerateInstanceVersion`).
Linux is not covered by that option either, but a static loader is the ordinary case there
and works. Rather than ship a package that crashes, this follows `compat.openssl` and
declares no windows xpm entry; consumers gate with `[target.'cfg(...)']`.

## `compat.curl`

Full-source direct build, which works because curl compiles every unselected protocol and
TLS backend to an EMPTY translation unit (`vtls/gtls.c` is `#ifdef USE_GNUTLS` end to end).
The source list is plain globs; only `lib/dllmain.c` is excluded.

The config header is the awkward part, and only on unix: `lib/config-win32.h` is checked in
and `curl_setup.h` selects it automatically when `HAVE_CONFIG_H` is absent, so Windows needs
nothing generated. linux/macosx get a generated `curl_config.h` branching on `__linux__` /
`__APPLE__`, with the Apple branch deliberately the conservative subset — an omitted
`HAVE_*` costs curl a fallback path, never correctness.

Generating it against the *right* compiler mattered: a first pass with the host `cc`
produced a config asserting `ssize_t` did not exist (its probe failed for an unrelated
sysroot reason), and curl then failed to compile against its own config.

TLS is OpenSSL on linux/macosx via `compat.openssl`, and Schannel on Windows — built into
the OS, and necessary because `compat.openssl` has no Windows build.

### `CURL_STATICLIB`, and a use for an mcpp quirk

`<curl/curl.h>` declares everything `__declspec(dllimport)` unless `CURL_STATICLIB` is set,
so every Windows consumer of a static libcurl needs it — and no consumer should have to
know that. The define has to be **always on AND consumer-visible**, and mcpp has no single
key for that: `cflags` is always on but package-private, while a feature's `defines` reach
the consumer but need naming.

`default = { implies = … }` is the combination that gives both. A default feature's own
`defines` are inert on mcpp (verified on 0.0.109 and 2026.7.29.1), but what it *implies* is
applied unconditionally — including when the consumer names some other feature. That is a
liability for expressing an exclusive choice; here "unconditionally on" is exactly right:

```lua
features = {
    ["default"]   = { implies = { "staticlib" } },
    ["staticlib"] = { defines = { "CURL_STATICLIB" } },
}
```

`tests/examples/curl` deliberately does **not** define it, and `static_assert`s that it
arrived from the package. Verified in both directions — deleting the `default` line makes
the assertion fire.

## `compat.sdl2` and `compat.glx-headers`

SDL is the mirror image of curl: upstream **checks in** `SDL_config_windows.h` and
`SDL_config_macosx.h` and dispatches to them from `include/SDL_config.h`; only Linux falls
through to `SDL_config_minimal.h`, which builds an SDL that can do essentially nothing. So
the generated header reproduces upstream's dispatch for windows/macosx and carries CMake's
Linux output inline.

`compat.glx-headers` exists because SDL's X11 driver includes `<GL/glx.h>`, and **no
package in this index had it** — `GL/glx.h` is not part of the Khronos OpenGL-Registry, so
`compat.opengl` cannot supply it. libglvnd is the canonical provider and is already named
as a wanted package in `2026-06-03-gl-runtime-packages-plan.md`. Headers only;
`compat.glx-runtime` still owns the runtime side. It overlaps `compat.opengl` on
`GL/gl.h`, so depend on one or the other, never both.

## Things only CI could find

Linux passed on the first try; every one of these was silent or misleading locally.

- **SDL2's tag archive cannot be extracted on Windows.** Two POSIX symlinks under
  `android-project-ant/`. The package then "installs" empty: reports success, compiles
  nothing, exports no include dirs, and every consumer fails with `'SDL.h' file not found`
  while the package itself never errors. `chriskohlhoff.asio` hit this and set the
  precedent, so GLOBAL points at a symlink-free repack on `xlings-res/sdl2` with the
  identical bytes mirrored to `mcpp-res`. Only the two symlink entries are removed.
- **SDL on Apple requires ARC.** Its cocoa classes declare `__weak SDL_WindowData *_data`,
  and a `__weak` ivar without `-fobjc-arc` is rejected outright (`'_data' is unavailable`).
  Upstream's CMake hard-fails when the compiler cannot do ARC, for this reason.
- **SDL on Windows needs `HAVE_LIBC`.** Upstream's config only sets it for `_MSC_VER`, and
  this index builds with clang++, so SDL compiled its own `memcpy` and collided with
  libvcruntime.
- **SDL's Windows condvar falls back to `thread/generic/SDL_syscond.c`** — but the rest of
  `thread/generic` shares basenames with `thread/windows`, so only that one file can be
  added. mcpp keys objects by basename in one flat per-link directory (mcpp#233), and
  pulling the whole directory would have them silently displace each other.
- **SDL's macOS config also enables the X11 driver**, dynamically loaded from `/opt/X11`.
  That would make `src/video/x11` a compile dependency on Apple for a driver nobody reaches
  with Cocoa present; the generated header `#undef`s it.
- **`src/joystick/iphoneos` serves macOS too** — `SDL_JOYSTICK_MFI` is on in upstream's
  macosx config and `SDL_gamecontroller.c` calls into it.
- **curl must be told which `strerror_r` it has** and refuses to build otherwise. Apple
  ships the POSIX one; glibc's returns `char*`.
- **curl needs `secur32` on Windows** (`InitSecurityInterfaceA`, for the SSPI auth Schannel
  builds on) and `CoreFoundation` + `SystemConfiguration` on macOS (proxy discovery).
- **The Linux SDL config's X11 had to be switched on by hand** — the CMake probe ran on a
  host without system X11 development headers — and libudev switched off, since it is a
  host library this index does not package.
- **`src/core/linux` is the one directory where a glob over-reaches**: `SDL_dbus.c` /
  `SDL_ibus.c` / `SDL_fcitx.c` / `SDL_ime.c` are not internally guarded and simply fail
  without dbus-1 headers.

## Verification

Cold, mcpp 0.0.109 (the `validate.yml` pin). All three platforms in CI, every member built
and run:

| | linux | macos | windows |
|---|---|---|---|
| `compat.vulkan` | `loader api 1.4.357, WSI trampolines linked` | same | skipped (deferred) |
| `compat.curl` | `ssl=OpenSSL/3.5.1` | `ssl=OpenSSL/3.5.1` | `ssl=Schannel` |
| `compat.sdl2` | `driver=dummy, 320x240 window, events round-tripped` | same | same |

### What the tests can and cannot assert

- **SDL2 genuinely runs.** Its `dummy` video driver is a real driver, so the test executes
  `SDL_Init`, `SDL_CreateWindow`, `SDL_GetWindowSize` and a full event round-trip. It is
  the only one here that can be exercised rather than merely linked.
- **Vulkan is driverless by construction.** The loader advertises WSI *surface* extensions
  only when an ICD supports them, so `VK_KHR_surface` is legitimately absent on a runner
  with no GPU — an early draft asserted on it and failed, which is exactly the "testing the
  runner's hardware" trap. Asserted instead: `vkEnumerateInstanceVersion` (answerable only
  by the loader), `VK_EXT_debug_utils` (loader-implemented, driver-independent), and a
  reference to `vkDestroySurfaceKHR` as link-time proof that `wsi.c` is in the library.
- **curl never opens a connection.** Asserted: init succeeds, `CURL_VERSION_SSL` with a
  non-null `ssl_version` (a curl built without TLS still links and runs — it just cannot do
  https), http+https in the protocol list, and that a handle accepts a real option set.

## CN mirrors

All five published to gitcode `mcpp-res`, byte-identical to GLOBAL, 200 on `curl`. SDL2's
GLOBAL is the `xlings-res/sdl2` repack described above rather than the upstream tag
archive.

## Follow-up

- `compat.vulkan` on Windows, if a supported static-loader path appears upstream.
- `compat.sdl2` currently has no Wayland backend; X11 only on Linux.
