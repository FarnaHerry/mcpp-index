-- compat.msdfgen — multi-channel signed distance field generator, core + the
-- FreeType font importer.
--
-- Shape A (C-source compat, C++ flavour) plus generated forwarding headers.
-- The user writes `#include <msdfgen/msdfgen.h>` and
-- `#include <msdfgen/msdfgen-ext.h>`.
--
-- WHY THE msdfgen/ PREFIX IS GENERATED.
-- Upstream keeps `msdfgen.h` and `msdfgen-ext.h` at the repository ROOT, but
-- every packaging of msdfgen (vcpkg, xmake-repo, distro packages) installs them
-- under `include/msdfgen/`, and that is what consumers' source actually says.
-- Building in place gives the root layout, so `mcpp_generated/msdfgen/*.h`
-- restores the conventional spelling. The raw `<msdfgen.h>` spelling keeps
-- working too -- the wrap directory is on the include path either way.
--
-- WHICH SOURCES, AND WHY NOT ALL OF ext/.
-- `core/` is dependency-free and always built. Of the four ext/ units only
-- `import-font.cpp` is here, because the other three each drag in a library
-- that is not in this index:
--
--   ext/resolve-shape-geometry.cpp   Skia
--   ext/import-svg.cpp               TinyXML2
--   ext/save-png.cpp                 libpng or LodePNG
--
-- Their HEADERS are still reachable through msdfgen-ext.h -- upstream gates
-- each behind its own macro, so the generated shim sets MSDFGEN_DISABLE_SVG
-- and MSDFGEN_DISABLE_PNG and the declarations simply are not there. Skia
-- needs no macro: its block is `#ifdef MSDFGEN_USE_SKIA`, off by default.
--
-- MSDFGEN_USE_CPP11 IS DELIBERATELY NOT SET.
-- It is not a build-only switch: it adds move constructors to `Bitmap`
-- (core/Bitmap.h:19), so it changes the layout and ABI of a type that crosses
-- the library boundary. A package cannot guarantee every consumer defines it
-- identically -- and a consumer that includes `<msdfgen.h>` directly would not
-- see a shim's definition at all -- so the two sides would silently disagree.
-- Leaving it off costs some moves and keeps one ABI.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "msdfgen",
    description = "msdfgen — multi-channel signed distance field generator (core + FreeType import)",
    licenses    = {"MIT"},
    repo        = "https://github.com/Chlumsky/msdfgen",
    type        = "package",

    xpm = {
        linux = {
            ["1.13"] = {
                url = {
                    GLOBAL = "https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.13.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/msdfgen/releases/download/1.13/msdfgen-1.13.tar.gz",
                },
                sha256 = "93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167",
            },
        },
        macosx = {
            ["1.13"] = {
                url = {
                    GLOBAL = "https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.13.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/msdfgen/releases/download/1.13/msdfgen-1.13.tar.gz",
                },
                sha256 = "93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167",
            },
        },
        windows = {
            ["1.13"] = {
                url = {
                    GLOBAL = "https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.13.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/msdfgen/releases/download/1.13/msdfgen-1.13.tar.gz",
                },
                sha256 = "93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        include_dirs = { "*", "mcpp_generated" },
        -- Shape E as well as A: msdfgen reaches its OWN config header through
        -- the same prefix (`core/base.h:7` does
        -- `#include <msdfgen/msdfgen-config.h>`), and CMake generates that file
        -- from `cmake/msdfgen-config.h.in`. Without it nothing compiles at all,
        -- not even core/.
        --
        -- Generating it here rather than passing -D flags is what makes the
        -- library and its consumers agree BY CONSTRUCTION: base.h is reached
        -- from every public header, so whatever this file says is what both
        -- sides see. Flags on the package's own compile line could not do
        -- that -- they are private to this build, and a consumer including
        -- <msdfgen/msdfgen-ext.h> would disagree about which declarations
        -- exist.
        --
        -- Values follow CMakeLists.txt:233-265 for a static build with
        -- MSDFGEN_USE_SKIA=OFF, MSDFGEN_DISABLE_SVG=ON, MSDFGEN_DISABLE_PNG=ON,
        -- MSDFGEN_USE_CPP11=OFF; version from vcpkg.json (1.13.0).
        --
        -- Long-bracket strings, not `..` concatenation: the descriptor reader
        -- is a lightweight Lua parser, not an evaluator, and reads a `..`
        -- expression's continuation lines as new keys
        -- ("malformed mcpp segment near key 'ifndef'").
        generated_files = {
            ["mcpp_generated/msdfgen/msdfgen-config.h"] = [==[
#pragma once

#define MSDFGEN_PUBLIC
#define MSDFGEN_EXT_PUBLIC

#define MSDFGEN_VERSION 1.13.0
#define MSDFGEN_VERSION_MAJOR 1
#define MSDFGEN_VERSION_MINOR 13
#define MSDFGEN_VERSION_REVISION 0
#define MSDFGEN_COPYRIGHT_YEAR 2025

#define MSDFGEN_EXTENSIONS
#define MSDFGEN_DISABLE_SVG
#define MSDFGEN_DISABLE_PNG
]==],
            -- The conventional install spelling. Upstream's own install does
            -- exactly this (CMakeLists.txt:285-289 writes a global msdfgen.h
            -- that forwards to msdfgen/msdfgen.h); here the direction is
            -- reversed because the build happens in the source tree, where the
            -- root header is the real one.
            ["mcpp_generated/msdfgen/msdfgen.h"] = [==[
#pragma once
#include <msdfgen.h>
]==],
            ["mcpp_generated/msdfgen/msdfgen-ext.h"] = [==[
#pragma once
#include <msdfgen-ext.h>
]==],
        },
        sources = {
            "*/core/*.cpp",
            "*/ext/import-font.cpp",
        },
        targets  = { ["msdfgen"] = { kind = "lib" } },
        deps     = { ["compat.freetype"] = "2.13.3" },
    },
}
