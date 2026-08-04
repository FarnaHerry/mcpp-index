# gRPC 收录可行性分析(2026-08-04)

目标:评估 <https://github.com/grpc/grpc/tree/master/examples/cpp/helloworld> 能否以
`pkgs/c/compat.grpc.lua` 的形态进入 mcpp-index。

**结论:不能。gRPC 属于 opencv/ffmpeg 那一档的重型库,应走 `opencv-m` 已经验证过的
「独立适配仓库 + 索引侧一条 Form-A 描述符」路线,而不是索引内的 compat 描述符。**

以下每一条判断都附了核实方式,评估基于上游 **v1.83.0**(`git ls-remote --tags` 取到的最新
release tag)。

---

## 1. 上游没有自包含 tarball —— 索引的 `url + sha256` 模型直接失配

mcpp-index 的每个版本条目都是「一个 tarball + 一个 sha256」。gRPC 没有这样的产物:

- v1.83.0 **没有任何 release asset**(`GET /repos/grpc/grpc/releases/tags/v1.83.0` → `assets: []`),
  只有 GitHub 自动生成的 tag 归档。
- 该归档里的 third_party 全是**空的 submodule 占位目录**。实测
  `tar -tzf grpc-1.83.0.tar.gz | grep -c third_party/<x>/`:

  | submodule | 归档内条目数 |
  |---|---|
  | abseil-cpp | 1(空) |
  | protobuf | 1(空) |
  | re2 | 1(空) |
  | boringssl-with-bazel | 1(空) |
  | zlib | 1(空) |
  | upb / utf8_range / address_sorting | 389 / 40 / 11(有内容) |

要拿到可构建的源码,必须自己 `git clone --recurse-submodules` 后重新打包,并把这个
**我们自己造的 tarball** 托管到 mcpp-res/xlings-res(GLOBAL 与 CN 两侧)。这条路本身有先例
(见记忆 `windows-symlink-archives`),但它意味着索引不再指向可被上游校验的产物,而是长期承担
一个 ~200MB 级别的自托管资产。这已经超出 `compat.*` 描述符的定位。

## 2. 构建形态:CMake 路线不可行,可行的是「整源码直编」

### 2.1 `install()` + CMake 会撞上 C++ ABI

`compat.openssl` / `compat.openblas` 用 `install()` 钩子跑上游自己的构建系统。对 gRPC 复用这个
模式会失败,原因是 **openssl 是 C,gRPC 是 C++**:

- `install()` 跑在 mcpp 的编译规则之外,只能用 PATH 上的 `cc`/`c++`(`compat.openssl.lua` 的注释
  已经写明这一点,并因此在 macOS 上被迫 `CC=/usr/bin/cc`)。
- 而 mcpp 链接消费者时用的是自己的工具链:linux x86_64 默认 `gcc@16.1.0`,macOS/Windows 默认
  `llvm@20.1.7`(`mcpp/docs/03-toolchains.md`);并且 `[build] cxx_runtime` 默认
  `self-contained` —— macOS 静态链 LLVM 自带的 libc++,linux clang 工具链显式链
  libc++.a/libc++abi.a(`mcpp/docs/05-mcpp-toml.md`)。
- gRPC 的 C++ 接口(`grpc::Status`、`ServerBuilder`、`ClientContext`)满是 `std::string`。
  外部 `c++` 编译出的 `libgrpc++.a` 与 mcpp 链接侧的标准库不一致时,是 std 类型布局层面的
  ABI 冲突,不是加个 `-l` 能补的。macOS(Apple clang + 系统 libc++ vs 静态 llvm libc++)
  风险最高。

C 语言的 `compat.openssl` 能绕过去,恰恰因为 C 没有这个问题。

### 2.2 整源码直编是可行的 —— 而且比预期干净

`compat.ffmpeg`(2281 TU / 28 个目录 glob)和 `opencv-m` 已经证明这条路走得通。gRPC 的体量在
同一量级,并且**上游已经把绝大多数生成物 check-in 了**,实测:

