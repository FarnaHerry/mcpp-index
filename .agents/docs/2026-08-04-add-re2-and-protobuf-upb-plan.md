# 新增 compat.re2 2022-04-01 与 compat.protobuf 的 upb feature(2026-08-04)

[gRPC 收录可行性分析](2026-08-04-grpc-feasibility-analysis.md) 的 **P1 索引侧**,承接
[P0](2026-08-04-add-abseil-and-protobuf-plan.md)。目标是把 gRPC 剩下的索引侧依赖补齐,使后续
`grpc-m` 独立仓只需 vendor gRPC 自己的源码。

产出:

| 变更 | 规模 | 说明 |
|---|---|---|
| `compat.re2@2022-04-01` | 22 TU | 新包,Shape A |
| `compat.protobuf` 新增 `upb` feature | 64 TU | 源码全在已收录的 protobuf tarball 内,无新下载 |

workspace 成员:`tests/examples/re2`、`tests/examples/protobuf-upb`。

## 0. 先做的事:确认 mcpp 真能编 gRPC

在写任何描述符之前先打掉最大的未知数 —— 用 mcpp 的 gcc@16.1.0 直接编 gRPC 1.83.0 的代表性 TU,
只给 include 路径,不打补丁、不跑 configure、不做 codegen:

```
OK  src/core/lib/slice/slice.cc
OK  src/core/lib/surface/call.cc                 <- core 里最重的之一
OK  src/core/lib/promise/activity.cc
OK  src/core/ext/upb-gen/google/protobuf/any.upb_minitable.c
OK  src/cpp/common/channel_arguments.cc
OK  src/cpp/client/channel_cc.cc                 <- C++ API 层
```

以及 SSL 层对着 **compat.openssl 3.5.1 的头文件**(不是 boringssl)4/4 通过:

```
OK  src/core/tsi/ssl_transport_security.cc
OK  src/core/credentials/transport/ssl/ssl_credentials.cc
OK  src/core/tsi/ssl/session_cache/ssl_session_openssl.cc
OK  src/core/tsi/ssl_telemetry_utils.cc
```

需要的 include 路径只有:仓库根、`include`、`src/core/ext/upb-gen`、`src/core/ext/upbdefs-gen`,
外加 abseil / protobuf(含 upb)/ openssl。另核实 gRPC 源码树内**没有任何 `.h.in` 或
`config.h.cmake`**,确认它不需要 configure 步骤。

结论:整源码直编路线成立,可以继续投入。

## 1. compat.re2 —— 为什么钉在 2022-04-01

`2022-04-01` 是 RE2 的上游 release tag,也是 **gRPC 1.83.0 的 pin**(其 `third_party/re2` submodule
= commit `0c5616d` = 该 tag)。这个 pin 是有意义的:gRPC 的 xds matcher
(`src/core/util/matchers.h`、`src/core/xds/grpc/xds_route_config*`)是照着这一版 API 写的。
RE2 在 2023 年的版本里把自己的字符串类型从 `re2::StringPiece` 换成了 `absl::string_view`
并引入了 Abseil 依赖,**新版不是 drop-in**。将来若有别的消费者需要新版,可按 `compat.catch2`
的先例在同一描述符里追加版本。

上游该 tag 没有 release asset,用 tag 归档(RE2 无 submodule,自包含),sha256 两次下载一致。

源码清单直接对应上游 `CMakeLists.txt` 的 `RE2_SOURCES`:`re2/*.cc` 全部(上游把测试放在
低一级的 `re2/testing/`,glob 够不到)+ `util/rune.cc` + `util/strutil.cc`。`util/` 逐条列而不通配,
因为该目录另外两个 TU **都带 `main()`**(`util/benchmark.cc`、`util/test.cc`,连同
`util/fuzz.cc` 与顶层 `testinstall.cc`),依赖的 `.o` 会全量入链,任何一个都会和消费者的
`main()` 撞车;`util/pcre.cc` 还会额外拖进 libpcre。

## 2. upb feature —— 组成必须严格照抄上游 libupb

upb 是 protobuf 自 v22 起吸收进来的小型 C 运行时,**源码就在已收录的 protobuf 35.1 tarball 里**
(根目录 `upb/`),所以不需要新包、不需要新下载。默认关闭:C++ 运行时一点都不用它,而它是 64 个额外
C TU。gRPC 是需要它的那个消费者 —— `src/core/ext/upb-gen/**.c` 就是 upb 代码。

两处**不能想当然**的地方:

1. **不能用 `upb/**/*.c` 通配。** 实测通配会多带 14 个文件,其中包含 **descriptor 表的两份替代构建**
   (`upb/reflection/stage0/…` 与 `upb/reflection/cmake/…`)以及 `upb/message/promote.c`、
   `decode_fast/` 的若干变体。同时编进多份 descriptor 变体 = 重复符号链接失败。因此逐条转录
   `src/file_lists.cmake` 的 `libupb_srcs`(63 条)。
