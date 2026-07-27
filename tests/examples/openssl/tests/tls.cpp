// compat.openssl end-to-end: headers resolve, both archives link, and the
// library initialises far enough to stand up a TLS context and run a digest.
//
// Touching BOTH archives is the point: libssl.a (SSL_CTX_new, TLS_method) and
// libcrypto.a (EVP_*). A link that silently dropped one, or picked up a host
// libssl.so instead of the package's own static build, fails here.
//
// HAVE_OPENSSL comes from this project's own cfg-gated cxxflags — the package
// is linux/macOS-only, so elsewhere this file is an empty main().
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#include <cstring>

int main() {
    // Built from the 3.5.1 tarball this descriptor pins.
    if (OPENSSL_VERSION_MAJOR != 3 || OPENSSL_VERSION_MINOR != 5) return 1;

    // libssl: a usable client context.
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr) return 2;
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    SSL* ssl = SSL_new(ctx);
    if (ssl == nullptr) { SSL_CTX_free(ctx); return 3; }
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    // libcrypto: SHA-256 of "abc" — first four bytes of the known digest.
    unsigned char digest[EVP_MAX_MD_SIZE] = {};
    unsigned int len = 0;
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    if (md == nullptr) return 4;
    if (EVP_DigestInit_ex(md, EVP_sha256(), nullptr) != 1
        || EVP_DigestUpdate(md, "abc", 3) != 1
        || EVP_DigestFinal_ex(md, digest, &len) != 1) {
        EVP_MD_CTX_free(md);
        return 5;
    }
    EVP_MD_CTX_free(md);
    if (len != 32) return 6;

    const unsigned char expected[4] = {0xba, 0x78, 0x16, 0xbf};
    if (std::memcmp(digest, expected, sizeof expected) != 0) return 7;
    return 0;
}
#else
int main() { return 0; }  // compat.openssl is linux/macOS-only; no-op elsewhere
#endif
