// compat.gmp end-to-end, C API: gmp.h resolves, the library links, and every
// source directory the descriptor compiles actually works.
//
// One assertion group per directory in the package's source list, because a
// build described by globs is exactly the kind that can lose a whole directory
// without anything failing to compile:
//
//   mpn/generic  every group below (mpz/mpq/mpf all bottom out in mpn)
//   mpz          factorial / powm / gcd / string round-trip / import-export
//   mpq          rational arithmetic and canonicalisation
//   mpf          the float layer, incl. mpf_sqrt
//   printf       gmp_snprintf
//   scanf        gmp_sscanf
//   rand         gmp_randinit / mpz_urandomm
//
// The expected values are precomputed independently of GMP (Python
// int/fractions), so a library that silently picked up a host libgmp, or one
// built from the wrong tables, fails instead of agreeing with itself.

#include <gmp.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {

// GMP allocates the result of mpz_get_str with ITS allocator, which is not
// necessarily the one behind std::free. The documented way to give it back is
// the free function GMP reports, and it is the only way that stays correct if a
// consumer ever installs custom memory functions.
void gmp_free(char* p) {
    if (p == nullptr) return;
    void (*freefunc)(void*, size_t) = nullptr;
    mp_get_memory_functions(nullptr, nullptr, &freefunc);
    freefunc(p, std::strlen(p) + 1);
}

int check_limb_shape() {
    // The descriptor vendors tables generated for limb=64/nail=0 and its
    // config.h refuses to compile without a 64-bit unsigned type. If gmp.h and
    // the compiled library ever disagreed about the limb — the LP64/LLP64
    // mistake a configure-baked header makes when it reaches Windows — every
    // other assertion here would still pass while mp_limb_t was half-width.
    if (GMP_LIMB_BITS != 64) return 10;
    if (sizeof(mp_limb_t) * 8 != GMP_LIMB_BITS) return 11;
    if (GMP_NAIL_BITS != 0) return 12;
    if (mp_bits_per_limb != GMP_LIMB_BITS) return 13;
    return 0;
}

int check_mpz() {
    // 100! — a known 158-digit value, compared digit by digit.
    {
        mpz_t f;
        mpz_init(f);
        mpz_fac_ui(f, 100);
        char* s = mpz_get_str(nullptr, 10, f);
        const char* expected =
            "93326215443944152681699238856266700490715968264381621468592963895217599993229915"
            "608941463976156518286253697920827223758251185210916864000000000000000000000000";
        int ok = s != nullptr && std::strcmp(s, expected) == 0;
        gmp_free(s);
        mpz_clear(f);
        if (!ok) return 20;
    }

    // Modular exponentiation, big enough to leave the single-limb paths:
    // 7^1234 mod (2^127 - 1).
    {
        mpz_t base, mod, res;
        mpz_init_set_ui(base, 7);
        mpz_init(mod);
        mpz_ui_pow_ui(mod, 2, 127);
        mpz_sub_ui(mod, mod, 1);
        mpz_init(res);
        mpz_powm_ui(res, base, 1234, mod);
        char* s = mpz_get_str(nullptr, 10, res);
        int ok = s != nullptr &&
                 std::strcmp(s, "55471114467826807814326117990010739464") == 0;
        gmp_free(s);
        mpz_clears(base, mod, res, nullptr);
        if (!ok) return 21;
    }

    // gcd / lcm on a pair with a known factorisation.
    //
    // The lcm is compared against an mpz built from a decimal string, not
    // through mpz_cmp_ui: that takes `unsigned long`, which is 32 bits on
    // LLP64 (Windows), so 13548070123626141 would be silently truncated and
    // the assertion would fail on Windows only. Every *_ui entry point in
    // this file is therefore kept under 2^32.
    {
        mpz_t a, b, g, l, expect;
        mpz_init_set_ui(a, 123456789);
        mpz_init_set_ui(b, 987654321);
        mpz_init(g);
        mpz_init(l);
        mpz_init_set_str(expect, "13548070123626141", 10);
        mpz_gcd(g, a, b);
        mpz_lcm(l, a, b);
        int ok = mpz_cmp_ui(g, 9) == 0 && mpz_cmp(l, expect) == 0;
        mpz_clears(a, b, g, l, expect, nullptr);
        if (!ok) return 22;
    }

    // Base-16 round trip through a 256-bit value, and mpz_sizeinbase.
    {
        const char* hex = "ffffffffffffffffffffffffffffffff"
                          "fffffffffffffffffffffffefffffc2f";  // secp256k1 p
        mpz_t p;
        mpz_init(p);
        if (mpz_set_str(p, hex, 16) != 0) { mpz_clear(p); return 23; }
        if (mpz_sizeinbase(p, 2) != 256) { mpz_clear(p); return 24; }
        char* back = mpz_get_str(nullptr, 16, p);
        int ok = back != nullptr && std::strcmp(back, hex) == 0;
        gmp_free(back);
        // It is prime, which walks probab_prime_p -> mpn_trialdiv -> the
        // vendored trialdivtab.h, and millerrabin -> mpz_powm.
        if (ok && mpz_probab_prime_p(p, 25) == 0) ok = 0;
        mpz_clear(p);
        if (!ok) return 25;
    }

    // import/export: the limb-order path, which is where a wrong
    // HAVE_LIMB_LITTLE_ENDIAN would show up.
    {
        const unsigned char be[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                                    0xfe, 0xdc, 0xba, 0x98};
        mpz_t v;
        mpz_init(v);
        mpz_import(v, sizeof(be), 1 /*most significant word first*/, 1, 0, 0, be);
        char* s = mpz_get_str(nullptr, 16, v);
        int ok = s != nullptr && std::strcmp(s, "123456789abcdeffedcba98") == 0;
        gmp_free(s);
        unsigned char out[sizeof(be)] = {0};
        size_t count = 0;
        mpz_export(out, &count, 1, 1, 0, 0, v);
        if (count != sizeof(be) || std::memcmp(out, be, sizeof(be)) != 0) ok = 0;
        mpz_clear(v);
        if (!ok) return 26;
    }

    return 0;
}

