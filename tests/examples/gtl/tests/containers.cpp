// compat.gtl — exercise the two container families that are the reason to pull
// gtl in: the Swiss-table flat_hash_map and the ordered btree_set. Every check
// asserts observable behaviour (contents, ordering, erasure), not just that the
// headers compile.
#include <gtl/phmap.hpp>
#include <gtl/btree.hpp>
import std;

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    // ---- flat_hash_map ---------------------------------------------------
    gtl::flat_hash_map<std::string, int> m;
    for (int i = 0; i < 1000; ++i) m.emplace(std::format("k{}", i), i);
    check(m.size() == 1000, "flat_hash_map size after 1000 emplaces");
    check(m.at("k0") == 0 && m.at("k999") == 999, "lookup returns what was stored");
    check(m.find("absent") == m.end(), "miss returns end()");

    // Rehashing must not lose or corrupt entries.
    long sum = 0;
    for (const auto& [k, v] : m) sum += v;
    check(sum == 999L * 1000 / 2, "every value survives rehashing");

    check(m.erase("k500") == 1, "erase reports one removal");
    check(m.erase("k500") == 0, "second erase reports none");
    check(m.size() == 999, "size after erase");
    check(!m.contains("k500"), "erased key is gone");

    // ---- parallel_flat_hash_map -----------------------------------------
    // Same API, internally sharded. Worth touching because it is gtl's
    // headline container and instantiates a different code path.
    gtl::parallel_flat_hash_map<int, int> pm;
    for (int i = 0; i < 500; ++i) pm[i] = i * i;
    check(pm.size() == 500, "parallel map size");
    check(pm[20] == 400, "parallel map value");

    // ---- btree_set -------------------------------------------------------
    // Unlike the hash maps, this one is ORDERED -- assert that.
    gtl::btree_set<int> s;
    for (int i : {42, 7, 99, 1, 63, 7}) s.insert(i);
    check(s.size() == 5, "btree_set drops the duplicate");
    check(std::ranges::is_sorted(s), "btree_set iterates in order");
    check(*s.begin() == 1 && *s.rbegin() == 99, "btree_set endpoints");
    check(*s.lower_bound(50) == 63, "btree_set lower_bound");

    if (ok) std::println("gtl OK");
    return ok ? 0 : 1;
}
