// Behavioral test for compat.re2.
//
// Exercises the parts of RE2 that gRPC's xds matchers actually use — full and
// partial match with captures, a pre-compiled RE2 object, replacement, and the
// RE2::Set multi-pattern API — so a package that compiled but linked nothing
// useful cannot pass:
//
//   RE2 ctor / FullMatch / PartialMatch  -> re2/re2.cc, parse.cc, compile.cc, prog.cc
//   captures + rewriting                 -> re2/re2.cc, regexp.cc, simplify.cc
//   RE2::Set                             -> re2/set.cc, dfa.cc, nfa.cc
//   invalid pattern reporting            -> re2/parse.cc, tostring.cc
//   UTF-8 handling                       -> util/rune.cc, unicode_groups.cc
//
// Returns non-zero on any mismatch.
#include <string>
#include <vector>

#include "re2/re2.h"
#include "re2/set.h"

namespace {

bool matching_ok() {
    if (!RE2::FullMatch("grpc-1.83.0", R"(grpc-\d+\.\d+\.\d+)")) return false;
    // A full match must NOT accept trailing junk — proves this is FullMatch
    // semantics and not an accidental substring search.
    if (RE2::FullMatch("grpc-1.83.0-rc1", R"(grpc-\d+\.\d+\.\d+)")) return false;
    return RE2::PartialMatch("grpc-1.83.0-rc1", R"(grpc-\d+\.\d+\.\d+)");
}

bool captures_ok() {
    const RE2 re(R"((\w+)://([^/:]+):(\d+))");   // pre-compiled, the way a matcher holds it
    if (!re.ok()) return false;

    std::string scheme, host;
    int port = 0;
    if (!RE2::FullMatch("dns://localhost:50051", re, &scheme, &host, &port)) return false;
    return scheme == "dns" && host == "localhost" && port == 50051;
}

bool rewrite_ok() {
    std::string s = "service/Echo, service/Ping";
    const int n = RE2::GlobalReplace(&s, R"(service/(\w+))", R"(grpc.\1)");
    return n == 2 && s == "grpc.Echo, grpc.Ping";
}

bool set_ok() {
    RE2::Set set(RE2::DefaultOptions, RE2::UNANCHORED);
    const int a = set.Add(R"(^/helloworld\.Greeter/)", nullptr);
    const int b = set.Add(R"(SayHello$)", nullptr);
    if (a < 0 || b < 0 || !set.Compile()) return false;

    std::vector<int> hits;
    if (!set.Match("/helloworld.Greeter/SayHello", &hits)) return false;
    if (hits.size() != 2) return false;

    std::vector<int> none;
    return !set.Match("/other.Service/Method", &none);
}

bool invalid_pattern_reported() {
    // An unbalanced group must be REJECTED with an error string, not silently
    // accepted — otherwise a broken matcher config would look valid.
    // log_errors(false) keeps RE2 from also printing the (expected) parse
    // failure to stderr, so real problems stay the only noise in the log.
    RE2::Options quiet;
    quiet.set_log_errors(false);
    const RE2 bad("(unclosed", quiet);
    return !bad.ok() && !bad.error().empty();
}

bool utf8_ok() {
    // '.' counts runes, not bytes, in UTF-8 mode (the default).
    return RE2::FullMatch("héllo", "h.llo") && RE2::PartialMatch("世界", "界");
}

}  // namespace

int main() {
    const bool ok = matching_ok() && captures_ok() && rewrite_ok() && set_ok()
                    && invalid_pattern_reported() && utf8_ok();
    return ok ? 0 : 1;
}
