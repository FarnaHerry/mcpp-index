// compat.plf-hive — assert the guarantee that distinguishes a hive from a
// vector or a deque: an element's ADDRESS stays valid while its neighbours are
// erased and while new elements are inserted. If the package silently resolved
// to something else, or the header were stubbed, these checks fail rather than
// passing vacuously.
#include <plf_hive.h>
import std;

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    plf::hive<int> h;

    // Fill, and remember where a few elements physically live.
    std::vector<plf::hive<int>::iterator> its;
    for (int i = 0; i < 200; ++i) its.push_back(h.insert(i));
    check(h.size() == 200, "size after 200 inserts");

    const int* addr_of_50  = &*its[50];
    const int* addr_of_150 = &*its[150];

    // Erase every even element. In a vector this would invalidate everything
    // after the first erasure; in a hive nothing moves.
    std::size_t erased = 0;
    for (int i = 0; i < 200; i += 2) {
        h.erase(its[static_cast<std::size_t>(i)]);
        ++erased;
    }
    check(erased == 100, "erased 100 elements");
    check(h.size() == 100, "size after erasures");
    check(&*its[51] != nullptr, "surviving iterator still dereferenceable");
    check(addr_of_50 != nullptr && addr_of_150 != nullptr, "addresses captured");

    // Odd elements survived, at their original addresses.
    const int* addr_of_51 = &*its[51];
    check(*its[51] == 51, "surviving element keeps its value");

    // Inserting again reuses the erased slots -- and must not relocate anything.
    for (int i = 0; i < 100; ++i) h.insert(1000 + i);
    check(h.size() == 200, "size after refill");
    check(&*its[51] == addr_of_51, "address stable across erase + insert");
    check(*its[51] == 51, "value stable across erase + insert");

    // The surviving originals are exactly the odd numbers.
    long odd_sum = 0;
    long expected = 0;
    for (int v : h) if (v < 1000) odd_sum += v;
    for (int i = 1; i < 200; i += 2) expected += i;
    check(odd_sum == expected, "surviving elements are exactly the odd ones");

    // Erasing through the range interface and clearing must leave it empty.
    h.clear();
    check(h.empty(), "empty after clear");
    check(h.begin() == h.end(), "begin == end when empty");

    if (ok) std::println("plf::hive OK");
    return ok ? 0 : 1;
}
