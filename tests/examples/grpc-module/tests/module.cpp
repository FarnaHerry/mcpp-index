// Behavioral test for the mcpplibs.grpc package, driven entirely through
// `import grpc;` — there is no <grpcpp/...> include in this file. Every call
// reaches a real symbol in the linked gRPC, so a package that resolved and
// "built" but linked nothing could not pass.
//
// Note the ORDER: textual #includes first, `import grpc;` last. The module
// carries the standard library in its BMI, so a std header included after the
// import arrives twice and GCC fails on std::string's operator==.
// HAVE_GRPC comes from this project's own cfg-gated cxxflags — the package is
// linux/macOS-only (see mcpp.toml), so elsewhere this file is an empty main().
#ifdef HAVE_GRPC

#include <cstdio>
#include <string>

import grpc;

int main() {
    // A version string read out of the library, not a header constant.
    const std::string version = grpc::Version();
    std::printf("grpc::Version() = %s\n", version.c_str());
    if (version.rfind("1.83", 0) != 0) return 1;

    // grpc::Status, error path included — an always-OK stub cannot pass.
    const grpc::Status bad(grpc::StatusCode::UNAVAILABLE, "down");
    if (bad.ok() || bad.error_code() != grpc::StatusCode::UNAVAILABLE ||
        bad.error_message() != "down" || !grpc::Status::OK.ok()) {
        return 1;
    }

    // A real channel object. Channels connect lazily, so no server is needed;
    // IDLE before the first RPC proves this is a live object from the library.
    auto channel = grpc::CreateChannel("127.0.0.1:1", grpc::InsecureChannelCredentials());
    if (!channel) return 1;
    if (channel->GetState(false) != GRPC_CHANNEL_IDLE) return 1;

    std::puts("grpc module: OK");
    return 0;
}

#else   // !HAVE_GRPC

int main() { return 0; }

#endif
