# 新增 compat.c-ares 1.34.5(2026-08-04)

[gRPC 收录可行性分析](2026-08-04-grpc-feasibility-analysis.md) P1 索引侧的最后一块,承接
[abseil+protobuf](2026-08-04-add-abseil-and-protobuf-plan.md) 与
[re2+upb](2026-08-04-add-re2-and-protobuf-upb-plan.md)。至此 `grpc-m` 的全部索引依赖备齐。

产出:`compat.c-ares@1.34.5`,91 TU,workspace 成员 `tests/examples/c-ares`。

## 1. 为什么默认要有它

一度打算第一版跳过 c-ares(上游确有官方裁剪开关 `grpc_no_ares=true` → `GRPC_ARES=0`)。**这个决定被推翻了**:
gRPC 上游默认就是启用 c-ares,行业惯例也如此,索引里的包不应该悄悄比上游少一块。正确形态是
**默认开启 + 可关闭** —— `grpc-m` 用 Cargo 式 `[features] default = ["ares"]`,消费者以
`default-features = false` 关掉。

方向不能反过来:mcpp 的 feature 是**只增不减**的,若把 `-DGRPC_ARES=0` 写进基础 flag 再让 feature
翻成 1,两个 `-DGRPC_ARES` 会同时出现在命令行。关闭那一侧因此由 `grpc-m` 的 `build.mcpp` 承担
(`mcpp::has_feature("ares")` 为假时才发 `GRPC_ARES=0` 并剔除对应 TU),依赖仍走
`[feature-deps.ares]`(build.mcpp 明确不允许添加注册表依赖)。

## 2. 形态:只缺一个 ares_config.h

c-ares 平时靠 configure/CMake 生成 `ares_config.h`。本包用**冻结的按 OS 快照**替代该步骤,即
`compat.ffmpeg` / `compat.curl` / `compat.sdl2` 已经在用的形态。

上游 release tarball(`make dist` 产物,不是 tag 归档)已经提供了另外两样,因此**不需要**合成:

- `include/ares_build.h` —— 已是生成好的成品;
- `src/lib/config-win32.h` —— Windows 的备用配置。

快照直接取自 gRPC 1.83.0 的 `third_party/cares/config_{linux,darwin,windows}/ares_config.h`。
这不是随手挪用:gRPC 把 c-ares 精确 pin 在同一个 release(submodule `d3a507e` == tag `v1.34.5`),
这三个文件正是它为"不跑 c-ares 自己的构建系统"而维护的,并在其自身 CI 的三平台上长期使用。

**已知取舍(如实记录)**:这三份快照比 1.34.5 的 `ares_config.h.cmake` 模板保守 —— 模板有 143 个
`HAVE_*`,快照只给出 96 个,缺的包括 `HAVE_EPOLL`、`HAVE_GETIFADDRS`、`HAVE_GETRANDOM`、
`HAVE_IF_NAMETOINDEX` 等。缺失项一律按 0 处理,c-ares 因此走可移植的退化路径(如 poll 而非 epoll)。
实测**91/91 TU 编译通过、零警告**,所以这是**性能/特性层面的退化,不是功能不可用**。日后可用
c-ares 自己的 CMake 重新生成更完整的快照。

## 3. 编译开关照抄上游

`cflags` 取自 gRPC 的 `third_party/cares/cares.BUILD`:

- `CARES_STATICLIB` —— 选择静态链接的 declspec;
- `HAVE_CONFIG_H` —— **没有它 `src/lib/ares_setup.h` 根本不会去 include `ares_config.h`**;
- `_GNU_SOURCE` —— 在 glibc 上是**功能性必需**:缺了它 `<unistd.h>`/`<string.h>` 不声明
  `gethostname`、`clock_gettime`、`strcasecmp`、`getservbyport_r`,实测有 4 个 TU 直接报
  implicit-declaration 错误;
- `_HAS_EXCEPTIONS=0`。

Windows 另加 `NOMINMAX` / `_CRT_SECURE_NO_DEPRECATE` / `_CRT_NONSTDC_NO_DEPRECATE` /
`_WIN32_WINNT=0x0600`,并链 `ws2_32` + `iphlpapi`;macOS 因快照定义了 `HAVE_LIBRESOLV` 而链 `-lresolv`;
linux 链 `-lpthread`。

`src/lib/**/*.c` 可以放心通配:上游把测试与工具放在 `test/` 和 `src/tools/`,该目录下**没有任何
TU 定义 `main()`**(已核对)。

## 4. 验证结论

与 CI 一致的配置(mcpp **2026.8.3.3**、gcc@16.1.0、`MCPP_INDEX_MIRROR=GLOBAL`、
`MCPP_BUILD_CACHE=local`),先删 `target/` 与 `.mcpp/` 冷构建:

```
c-ares          test result ok. 1 passed; 0 failed
objects: 92  (= 91 源 + 1 测试)
```

- **测试全程离线**,不发任何 DNS 查询,因此不依赖 runner 的网络状况。覆盖面按"每一项落在库的不同部分"
  挑选:`ares_library_init/cleanup`、`ares_init_options`+`ares_destroy`、
  CSV 服务器列表的**设置与回读往返**(真正走字符串与记录解析)、`ares_inet_pton/ntop` 双向、
  `ares_strerror`。并且断言**畸形输入必须被拒**(非法服务器 CSV、`999.1.1.1`),否则前面的"解析成功"
  什么也证明不了。版本号也断言到 1.34.5,避免悄悄换版本还能通过。
- **CN 镜像已闭环**:`mcpp-res/c-ares@1.34.5` 返回 `http=200` 且与 GLOBAL **字节一致**。
- **跨包 basename 撞名检查**:c-ares 与索引中已有的 abseil / protobuf / upb / re2 **零撞名**。
  (顺带记录:其余包之间存在若干撞名,如 abseil×protobuf 的 `parser.cc`、abseil×gRPC 的
  `status.cc`/`time.cc`,mcpp 会做嵌套消歧,且 CI 已用 `MCPP_BUILD_CACHE=local` 绕开 mcpp#344
  的缓存键问题 —— 见 [re2+upb 文档](2026-08-04-add-re2-and-protobuf-upb-plan.md) §6.2。)

## 5. gRPC 整库编译验证(索引侧到此为止的意义)

在本包落地的同时,已用 mcpp 的 gcc@16.1.0 对 gRPC 1.83.0 做了**全量编译验证**:

```
1001 个 TU 全部通过(999 个 gRPC 源 + 2 个 third_party/address_sorting)
```

其中包含走**真实 c-ares 头文件与本包配置快照**的解析器路径。所需的额外 include 只有
`third_party/address_sorting/include` 与 `third_party/xxhash`(纯头),两者都在 gRPC 自带的
third_party 里有实体内容。gRPC 的 1000 个源文件**没有一个来自 third_party**,因此 `grpc-m`
只需 vendor `src/` + `include/`,abseil/protobuf/re2/c-ares/openssl 全部由索引提供。

唯一必须排除的是 `src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c` ——
它与 `compat.protobuf` 的 `upb` feature 带入的 bootstrap 版本**逐字节完全相同**,同时编入会重复符号。
