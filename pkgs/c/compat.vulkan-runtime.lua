-- compat.vulkan-runtime — host Vulkan ICD adapter for mcpp Linux applications.
--
-- The exact counterpart of `compat.glx-runtime`, for the same reason and in the
-- same shape. A GPU driver cannot be a package: the ICD has to match the kernel
-- driver on the machine it runs on, so the GL runtime plan
-- (.agents/docs/2026-06-03-gl-runtime-packages-plan.md) settled on modelling it
-- as a HOST CAPABILITY rather than "silently pretending vendor drivers are
-- normal redistributable packages". Nothing is vendored here either — this is a
-- symlink farm plus the metadata that makes it reachable.
--
-- WHAT IT FIXES. `compat.vulkan` builds the Khronos loader, and the loader finds
-- every ICD manifest on the host correctly. It then fails to dlopen a single
-- driver:
--
--   DRIVER: Found the following files: /usr/share/vulkan/icd.d/lvp_icd.json …
--   ERROR: libvulkan_lvp.so: cannot open shared object file
--
-- The libraries are right there in /usr/lib/x86_64-linux-gnu. What cannot reach
-- them is the process: an mcpp-built binary runs under mcpp's OWN glibc
--
--   interp: …/xpkgs/xim-x-glibc/2.39/lib64/ld-linux-x86-64.so.2
--   rpath : …/xim-x-glibc/2.39/lib64:…/xim-x-gcc/…/lib64:$ORIGIN
--
-- so a bare-soname dlopen from inside the sandbox does not search the host's
-- library path at all. `runtime.library_dirs` below puts a package-owned
-- directory of symlinks on that path, which is precisely how `compat.glx-runtime`
-- makes host OpenGL work — and why the OpenGL backends already run while Vulkan
-- did not.
--
-- THE PATTERN LIST covers the ICDs plus their transitive dependencies, because
-- the whole chain has to resolve through the same directory. Mesa's software
-- rasterizer pulls LLVM; NVIDIA pulls its own family. `libstdc++` is in the list
-- and that is not an oversight: mcpp links libstdc++ STATICALLY (it is absent
-- from a built binary's NEEDED), so a dlopen'd C++ ICD like lavapipe has nothing
-- to resolve against unless the host copy is provided here.
--
-- NOTHING IS REQUIRED. Unlike `compat.glx-runtime`, which errors when libGL is
-- missing, a machine with no Vulkan driver at all is a legitimate configuration
-- — every CI runner in this repo is one. The farm is then simply empty and the
-- loader reports its own four extensions, which is what
-- `tests/examples/vulkan` asserts.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "vulkan-runtime",
    description = "Host Vulkan ICD runtime adapter for mcpp Linux applications",
    licenses    = {"Apache-2.0"},
    repo        = "https://github.com/KhronosGroup/Vulkan-Loader",
    type        = "package",

    xpm = {
        linux = {
            ["2026.07.29"] = {
                -- Nothing is downloaded that matters: the package's content is
                -- the symlink farm install() builds from the host. This is just
                -- a stable, tiny anchor so the xpm entry is well-formed, the
                -- same trick compat.glx-runtime uses with an OpenGL-Registry
                -- README.
                url    = "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Loader/vulkan-sdk-1.4.357.0/README.md",
                sha256 = "21ec0987a05bd680ecd11f8be747e27744d7558f7318736f6cb8a5c5ec1b8ba8",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        sources      = { "mcpp_generated/vulkan_runtime_empty.c" },
        targets      = { ["vulkan_runtime"] = { kind = "lib" } },
        deps         = {},
        runtime = {
            library_dirs = { "mcpp_generated/vulkan_runtime/lib" },
            capabilities = { "vulkan.icd.driver" },
            provides     = { "vulkan.icd.driver" },
        },
    },
}

import("xim.libxpkg.pkginfo")
import("xim.libxpkg.log")

local function sh_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function split_paths(value)
    local out = {}
    if not value or value == "" then
        return out
    end
    for item in tostring(value):gmatch("[^:]+") do
        if item ~= "" then
            table.insert(out, item)
        end
    end
    return out
end

local function candidate_dirs()
    local out = {}
    local seen = {}
    local function add(dir)
        if dir and dir ~= "" and not seen[dir] and os.isdir(dir) then
            seen[dir] = true
            table.insert(out, dir)
        end
    end

    for _, dir in ipairs(split_paths(os.getenv("MCPP_HOST_VULKAN_LIBRARY_PATH"))) do
        add(dir)
    end
    add("/lib/x86_64-linux-gnu")
    add("/usr/lib/x86_64-linux-gnu")
    add("/lib64")
    add("/usr/lib64")
    add("/usr/lib")
    return out
end

-- ICDs first, then the transitive set they pull in — the whole chain has to
-- resolve through this one directory. Verified against Mesa's lavapipe (LLVM,
-- drm, expat, xcb, wayland, zstd) and NVIDIA's ICD, which is libGLX_nvidia.so.0
-- and drags the libnvidia* family.
--
-- EVERY DEPENDENCY PATTERN IS VERSIONED (`lib*.so.*`), deliberately. mcpp puts
-- `runtime.library_dirs` on the LINK line as well as the runtime path, so a bare
-- `libxcb.so` harvested here would shadow this index's own `compat.xcb` and the
-- link fails with `undefined reference to XauDisposeAuth`. Versioned sonames are
-- invisible to the linker (it resolves `-lxcb` through `libxcb.so`/`libxcb.a`)
-- and are exactly what dlopen asks for, so the split is not a workaround so much
-- as the correct spelling. `compat.glx-runtime` never hit this only because the
-- GL family it harvests is not otherwise linked from the index.
--
-- The Mesa ICDs themselves are genuinely named `libvulkan_lvp.so` with no
-- version, which is safe: nothing links `-lvulkan_lvp`.
--
-- The host's own `libvulkan.so*` is deliberately NOT harvested: `compat.vulkan`
-- builds the loader itself, as a shared object with the canonical
-- `libvulkan.so.1` soname, and a second one on the path would be resolved by
-- SDL2's `dlopen` instead. One loader per process is the whole point.
local host_vulkan_patterns = {
    -- Mesa ICDs: lavapipe, intel, radeon, nouveau, virtio, asahi, gfxstream
    "libvulkan_*.so",
    -- NVIDIA's ICD and its family
    "libGLX_nvidia.so.*",
    "libnvidia*.so.*",
    -- transitive dependencies, versioned only
    "libLLVM*.so.*",
    "libdrm*.so.*",
    "libexpat.so.*",
    -- The X client stack, including its own auth dependencies. Incomplete is
    -- worse than absent here: a farm carrying libxcb.so.1 but not libXau.so.6
    -- shadows the host copy that would otherwise have resolved, and the
    -- executable fails to start.
    "libxcb*.so.*",
    "libX11-xcb.so.*",
    "libXau.so.*",
    "libXdmcp.so.*",
    "libbsd.so.*",
    "libmd.so.*",
    "libxshmfence.so.*",
    "libwayland-client.so.*",
    "libz.so.*",
    "libzstd.so.*",
    "libelf.so.*",
    "libffi.so.*",
    "libedit.so.*",
    "libtinfo.so.*",
    "libxml2.so.*",
    "libstdc++.so.*",
}

local function link_runtime_libs(outdir)
    os.mkdir(outdir)
    for _, dir in ipairs(candidate_dirs()) do
        for _, pattern in ipairs(host_vulkan_patterns) do
            os.exec(
                "for lib in " .. sh_quote(dir) .. "/" .. pattern ..
                "; do [ -e \"$lib\" ] || continue; " ..
                "ln -sf \"$lib\" " .. sh_quote(outdir) .. "/\"$(basename \"$lib\")\"; " ..
                "done"
            )
        end
    end
    return true
end

function install()
    os.tryrm(pkginfo.install_dir())
    os.mkdir(pkginfo.install_dir())

    local generated = path.join(pkginfo.install_dir(), "mcpp_generated")
    os.mkdir(generated)
    io.writefile(path.join(generated, "vulkan_runtime_empty.c"),
        "int mcpp_compat_vulkan_runtime_anchor(void) { return 0; }\n")

    return link_runtime_libs(path.join(generated, "vulkan_runtime", "lib"))
end
