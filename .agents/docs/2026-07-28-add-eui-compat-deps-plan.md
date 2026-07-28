# Design doc: add EUI-NEO compat dependency packages

Date: 2026-07-28

## Motivation

[EUI-NEO](https://github.com/sudoevolve/EUI-NEO) is a declarative retained-mode C++20 UI
framework. Its v0.5.3 release depends on several third-party C libraries that have no mcpp
support. This batch adds 6 of those dependencies as `compat.*` packages, plus a placeholder
for the 7th (`compat.eui` — the framework itself) which will follow in a separate PR once
its 8-dependency chain is fully end-to-end tested.

## Packages (this PR)

| # | Package | Version | Shape | Upstream |
|---|---------|---------|-------|----------|
| 1 | `compat.md4c` | 0.5.3 | C source (1 TU) | https://github.com/mity/md4c |
| 2 | `compat.yyjson` | 0.12.0 | C source (1 TU) | https://github.com/ibireme/yyjson |
| 3 | `compat.glad` | 0.0.0-651a425 | C source (1 TU) | https://github.com/libigl/libigl-glad |
| 4 | `compat.tray` | 0.0.0-8dd1358 | Header-only | https://github.com/zserge/tray |
| 5 | `compat.libpng` | 1.6.43 | C source (15 TU) | https://github.com/pnggroup/libpng |
| 6 | `compat.freetype` | 2.13.3 | C source (26 TU) | https://github.com/freetype/freetype |

## Shape decisions

### md4c, yyjson, glad — Single-source C libraries
Each has exactly one `.c` file and one public header. Standard Form B descriptor:
`language = "c++23"`, `c_standard = "c99"`, include directory + single source glob.
No features, no dependencies (glad needs `-ldl` on Linux/macOS for `dlopen`).

### tray — Header-only
Single-file header `tray.h` with `#define TRAY_IMPLEMENTATION` pattern (stb-style).
Uses a generated anchor `.c` file to prevent an empty static library.
No dependencies, no features.

### libpng — 15-source C library with generated config
Upstream tarball does NOT include `pnglibconf.h` — it is generated at CMake time
from `scripts/pnglibconf.h.prebuilt`. We ship the prebuilt version as a
`generated_files` entry, mirroring the approach used by `compat.zlib`.

Depends on `compat.zlib` (already in index). Hardware optimizations disabled
via `-DPNG_HARDWARE_OPTIMIZATIONS=0`.

### freetype — 26-source aggregate build
Uses FreeType's aggregate source file pattern (one `.c` per module directory).
The `ftbase.c` aggregate covers 18 base files but NOT `ftinit.c`, `ftglyph.c`,
`ftbitmap.c`, `ftbbox.c`, or `ftmm.c` — those are compiled individually.

Critical define: `-DFT2_BUILD_LIBRARY` prevents `fterrors.h` from undefining
`FT_ERR_PREFIX`, which is required for aggregate builds to link correctly.

Platform-specific sources: `builds/windows/{ftdebug,ftsystem}.c` on Windows,
`builds/unix/ftsystem.c` on Linux/macOS.

Depends on `compat.libpng` (this PR) → `compat.zlib` (existing).

## CN mirror

No `mcpp-res` write access. CN URLs use placeholder gitcode.com paths with
the upstream GitHub sha256. Maintainers will upload byte-identical tarballs
and update the CN URLs post-merge.

## Features

None of these packages expose optional features. All compile the full library.

- md4c, yyjson, glad, tray: single compilation unit — nothing to gate
- libpng: all 15 source files are mandatory for basic read/write
- freetype: all modules are required by EUI-NEO (autofit, cff, truetype, sfnt, etc.)

## Verification

All 6 packages pass `mcpp test -p <member>` on Windows x86_64 with llvm@20.1.7:

```
✅ compat.md4c     test_md4c ... ok  (1 passed, 0 failed)
✅ compat.yyjson   parse ... ok      (1 passed, 0 failed)
✅ compat.glad     header ... ok     (1 passed, 0 failed)
✅ compat.tray     header ... ok     (1 passed, 0 failed)
✅ compat.libpng   read ... ok       (1 passed, 0 failed)
✅ compat.freetype init ... ok       (1 passed, 0 failed)
```

CI mcpp version: 0.0.109 (from validate.yml). Local mcpp: 2026.7.27.1.

## Follow-up

`compat.eui` (the 7th package) wraps EUI-NEO itself as a C++ module. It depends
on all 6 packages here plus `compat.glfw`, `compat.opengl`, and `compat.zlib`.
Will be submitted as a separate PR once the full 8-dependency chain passes
end-to-end `mcpp test`.
