// compat.gmp `gmpxx` feature: the C++ bindings compile against the consumer's
// own toolchain and link.
//
// The stream operators are the load-bearing part. Everything else in gmpxx.h is
// inline in the header, so a member that only did arithmetic would link with
// the feature TURNED OFF and prove nothing. `os << mpz_class` resolves to
// __gmp_ostream_operator, which lives in cxx/osmpz.cc — one of the eleven
// sources the feature gates.
//
// It is also the one place where "compiled by the consumer's toolchain" is
// actually tested: those symbols carry std::ostream / std::string in their
// mangled names, so a prebuilt libgmpxx would resolve on libstdc++ and fail on
// libc++.

#include <gmpxx.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int check_mpz_class() {
    // 128-bit-ish multiply, formatted through the ostream operator.
    mpz_class a("123456789012345678901234567890");
    mpz_class b = a * a;
    std::ostringstream os;
    os << b;
    if (os.str() != "15241578753238836750495351562536198787501905199875019052100")
        return 20;

    // Operator overloading across mixed types, and the comparison operators.
    mpz_class c = a + 10;
    if (c - 10 != a) return 21;
    if (!(a < b) || !(b > a)) return 22;

    // Number-theoretic helpers reached through the C++ names.
    mpz_class g = gcd(mpz_class(123456789), mpz_class(987654321));
    if (g != 9) return 23;

    // get_str / set_str round trip via std::string.
    std::string hex = b.get_str(16);
    mpz_class back(hex, 16);
    if (back != b) return 24;

    // istream operator — the other half of the gated sources (cxx/ismpz.cc).
    std::istringstream is("999999999999999999999999999999");
    mpz_class parsed;
    is >> parsed;
    if (parsed != mpz_class("999999999999999999999999999999")) return 25;
    return 0;
}

int check_mpq_class() {
    mpq_class q(1, 3);
    q += mpq_class(1, 6);          // 1/2
    q.canonicalize();
    std::ostringstream os;
    os << q;
    if (os.str() != "1/2") return 30;
    if (q.get_num() != 1 || q.get_den() != 2) return 31;

    mpq_class r = q * mpq_class(4, 3);   // 2/3
    r.canonicalize();
    if (r != mpq_class(2, 3)) return 32;
    return 0;
}

int check_mpf_class() {
    mpf_class two(2, 256);
    mpf_class root = sqrt(two);
    if (!(root > 1.41 && root < 1.42)) return 40;
    // Squaring it back has to land within the working precision.
    mpf_class err = abs(root * root - 2);
    if (!(err < mpf_class(1e-30, 256))) return 41;

    std::ostringstream os;
    os << mpf_class(1.5, 64);
    if (os.str() != "1.5") return 42;
    return 0;
}

}  // namespace

int main() {
    int rc = check_mpz_class();
    if (rc == 0) rc = check_mpq_class();
    if (rc == 0) rc = check_mpf_class();
    if (rc != 0) {
        std::fprintf(stderr, "compat.gmp gmpxx: check failed, code %d\n", rc);
        return rc;
    }
    std::cout << "compat.gmp gmpxx: mpz_class/mpq_class/mpf_class + streams ok"
              << std::endl;
    return 0;
}
