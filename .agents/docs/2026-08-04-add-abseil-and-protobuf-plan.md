# 新增 compat.abseil 20250512.1 与 compat.protobuf 35.1(2026-08-04)

本次变更为 [gRPC 收录可行性分析](2026-08-04-grpc-feasibility-analysis.md) 的 **P0**:先把 gRPC 最大的两个前置
依赖以普通 compat 包的形态收进索引。gRPC 本身仍走独立适配仓路线,不在本次范围内。

产出:

| 包 | 版本 | TU 数 | 形态 |
|---|---|---|---|
| `compat.abseil` | `20250512.1` | 151 | Shape A(C++ 源码 compat) |
| `compat.protobuf` | `35.1` | 80(79 C++ + 1 C) | Shape A,依赖 `compat.abseil`,含 `gzip` feature |

workspace 成员:`tests/examples/abseil`、`tests/examples/protobuf`、`tests/examples/protobuf-gzip`。

## 1. 形态判定:为什么不需要独立仓

最初的 gRPC 分析曾建议 protobuf 也做成 `protobuf-m` 独立仓。核实上游发布物后推翻了这一判断——三条性质
让它们正好落进索引标准模板:

1. **有正式的自包含 release asset**。不是 GitHub tag 归档,而是真实发布产物:
   `abseil-cpp-20250512.1.tar.gz`(2.2MB)、`protobuf-35.1.tar.gz`(7.1MB)。均无 submodule;
   protobuf 的 `third_party/` 只含 utf8_range,且是实体源码。sha256 两次下载一致。
2. **无需 configure**。两者的平台差异全在源码内 `#ifdef`(`absl/base/config.h` / `port_def.inc`),
   所以三平台共用同一个 tarball、同一份 sha256、同一份源码清单。
3. **无需 protoc 自举**。protobuf 把 14 个 `.pb.cc`(well-known types + `descriptor.pb.cc`)**check-in**
   在源码树里,构建运行时完全不需要先有一个 protoc。

这与 gRPC 形成鲜明对比:gRPC 的 tag 归档里 abseil/protobuf/re2/boringssl/zlib 全是空 submodule 占位,
必须自行打包并双镜像自托管——那才是需要独立仓的理由。

## 2. 版本对齐:一份 abseil,不是两份

版本号与上游逐字对应,这不是风格问题而是正确性要求:

- `compat.abseil` 的 `20250512.1` **就是** Abseil 的 LTS tag(`ABSL_LTS_RELEASE_VERSION 20250512` /
  `ABSL_LTS_RELEASE_PATCH_LEVEL 1`)。
- protobuf 35.1 的 `MODULE.bazel` 声明 `abseil-cpp 20250512.1`;gRPC 1.83.0 的 abseil submodule 固定在
  commit `76bb243`,即 "Abseil LTS Branch, May 2025, Patch 1" —— **同一个发布**。
- gRPC 1.83.0 的 protobuf submodule 固定在 commit `35cd01f`,与上游 tag `v35.1` **完全相等**(已核对)。

因此后续 gRPC 包可以直接依赖这两个包,而不是各自 vendor 一份 abseil 在链接期撞车。

## 3. 源码清单的取法

两个包取清单的方式不同,各自取决于上游给了什么:

- **protobuf —— 逐条转录上游权威清单**。`src/file_lists.cmake` 里的 `libprotobuf_srcs`(79 条)是上游从
  Bazel 规则自动生成的。**不能用目录 glob 代替**:`src/google/protobuf/**/*.cc` 会同时扫进 libprotoc
  (另外 157 个 TU)和单元测试,而运行时自己有两个 TU 恰恰住在 `compiler/` 下
  (`importer.cc`、`parser.cc`——`.proto` 文本解析器属于运行时),没有任何目录层面的 glob 能干净切开两者。
  `libprotobuf_lite_srcs` 是该清单的真子集,故 lite 无需单独处理。
