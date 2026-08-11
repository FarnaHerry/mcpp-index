-- Form B inline descriptor for SPIRV-Reflect — Khronos' official reflection
-- library for SPIR-V modules. Given a compiled shader blob it enumerates
-- descriptor bindings, sets, push-constant blocks, and interface variables, which
-- is how a renderer builds pipeline layouts from shaders instead of duplicating
-- the binding table by hand.
--
-- SHAPE: one C translation unit. `spirv_reflect.c` at the tarball root is the
-- entire library (upstream's CMake `spirv-reflect-static` target compiles exactly
-- this file); everything else in the tree is the `spirv-reflect` CLI, tests, and
-- the GoogleTest submodule, none of which belong in a consumable package.
--
-- INCLUDE PATHS -- both are needed, for different reasons:
--
--   `*`         so consumers write `#include <spirv_reflect.h>`, the name
--               upstream's README and every downstream project use.
--   `*/include` so the SPIRV_REFLECT_USE_SYSTEM_SPIRV_H path resolves.
--               spirv_reflect.h:35-37 picks between `<spirv/unified1/spirv.h>`
--               (that macro defined) and `"./include/spirv/unified1/spirv.h"`
--               (the default, relative to the header, which works with `*`
--               alone). Exposing `include/` too means BOTH spellings resolve to
--               the SAME bundled grammar header, so a consumer that defines the
--               macro -- e.g. because something else in its graph already
--               supplies the Khronos headers -- does not silently get a
--               different SPIR-V revision than the one this .c was written for.
--
-- VERSIONING: upstream cuts no semver releases; it tags in lockstep with the
-- Vulkan SDK (`vulkan-sdk-<x.y.z.w>`). The version key drops the `vulkan-sdk-`
-- prefix so it sorts numerically and lines up with compat.vulkan-headers /
-- compat.vulkan of the same SDK -- keep the three moving together.
--
-- No deps: the library is freestanding C99 over the bundled SPIR-V grammar and
-- does not need the Vulkan headers or the loader (it parses a SPIR-V blob, it
-- does not call Vulkan).
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "spirv-reflect",
    description = "Khronos SPIRV-Reflect — reflection of descriptor bindings and interfaces from SPIR-V modules",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/KhronosGroup/SPIRV-Reflect",
    type        = "package",

    xpm = {
        linux = {
            ["1.4.357.0"] = {
                url    = {
                    GLOBAL = "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/spirv-reflect/releases/download/1.4.357.0/spirv-reflect-1.4.357.0.tar.gz",
                },
                sha256 = "c865bd55459c5b8020a6ac962e462fc33eb4bf2dae8bf7c474357c58ce22a95d",
            },
        },
        macosx = {
            ["1.4.357.0"] = {
                url    = {
                    GLOBAL = "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/spirv-reflect/releases/download/1.4.357.0/spirv-reflect-1.4.357.0.tar.gz",
                },
                sha256 = "c865bd55459c5b8020a6ac962e462fc33eb4bf2dae8bf7c474357c58ce22a95d",
            },
        },
        windows = {
            ["1.4.357.0"] = {
                url    = {
                    GLOBAL = "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/spirv-reflect/releases/download/1.4.357.0/spirv-reflect-1.4.357.0.tar.gz",
                },
                sha256 = "c865bd55459c5b8020a6ac962e462fc33eb4bf2dae8bf7c474357c58ce22a95d",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        include_dirs = { "*", "*/include" },
        -- Upstream's spirv-reflect-static target is exactly this one file.
        sources      = { "*/spirv_reflect.c" },
        targets      = { ["spirv-reflect"] = { kind = "lib" } },
        deps         = { },
    },
}
