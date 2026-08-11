// compat.vulkan-memory-allocator — drive VMA's VIRTUAL allocator.
//
// Everything else in VMA needs a VkDevice, which a CI runner with no GPU and no
// driver cannot create. The virtual allocator is the same sub-allocation
// algorithm with the Vulkan calls stripped out: it hands out offsets inside a
// notional block. That makes it the one part of VMA whose BEHAVIOUR can be
// asserted here -- and since it is compiled from the same VMA_IMPLEMENTATION
// TU as the rest, it still proves the package's implementation is built and
// linked (these symbols do not exist in the header-only view).
#include <vk_mem_alloc.h>
import std;

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    constexpr VkDeviceSize BLOCK = 1u << 20;   // 1 MiB

    VmaVirtualBlockCreateInfo bci{};
    bci.size = BLOCK;

    VmaVirtualBlock block = VK_NULL_HANDLE;
    check(vmaCreateVirtualBlock(&bci, &block) == VK_SUCCESS, "virtual block created");
    check(block != VK_NULL_HANDLE, "block handle is non-null");
    if (block == VK_NULL_HANDLE) return 1;

    check(vmaIsVirtualBlockEmpty(block), "a fresh block is empty");

    // Allocate a set of differently-sized, differently-aligned chunks.
    struct Chunk { VmaVirtualAllocation h; VkDeviceSize off, size; };
    std::vector<Chunk> chunks;
    for (int i = 0; i < 16; ++i) {
        VmaVirtualAllocationCreateInfo aci{};
        aci.size      = static_cast<VkDeviceSize>(1024u << (i % 5));
        aci.alignment = 256;

        VmaVirtualAllocation h{};
        VkDeviceSize off = 0;
        if (vmaVirtualAllocate(block, &aci, &h, &off) != VK_SUCCESS) {
            check(false, "vmaVirtualAllocate succeeded");
            break;
        }
        check(off % 256 == 0, "returned offset honours the requested alignment");
        check(off + aci.size <= BLOCK, "allocation stays inside the block");
        chunks.push_back({h, off, aci.size});
    }
    check(chunks.size() == 16, "all 16 allocations succeeded");
    check(!vmaIsVirtualBlockEmpty(block), "block is not empty while allocations live");

    // The whole point of an allocator: no two live allocations may overlap.
    std::ranges::sort(chunks, {}, &Chunk::off);
    for (std::size_t i = 1; i < chunks.size(); ++i) {
        check(chunks[i - 1].off + chunks[i - 1].size <= chunks[i].off,
              "live allocations do not overlap");
    }

    // Statistics must account for what we asked for.
    VkDeviceSize requested = 0;
    for (const auto& c : chunks) requested += c.size;

    VmaStatistics stats{};
    vmaGetVirtualBlockStatistics(block, &stats);
    check(stats.allocationCount == chunks.size(), "stats report every allocation");
    check(stats.allocationBytes == requested, "stats report the requested bytes");

    // Freeing must return the space: after freeing everything the block is
    // empty again and a single block-sized allocation fits.
    for (const auto& c : chunks) vmaVirtualFree(block, c.h);
    check(vmaIsVirtualBlockEmpty(block), "block is empty again after freeing all");

    VmaVirtualAllocationCreateInfo whole{};
    whole.size = BLOCK;
    VmaVirtualAllocation big{};
    VkDeviceSize big_off = 0;
    check(vmaVirtualAllocate(block, &whole, &big, &big_off) == VK_SUCCESS,
          "the entire block is allocatable once freed");
    check(big_off == 0, "the block-sized allocation starts at offset 0");
    vmaVirtualFree(block, big);

    // And an over-sized request must FAIL rather than silently succeed.
    VmaVirtualAllocationCreateInfo toobig{};
    toobig.size = BLOCK * 2;
    VmaVirtualAllocation none{};
    VkDeviceSize none_off = 0;
    check(vmaVirtualAllocate(block, &toobig, &none, &none_off) != VK_SUCCESS,
          "an over-sized request is rejected");

    vmaDestroyVirtualBlock(block);

    if (ok) std::println("VulkanMemoryAllocator OK");
    return ok ? 0 : 1;
}