- **abseil —— 通配 + 按上游命名约定裁剪**。abseil 没有等价的清单文件,但它的测试/基准 TU 命名规律极强,
  5 条否定 glob(`*_test.cc` / `*_test_common.cc` / `*_testing.cc` / `*_benchmark.cc` / `*_benchmarks.cc`)
  就吃掉了绝大部分,再逐个排除 14 个漏网的。分四类,每一类都有理由,不是凑数:

  1. 仍会 include gtest/gmock/benchmark 的(本包无测试框架依赖,根本编不过);
  2. 能编过但不属于任何上游 library target 的测试支撑 TU;
  3. **自带 `main()` 的**——这一类是硬约束:依赖的 `.o` 会全量进入消费者的链接(不是从 archive 里惰性挑选),
     任何一个都会和消费者自己的 `main()` 撞车,让每一个依赖此包的构建都失败。
     实测确有 4 个:`print_hash_of.cc`、`gaussian_distribution_gentables.cc`、
     `randen_benchmarks.cc`、`raw_hash_set_probe_benchmark.cc`(后两个已被 glob 覆盖)。

  第一版描述符漏掉了通配的 `*_benchmark.cc`,直接表现为一大片
  `undefined symbol: benchmark::internal::Benchmark::…` 链接错误——这也是"漏了就一定会炸"的证据。

## 4. 几个刻意的取舍

- **`randen_hwaes.cc` 不加 `-maes -msse4.1`**(上游把它放在独立 CMake target 上)。这是正确而非退化:
  没有加速 AES 时该文件走 `!ABSL_RANDEN_HWAES_IMPL` 分支,`HasRandenHwAesImplementation()` 返回 false,
  而 `randen.cc` 的判断是 `HasRandenHwAesImplementation() && CPUSupportsRandenHwAes()` —— 短路,
  桩函数不可达,`absl::BitGen` 走 RandenSlow。若改为全包加这两个 flag,等于抬高每个 TU 的基线 ISA,
  对一个要求可移植的索引包是更差的交易。
- **utf8_range 只取 `utf8_range.c` 一个文件**。上游 `third_party/utf8_range/CMakeLists.txt` 里
  `utf8_range` 和 `utf8_validity` 两个 target 都只由这一个文件构成;同目录其余 `.c`
  (naive/lookup/lemire-\*/range-\*)是基准实现,而 `main.c` 是基准驱动,其 `main()` 会与消费者撞车。
- **`gzip` 做成 feature 而非默认**。`io/gzip_stream.cc` 整个被 `#if HAVE_ZLIB` 包住,默认编成空 TU,
  于是默认构建**完全不带 zlib 依赖**;开启 feature 才定义宏并拉入 `compat.zlib`。
  `defines` 只作用于本包 TU,而这个开关恰恰只被 gzip_stream.cc 读取,作用域正好。

## 5. 验证结论

全部使用与 CI 完全一致的配置:mcpp **2026.8.3.3**(`validate.yml` 的 `MCPP_VERSION`)、
工具链 gcc@16.1.0、`MCPP_INDEX_MIRROR=GLOBAL`、`MCPP_BUILD_CACHE=local`,三个成员均先清空
`target/` 与 `.mcpp/` 冷构建:

```
::: abseil :::         Compiling compat.abseil v20250512.1   test result ok. 1 passed; 0 failed  (18.57s)
::: protobuf :::       Compiling compat.protobuf v35.1       test result ok. 1 passed; 0 failed  (63.35s)
::: protobuf-gzip :::  Compiling compat.protobuf v35.1       test result ok. 1 passed; 0 failed  (54.48s)
```

