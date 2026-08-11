# Adding the six remaining XRGUI dependencies to mcpp-index

Date: 2026-08-12

## Why these six

[PR #206](https://github.com/mcpplibs/mcpp-index/pull/206) added the three
*compiled* libraries XRGUI ([Sunrisepeak/xrgui#1](https://github.com/Sunrisepeak/xrgui/pull/1))
was missing — harfbuzz, msdfgen, mimalloc. What remained were six libraries the
project was reaching through **git submodules and `-I` flags** instead of through
a package manager.

That turned out to be worth fixing for a reason beyond XRGUI. Reading its
`xmake.lua` shows line 44:

```lua
add_requires("nanosvg", "spirv-reflect", "gtl", "glfw", "miniaudio")
```

The xmake build gets four of these from **xrepo packages**. The `external/gtl`,
`external/nanosvg`, `external/miniaudio` and `external/spirv_reflect` submodules
it also carries are not what that build compiles against — they are vestigial.
So packaging them is not an mcpp-specific accommodation; it is catching mcpp up
to what the project's own primary build already does.

All six stand on their own merits, independent of XRGUI:

| package | upstream | why it belongs in a general index |
|---|---|---|
| `compat.miniaudio` | mackron | the single-file audio library, ~4k stars |
| `compat.spirv-reflect` | **KhronosGroup** | official SPIR-V reflection |
| `compat.nanosvg` | memononen | the small SVG parser, ~2.5k stars |
| `compat.vulkan-memory-allocator` | **GPUOpen/AMD** | the standard Vulkan allocator |
| `compat.gtl` | greg7mdp | Swiss-table hash maps (parallel-hashmap's successor) |
| `compat.plf-hive` | mattreecebentley | reference implementation of proposed `std::hive` |

Not packaged, deliberately: `small_vector`, `allocator2d`, `mo_yanxi_utility`,
`mo_yanxi_vulkan_wrapper` and `mo_yanxi_react_flow` are the XRGUI author's own
code. They are the project, not its dependencies, and they belong in the project's
own tree.

## Shapes

Three distinct shapes, and the middle one is new to this index.

### A — header-only + anchor TU (`gtl`, `plf-hive`)

The established shape (compat.eigen, compat.CLI11): expose the headers, carry a
trivial TU so mcpp has a buildable `lib` target.

The only judgement is *which* directory to expose. `gtl` gets `*/include`, not
the tarball root, because `tests/` and `examples/` carry headers of their own and
`include/` is exactly what upstream's INTERFACE target publishes. `plf-hive` gets
`*` because the entire library is one file sitting at the root.

### B — single-header library + a GENERATED implementation TU (`nanosvg`, `vulkan-memory-allocator`)

Both are stb-style: the header holds the implementation behind a macro, and
upstream ships no `.c`/`.cpp` to instantiate it. Leaving that to the consumer
would make the package a header drop rather than something you link, and would
hand every consumer the same duplicate-symbol hazard. So the package generates
the implementation TU once.

The cost is a rule consumers must follow, and it is stated in both descriptors:
**do not define the implementation macro again** — the failure is a link error,
so it surfaces late.

`vulkan-memory-allocator` additionally forced a *policy* choice. VMA defaults to
`VMA_STATIC_VULKAN_FUNCTIONS 1`, which references `vkBindBufferMemory2`,
`vkGetPhysicalDeviceProperties2` and six more **by name**. Against a headers-only
dependency that is eight undefined symbols — observed, not predicted:

```
vk_mem_alloc.h:13600: undefined reference to `vkGetBufferMemoryRequirements2'
vk_mem_alloc.h:13601: undefined reference to `vkGetImageMemoryRequirements2'
… six more
```

Adding `compat.vulkan` would have made that link, and would have been the wrong
fix: it forces a Vulkan loader on every consumer of a *memory allocator*, and it
fights consumers that dispatch through volk or their own device table. The
generated TU selects `VMA_DYNAMIC_VULKAN_FUNCTIONS` instead, so VMA resolves
every entry point through `VmaVulkanFunctions` and the package links against the
Vulkan **headers** alone.

### C — one upstream TU is the whole library (`miniaudio`, `spirv-reflect`)

`miniaudio.c` is upstream's own two-line `MINIAUDIO_IMPLEMENTATION` driver and
its CMake library target. `spirv_reflect.c` is exactly upstream's
`spirv-reflect-static` target. In both cases `sources` is one line that cannot
drift from a release.

Two details worth recording:

- miniaudio's Linux link line is `-ldl -lpthread -lm` and deliberately **not**
  `-lasound` / `-lpulse`. miniaudio `dlopen`s its backends, so hard-linking them
  would break the package on a machine that has neither, for no gain.
- spirv-reflect exposes **both** `*` and `*/include`. `spirv_reflect.h:35-37`
  chooses between `<spirv/unified1/spirv.h>` and the bundled
  `"./include/spirv/unified1/spirv.h"` depending on
  `SPIRV_REFLECT_USE_SYSTEM_SPIRV_H`. Exposing both makes the two spellings
  resolve to the *same* bundled grammar header, so a consumer that defines that
  macro cannot silently get a different SPIR-V revision than this `.c` was
  written against.

## Versioning the two untagged libraries

`nanosvg` and `plf_hive` cut no tags and publish no releases. Following the
`compat.khrplatform` precedent (which mirrors the untagged EGL-Registry), each
pins a **commit archive** under a **date version** taken from that commit's date:
`nanosvg = 2026.07.09`, `plf-hive = 2026.07.31`. Date keys sort correctly against
any future snapshot.

`spirv-reflect` has the opposite problem — it tags in lockstep with the Vulkan
SDK (`vulkan-sdk-1.4.357.0`). The version key drops the prefix so it sorts
numerically and lines up with `compat.vulkan-headers` / `compat.vulkan` of the
same SDK. Keep the three moving together.

## Tests: what each one would catch

Every test asserts *behaviour*. A stub that linked but did nothing fails all six.

| test | the property it pins down |
|---|---|
| `plf-hive/hive.cpp` | element **addresses survive** erasing their neighbours and refilling — the guarantee that distinguishes a hive from a vector |
| `gtl/containers.cpp` | 1000 entries survive rehashing; `btree_set` iterates **in order** and `lower_bound` lands correctly |
| `nanosvg/parse.cpp` | parsed geometry matches the declared rect; the rasterized centre pixel is opaque red, the corner is untouched, and coverage is 2300–2700 px for a 50×50 rect. Links `nsvgParse` **and** `nsvgRasterize`, so a package that instantiated only one half fails here |
| `miniaudio/roundtrip.cpp` | encode → decode a 480 Hz sine and compare samples (`< 1e-5`), **plus** a peak check so silence cannot pass vacuously. Device-free by design: CI has no sound card |
| `spirv-reflect/reflect.cpp` | reflects a **real** glslc-compiled compute shader (embedded as words, no compiler needed at test time) and checks set 0/binding 3 storage, set 1/binding 0 uniform, and a 64-byte push block. Plus a negative case: bad magic must be **rejected**, or a always-succeed stub would pass everything above |
| `vulkan-memory-allocator/virtual_block.cpp` | the virtual allocator — 16 allocations, offsets honour alignment, **no two overlap**, stats match, freeing returns the space, and an over-sized request is refused. The one part of VMA assertable with no GPU |

## Verification performed locally

- All six `mcpp test` runs pass (`1 passed; 0 failed` each).
- `check_mirror_urls` / `check_package_name` / `check_platform_version_parity`
  pass on all six; `check_cross_package_refs` passes across all 96 descriptors.
- `mcpp xpkg parse` passes for **all 96** descriptors using the *pinned* CI
  version 2026.8.10.3, downloaded for the purpose — not the newer local build,
  because that check is what enforces "floor first, new grammar after".
- All six CN mirrors return HTTP 200 and are **byte-identical** to GLOBAL,
  which is what `mirror-cn-reachable` verifies.
