-- compat.re2 — RE2, Google's linear-time regular expression engine.
--
-- Shape A (C++ source compat): no configure step, no code generation, no
-- submodules, and platform handling is entirely in-source, so one source list
-- and one sha256 cover linux/macosx/windows.
--
-- WHY THIS VERSION. `2022-04-01` is an upstream RE2 release tag, and it is the
-- one gRPC 1.83.0 pins (its third_party/re2 submodule is commit 0c5616d ==
-- tag 2022-04-01). That pin is the point: gRPC's xds matchers
-- (src/core/util/matchers.h, src/core/xds/grpc/xds_route_config*) are written
-- against this API. RE2 switched its own string type from re2::StringPiece to
-- absl::string_view in the 2023 releases and took on an Abseil dependency
-- there, so a newer RE2 is not a drop-in for this consumer. Newer releases can
-- be added as ADDITIONAL versions of this same descriptor when something wants
-- them — the index carries multi-version packages already (compat.catch2).
--
-- The GitHub tag archive is used rather than a release asset because upstream
-- publishes no assets for this tag; the archive is self-contained (RE2 has no
-- submodules) and its digest was verified stable across two downloads.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "re2",
    description = "RE2 — fast, safe, thread-friendly regular expression engine (static)",
    licenses    = {"BSD-3-Clause"},
    repo        = "https://github.com/google/re2",
    type        = "package",

    xpm = {
        linux = {
            ["2022-04-01"] = {
                url = {
                    GLOBAL = "https://github.com/google/re2/archive/refs/tags/2022-04-01.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/re2/releases/download/2022-04-01/re2-2022-04-01.tar.gz",
                },
                sha256 = "1ae8ccfdb1066a731bba6ee0881baad5efd2cd661acd9569b689f2586e1a50e9",
            },
        },
        macosx = {
            ["2022-04-01"] = {
                url = {
                    GLOBAL = "https://github.com/google/re2/archive/refs/tags/2022-04-01.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/re2/releases/download/2022-04-01/re2-2022-04-01.tar.gz",
                },
                sha256 = "1ae8ccfdb1066a731bba6ee0881baad5efd2cd661acd9569b689f2586e1a50e9",
            },
        },
        windows = {
            ["2022-04-01"] = {
                url = {
                    GLOBAL = "https://github.com/google/re2/archive/refs/tags/2022-04-01.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/re2/releases/download/2022-04-01/re2-2022-04-01.tar.gz",
                },
                sha256 = "1ae8ccfdb1066a731bba6ee0881baad5efd2cd661acd9569b689f2586e1a50e9",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        -- RE2 is included as <re2/re2.h>, so the include root is the tarball
        -- wrap dir. `*` absorbs `re2-2022-04-01/`.
        include_dirs = { "*" },

        -- Upstream's RE2_SOURCES (CMakeLists.txt) exactly: every .cc directly
        -- under re2/, plus two of the four under util/. The re2/ glob is safe
        -- because upstream keeps its tests one level down in re2/testing/,
        -- which `re2/*.cc` does not reach.
        --
        -- util/ is listed file-by-file on purpose — the other two TUs there
        -- both define main() (util/benchmark.cc, util/test.cc, and likewise
        -- util/fuzz.cc and the top-level testinstall.cc). A dependency's
        -- objects all enter the consumer's link, so any of them would collide
        -- with the consumer's own main(). util/pcre.cc is excluded for a
        -- second reason: it is upstream's PCRE comparison harness and would
        -- drag in libpcre.
        sources = {
            "*/re2/*.cc",
            "*/util/rune.cc",
            "*/util/strutil.cc",
        },

        targets = { ["re2"] = { kind = "lib" } },
        deps    = { },

        linux   = { ldflags = { "-lpthread" } },
        -- macOS: libSystem carries pthread.
        windows = {
            -- RE2's util/mutex.h includes <windows.h>, whose min/max
            -- function-like MACROS then eat RE2's own accessors: regexp.cc
            -- calls `a->min()` / `a->max()` on Regexp, and the preprocessor
            -- turns those into "too few arguments provided to function-like
            -- macro invocation". Upstream's CMake never has to say this
            -- because a CMake consumer normally sets NOMINMAX project-wide;
            -- here the package owns its own flags. No extra import libs —
            -- RE2 uses only the CRT and the Win32 CRITICAL_SECTION already in
            -- kernel32.
            cxxflags = { "-DNOMINMAX", "-DWIN32_LEAN_AND_MEAN" },
        },
    },
}