- **实际编译数已核对**,不是"绿了就算":`compat.abseil` 151 个 `.o` 入链,`compat.protobuf` 80 个。
- **测试用例只打真符号**。abseil 侧覆盖 StrCat/StrFormat/StrSplit、Cord(撑到 btree 表示)、Status/StatusOr、
  FormatTime/ParseTime(走 vendored cctz)、Mutex+Notification(双线程计数)、flat_hash_map;
  protobuf 侧**全程不用任何 protoc 产物**,走 `.proto` 文本 → 描述符 → DynamicMessage → wire/TextFormat/JSON
  往返 + well-known types + UTF-8 校验(打到 utf8_range.c)。截断报文必须被拒,以证明解析确实在做事。
- **`gzip` feature 做了负向验证**:把成员依赖改回裸 `protobuf = "35.1"` 后,链接给出
  `undefined symbol: google::protobuf::io::GzipOutputStream::…` 等 10+ 条,证明门控真实生效而非默认即编入。
- **CN 镜像已闭环**:`mcpp-res/abseil@20250512.1`、`mcpp-res/protobuf@35.1` 均 `http=200` 且与 GLOBAL
  **字节一致**(sha256 逐一比对)。

## 6. 两个已知问题(均为 mcpp 侧,非本描述符缺陷)

### 6.1 `randen_round_keys.cc` 的假 module import 警告

每次构建 `compat.abseil` 都会打印:

```
randen_round_keys.cc: module 'binascii' imported but not provided in this build
```

该文件把生成常量用的 Python 脚本贴在 `/* … */` 块注释里,其中 `import binascii` 位于第 0 列。mcpp 的 M1
文本扫描会剥掉 `//` 行注释和裸字符串体(`src/modgraph/scanner.cppm` 的 `strip_raw_strings`),但**不剥块注释**,
于是这一行被当成 C++23 module import。

**无法从描述符侧消除**:`scan_overrides` 正是为此设计的,但 mcpp 拒绝"既无 provides 也无 imports"的条目
(`src/manifest/toml.cppm`),而"无 provides、无 imports"恰恰是这个文件的真实答案;编造一条假边则会
在与编译器 P1689 输出对账时炸掉。警告是纯装饰性的,TU 编译与链接均正常。修复应在 mcpp 侧(扫描时剥块注释,
或允许空的 scan_overrides 条目)。

### 6.2 本组包新增一对 mcpp#344 的撞名样本

`compat.abseil` 的 `absl/strings/internal/str_format/parser.cc` 与 `compat.protobuf` 的
`google/protobuf/compiler/parser.cc` **basename 相同**(经比对,这是两包间唯一的一处)。

mcpp 的目标文件消歧依赖**整个构建图**(只有撞名时才用嵌套路径),而包级构建缓存键**不含**这个上下文,
于是同一个缓存条目可能持有两种不兼容的布局。本地用默认全局缓存可稳定复现:

```
1) 单跑 abseil        -> 以扁平 obj/parser.o 建缓存,通过
2) 跑 protobuf        -> 拉入 abseil,此时撞名,改要嵌套路径;缓存键未变而命中旧条目
                         ninja: error: '.../obj/compat_abseil/.../parser.o' missing and no known rule to make it
3) 再单跑 abseil      -> 又通过
```

这正是已知的 [mcpp#344](https://github.com/mcpp-community/mcpp/issues/344)(仓库此前由 `tests/examples/archive`
的 zlib + bzip2 双 `compress.c` 撞出),**CI 不受影响**:`validate.yml` 的测试步骤已设
`MCPP_BUILD_CACHE: local` 绕开包级缓存。上游已在 **2026.8.3.4** 修复
(`702c1c8 fix(cache): a package's object layout must not depend on the consumer`),已用本地 2026.8.3.5 验证
其目标文件布局改为无条件嵌套(197/197)。**结论:本次不动 `MCPP_VERSION`**;等 CI 版本自然推进到 ≥2026.8.3.4
时,可连同 `MCPP_BUILD_CACHE: local` 这条绕行一起移除。

## 7. 下一步

P1 起进入 gRPC 本体,走独立适配仓(`grpc-m`)路线;本次两个包将作为它的依赖被直接复用,
不再重复 vendor。