| 组件 | 非测试源文件数 | 备注 |
|---|---|---|
| grpc `src/core` | 994 | 已含 `ext/upb-gen` 182 + `ext/upbdefs-gen` 166 的 check-in 生成码 |
| grpc `src/cpp` | 91 | C++ 同步/异步 API |
| abseil-cpp | 206 | Apache-2.0 |
| protobuf `src` | 287 | 含 14 个 check-in 的 `.pb.cc`(well-known types) |
| re2 | 28 | BSD-3-Clause |
| c-ares | 94 | MIT |
| **合计** | **≈1700** | 全部为重型 C++17 TU |

关键有利事实:

- **不需要跑 protoc 来构建库本身** —— upb 生成码与 protobuf 的 `.pb.cc` 都已在源码树内。
- **不需要跑 configure** —— c-ares 的平台配置 grpc 上游已经按 OS 冻结好了
  (`third_party/cares/config_linux` / `config_darwin` / `config_windows`),形态与
  `opencv-m` 的 `gen/<os>/` 快照完全一致;absl/protobuf/re2 无 configure。
- **SSL 可复用本仓已有的 `compat.openssl` 3.5.1**,不必再 vendor boringssl(451 个文件 + 汇编)。
  gRPC 官方支持 OpenSSL(`gRPC_SSL_PROVIDER=package`,`cmake/ssl.cmake`),整个 core 里只有
  9 个文件带 `OPENSSL_IS_BORINGSSL` 分支,且 OpenSSL 是非-boringssl 的默认分支。

代价:~1700 个重型 C++ TU 的编译时间远超 ffmpeg 的 C 代码。CI 的 `workspace` job
`timeout-minutes: 150` 有余量,但每晚的 full regression 要在三平台各付一次这个成本。

## 3. helloworld 需要 codegen —— 消费侧机制存在,但工具链分发是缺口

`examples/cpp/helloworld` 必须先用 `protoc` + `grpc_cpp_plugin` 把 `helloworld.proto` 生成
`helloworld.pb.cc` / `helloworld.grpc.pb.cc` 才能编译。

**好消息:mcpp 有 `build.mcpp`**(`mcpp/docs/07-build-mcpp.md`),这是 Cargo `build.rs` 的对应物,
足以承载 codegen:消费者的 `build.mcpp` 调用 protoc,然后 `mcpp::generated(...)` 把产物加进构建,
`mcpp::include_dir(...)` 加进搜索路径。`opencv-m` 已经在用这个机制。

**缺口:插件二进制怎么到消费者手上。**

- `mcpp::dep_dir("grpc")` 给的是依赖的 **install 目录(源码)**,不是构建产物目录;mcpp
  **没有**把依赖的 `kind = "bin"` target 暴露给消费者的机制(已在 mcpp 源码与文档中确认)。
- 因此 protoc(144 个 `.cc`)与 grpc_cpp_plugin(15 个 `.cc`)不能靠包依赖图交付。

可行解是走 xim 构建环境(`[xlings] deps`,与 `xim:make` / `xim:cmake` / `xim:nasm` 同一层):
把两个插件做成一个 `xim:grpc-tools` 预编译包托管到 xlings-res,消费者
`[xlings] deps = ["grpc-tools@1.83.0"]`,`build.mcpp` 里直接调。protoc 上游本身就发布各平台
预编译产物;grpc_cpp_plugin 需要我们自行构建并托管。

## 4. 建议路线:`grpc-m` 独立适配仓库

与 `opencv-m` 同构(该仓库的 `mcpp.toml` 注释明确记录了它是「从 mcpp-index 的
`compat.opencv.lua` 迁出来的单仓形态」—— gRPC 应当直接从终点开始,跳过中间那一步):

