// compat.mimalloc — assert the allocator is real, not just that it links.
//
// Every check below can fail: mi_usable_size reports what mimalloc actually
// reserved (so it proves the pointer came from mimalloc's own bookkeeping and
// not from a fallback), mi_zalloc must return zeroed memory, and a private
// heap must be able to hand out and destroy blocks independently of the
// default heap.
#include <mimalloc.h>
import std;

int main() {
    bool ok = true;

    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    // 1. Basic allocation, and mimalloc knows the block's real size.
    void* p = mi_malloc(1024);
    check(p != nullptr, "mi_malloc(1024) returned null");
    if (p) {
        // mimalloc rounds up to a size class, so usable >= requested.
        check(mi_usable_size(p) >= 1024, "mi_usable_size < requested size");
        std::memset(p, 0xAB, 1024);
        check(static_cast<unsigned char*>(p)[1023] == 0xAB, "written byte did not survive");
        mi_free(p);
    }

    // 2. mi_zalloc must actually zero.
    constexpr std::size_t n = 512;
    auto* z = static_cast<unsigned char*>(mi_zalloc(n));
    check(z != nullptr, "mi_zalloc returned null");
    if (z) {
        check(std::all_of(z, z + n, [](unsigned char c) { return c == 0; }),
              "mi_zalloc did not zero the block");
        mi_free(z);
    }

    // 3. Aligned allocation honours the alignment.
    void* a = mi_malloc_aligned(256, 64);
    check(a != nullptr, "mi_malloc_aligned returned null");
    if (a) {
        check(reinterpret_cast<std::uintptr_t>(a) % 64 == 0, "alignment not honoured");
        mi_free(a);
    }

    // 4. A private heap allocates and tears down independently.
    mi_heap_t* heap = mi_heap_new();
    check(heap != nullptr, "mi_heap_new returned null");
    if (heap) {
        void* hp = mi_heap_malloc(heap, 4096);
        check(hp != nullptr, "mi_heap_malloc returned null");
        check(mi_heap_contains(heap, hp), "heap does not contain its own block");
        mi_heap_destroy(heap);  // frees hp with it
    }

    std::println("compat.mimalloc smoke test: {}", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
