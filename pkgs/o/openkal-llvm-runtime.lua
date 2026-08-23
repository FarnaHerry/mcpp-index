-- openkal-llvm-runtime --- LLVM's runtime libraries, configured for openkal.
--
-- libc++, libc++abi, libunwind and compiler-rt's builtins, built from source
-- against openkal-musl rather than against a host C library. This is the entry
-- point for a program that means to reach several machines from one source: it
-- names this package, and the C library, the platform implementation and the
-- specification follow from the graph beneath it.
--
-- No `deps'. The package names openkal-musl in its own manifest, and that
-- package in turn selects the implementation of openkal for the target being
-- built. A program adds one line and gets the whole stack.
--
-- The tarball carries the LLVM sources this build compiles, which is why it is
-- larger than every other descriptor here by two orders of magnitude. What it
-- does NOT carry is a prebuilt binary for any target: the runtime is compiled
-- for the target being built, by whichever compiler is running, which is the
-- property that makes one source reach four object formats.
package = {
    spec        = "1",
    namespace   = "mcpplibs",
    name        = "openkal-llvm-runtime",
    description = "LLVM's C++ runtime libraries -- libc++, libc++abi and libunwind -- configured for openkal-musl rather than for a host C library",
    licenses    = {"Apache-2.0 WITH LLVM-exception"},
    repo        = "https://github.com/mcpplibs/openkal-llvm-runtime",
    type        = "package",

    xpm = {
        linux = {
            ["0.1.0"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/openkal-llvm-runtime/archive/refs/tags/0.1.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/openkal-llvm-runtime/releases/download/0.1.0/openkal-llvm-runtime-0.1.0.tar.gz",
                },
                sha256 = "100865877d616b18e9c9bc64e7edd90fe147a544955fb47c19d68d02e35701cb",
            },
        },
        macosx = {
            ["0.1.0"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/openkal-llvm-runtime/archive/refs/tags/0.1.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/openkal-llvm-runtime/releases/download/0.1.0/openkal-llvm-runtime-0.1.0.tar.gz",
                },
                sha256 = "100865877d616b18e9c9bc64e7edd90fe147a544955fb47c19d68d02e35701cb",
            },
        },
        windows = {
            ["0.1.0"] = {
                url    = {
                    GLOBAL = "https://github.com/mcpplibs/openkal-llvm-runtime/archive/refs/tags/0.1.0.tar.gz",
                    CN     = "https://gitcode.com/mcpp-res/openkal-llvm-runtime/releases/download/0.1.0/openkal-llvm-runtime-0.1.0.tar.gz",
                },
                sha256 = "100865877d616b18e9c9bc64e7edd90fe147a544955fb47c19d68d02e35701cb",
            },
        },
    },
}
