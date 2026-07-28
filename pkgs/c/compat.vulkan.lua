-- compat.vulkan — the Khronos Vulkan loader, built from source as a static lib.
--
-- This is the thing a Vulkan program LINKS: `vkCreateInstance` and friends are
-- trampolines the loader owns, which then dispatch into whatever ICD (GPU
-- driver) the system advertises. Headers alone are not enough, which is why
-- `compat.vulkan-headers` is a separate package this one depends on.
--
-- Buildable as a plain source list — no CMake, no Python, no assembler:
--
--   * `loader/generated/` (vk_loader_extensions.c, vk_object_types.h, …) is
--     CHECKED IN upstream, so the codegen step CMake would run is unnecessary.
--   * The assembly path is optional. Upstream's CMake compiles
--     `dev_ext_trampoline.c` + `phys_dev_ext.c` against hand-written GAS/MASM
--     and a generated `gen_defines.asm` (which needs building and RUNNING
--     asm_offset, then a Python script to scrape its output). That whole chain
--     is gated on `UNKNOWN_FUNCTIONS_SUPPORTED`; upstream itself degrades
--     gracefully when no working assembler is found, and
--     `unknown_function_handling.c` compiles a pure-C fallback instead. We take
--     that fallback deliberately: the cost is that unknown DEVICE extension
--     entry points (ones this loader version has never heard of) get no
--     trampoline, which no consumer in this index uses.
--
-- WINDOWS IS DEFERRED. A statically linked loader is not something upstream
-- supports there: the only static option in its CMake is `APPLE_STATIC_LOADER`,
-- gated to macOS and carrying the warning that it "will only work on MacOS and
-- is not supported" elsewhere. Built anyway, the Windows loader links but faults
-- at the first entry point (0xC0000005 out of vkEnumerateInstanceVersion). Linux
-- is not covered by that option either, but a static loader is the ordinary
-- case there and works — Chromium ships one. Rather than carry a package that
-- crashes, this follows `compat.openssl` and declares no windows xpm entry;
-- consumers gate with `[target.'cfg(...)']`.
--
-- SYSCONFDIR / FALLBACK_*_DIRS are the ICD and layer manifest search paths.
-- Upstream's CMake derives them from the install prefix; the values below are
-- the FHS/XDG defaults, which is what a system-installed driver actually uses —
-- this package must find the HOST's ICDs, not any path of its own.
--
-- All `mcpp` paths are GLOBS relative to the verdir; the leading `*/` absorbs
-- the GitHub tarball's `Vulkan-Loader-vulkan-sdk-1.4.357.0/` wrap layer.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "vulkan",
    description = "Khronos Vulkan loader — static ICD-dispatch library",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/KhronosGroup/Vulkan-Loader",
    type        = "package",

    xpm = {
        linux = {
            ["1.4.357.0"] = {
                url = {
                    GLOBAL = "https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/vulkan/releases/download/1.4.357.0/vulkan-1.4.357.0.tar.gz",
                },
                sha256 = "54f2537df22313768da0317dda2abdaaab7711b4081c48c869a79db343d0ae70",
            },
        },
        macosx = {
            ["1.4.357.0"] = {
                url = {
                    GLOBAL = "https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/vulkan/releases/download/1.4.357.0/vulkan-1.4.357.0.tar.gz",
                },
                sha256 = "54f2537df22313768da0317dda2abdaaab7711b4081c48c869a79db343d0ae70",
            },
        },
        -- windows deferred, see the note at the top of this file.
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",

        include_dirs = { "*/loader", "*/loader/generated", "mcpp_generated" },

        -- SYSCONFDIR / FALLBACK_*_DIRS have to reach the compiler as STRING
        -- literals, and `-DSYSCONFDIR="/etc"` does not survive the trip: mcpp
        -- splits flags without honouring the quotes (mcpp#234), so loader.c
        -- ends up seeing a bare `/etc` and fails with "expected expression
        -- before '/' token". Carrying them in a force-included header sidesteps
        -- the command line entirely — the same move `compat.opencv5` made for
        -- its space-bearing defines.
        generated_files = {
            ["mcpp_generated/mcpp_vulkan_paths.h"] = [==[
/* Manifest search paths for the Vulkan loader — see the descriptor note. */
#pragma once
#define SYSCONFDIR           "/etc"
#define FALLBACK_CONFIG_DIRS "/etc/xdg"
#define FALLBACK_DATA_DIRS   "/usr/local/share:/usr/share"
]==],
        },

        -- Upstream NORMAL_LOADER_SRCS, minus the OPT_LOADER_SRCS pair that only
        -- builds with the assembly path (see the header note).
        sources = {
            "*/loader/allocation.c",
            "*/loader/cJSON.c",
            "*/loader/debug_utils.c",
            "*/loader/extension_manual.c",
            "*/loader/gpa_helper.c",
            "*/loader/loader.c",
            "*/loader/loader_environment.c",
            "*/loader/loader_json.c",
            "*/loader/log.c",
            "*/loader/settings.c",
            "*/loader/terminator.c",
            "*/loader/trampoline.c",
            "*/loader/unknown_function_handling.c",
            "*/loader/wsi.c",
        },

        targets = { ["vulkan"] = { kind = "lib" } },
        deps    = { ["compat.vulkan-headers"] = "1.4.357.0" },

        -- VK_ENABLE_BETA_EXTENSIONS is not optional despite the name: the
        -- checked-in generated/vk_object_types.h references
        -- VK_OBJECT_TYPE_CUDA_MODULE_NV, which the headers only declare under
        -- this macro. Without it the loader does not compile at all.
        cflags = { "-DVK_ENABLE_BETA_EXTENSIONS" },

        linux = {
            -- LOADER_ENABLE_LINUX_SORT is what upstream sets alongside
            -- loader_linux.c: it sorts physical devices by PCI bus info so
            -- device 0 is the discrete GPU rather than whichever ICD replied
            -- first.
            sources = { "*/loader/loader_linux.c" },
            cflags  = {
                "-D_GNU_SOURCE",
                "-DHAVE_ALLOCA_H",
                "-DLOADER_ENABLE_LINUX_SORT",
                "-DVK_USE_PLATFORM_XLIB_KHR",
                "-DVK_USE_PLATFORM_XCB_KHR",
                "-include", "mcpp_vulkan_paths.h",
            },
            -- The two VK_USE_PLATFORM_X*_KHR defines make vulkan_xlib.h /
            -- vulkan_xcb.h pull in <X11/Xlib.h> and <xcb/xcb.h>, so the X
            -- headers are a COMPILE dependency of the loader here, not just of
            -- whoever creates the surface. xorgproto carries <X11/X.h>, which
            -- Xlib.h includes. Only headers are needed — the loader never links
            -- against Xlib; the ICD does.
            deps = {
                ["compat.x11"]       = "1.8.13",
                ["compat.xcb"]       = "1.17.0",
                ["compat.xorgproto"] = "2025.1",
            },
            -- dlopen for the ICDs and layers; pthread for the loader's locks.
            ldflags = { "-ldl", "-lpthread", "-lm" },
            runtime = {
                -- The loader itself is linked statically, but a Vulkan program
                -- is still useless without an installed ICD. Model that as a
                -- capability rather than pretending a vendor driver is a
                -- redistributable package — same call `compat.glfw` makes for
                -- `opengl.glx.driver` (see the GL runtime plan doc).
                capabilities = { "vulkan.icd.driver" },
            },
        },

        macosx = {
            -- No loader_linux.c and no LINUX_SORT. VK_USE_PLATFORM_METAL_EXT
            -- costs nothing here: vulkan_metal.h typedefs CAMetalLayer to void
            -- outside an Objective-C TU, so it needs no Metal SDK to compile.
            cflags = {
                "-D_GNU_SOURCE",
                -- Both surface platforms, not just Metal: wsi.c `#error`s with
                -- "VK_USE_PLATFORM_MACOS_MVK not defined!" when only one of the
                -- pair is present on Apple.
                "-DVK_USE_PLATFORM_METAL_EXT",
                "-DVK_USE_PLATFORM_MACOS_MVK",
                -- Upstream's own switch for a statically linked loader; it is
                -- what makes the entry points resolve without the DLL-style
                -- export table.
                "-DAPPLE_STATIC_LOADER",
                "-include", "mcpp_vulkan_paths.h",
            },
            -- CoreFoundation for CFRelease & friends: the loader reads bundle
            -- paths when it looks for ICDs.
            ldflags = { "-framework", "CoreFoundation", "-lpthread", "-lm" },
            runtime = {
                -- macOS has no native Vulkan; the ICD is MoltenVK, layered over
                -- Metal. The loader loads it exactly like any other ICD.
                capabilities = { "vulkan.icd.driver" },
            },
        },

        -- No `windows` block: see the deferral note at the top.
    },
}
