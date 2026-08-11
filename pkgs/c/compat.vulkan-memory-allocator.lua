-- Form B inline descriptor for the Vulkan Memory Allocator (VMA) — AMD/GPUOpen's
-- allocator for Vulkan device memory. Vulkan hands you a small number of large
-- heaps and expects you to sub-allocate; VMA does that sub-allocation, picks the
-- memory type for a usage, and handles buffer/image suballocation, defragmentation
-- and budgeting.
--
-- SHAPE: header-only WITH a generated implementation TU. `include/vk_mem_alloc.h`
-- carries declarations plus, behind VMA_IMPLEMENTATION, the implementation.
-- Upstream ships no implementation .c/.cpp of its own (its `VmaUsage.cpp` lives
-- under `src/` with the test app, not the library), so this package generates
-- one -- that is what makes it linkable rather than a header drop.
--
-- CONSEQUENCE FOR CONSUMERS: do NOT also define VMA_IMPLEMENTATION. It is
-- already compiled into this package's lib, and defining it again duplicates
-- every symbol -- a LINK error, so it surfaces late. Just
-- `#include <vk_mem_alloc.h>` and link.
--
-- IT IS C++, NOT C: despite the C-shaped API, the implementation is C++14 and
-- must be compiled as C++ (upstream says so, and the file is full of templates
-- and STL containers). Hence the generated TU is `.cpp`, not `.c`.
--
-- DEPENDENCY: compat.vulkan-headers, because vk_mem_alloc.h includes
-- <vulkan/vulkan.h> for the handle and enum types. Headers only -- and that is
-- a deliberate choice that the implementation TU has to be built to match:
--
--   VMA_STATIC_VULKAN_FUNCTIONS defaults to 1, which makes the implementation
--   reference vkGetBufferMemoryRequirements2, vkBindBufferMemory2,
--   vkGetPhysicalDeviceProperties2 and friends DIRECTLY. Those symbols live in
--   the loader, so with a headers-only dependency the package would not link
--   (verified: eight undefined references out of vk_mem_alloc.h:13600+).
--
--   Pulling compat.vulkan to satisfy them would be the wrong fix: it would make
--   every consumer of a memory allocator link a Vulkan loader whether or not it
--   loads Vulkan that way, and it would fight consumers that dispatch through
--   volk or their own device-level table.
--
--   So the generated TU selects the DYNAMIC path instead. VMA then calls
--   nothing by name; it resolves every entry point through the pointers in
--   VmaVulkanFunctions.
--
-- CONSEQUENCE FOR CONSUMERS: fill in `VmaAllocatorCreateInfo::pVulkanFunctions`
-- with at least `vkGetInstanceProcAddr` and `vkGetDeviceProcAddr` -- VMA fetches
-- the rest itself from those two. This is the mode engines that use volk or a
-- custom loader already run in. (The virtual-allocator API needs none of this,
-- which is why it works with no Vulkan at all.)
--
-- Pinned to the same SDK line as compat.vulkan-headers / compat.spirv-reflect
-- (1.4.357.0); VMA's own 3.4.0 release targets Vulkan 1.4.
package = {
    spec        = "1",
    namespace   = "compat",
    name        = "vulkan-memory-allocator",
    description = "AMD Vulkan Memory Allocator — device memory sub-allocation for Vulkan",
    licenses    = {"MIT"},
    repo        = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator",
    type        = "package",

    xpm = {
        linux = {
            ["3.4.0"] = {
                url    = {
                    GLOBAL = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/vulkan-memory-allocator/releases/download/3.4.0/vulkan-memory-allocator-3.4.0.tar.gz",
                },
                sha256 = "822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1",
            },
        },
        macosx = {
            ["3.4.0"] = {
                url    = {
                    GLOBAL = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/vulkan-memory-allocator/releases/download/3.4.0/vulkan-memory-allocator-3.4.0.tar.gz",
                },
                sha256 = "822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1",
            },
        },
        windows = {
            ["3.4.0"] = {
                url    = {
                    GLOBAL = "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/vulkan-memory-allocator/releases/download/3.4.0/vulkan-memory-allocator-3.4.0.tar.gz",
                },
                sha256 = "822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1",
            },
        },
    },

    mcpp = {
        language     = "c++23",
        import_std   = false,
        c_standard   = "c11",
        include_dirs = { "*/include" },
        -- Upstream ships no implementation TU for the library; supply one.
        -- C++, not C: the implementation is C++14 despite the C-shaped API.
        generated_files = {
            ["mcpp_generated/vma_impl.cpp"] = [==[
// Resolve Vulkan through VmaVulkanFunctions rather than by symbol name, so
// this package links against the Vulkan HEADERS alone -- no loader. See the
// descriptor header for why, and for what the consumer must supply.
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
]==],
        },
        sources      = { "mcpp_generated/vma_impl.cpp" },
        targets      = { ["vulkan-memory-allocator"] = { kind = "lib" } },
        -- Headers only: VMA calls Vulkan through pointers the consumer supplies,
        -- so the loader is the consumer's dependency, not ours.
        deps         = { ["compat.vulkan-headers"] = "1.4.357.0" },
    },
}
