-- compat.md4c — MD4C Markdown parser for C.
-- Single-source ANSI C library, no dependencies.
-- EUI-NEO uses this for Markdown rendering via `#include "md4c.h"`.
--
-- All `mcpp` paths are GLOBS relative to the verdir; the leading `*/`
-- absorbs the GitHub tarball's `md4c-release-0.5.3/` wrap layer.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "md4c",
    description = "MD4C — Markdown parser for C",
    licenses    = {"MIT"},
    repo        = "https://github.com/mity/md4c",
    type        = "package",

    xpm = {
        linux = {
            ["0.5.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/md4c/releases/download/0.5.3/md4c-0.5.3.tar.gz",
                },
                sha256 = "353c346f376b87c954a13f3415ede2d51264cc61dc5abcd38ff1d2aa0d059b9e",
            },
        },
        macosx = {
            ["0.5.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/md4c/releases/download/0.5.3/md4c-0.5.3.tar.gz",
                },
                sha256 = "353c346f376b87c954a13f3415ede2d51264cc61dc5abcd38ff1d2aa0d059b9e",
            },
        },
        windows = {
            ["0.5.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/md4c/releases/download/0.5.3/md4c-0.5.3.tar.gz",
                },
                sha256 = "353c346f376b87c954a13f3415ede2d51264cc61dc5abcd38ff1d2aa0d059b9e",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c99",
        include_dirs = { "*/src" },
        sources      = { "*/src/md4c.c" },
        targets      = { ["md4c"] = { kind = "lib" } },
        deps         = {},
    },
}
