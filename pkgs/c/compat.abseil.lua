-- compat.abseil — Abseil C++ LTS 20250512.1, built straight from the upstream
-- release tarball as one static archive.
--
-- Shape A (C++ source compat): no configure step, no code generation, no
-- submodules. Abseil's platform handling is entirely in-source `#ifdef`
-- (`absl/base/config.h`), so one source list covers linux/macosx/windows and
-- the three xpm blocks share a single tarball and sha256.
--
-- Version numbering follows upstream verbatim: `20250512.1` IS the Abseil LTS
-- tag (`ABSL_LTS_RELEASE_VERSION 20250512` / `ABSL_LTS_RELEASE_PATCH_LEVEL 1`
-- in absl/base/config.h). Not a coincidence worth losing: protobuf 35.1
-- (MODULE.bazel) and gRPC 1.83.0 (its third_party/abseil-cpp submodule pin,
-- commit 76bb243 = "Abseil LTS Branch, May 2025, Patch 1") BOTH resolve to
-- exactly this release. Keeping the package at the upstream spelling is what
-- lets compat.protobuf — and later gRPC — depend on one shared Abseil instead
-- of each vendoring its own copy and colliding at link time.
--
-- The upstream release ASSET is used, not the GitHub tag archive:
-- abseil-cpp-20250512.1.tar.gz is a real published artifact with a stable
-- digest (verified by downloading twice).
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "abseil",
    description = "Abseil — Google's C++ common libraries (LTS 20250512.1, static)",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/abseil/abseil-cpp",
    type        = "package",

    xpm = {
        linux = {
            ["20250512.1"] = {
                url = {
                    GLOBAL = "https://github.com/abseil/abseil-cpp/releases/download/20250512.1/abseil-cpp-20250512.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/abseil/releases/download/20250512.1/abseil-20250512.1.tar.gz",
                },
                sha256 = "9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db",
            },
        },
        macosx = {
            ["20250512.1"] = {
                url = {
                    GLOBAL = "https://github.com/abseil/abseil-cpp/releases/download/20250512.1/abseil-cpp-20250512.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/abseil/releases/download/20250512.1/abseil-20250512.1.tar.gz",
                },
                sha256 = "9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db",
            },
        },
        windows = {
            ["20250512.1"] = {
                url = {
                    GLOBAL = "https://github.com/abseil/abseil-cpp/releases/download/20250512.1/abseil-cpp-20250512.1.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/abseil/releases/download/20250512.1/abseil-20250512.1.tar.gz",
                },
                sha256 = "9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        -- Abseil headers are addressed as <absl/...>, so the include root is
        -- the tarball wrap dir itself. `*` absorbs `abseil-cpp-20250512.1/`.
        include_dirs = { "*" },

        -- Upstream keeps tests, benchmarks and small generator tools in the
        -- SAME directories as the library sources, so the wildcard has to be
        -- trimmed. Each exclusion below is one of three kinds; none is
        -- cosmetic.
        sources = {
            "*/absl/**/*.cc",

            -- (1) The naming conventions upstream uses for its own test and
            -- benchmark TUs. These carry the bulk of the exclusions (~250
            -- files) and must come first — `*_benchmark.cc` alone accounts for
            -- every google/benchmark reference in the tree.
            "!*/absl/**/*_test.cc",
            "!*/absl/**/*_test_common.cc",
            "!*/absl/**/*_testing.cc",
            "!*/absl/**/*_benchmark.cc",
            "!*/absl/**/*_benchmarks.cc",

            -- (2) The stragglers the conventions above miss: TUs that still
            -- pull in gtest/gmock/benchmark, which this package has no
            -- dependency on and therefore cannot link.
            "!*/absl/log/internal/test_actions.cc",
            "!*/absl/log/internal/test_helpers.cc",
            "!*/absl/log/internal/test_matchers.cc",
            "!*/absl/log/scoped_mock_log.cc",
            "!*/absl/status/internal/status_matchers.cc",
            "!*/absl/random/benchmarks.cc",

            -- (3) Test-support TUs that compile cleanly but belong to no
            -- upstream library target (they exist only for absl's own test and
            -- benchmark binaries).
            "!*/absl/base/internal/atomic_hook_test_helper.cc",
            "!*/absl/base/internal/scoped_set_env.cc",
            "!*/absl/container/internal/test_instance_tracker.cc",
            "!*/absl/debugging/internal/stack_consumption.cc",
            "!*/absl/flags/flag_test_defs.cc",
            "!*/absl/random/internal/chi_square.cc",
            "!*/absl/random/internal/nanobenchmark.cc",
            "!*/absl/strings/internal/pow10_helper.cc",

            -- (4) TUs that define their own main(). These MUST stay out: a
            -- dependency's objects all enter the consumer's link (they are not
            -- lazily selected out of an archive), so either one would collide
            -- with the consumer's own main() and break every build that
            -- depends on this package.
            "!*/absl/hash/internal/print_hash_of.cc",
            "!*/absl/random/internal/gaussian_distribution_gentables.cc",
        },

        -- KNOWN WARNING, not a defect in this descriptor: every build of this
        -- package prints
        --   randen_round_keys.cc: module 'binascii' imported but not provided
        -- randen_round_keys.cc documents how its constants were produced by
        -- pasting the generator INSIDE a `/* … */` block comment, and that
        -- Python starts with `import binascii` at column 0. mcpp's M1 text
        -- scan strips `//` comments and raw-string bodies
        -- (src/modgraph/scanner.cppm strip_raw_strings) but NOT block
        -- comments, so the line reads as a C++23 module import.
        --
        -- It cannot be silenced from here: `scan_overrides` is the mechanism
        -- for exactly this, but mcpp rejects an entry that "declares neither
        -- provides nor imports" (src/manifest/toml.cppm), and an all-empty
        -- scan result — no provides, no imports — is precisely the true
        -- answer for this file. Declaring a fake edge instead would be
        -- reconciled against the compiler's P1689 output and fail the build.
        -- The warning is cosmetic; the TU compiles and links correctly. Fixing
        -- it belongs upstream in mcpp (strip block comments in the text scan,
        -- or allow an empty scan_overrides entry).

        targets = { ["abseil"] = { kind = "lib" } },
        deps    = { },

        -- randen_hwaes.cc is compiled WITHOUT `-maes -msse4.1` (upstream puts
        -- them on a dedicated CMake target). That is correct, not a
        -- regression: without accelerated AES the file takes its
        -- `!ABSL_RANDEN_HWAES_IMPL` branch, where
        -- `HasRandenHwAesImplementation()` returns false — and randen.cc gates
        -- on `HasRandenHwAesImplementation() && CPUSupportsRandenHwAes()`, so
        -- the stubs are unreachable and absl::BitGen takes RandenSlow. Adding
        -- the flags package-wide instead would raise the baseline ISA for
        -- every TU, which is a worse trade for a portable index package.

        linux = {
            -- Abseil's synchronization/time layers need pthread; -lrt covers
            -- clock_gettime on pre-2.17 glibc (folded into libc since).
            ldflags = { "-lpthread", "-lrt" },
        },
        macosx = {
            -- libSystem carries pthread and the clock APIs, but NOT the time
            -- zone lookup: on Apple platforms cctz's time_zone_lookup.cc reads
            -- the system zone through CFTimeZoneCopyDefault / CFStringGet* /
            -- CFRelease, so CoreFoundation has to be on the link line.
            -- Upstream does the same — absl/time/CMakeLists.txt carries
            -- `$<$<PLATFORM_ID:Darwin,…>:-Wl,-framework,CoreFoundation>`.
            -- Without it the package builds and only fails at LINK time in the
            -- consumer, with six undefined CF* symbols.
            ldflags = { "-framework", "CoreFoundation" },
        },
        windows = {
            -- Upstream's own MSVC copts (absl/copts/GENERATED_AbseilCopts.cmake
            -- ABSL_MSVC_FLAGS). NOMINMAX is load-bearing, not hygiene:
            -- <windows.h> defines min/max as function-like MACROS, which turns
            -- every `std::numeric_limits<time_t>::max()` in absl/time/time.cc
            -- into "too few arguments provided to function-like macro
            -- invocation". WIN32_LEAN_AND_MEAN trims the same header down, and
            -- _CRT_SECURE_NO_WARNINGS silences the CRT deprecation noise.
            cxxflags = { "-DNOMINMAX", "-DWIN32_LEAN_AND_MEAN", "-D_CRT_SECURE_NO_WARNINGS" },
            -- absl/base/CMakeLists.txt links -ladvapi32 for the Windows
            -- entropy/thread-identity paths.
            ldflags  = { "-ladvapi32" },
        },
    },
}
