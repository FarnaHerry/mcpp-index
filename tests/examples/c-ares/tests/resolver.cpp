// Behavioral test for compat.c-ares.
//
// Entirely OFFLINE — no DNS query is ever sent, so the result does not depend
// on the runner's network. What it does exercise is the machinery gRPC's
// resolver actually leans on, and each piece lives in a different part of the
// library:
//
//   ares_library_init / _cleanup   -> src/lib/ares_library_init.c
//   ares_init_options / ares_destroy -> ares_init.c, ares_options.c, util/*
//   set/get servers as CSV         -> ares_options.c, str/*, record/* (parsing
//                                     an address list is real string+record work)
//   ares_inet_pton / ares_inet_ntop -> inet_net_pton.c, inet_ntop.c
//   ares_strerror                  -> ares_strerror.c
//
// Returns non-zero on any mismatch.
#include <cstring>
#include <string>

#include "ares.h"

namespace {

bool version_ok() {
    int num = 0;
    const char* s = ares_version(&num);
    // The descriptor pins 1.34.5; assert the header/library agree with it, so a
    // silently different vendored version cannot pass.
    return s != nullptr && std::string(s).find("1.34.5") != std::string::npos &&
           ((num >> 16) & 0xff) == 1 && ((num >> 8) & 0xff) == 34 && (num & 0xff) == 5;
}

bool channel_and_servers_ok() {
    struct ares_options opts;
    std::memset(&opts, 0, sizeof(opts));
    opts.flags = ARES_FLAG_NOSEARCH | ARES_FLAG_NOALIASES;

    ares_channel_t* ch = nullptr;
    if (ares_init_options(&ch, &opts, ARES_OPT_FLAGS) != ARES_SUCCESS) return false;

    bool ok = false;
    // Setting servers parses the CSV into c-ares' internal server list; reading
    // it back re-serializes it. A stub would not survive the round trip.
    if (ares_set_servers_csv(ch, "127.0.0.1:5353,[::1]:5354") == ARES_SUCCESS) {
        char* csv = ares_get_servers_csv(ch);
        if (csv != nullptr) {
            const std::string got(csv);
            ok = got.find("127.0.0.1") != std::string::npos &&
                 got.find("5353") != std::string::npos &&
                 got.find("::1") != std::string::npos;
            ares_free_string(csv);
        }
    }

    // Garbage must be REJECTED — otherwise the "parse" above proves nothing.
    if (ok && ares_set_servers_csv(ch, "not-an-address:::") == ARES_SUCCESS) ok = false;

    ares_destroy(ch);
    return ok;
}

bool inet_conversions_ok() {
    unsigned char buf[16];
    char out[64];

    if (ares_inet_pton(AF_INET, "192.0.2.7", buf) != 1) return false;
    if (ares_inet_ntop(AF_INET, buf, out, sizeof(out)) == nullptr) return false;
    if (std::string(out) != "192.0.2.7") return false;

    if (ares_inet_pton(AF_INET6, "2001:db8::1", buf) != 1) return false;
    if (ares_inet_ntop(AF_INET6, buf, out, sizeof(out)) == nullptr) return false;
    if (std::string(out) != "2001:db8::1") return false;

    // Malformed input must fail.
    return ares_inet_pton(AF_INET, "999.1.1.1", buf) != 1;
}

bool strerror_ok() {
    const char* ok = ares_strerror(ARES_SUCCESS);
    const char* nf = ares_strerror(ARES_ENOTFOUND);
    return ok != nullptr && nf != nullptr && std::strlen(nf) > 0 &&
           std::string(ok) != std::string(nf);
}

}  // namespace

int main() {
    if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) return 1;

    const bool ok = version_ok() && channel_and_servers_ok() &&
                    inet_conversions_ok() && strerror_ok();

    ares_library_cleanup();
    return ok ? 0 : 1;
}
