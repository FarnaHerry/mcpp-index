-- Form B inline descriptor for gtl (Greg's Template Library) — a C++20 header
-- library whose best-known members are the `flat_hash_map` / `parallel_flat_hash_map`
-- family (the successor to the author's `parallel-hashmap`, itself derived from
-- Abseil's Swiss tables) plus btree containers, a bit_vector, an LRU cache, and
-- assorted vector/utility bits.
--
-- HEADER-ONLY: every header lives under `include/gtl/`, so exposing `include/`
-- is the whole build and consumers write `#include <gtl/phmap.hpp>`. A trivial
-- anchor TU gives mcpp a buildable `lib` target (same shape as compat.eigen /
-- compat.opengl / compat.khrplatform).
--
-- Why `*/include` and not `*`: the tarball root also carries `tests/` and
-- `examples/`, and several of those directories contain headers of their own.
-- Naming `include/` exactly is what upstream's CMake target does
-- (`target_include_directories(gtl INTERFACE include)`), so a consumer sees the
-- same header set here as under CMake — no accidental resolution into test code.
--
-- LANGUAGE FLOOR: gtl requires C++20 and says so (`#error "gtl requires C++20 or
-- later"` in bits.hpp). `language = "c++23"` here is the package's own compile
-- setting for the anchor TU; the headers themselves are compiled in the
-- consumer's mode, which must be C++20 or newer.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "gtl",
    description = "Greg's Template Library — C++20 hash maps, btrees, bit vectors and utilities (header-only)",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/greg7mdp/gtl",
    type        = "package",

    xpm = {
        linux = {
            ["1.2.0"] = {
                url    = {
                    GLOBAL = "https://github.com/greg7mdp/gtl/archive/refs/tags/v1.2.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gtl/releases/download/1.2.0/gtl-1.2.0.tar.gz",
                },
                sha256 = "1547ab78f62725c380f50972f7a49ffd3671ded17a3cb34305da5c953c6ba8e7",
            },
        },
        macosx = {
            ["1.2.0"] = {
                url    = {
                    GLOBAL = "https://github.com/greg7mdp/gtl/archive/refs/tags/v1.2.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gtl/releases/download/1.2.0/gtl-1.2.0.tar.gz",
                },
                sha256 = "1547ab78f62725c380f50972f7a49ffd3671ded17a3cb34305da5c953c6ba8e7",
            },
        },
        windows = {
            ["1.2.0"] = {
                url    = {
                    GLOBAL = "https://github.com/greg7mdp/gtl/archive/refs/tags/v1.2.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/gtl/releases/download/1.2.0/gtl-1.2.0.tar.gz",
                },
                sha256 = "1547ab78f62725c380f50972f7a49ffd3671ded17a3cb34305da5c953c6ba8e7",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        -- Exactly what upstream's INTERFACE target exposes: `<gtl/…>`.
        include_dirs = { "*/include" },
        generated_files = {
            ["mcpp_generated/gtl_anchor.c"] = [==[
int mcpp_compat_gtl_headers_anchor(void) { return 0; }
]==],
        },
        sources      = { "mcpp_generated/gtl_anchor.c" },
        targets      = { ["gtl"] = { kind = "lib" } },
        deps         = { },
    },
}
