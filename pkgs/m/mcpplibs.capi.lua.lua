-- Form B descriptor for the immutable 0.0.3 payload.  Its upstream
-- mcpp.toml predates exact package selectors and declares bare `lua`, which
-- now means `(mcpplibs, lua)` and cannot identify the real C library at
-- `(compat, lua)`.  Keep the release bytes unchanged and carry the canonical
-- dependency here until a newer upstream release can return to Form A.
package = {
    spec        = "1",
    namespace = "mcpplibs.capi",
    name        = "lua",
    description = "C++23 module wrapping the Lua 5.4 C API — `import mcpplibs.capi.lua;`",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/mcpplibs/lua",
    type        = "package",

    xpm = {
        linux = {
            ["0.0.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/lua/archive/refs/tags/0.0.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/capi.lua/releases/download/0.0.3/capi.lua-0.0.3.tar.gz",
                },
                sha256 = "f7f46c3cd193dc4527be5f3e5cfc29d7e322d5d3db56b9bdb060f289090088d6",
            },
        },
        macosx = {
            ["0.0.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/lua/archive/refs/tags/0.0.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/capi.lua/releases/download/0.0.3/capi.lua-0.0.3.tar.gz",
                },
                sha256 = "f7f46c3cd193dc4527be5f3e5cfc29d7e322d5d3db56b9bdb060f289090088d6",
            },
        },
        windows = {
            ["0.0.3"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/lua/archive/refs/tags/0.0.3.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/capi.lua/releases/download/0.0.3/capi.lua-0.0.3.tar.gz",
                },
                sha256 = "f7f46c3cd193dc4527be5f3e5cfc29d7e322d5d3db56b9bdb060f289090088d6",
            },
        },
    },

    mcpp = {
        schema       = "0.1",
        language     = "c++23",
        import_std   = false,
        modules      = { "mcpplibs.capi.lua" },
        include_dirs = { "*/src/capi" },
        sources      = {
            "*/src/capi/lua.cppm",
            "*/src/capi/lua.cpp",
        },
        targets = { ["capi-lua"] = { kind = "lib" } },
        deps    = { ["compat.lua"] = "5.4.7" },
    },
}