```
grpc-m/
  third_party/grpc-1.83.0/        # 剪枝后的上游 vendor(含 absl/protobuf/re2/cares 子模块)
  gen/{common,linux,macosx,windows}/   # 平台配置快照(c-ares 的可直接取上游冻结版)
  build.mcpp                     # 平台条件的 include dirs / 源码选择
  src/*.cppm                     # 可选:C++23 module 层(import grpc;)
  tools/vendor/port_descriptor.py  # 对标 opencv-m 的 vendoring 脚本,产物可复现
  examples/helloworld/           # 端到端验证
  mcpp.toml
```

索引侧最终只增加**一条 Form-A 描述符** `pkgs/g/grpc.lua`(形态同 `pkgs/o/opencv.lua`),
`mcpp` 字段指向该仓自带的 `mcpp.toml`,索引不承载任何构建信息。

建议分期:

- **P0 —— protobuf + abseil(已完成,见下方修订)**:**不需要独立仓**,已作为普通 compat 包直接进索引。
- **P1 —— grpc core + C++ 同步 API**:加 re2 / c-ares / grpc `src/core` + `src/cpp`,
  SSL 复用 `compat.openssl`,跑通 helloworld(unary,同步)。
- **P2 —— 工具链分发**:`xim:grpc-tools`(protoc + grpc_cpp_plugin 预编译,三平台)。
  注:protobuf 上游已发布全平台 protoc 预编译包(`protoc-35.1-{linux-x86_64,osx-aarch_64,win64,…}.zip`),
  只有 `grpc_cpp_plugin` 需要我们自建托管。
- **P3 —— 索引登记**:`pkgs/g/grpc.lua` Form-A + CN 镜像 + workspace 成员。

## 4.1 修订(2026-08-04,P0 落地后)

上文第 4 节最初提议把 protobuf 也做成 `protobuf-m` 独立仓。**这一判断在核实上游发布物后被推翻**,
实际结论更简单:

- abseil 与 protobuf **都发布了正式的自包含 release asset**
  (`abseil-cpp-20250512.1.tar.gz` 2.2MB、`protobuf-35.1.tar.gz` 7.1MB),无 submodule、无 configure、
  生成码(14 个 `.pb.cc`)已 check-in,因此**不存在自举 protoc 的问题**;protobuf 还自带权威源码清单
  `src/file_lists.cmake`。
- 三条性质合起来正好落在索引标准的 Shape A 模板里,于是 P0 以两个普通 compat 包落地:
  `compat.abseil@20250512.1`(151 TU)与 `compat.protobuf@35.1`(79 TU + utf8_range),
  详见 [2026-08-04-add-abseil-and-protobuf-plan.md](2026-08-04-add-abseil-and-protobuf-plan.md)。
- 需要独立仓的**只有 gRPC 本身** —— §1 的"上游无自包含 tarball"与 §2 的构建规模问题都只出现在 gRPC,
  protobuf/abseil 一条都不占。

## 5. 不建议的做法

| 做法 | 为什么不行 |
|---|---|
| `pkgs/c/compat.grpc.lua` + `install()` 跑 CMake | C++ ABI 与 mcpp 工具链不一致(§2.1);CI 每次冷构建 |
| 上传各平台预编译静态库 | C++ 静态库跨标准库/glibc 版本不可移植,比 openblas 的 C 场景脆弱得多 |
| 只收录 grpc、让用户自备 protobuf | 上游无自包含 tarball,protobuf 版本必须与 grpc 严格配对 |
| 在索引里放 compat.grpc 先跑通再迁出 | opencv 走过这条路并已迁走;重复一遍只是多一次迁移 |

---

## 附:核实命令

```bash
git ls-remote --tags https://github.com/grpc/grpc.git   # → v1.83.0
curl -sL https://api.github.com/repos/grpc/grpc/releases/tags/v1.83.0 | jq .assets  # → []
tar -tzf grpc-1.83.0.tar.gz | grep -c '^grpc-1.83.0/third_party/abseil-cpp/'        # → 1
git clone --depth 1 -b v1.83.0 ... && git submodule update --init --depth 1 ...     # → 550MB
grep -rl OPENSSL_IS_BORINGSSL src/core src/cpp include | wc -l                      # → 9
```