2. **必须额外加上 `upb/reflection/cmake/google/protobuf/descriptor.upb_minitable.c`**,它**不在**
   `libupb_srcs` 里。上游 `cmake/libupb.cmake` 正是把这一个文件作为 `bootstrap_sources` 叠加在清单之上,
   因为整个 reflection 层经 `upb/reflection/descriptor_bootstrap.h` 都要用 descriptor 表。
   漏掉它能编译通过,**只在链接期炸**。

`include_dirs` 因此追加了两条(tarball 根、`upb/reflection/cmake`)。它们是**无条件**声明的:
`features` 能门控 sources/defines/deps,**不能门控 include dirs**,而 feature 自己的 TU 需要它们才能编译。
两条都是纯追加、不构成遮蔽 —— 根目录只提供 `upb/…`(protobuf 的 C++ 头在 `src/` 下而非根),
bootstrap 目录提供的是 `google/protobuf/descriptor.upb*.h`,与隔壁 C++ 的 `descriptor.h` 文件名不同。

## 3. 验证结论

全部使用与 CI 一致的配置(mcpp **2026.8.3.3**、gcc@16.1.0、`MCPP_INDEX_MIRROR=GLOBAL`、
`MCPP_BUILD_CACHE=local`),每个成员先删 `target/` 冷构建:

```
abseil          test result ok. 1 passed; 0 failed  (17.84s)
protobuf        test result ok. 1 passed; 0 failed  (40.88s)
protobuf-gzip   test result ok. 1 passed; 0 failed  (51.99s)
protobuf-upb    test result ok. 1 passed; 0 failed  (70.20s)
re2             test result ok. 1 passed; 0 failed  ( 2.35s)
```

前三个是**回归**:本次改了 `compat.protobuf` 的 `include_dirs`,必须确认既有成员未受影响。

- **目标文件数已核对**:`re2` 23 个(22 源 + 1 测试);`protobuf-upb` 296 个
  = 151(abseil)+ 80(protobuf)+ 64(upb)+ 1(测试)。
- **`upb` feature 做了负向验证**:把成员依赖改回裸 `protobuf = "35.1"` 后,目标文件数从 296 落到
  **232(正好少 64 个)**,链接给出 `undefined reference to upb_Arena_Init` / `upb_Decode` /
  `upb_DefPool_Free`,以及 **`google__protobuf__FileDescriptorProto_msg_init`** —— 最后这一个正好
  证明 §2 说的 bootstrap descriptor 表既是必需的、也确实由该 feature 带入。
  (注意关键词:这里工具链是 gcc,用 GNU ld,报的是 `undefined reference`;lld 才报 `undefined symbol`。)
- **测试打的是真符号**。re2 侧覆盖 FullMatch/PartialMatch(并断言 FullMatch **不**接受尾部残余)、
  预编译 RE2 对象取捕获、GlobalReplace、`RE2::Set` 多模式匹配、非法模式必须被拒、UTF-8 按 rune 计数;
  upb 侧让**两个运行时对同一段字节交叉验证** —— C++ 运行时构造并序列化 `FileDescriptorProto`,
  再由 upb 解析、灌进 `upb_DefPool`、回答反射查询,并断言一个从未声明过的名字**查不到**。
- **CN 镜像已闭环**:`mcpp-res/re2@2022-04-01` 返回 `http=200` 且与 GLOBAL **字节一致**。

## 4. 下一步(grpc-m)

索引侧到此备齐:`compat.abseil` / `compat.protobuf`(+`upb`)/ `compat.re2` / `compat.openssl`。
`grpc-m` 只需 vendor gRPC 自己的源码。已知的两个待办:

- **c-ares 暂不引入。** 上游有官方裁剪开关 `grpc_no_ares=true` → `GRPC_ARES=0`
  (`BUILD:75`、`bazel/grpc_build_system.bzl:175`),且只有 10 个文件 include `ares.h`,全在
  `resolver/dns/c_ares/` 与 `event_engine/*ares*`,可 glob 排除。第一版走 gRPC 原生 DNS 解析器,
  helloworld 连 `localhost` 用不到异步 DNS。将来以 `ares` feature + `compat.c-ares@1.34.5` 补上
  (grpc 已冻好 `third_party/cares/config_{linux,darwin,windows}/ares_config.h`,可作为配置快照)。
- **descriptor 表二选一。** gRPC 自带 `src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c`,
  与本 feature 带入的 bootstrap 版本**定义同一批符号**。grpc-m 必须排除其中一份,否则重复符号。
