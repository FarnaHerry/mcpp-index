-- Form B inline descriptor for NanoSVG — a small SVG parser that flattens paths
-- to cubic beziers, plus an optional scanline rasterizer. Two headers, no
-- dependencies; the usual reason to reach for it is turning icon SVGs into
-- geometry or bitmaps at load time.
--
-- SHAPE: two single-file libraries in the stb tradition. `src/nanosvg.h` and
-- `src/nanosvgrast.h` each hold declarations plus, behind
-- NANOSVG_IMPLEMENTATION / NANOSVGRAST_IMPLEMENTATION, the implementation.
-- Unlike miniaudio, upstream ships NO implementation .c -- its own examples
-- define the macros inline -- so this package generates one. That is what makes
-- it a linkable package instead of a pile of headers, and it means a consumer
-- writes only `#include <nanosvg.h>` and links.
--
-- CONSEQUENCE FOR CONSUMERS: do NOT also define NANOSVG_IMPLEMENTATION or
-- NANOSVGRAST_IMPLEMENTATION. Both implementations are already compiled into
-- this package's lib; defining either macro again duplicates every symbol,
-- which is a LINK error and therefore surfaces late.
--
-- INCLUDE SPELLING: `include_dirs = { "*/src" }`, so the include is
-- `<nanosvg.h>` / `<nanosvgrast.h>` -- the spelling upstream's own examples and
-- README use. Note some package managers install these under a `nanosvg/`
-- subdirectory, making the include read `<nanosvg/nanosvg.h>`; that nesting is
-- not upstream's layout and is not reproduced here.
--
-- VERSIONING: upstream cuts no tags and publishes no releases -- development is
-- a linear series of commits on master. Following compat.khrplatform (which
-- mirrors the untagged EGL-Registry), this pins a commit archive under a DATE
-- version; `2026.07.09` is the commit date of 239e102e.
--
-- The rasterizer needs no separate feature gate: it is one more header behind
-- the same include dir, and its implementation costs one function that the
-- linker drops when unused.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "nanosvg",
    description = "NanoSVG — small SVG parser that flattens paths to beziers, with an optional rasterizer",
    licenses    = {"Zlib"},
    repo        = "https://github.com/memononen/nanosvg",
    type        = "package",

    xpm = {
        linux = {
            ["2026.07.09"] = {
                url    = {
                    GLOBAL = "https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/nanosvg/releases/download/2026.07.09/nanosvg-2026.07.09.tar.gz",
                },
                sha256 = "2bc68bdb518d7800252042e5cad50a0ab321596f0cbf49ef2a752926329063d2",
            },
        },
        macosx = {
            ["2026.07.09"] = {
                url    = {
                    GLOBAL = "https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/nanosvg/releases/download/2026.07.09/nanosvg-2026.07.09.tar.gz",
                },
                sha256 = "2bc68bdb518d7800252042e5cad50a0ab321596f0cbf49ef2a752926329063d2",
            },
        },
        windows = {
            ["2026.07.09"] = {
                url    = {
                    GLOBAL = "https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/nanosvg/releases/download/2026.07.09/nanosvg-2026.07.09.tar.gz",
                },
                sha256 = "2bc68bdb518d7800252042e5cad50a0ab321596f0cbf49ef2a752926329063d2",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        -- Upstream's own spelling: `<nanosvg.h>`, `<nanosvgrast.h>`.
        include_dirs = { "*/src" },
        -- Upstream ships no implementation TU, so supply one. Both single-file
        -- libraries are instantiated here, once, for the whole package.
        generated_files = {
            ["mcpp_generated/nanosvg_impl.c"] = [==[
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>

#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>
]==],
        },
        sources      = { "mcpp_generated/nanosvg_impl.c" },
        targets      = { ["nanosvg"] = { kind = "lib" } },
        deps         = { },
        linux   = { ldflags = { "-lm" } },
        macosx  = { ldflags = { "-lm" } },
    },
}
