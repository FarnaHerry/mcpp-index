# 新增 compat.harfbuzz / compat.msdfgen / compat.mimalloc

> 日期：2026-08-12 · 验证工具链：mcpp `2026.8.10.3`（与 `validate.yml` 的 `MCPP_VERSION` 对齐）+ gcc `16.1.0`

## 1. 动机

这三个库是把 [XRGUI](https://github.com/Sunrisepeak/xrgui)（C++23 模块化保留式 GUI 库）
适配到 mcpp 时，索引里**唯一还缺的三个编译型依赖**。适配记录见
[Sunrisepeak/xrgui#1](https://github.com/Sunrisepeak/xrgui/pull/1)：该项目的源码符合性问题
已全部清零，根包构建停在且仅停在这三行：

```
fatal error: mimalloc.h: No such file or directory
fatal error: hb.h: No such file or directory
fatal error: msdfgen/msdfgen-ext.h: No such file or directory
```

三者本身也都是通用库（分配器 / 文本整形 / SDF 生成），不是为单个下游定制的。

## 2. 形态判定

| 包 | 形态 | 判定依据 |
|---|---|---|
| `compat.mimalloc` | **A**（C 源码 compat） | 纯 C，18 个 TU，用户写 `#include <mimalloc.h>` |
| `compat.harfbuzz` | **A**（C++ 版） | 上游自带 amalgamation，单 TU |
| `compat.msdfgen` | **A + E** | C++ 源码，且**必须**生成 `msdfgen-config.h` |

### 2.1 mimalloc —— 为什么源列表是枚举而非通配

`src/*.c` 会在三个地方出错，且**每一处都是链接期而非编译期报错**：

- `src/static.c` 是 amalgamation，`#include` 了整个库，与其它文件同时编入会让每个符号重复；
- `src/free.c` 被 `alloc.c` 第 22 行 `#include`，不是独立 TU；
- `src/alloc-override.c` 同理（`alloc.c:21`）。

所以源列表取上游自己的 `mi_sources`（`CMakeLists.txt:75-93`）—— 那是「哪些文件是 TU」
的唯一权威答案。`src/prim/prim.c` 是分发器，按宿主 `#include` 对应平台实现，一条就覆盖三平台。

`MI_MALLOC_OVERRIDE` 保持关闭（`alloc-override.c:13` 是它的门）。mimalloc 能接管全局
malloc/free，但让一个依赖悄悄替换进程分配器不该由包决定。`mi_*` API 不受影响。

### 2.2 harfbuzz —— 用上游的 amalgamation

上游是 meson 构建，在此复刻意味着跟踪 ~137 个 `.cc` 加一份生成的 config。
`src/harfbuzz.cc` 正是上游为这种场景提供的「只编一个文件」路径，是**上游自己支持的**，
所以 `sources` 只有一行，且不会随 release 演进而脱节。

该 amalgamation 同时 `#include` 了本包并未启用的后端（CoreText / DirectWrite / GDI /
GLib / Graphite2 / ICU），每个都在自己的 `HAVE_*` 门后，缺门时编译为空。因此**只声明
`HAVE_FREETYPE`** 即可精确选中 FreeType 桥接，无需改动任何上游文件。

不需要 `config.h`：HarfBuzz 只在 `HAVE_CONFIG_H` 下读它，这里不定义，`hb-config.hh`
提供默认值。这正是它能保持纯源码包、无 configure 步骤的原因。

**`HB_NO_MT` 刻意不设。** 它会去掉 HarfBuzz 的原子操作与锁，仅当消费者保证单线程使用时
才成立 —— 而共享索引包无法替它的消费者作此承诺。

### 2.3 msdfgen —— config 不是可选项

`core/base.h` 第 7 行就是 `#include <msdfgen/msdfgen-config.h>`，而该文件由 CMake 从
`cmake/msdfgen-config.h.in` 生成。**不生成它连 `core/` 都编不了**，这也是本包必须叠加
形态 E 的原因。

选择生成 config 而非传 `-D` 旗标，还有一层更重要的理由：**它让库与消费者天然一致**。
`base.h` 被每个公开头间接包含，所以这份文件是「SVG / PNG / Skia 哪些存在」的唯一出处。
包自己编译时的 `cxxflags` 是私有的，消费者 `#include <msdfgen/msdfgen-ext.h>` 时看不到，
两边就会对「有哪些声明」产生分歧。

`ext/` 四个单元只编 `import-font.cpp`，其余三个各需一个本索引没有的库：

| 单元 | 需要 |
|---|---|
| `ext/resolve-shape-geometry.cpp` | Skia |
| `ext/import-svg.cpp` | TinyXML2 |
| `ext/save-png.cpp` | libpng 或 LodePNG |

它们的**头文件**仍可经 `msdfgen-ext.h` 到达，但上游给每个都加了宏门，生成的 config
里写上 `MSDFGEN_DISABLE_SVG` / `MSDFGEN_DISABLE_PNG` 后，那些声明根本不存在。
Skia 不需要宏：它的块是 `#ifdef MSDFGEN_USE_SKIA`，默认即关。

**`MSDFGEN_USE_CPP11` 刻意不开。** 它不是构建期开关：它给 `Bitmap` 增加移动构造
（`core/Bitmap.h:19`），即**改变了一个跨库边界类型的布局与 ABI**。包无法保证每个消费者都
同样定义它 —— 而直接 `#include <msdfgen.h>` 的消费者根本看不到任何 shim 里的定义 ——
两边就会静默分歧。关掉它的代价只是少了几次移动，换来单一 ABI。

### 2.4 msdfgen 的 `msdfgen/` 前缀

上游把 `msdfgen.h` / `msdfgen-ext.h` 放在**仓库根**，但所有打包方式（vcpkg、xmake-repo、
发行版）都装到 `include/msdfgen/` 下，消费者源码写的也是这个。原地构建拿到的是根布局，
所以 `mcpp_generated/msdfgen/*.h` 把约定拼写补回来。裸 `<msdfgen.h>` 同样可用 ——
wrap 目录本来就在 include 路径上。

方向与上游 install 相反：上游写一个全局 `msdfgen.h` 转发到 `msdfgen/msdfgen.h`
（`CMakeLists.txt:285-289`），这里因为在源码树内构建，根部那个才是真的。

## 3. CN 镜像

三个都在 gitcode `mcpp-res` 下建仓并发 release，上传与 GLOBAL **字节一致**的 tarball。

| slug | 版本 | sha256 |
|---|---|---|
| `mimalloc` | 3.4.5 | `19a43af0645c57d348e729d5b31e23e912582911bb1047f795790834d3416221` |
| `harfbuzz` | 14.3.0 | `566e996a1b40486954fb7110ffe6eb88a0f7958bb466cdb023b0302618acea4a` |
| `msdfgen` | 1.13 | `93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167` |

sha256 按 SOP 计算两遍确认稳定。闭环校验（CN 返回 200 且与 GLOBAL 字节一致）三个全过。

## 4. feature 评估

**三个包都没有实现 feature**，各有其因：

- **mimalloc**：可选项（override、secure、valgrind、统计档位）全是**编译期 define**，
  而 0.0.68 的 features 表仅能门控 `sources`。按 SKILL 的判定准则，这类不可门控。
- **harfbuzz**：其余后端（CoreText / DirectWrite / Graphite2 / ICU / GLib）确实是可编译源码，
  但它们已在 amalgamation 内部，无法按源文件切分；开关同样是 define（`HAVE_*`）。
- **msdfgen**：`ext/` 三个单元**形式上**符合 feature 的定义（额外可编译源码），但每个都需要
  一个本索引尚无的库（Skia / TinyXML2 / libpng-或-LodePNG），且开关还要同时写进生成的 config
  才能让消费者看见。等这些依赖进索引后可再评估。

## 5. 验证

测试成员：`tests/examples/{mimalloc,harfbuzz,msdfgen}`，均已登记进根 `mcpp.toml`
的 `[workspace].members`。

每个测试都有**可失败的有效断言**，而不是只验证链接成功：

- **mimalloc** —— `mi_usable_size` 必须 ≥ 请求大小（证明指针来自 mimalloc 自己的簿记
  而非某个 fallback）、`mi_zalloc` 必须真的清零、`mi_malloc_aligned` 必须对齐、
  私有 heap 必须 `mi_heap_contains` 自己发出的块。
- **harfbuzz** —— 真的整形一段文本：`glyph_count` 必须等于输入长度（若
  `hb-ot-shape.cc` 没被编进 amalgamation，这里会是 0），`cluster` 必须跟踪输入偏移；
  再取 `hb_ft_face_create_referenced` 的地址，证明 FreeType 后端**编进来了**而不只是
  被声明（缺 `-DHAVE_FREETYPE` 时这是链接期错误）。
- **msdfgen** —— 手工构造一个正方形，生成 MSDF，断言场是**有符号的**（内部 > 0.5、
  外部 < 0.5，全零位图两条都过不了），且**三通道确有差异**（否则说明 edge coloring
  没跑，"多通道"这件事没发生）；最后经 `msdfgen::initializeFreetype` 证明
  `ext/import-font` 在位、传递依赖 `compat.freetype` 到达了链接行。
  测试用 `msdfgen/` 前缀 include，让生成的 shim 也被 CI 覆盖。
  缠绕方向交给上游的 `shape.orientContours()` 归一，断言因而不依赖轮廓的书写顺序。

冷验证（先删 `target/` 与 `.mcpp/`）结果：

```
mimalloc    test result ok. 1 passed; 0 failed; finished in 4.32s
harfbuzz    test result ok. 1 passed; 0 failed; finished in 31.35s
msdfgen     test result ok. 1 passed; 0 failed; finished in 14.63s
```

## 6. 过程中踩到的两个坑

1. **`generated_files` 的多行内容必须用长括号串**（`[==[ … ]==]`），不能用 `..` 拼接。
   描述符读取器是轻量 Lua **解析器**而非求值器，会把 `..` 表达式的续行当成新的 key，
   报 `malformed mcpp segment near key 'ifndef'`。既有包（如 `compat.libpng` 的
   `pnglibconf.h`）用的就是长括号串。
2. **msdfgen 的符号约定不能靠猜**。初版测试按书写顺序假定内外，结果 `inside=-1.375`、
   `outside=2.375` —— 方向恰好相反。正解是调用上游自己的 `shape.orientContours()`
   做归一（它的独立工具对导入几何做的就是这件事），而不是把断言反过来写。

## 7. 注意事项

- harfbuzz 的 tarball 是 36 MB，是本索引里较大的一个；CI 拉取耗时可见（构建本身很快，
  单 TU）。
- msdfgen 版本号在 `xpm` 里是裸 `1.13`（上游 tag 是 `v1.13`，`vcpkg.json` 里写 `1.13.0`）。
  生成的 config 里 `MSDFGEN_VERSION` 取 `1.13.0`，与上游 `version.cmake` 的读取结果一致。
- mimalloc 的 `MSDFGEN`/`HB` 之外还声明了平台链接库（linux 的 `-lpthread -lrt -latomic`，
  windows 的 `psapi/shell32/user32/advapi32/bcrypt`），取自 `CMakeLists.txt:614-626`。
