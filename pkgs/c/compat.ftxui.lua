-- M6.x glob-aware Form B descriptor for FTXUI 6.1.9 and 7.0.3.
--
-- Pure C++ library (no C++23 modules); compiled sources + public headers.
-- Uses mcpp 0.0.4's glob exclusion (`!` prefix) to skip the
-- *_test.cpp / *_fuzzer.cpp files that live alongside the library
-- sources in the same directories (6.1.9: ~30 test / ~16 fuzzer;
-- 7.0.3: 47 test / 6 fuzzer).
--
-- 7.0.3: source layout is unchanged (include/ftxui + src/ftxui/{screen,dom,
-- component,util}), so the 6.1.9 globs apply verbatim, and the public header
-- API this index's smoke test uses (hbox/text/separator, `Dimension::Fit`,
-- `Screen::Create(Dimensions, Dimensions)`, `Render`) is source-compatible
-- with 6.1.9. Upstream added C++20 module units (src/ftxui/*.cppm,
-- FTXUI_BUILD_MODULES, off by default); the `*.cpp` globs never match them,
-- and the plain .cpp sources still compile header-only style.
--
-- ONE version skew the globs cannot express (no per-version build blocks,
-- mcpp-community/mcpp#290): FTXUI 7 moved Loop's method definitions from
-- loop.cpp into app.cpp and dropped loop.cpp from the CMake build, but the
-- stale file still ships in the 7.0.3 tarball. Compiling both duplicates
-- `Loop::{~Loop,RunOnce,...}` at the consumer's link (a dependency's objects
-- ALL enter the link — no lazy archive selection). 6.1.9 has no app.cpp and
-- genuinely needs loop.cpp. The install() hook below deletes loop.cpp only
-- when app.cpp exists, i.e. exactly on the 7.x layout.
--
-- Produces a single static archive `libftxui.a` covering all three
-- upstream cmake targets (ftxui-screen, ftxui-dom, ftxui-component).

package = {
    spec        = "1",
    namespace = "compat",
    name        = "ftxui",
    description = "C++ Functional Terminal User Interface (screen + dom + component)",
    licenses    = {"MIT"},
    repo        = "https://github.com/ArthurSonzogni/FTXUI",
    type        = "package",

    xpm = {
        linux = {
            ["6.1.9"] = {
                url    = {
                    GLOBAL = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/ftxui/releases/download/6.1.9/ftxui-6.1.9.tar.gz",
                },
                sha256 = "45819c1e54914783d4a1ca5633885035d74146778a1f74e1213cdb7b76340e71",
            },
            -- 7.0.3 has no CN mirror yet (never published to mcpp-res);
            -- plain-string GLOBAL fallback. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["7.0.3"] = {
                url    = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.tar.gz",
                sha256 = "e7c62ffe19009759821b4f0f8df7f2a6fb83784c3a9f1477d81f56d3ee723c88",
            },
        },
        macosx = {
            ["6.1.9"] = {
                url    = {
                    GLOBAL = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/ftxui/releases/download/6.1.9/ftxui-6.1.9.tar.gz",
                },
                sha256 = "45819c1e54914783d4a1ca5633885035d74146778a1f74e1213cdb7b76340e71",
            },
            -- 7.0.3 has no CN mirror yet (never published to mcpp-res);
            -- plain-string GLOBAL fallback. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["7.0.3"] = {
                url    = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.tar.gz",
                sha256 = "e7c62ffe19009759821b4f0f8df7f2a6fb83784c3a9f1477d81f56d3ee723c88",
            },
        },
        windows = {
            ["6.1.9"] = {
                url    = {
                    GLOBAL = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/ftxui/releases/download/6.1.9/ftxui-6.1.9.tar.gz",
                },
                sha256 = "45819c1e54914783d4a1ca5633885035d74146778a1f74e1213cdb7b76340e71",
            },
            -- 7.0.3 has no CN mirror yet (never published to mcpp-res);
            -- plain-string GLOBAL fallback. Flip to { GLOBAL, CN } if it gets
            -- mirrored — sha256 stays the same.
            ["7.0.3"] = {
                url    = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.tar.gz",
                sha256 = "e7c62ffe19009759821b4f0f8df7f2a6fb83784c3a9f1477d81f56d3ee723c88",
            },
        },
    },

    -- Form B `mcpp` segment: paths are globs relative to the verdir.
    -- The leading `*/` absorbs the GitHub tarball's `FTXUI-<ver>/` wrap.
    mcpp = {
        language     = "c++23",
        import_std   = false,          -- pure compiled lib, no `import std;`
        include_dirs = { "*/include", "*/src" },   -- src/ for private headers (box_helper.hpp etc.),
        sources = {
            "*/src/ftxui/**/*.cpp",
            "!*/src/ftxui/**/*_test.cpp",      -- gtest files (30+ in 6.1.9, 47 in 7.0.3)
            "!*/src/ftxui/**/*_fuzzer.cpp",     -- fuzz targets (16 in 6.1.9, 6 in 7.0.3)
        },
        targets = { ["ftxui"] = { kind = "lib" } },
        deps    = { },
        windows = {
            cxxflags = { "-DUNICODE", "-D_UNICODE" },
        },
    },
}

import("xim.libxpkg.pkginfo")

function install()
    -- Reproduce the default unpack shape — install_dir/<wrap>/src/... — so
    -- the descriptor's `*/` globs keep matching exactly one wrap level (same
    -- normalisation as compat.eui-neo). ⚠️ NO SHELL and no directory listing
    -- in this sandbox: the wrap is asked about by name, not discovered.
    local v = pkginfo.version()
    local idir = pkginfo.install_dir()
    local layer = path.join(idir, "FTXUI-" .. v)
    os.tryrm(idir)
    os.mkdir(idir)
    for _, name in ipairs({ "FTXUI-" .. v, "ftxui-" .. v,
                            "FTXUI-v" .. v, "ftxui-v" .. v }) do
        if os.isfile(path.join(name, "CMakeLists.txt")) then
            os.mv(name, layer)
            break
        end
    end
    if not os.isfile(path.join(layer, "CMakeLists.txt")) then
        log.error("ftxui: no CMakeLists.txt under %s after unpacking; the "
                  .. "archive layout is neither wrapped nor flat", layer)
        return false
    end

    -- See the header comment: drop the stale loop.cpp on the 7.x layout
    -- (app.cpp present) where its Loop methods are duplicated; keep it for
    -- 6.1.9, which has no app.cpp.
    if os.isfile(path.join(layer, "src", "ftxui", "component", "app.cpp")) then
        os.tryrm(path.join(layer, "src", "ftxui", "component", "loop.cpp"))
    end
    return true
end
