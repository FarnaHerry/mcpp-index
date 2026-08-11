-- Form B inline descriptor for plf::hive — the reference implementation of the
-- container proposed for the standard as `std::hive` (P0447). A hive is an
-- unordered sequence container with stable element addresses across insert and
-- erase, no reallocation, and O(1) erasure — the shape you want for the "pool of
-- long-lived objects that are constantly created and destroyed and pointed at"
-- problem that std::vector and std::deque both handle badly.
--
-- HEADER-ONLY, and unusually literally so: the entire library is ONE file,
-- `plf_hive.h`, sitting at the tarball root beside the license and the test
-- suite. So `include_dirs = { "*" }` (the `*` absorbs the archive's
-- `plf_hive-<sha>/` wrap layer) is the whole story, and consumers write
-- `#include <plf_hive.h>`. A trivial anchor TU gives mcpp a buildable `lib`
-- target, the same shape compat.eigen / compat.opengl / compat.khrplatform use.
--
-- VERSIONING: upstream cuts no tags and publishes no releases — development is
-- a linear series of commits on master, and the README tracks a version number
-- that does not appear in the repository as a ref. Following the precedent set
-- by compat.khrplatform (which mirrors the untagged EGL-Registry), this pins a
-- commit archive under a DATE version. `2026.07.31` is the commit date of
-- 085899f5, so the version key sorts correctly against any future snapshot.
--
-- `plf_hive_test_suite.cpp` is upstream's own test driver, not part of the
-- library; it is not compiled here and the include path does not hide it (a
-- consumer that wants it can name it explicitly).
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "plf-hive",
    description = "plf::hive — reference implementation of the proposed std::hive (header-only)",
    licenses    = {"Zlib"},
    repo        = "https://github.com/mattreecebentley/plf_hive",
    type        = "package",

    xpm = {
        linux = {
            ["2026.07.31"] = {
                url    = {
                    GLOBAL = "https://github.com/mattreecebentley/plf_hive/archive/085899f55591e77d49ed168be4594200aa0f0c3a.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/plf-hive/releases/download/2026.07.31/plf-hive-2026.07.31.tar.gz",
                },
                sha256 = "507555191c27768dc469cf7e435ceca0f2819a241f141b3d36a61c6396e500c4",
            },
        },
        macosx = {
            ["2026.07.31"] = {
                url    = {
                    GLOBAL = "https://github.com/mattreecebentley/plf_hive/archive/085899f55591e77d49ed168be4594200aa0f0c3a.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/plf-hive/releases/download/2026.07.31/plf-hive-2026.07.31.tar.gz",
                },
                sha256 = "507555191c27768dc469cf7e435ceca0f2819a241f141b3d36a61c6396e500c4",
            },
        },
        windows = {
            ["2026.07.31"] = {
                url    = {
                    GLOBAL = "https://github.com/mattreecebentley/plf_hive/archive/085899f55591e77d49ed168be4594200aa0f0c3a.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/plf-hive/releases/download/2026.07.31/plf-hive-2026.07.31.tar.gz",
                },
                sha256 = "507555191c27768dc469cf7e435ceca0f2819a241f141b3d36a61c6396e500c4",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        -- Tarball root: `#include <plf_hive.h>`.
        include_dirs = { "*" },
        generated_files = {
            ["mcpp_generated/plf_hive_anchor.c"] = [==[
int mcpp_compat_plf_hive_headers_anchor(void) { return 0; }
]==],
        },
        sources      = { "mcpp_generated/plf_hive_anchor.c" },
        targets      = { ["plf-hive"] = { kind = "lib" } },
        deps         = { },
    },
}
