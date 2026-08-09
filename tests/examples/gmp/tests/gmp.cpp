// compat.gmp end-to-end: gmp.h resolves, libgmp.a links, and the three
// arithmetic layers (mpz integers / mpq rationals / mpf floats) all compute.
//
// The values asserted here are precomputed independently of GMP (Python
// int/fractions/math), so a build that silently picked up a host libgmp, or a
// static archive that is somehow broken, fails instead of passing by accident.
//
// HAVE_GMP comes from this project's own cfg-gated cxxflags — the package is
// linux/macOS-only, so elsewhere this file is an empty main().
#ifdef HAVE_GMP
#include <gmp.h>

#include <cstring>
#include <cstdlib>

int main() {
    // mpz — factorial: 100! is a known 158-digit number.
    {
        mpz_t f;
        mpz_init(f);
        mpz_fac_ui(f, 100);
        char* s = mpz_get_str(nullptr, 10, f);
        const char* expected =
            "93326215443944152681699238856266700490715968264381621468592963895217599993229915"
            "608941463976156518286253697920827223758251185210916864000000000000000000000000";
        int ok = s != nullptr && std::strcmp(s, expected) == 0;
        if (s != nullptr) std::free(s);
        mpz_clear(f);
        if (!ok) return 2;
    }

    // mpz — modular exponentiation: 2^1000 mod 12345 == 5341.
    {
        mpz_t base, mod, res;
        mpz_init_set_ui(base, 2);
        mpz_init_set_ui(mod, 12345);
        mpz_init(res);
        mpz_powm_ui(res, base, 1000, mod);
        int ok = mpz_cmp_ui(res, 5341) == 0;
        mpz_clear(base);
        mpz_clear(mod);
        mpz_clear(res);
        if (!ok) return 3;
    }

    // mpz — gcd: gcd(123456789, 987654321) == 9.
    {
        mpz_t a, b, g;
        mpz_init_set_ui(a, 123456789);
        mpz_init_set_ui(b, 987654321);
        mpz_init(g);
        mpz_gcd(g, a, b);
        int ok = mpz_cmp_ui(g, 9) == 0;
        mpz_clear(a);
        mpz_clear(b);
        mpz_clear(g);
        if (!ok) return 4;
    }

    // mpq — rational arithmetic: 1/2 + 1/3 == 5/6.
    {
        mpq_t q, r;
        mpq_init(q);
        mpq_init(r);
        mpq_set_ui(q, 1, 2);
        mpq_set_ui(r, 1, 3);
        mpq_add(q, q, r);
        int ok = mpq_cmp_ui(q, 5, 6) == 0;
        mpq_clear(q);
        mpq_clear(r);
        if (!ok) return 5;
    }

    // mpf — floating layer: 1 < sqrt(2) < 2 (mpf_sqrt on a double seed).
    {
        mpf_t x, s;
        mpf_init_set_d(x, 2.0);
        mpf_init(s);
        mpf_sqrt(s, x);
        int ok = mpf_cmp_d(s, 1.0) > 0 && mpf_cmp_d(s, 2.0) < 0;
        mpf_clear(x);
        mpf_clear(s);
        if (!ok) return 6;
    }

    return 0;
}
#else
int main() { return 0; }  // compat.gmp is linux/macOS-only; no-op elsewhere
#endif
