-- compat.freetype — FreeType 2.13.3 font rendering engine.
-- Built from aggregate source files (~20 .c files), depends on compat.libpng.
-- EUI-NEO uses FreeType for all text/font rendering.
--
-- Build strategy: aggregate source files (one per module directory).
-- This is the recommended approach for non-CMake/autotools builds.
--
-- All `mcpp` paths are GLOBS relative to the verdir; the leading `*/`
-- absorbs the tarball's `freetype-VER-2-13-3/` wrap layer.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "freetype",
    description = "FreeType 2 — portable font engine with TrueType/CFF/WOFF/SVG support",
    licenses    = {"FreeType"},
    repo        = "https://gitlab.freedesktop.org/freetype/freetype",
    type        = "package",

    xpm = {
        linux = {
            ["2.13.3"] = {
                url    = "https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz",
                sha256 = "bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103",
            },
        },
        macosx = {
            ["2.13.3"] = {
                url    = "https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz",
                sha256 = "bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103",
            },
        },
        windows = {
            ["2.13.3"] = {
                url    = "https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz",
                sha256 = "bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "gnu11",
        include_dirs = { "*/include" },
        sources = {
            -- Base (aggregate + required extra files)
            "*/src/base/ftbase.c",
            "*/src/base/ftinit.c",
            "*/src/base/ftglyph.c",
            "*/src/base/ftbitmap.c",
            "*/src/base/ftbbox.c",
            -- Base extra (individual)
            "*/src/base/ftmm.c",
            -- Modules (aggregates)
            "*/src/autofit/autofit.c",
            "*/src/bdf/bdf.c",
            "*/src/cff/cff.c",
            "*/src/cid/type1cid.c",
            "*/src/cache/ftcache.c",
            "*/src/gzip/ftgzip.c",
            "*/src/lzw/ftlzw.c",
            "*/src/pcf/pcf.c",
            "*/src/pfr/pfr.c",
            "*/src/psaux/psaux.c",
            "*/src/pshinter/pshinter.c",
            "*/src/psnames/psnames.c",
            "*/src/raster/raster.c",
            "*/src/sdf/sdf.c",
            "*/src/sfnt/sfnt.c",
            "*/src/smooth/smooth.c",
            "*/src/svg/svg.c",
            "*/src/truetype/truetype.c",
            "*/src/type1/type1.c",
            "*/src/type42/type42.c",
            "*/src/winfonts/winfnt.c",
        },
        targets = { ["freetype"] = { kind = "lib" } },
        deps    = { ["compat.libpng"] = "1.6.43" },
        cflags  = { "-DFT2_BUILD_LIBRARY", "-DFT_DISABLE_ZLIB", "-DFT_DISABLE_BZIP2", "-DFT_DISABLE_HARFBUZZ", "-DFT_DISABLE_BROTLI", "-Wno-implicit-function-declaration", "-D_DARWIN_C_SOURCE" },
        linux = {
            ldflags  = { "-lm" },
            sources  = { "*/builds/unix/ftsystem.c" },
        },
        macosx = {
            ldflags  = { "-lm" },
            sources  = { "*/builds/unix/ftsystem.c" },
            cflags   = { "-include", "fcntl.h" },
        },
        windows = {
            sources = {
                "*/builds/windows/ftdebug.c",
                "*/builds/windows/ftsystem.c",
            },
        },
    },
}