int check_mpq() {
    mpq_t q, r;
    mpq_init(q);
    mpq_init(r);
    mpq_set_ui(q, 1, 2);
    mpq_set_ui(r, 1, 3);
    mpq_add(q, q, r);                       // 5/6
    int ok = mpq_cmp_ui(q, 5, 6) == 0;
    mpq_set_ui(r, 10, 4);                   // canonicalises to 5/2
    mpq_canonicalize(r);
    if (mpz_cmp_ui(mpq_numref(r), 5) != 0 || mpz_cmp_ui(mpq_denref(r), 2) != 0) ok = 0;
    mpq_div(q, q, r);                       // (5/6) / (5/2) = 1/3
    if (mpq_cmp_ui(q, 1, 3) != 0) ok = 0;
    mpq_clear(q);
    mpq_clear(r);
    return ok ? 0 : 30;
}

int check_mpf() {
    mpf_set_default_prec(256);
    mpf_t x, s, one;
    mpf_init_set_ui(x, 2);
    mpf_init(s);
    mpf_init_set_ui(one, 1);
    mpf_sqrt(s, x);                          // sqrt(2)
    int ok = mpf_cmp_d(s, 1.0) > 0 && mpf_cmp_d(s, 2.0) < 0;
    // s*s must be 2 to well within the working precision.
    mpf_mul(x, s, s);
    mpf_sub_ui(x, x, 2);
    mpf_abs(x, x);
    mpf_div_2exp(one, one, 200);             // 2^-200
    if (mpf_cmp(x, one) >= 0) ok = 0;
    mpf_clears(x, s, one, nullptr);
    return ok ? 0 : 40;
}

int check_printf_scanf() {
    // printf/ — gmp_snprintf with the %Zd conversion.
    mpz_t v;
    mpz_init(v);
    mpz_ui_pow_ui(v, 3, 40);                 // 12157665459056928801
    char buf[64] = {0};
    int n = gmp_snprintf(buf, sizeof(buf), "[%Zd]", v);
    int ok = n == 22 && std::strcmp(buf, "[12157665459056928801]") == 0;
    mpz_clear(v);
    if (!ok) return 50;

    // scanf/ — the same value back out of a string.
    mpz_t w;
    mpz_init(w);
    if (gmp_sscanf("12157665459056928801", "%Zd", w) != 1) { mpz_clear(w); return 51; }
    mpz_t expect;
    mpz_init(expect);
    mpz_ui_pow_ui(expect, 3, 40);
    ok = mpz_cmp(w, expect) == 0;
    mpz_clears(w, expect, nullptr);
    return ok ? 0 : 52;
}

int check_rand() {
    // rand/ — a seeded Mersenne Twister is deterministic, so this asserts the
    // generator runs AND that every draw lands in range.
    gmp_randstate_t st;
    gmp_randinit_mt(st);
    gmp_randseed_ui(st, 20260810u);
    mpz_t n, x;
    mpz_init(n);
    mpz_ui_pow_ui(n, 10, 30);
    mpz_init(x);
    int ok = 1;
    for (int i = 0; i < 64; ++i) {
        mpz_urandomm(x, st, n);
        if (mpz_sgn(x) < 0 || mpz_cmp(x, n) >= 0) { ok = 0; break; }
    }
    mpz_clears(n, x, nullptr);
    gmp_randclear(st);
    return ok ? 0 : 60;
}

}  // namespace

int main() {
    int rc = check_limb_shape();
    if (rc == 0) rc = check_mpz();
    if (rc == 0) rc = check_mpq();
    if (rc == 0) rc = check_mpf();
    if (rc == 0) rc = check_printf_scanf();
    if (rc == 0) rc = check_rand();
    if (rc != 0) {
        std::fprintf(stderr, "compat.gmp: check failed, code %d\n", rc);
        return rc;
    }
    std::printf("compat.gmp %s: mpz/mpq/mpf/printf/scanf/rand ok (limb %d bits)\n",
                gmp_version, GMP_LIMB_BITS);
    return 0;
}
