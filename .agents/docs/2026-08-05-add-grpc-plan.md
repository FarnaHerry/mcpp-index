# 新增 mcpplibs.grpc 1.83.0(2026-08-05)

[gRPC 收录可行性分析](2026-08-04-grpc-feasibility-analysis.md) 的终点。索引侧此前已备齐依赖
([abseil+protobuf](2026-08-04-add-abseil-and-protobuf-plan.md)、
[re2+upb](2026-08-04-add-re2-and-protobuf-upb-plan.md)、
[c-ares](2026-08-04-add-c-ares-plan.md)),本次加入 gRPC 本体。

产出:`pkgs/g/grpc.lua`(Form A,指向 [mcpplibs/grpc-m](https://github.com/mcpplibs/grpc-m)
的 release)+ workspace 成员 `tests/examples/grpc-module`。

## 1. 为什么是独立仓库

这是本次唯一一个不能用 compat 描述符表达的库,理由只有一条但很硬:**gRPC 不发布任何自包含的
源码产物**。v1.83.0 完全没有 release asset,而其 tag 归档里 abseil / protobuf / re2 /
boringssl / zlib 全是**空的 submodule 占位**(每个只有一个目录条目),`url` + `sha256` 无处可指。

`grpc-m` 的 release tarball 就是那个缺失的产物:上游 `src/` 与 `include/` **零补丁** vendor,
外加 gRPC 自己真正带内容的两块 third_party(`address_sorting`、`xxhash`)。

**而它不 vendor 的东西才是它属于本索引的理由**:abseil、protobuf(+upb)、re2、c-ares、
OpenSSL、zlib 全部取自本索引的包。于是一个同时直接使用 protobuf 或 abseil 的消费者链进去的是
**同一份**,而不是和第二份 vendored 副本撞车。

## 2. 构建形态

无 CMake、无 Bazel、无 configure —— 这不是取巧,而是核实过的事实:gRPC 源码树里**没有任何
`.h.in` 或 `config.h.cmake`**,且其 upb 生成码上游已 check-in,所以 mcpp 只需要 include 路径。
1001 个 TU 全部由解析出的工具链编译,**没有任何外部构建系统参与**,因此不存在
`compat.openssl` 那种"外部 `c++` 编出的产物与 mcpp 链接侧 C++ ABI 不一致"的风险
(那正是可行性分析里否掉 `install()` + CMake 路线的原因)。

源码清单是**上游自己的**:`add_library(gpr)` + `add_library(grpc)` + `add_library(grpc++)` +
`add_library(address_sorting)` 之并(995 条,c-ares 的 7 条另计)。`grpc-m` 的
`tools/gen_sources.py` 能从上游 checkout 重新生成它,`--check` 在该仓 CI 里运行,证明 manifest
与 vendored 源码树没有漂移。

刻意排除一个文件:`src/core/ext/upb-gen/google/protobuf/descriptor.upb_minitable.c` ——
与 `compat.protobuf` 的 `upb` feature 已编译的 bootstrap 版本**逐字节完全相同**,两份同编会重复符号。

## 3. `import grpc;` 与顺序约束

`grpc-m` 提供真实的 C++23 模块层(`src/grpc.cppm`),它同时也充当 mcpp `kind = "lib"` 约定要求的
lib root。导出面只取**公开 API**:`namespace grpc` 里还有约 280 个实现细节名字
(`grpc::internal`、`CallOp*` 机器),整表扫描会把它们一起导出。每个名字都由编译器验证 ——
`export using ::grpc::X;` 在 X 不存在时直接编译失败 —— 这已经当场抓出三个并不存在的名字
(`GrpcLibraryCodegen`、`DisableDefaultHealthCheckService`,以及补 include 之前的
`ServerReaderWriter`)。

**`import grpc;` 必须放在该 TU 所有 textual `#include` 之后**,标准库头也算。全局模块片段把大半个
标准库带进了 BMI,import 之后再 textual include 会让同一批声明到达两次。两种顺序都实测过
(gcc 16.1.0):

- import 在前 → `redefinition of std::__is_constant_evaluated` 加一大片 `<limits>` 冲突
- 仅有 std 头跟在 import 之后 → `std::string` 上的 `ambiguous overload for operator==`
- **include 全部在前、import 在最后 → 正常**,因为导出实体属于**全局模块**,textual 视图与
  import 视图是同一批实体

这不是本包发明的约束:gRPC 的代码生成器产出的是**头文件**,任何真实程序都会同时用到两个世界。

## 4. `ares` feature 的方向是被迫的

c-ares **默认开启**,与上游 gRPC 及各发行版一致;`default-features = false` 关闭。

方向不能反过来:mcpp 的 feature 是**只增不减**的,若把 `-DGRPC_ARES=0` 写进基础 flag,再让
feature 翻成 1,两个 `-DGRPC_ARES` 会同时出现在命令行。因此 manifest 对 `GRPC_ARES` **只字不提**
(`port_platform.h` 用 `#ifndef` 兜底成 1,所以"沉默"就是开启态),关闭态由 `build.mcpp` 在
feature 缺席时发 `GRPC_ARES=0`;而那 7 个解析器 TU 走相反方向 —— 声明式地放在
`[features.ares].sources` 里。

## 5. 验证结论

`grpc-m` 侧(与本索引 CI 同款配置:mcpp 2026.8.3.3、gcc@16.1.0、`MCPP_BUILD_CACHE=local`,
且**从已发布索引解析、无任何本地重定向**),1412 个目标文件入链:

```
mcpp test            -> grpc::Version() = 1.83.0 ; module: OK
examples/helloworld  -> server listening on 127.0.0.1:34733
                        Greeter replied: Hello mcpp
                        Greeter rejected the empty name: name must not be empty
```

helloworld 刻意做成**单进程**:在真实 loopback 端口上起真实服务端,经真实 HTTP/2 通道发起真实
unary RPC,所以 `mcpp run` 用退出码回答"gRPC 到底能不能用"。**错误路径也走线** —— 空 name 必须
从服务端回 `INVALID_ARGUMENT` —— 因此一个"什么都答 OK"的桩通不过。

索引侧成员 `tests/examples/grpc-module` 全程只用 `import grpc;`,不含任何 protoc 产物:
gRPC 的 codegen 需要 mcpp 无法交付给消费者的宿主工具,所以生成物那条路径由 grpc-m 自己的
example 覆盖,索引成员验证模块面。

## 6. 遗留:codegen 工具的分发

`protoc` 与 `grpc_cpp_plugin` 目前仍需用户自备(`grpc-m` 的模板与 example 把生成产物**签入**,
所以开箱即用)。根本原因是 mcpp 没有把依赖的构建产物交给消费者的机制:`mcpp::dep_dir()` 给的是
依赖的**源码目录**,而 `kind = "bin"` 的 target 不对消费者暴露。

有利条件已核实:protobuf 上游发布**全平台预编译 protoc**,所以缺口只有 `grpc_cpp_plugin`
(本次为生成 helloworld 的 stub,已用 vendored 源码在本地编出过它,157 TU 的 libprotoc + 3 个
grpc compiler TU)。后续可做成 `xim:grpc-tools` 预编译包,消费者以 `[xlings] deps` 声明,
再由 `build.mcpp` 调用 —— 这条链路 mcpp 已经具备,只差把工具打包。
