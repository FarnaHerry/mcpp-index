# Add Galay 5.0.2 (`gzj-creator.galay`)

Date: 2026-08-30
Upstream: <https://github.com/gzj-creator/galay>
Tag: `v5.0.2` (`d58976711790e47d5b0ad272e068d516192a1a1e`)
Status: upstream has published the Clang 22 module fix as v5.0.2. The index
follows that immutable archive; local GCC/LLVM module and consumer checks,
archive reproducibility, and the mcpp 2026.8.27.2 package tests pass. The
post-sync PR validation is green in validate run 33291129061 and site-check
run 33291129055.

## 1. Shape and identity

Galay is source type (b), a library already developed for mcpp. Its v5.0.2
release carries a complete `mcpp.toml`, so the index entry is Form A and does
not duplicate its build recipe.

- Package identity: `namespace = "gzj-creator"`, `name = "galay"`. The
  namespace names the upstream owner; the package name remains one atomic
  segment.
- The upstream manifest builds the default `galay.utils` and `galay.kernel`
  named modules plus their C++ implementations. SSL, HTTP, WebSocket, HTTP/2,
  database, RPC, MCP, and tracing sources remain opt-in features in that
  manifest, with the corresponding feature dependencies preserved.
- The release manifest declares `platforms = ["linux", "macos"]`. The index
  therefore publishes `linux` and `macosx` entries only; Windows is omitted
  until the upstream manifest gains a Windows-compatible build.
- License and description are taken from the upstream manifest: Apache-2.0,
  C++23 coroutine networking and protocol framework.

## 2. Source and hash

Both platform entries use the immutable GitHub tag archive:

    https://github.com/gzj-creator/galay/archive/refs/tags/v5.0.2.tar.gz

The archive is 5,231,630 bytes. `sha256sum` was run twice on the complete
archive and returned:

    93a93fabcfeb1b0ae160f3082ed472571532ce208c112bf2697f94267b27332a

`tar -tzf` succeeds and confirms the root `mcpp.toml`, the tracked include
layout, and the fifteen named C++23 module interfaces are present.

The v5.0.1 upstream patch remains intact: the generated module preludes only
include `intrin.h` for `_MSC_VER` and `emmintrin.h` on x86. This addresses the
earlier v5.0.0 Linux LLVM/macOS intrinsic-header failure without changing the
new release's guards.

The v5.0.2 fix addresses the separate Linux LLVM failure found in CI run
33269750913, job 99146034674, with mcpp 2026.8.27.2 and LLVM 22.1.8. In
v5.0.1, `async_aio.h` closed `namespace galay::async` and then defined the
`AioCommitAwaitable::await_suspend` function template with a globally
qualified-id. Because `galay_kernel.cppm` includes that header inside
`export extern "C++"`, Clang 22 rejected the definition as not being at
namespace scope and produced cascading `this`, `handle`, `m_waker`,
`m_controller`, and `m_result` errors. v5.0.2 puts the definition back inside
the `namespace galay::async` block. The declaration, template visibility,
ABI, and Linux `USE_EPOLL` implementation remain unchanged.

## 3. CN mirror

`gtc` is not installed in this environment and no GitCode write credential is
available. Following `docs/cn-mirror.md`, the descriptor uses the plain GLOBAL
URL rather than inventing a mirror table. CN consumers fall back to GitHub;
the `mcpp-res/galay` mirror can be added later without changing the package
identity or version.

## 4. Workspace member

`tests/examples/galay` is a Unix-gated public-package consumer. Its member
manifest has exactly one project index redirect:

    [indices]
    gzj-creator = { path = "../../.." }

The test imports `galay.utils` and `galay.kernel`, checks Base64 and string
helpers, links the out-of-line `kernel::Buffer` implementation, and validates
IPv4 `kernel::Host` construction. Windows compiles a no-op `main()` because
the upstream package has no Windows platform entry.

The first RED run failed as expected before the descriptor existed:

    error: dependency 'gzj-creator.galay': no package found for exact selector

After adding the descriptor, the first executable run caught an incorrect
assumption about `Buffer::clear()` retaining its length; the assertion was
changed to require the documented empty state. The corrected test then passed.

## 5. Validation

- `mcpp xpkg parse pkgs/g/gzj-creator.galay.lua` passed with the Form-A result
  and Linux/macOS version lists.
- `mcpp test -p galay` with the v5.0.2 descriptor passed on the local default
  GCC toolchain: `test result ok. 1 passed; 0 failed`.
- The CI-pinned mcpp 2026.8.27.2 Linux default and LLVM 22.1.8 tests pass:
  `test result ok. 1 passed; 0 failed` for each toolchain.
- CI run 33269372157 passed on Linux default, macOS default, Windows default,
  lint, mirror reachability, graphics side-effect, and build checks.
- The upstream Clang 22 regression test (`kernel.alignsrc`) and the complete
  CMake/Ninja Linux module surface pass with both GCC and LLVM 22.1.8,
  including `USE_EPOLL` and `galay.kernel`.
- A module consumer importing both `galay.utils` and `galay.kernel` compiles,
  instantiates `co_await AsyncAio::commit()`, and runs successfully under LLVM
  22.1.8; this confirms the template definition remains visible and member
  accesses bind to the awaitable instance.
- The previous CI failure was reproduced from run 33269750913/job 99146034674
  before the upstream patch and is resolved by v5.0.2; the v5.0.1 intrinsic
  guards remain covered by the prelude regression test.
- PR #285 post-sync validate run 33291129061 passed Linux default GCC, Linux
  LLVM 22.1.8, macOS default, Windows default, lint, mirror-cn-reachable,
  graphics side-effect, member selection, and timing jobs. Site-check run
  33291129055 also passed.
- The build compiled 26 Galay units, including both default module interfaces,
  the kernel implementation units, and the transitive libaio package.
- All six descriptor lint checks passed, and all 134 package descriptors passed
  `mcpp xpkg parse` with the CI-pinned binary.
- Optional Galay features are intentionally not enabled by this minimal
  member. They remain upstream-owned feature/dependency decisions and need
  dedicated protocol/database environments before being advertised as tested.

## 6. Follow-up

When upstream publishes Windows support or a maintainer creates the
`mcpp-res/galay` release asset, add the platform/mirror entry with the same
archive bytes. The v5.0.2 release remains Linux/macOS in its own manifest, so
the Windows example continues to compile a no-op test until a Windows package
entry exists.
