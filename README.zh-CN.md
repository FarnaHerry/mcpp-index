# mcpp-index

[English](README.md) | **简体中文**

> [`mcpp`](https://github.com/mcpp-community/mcpp) 构建工具的默认包索引仓库。
> 在线浏览所有包:**https://mcpplibs.github.io/mcpp-index/**

本仓收录可被 `mcpp` 直接 `add` 的 C++23 包,既包含 `import` 即用的模块化库,也包含以 `compat` 形态从上游源码或
头文件构建的第三方 C/C++ 库。每个包对应一个 `pkgs/<首字母>/<包名>.lua` 描述文件。

## 使用

```bash
mcpp add ftxui@6.1.9            # 添加依赖到 mcpp.toml
mcpp build                     # 自动拉取源码并构建,依赖沿链路自动传递

mcpp search <keyword>          # 搜索并刷新索引
mcpp self config --mirror CN   # 切换至国内镜像,默认使用 GLOBAL 上游源
```

完整包列表见 **[在线索引站](https://mcpplibs.github.io/mcpp-index/)**。

## 包生态与贡献

本仓收录两类包:

- **原生 mcpp 模块库**:以 C++23 模块发布、`import` 即用,包括 `mcpplibs.*`、`nlohmann.json`、`imgui`、`ffmpeg`、`opencv`,以及由
  用户基于 mcpp 开发并登记进索引的库(如 `tensorvia-cpu`)。其上游通常自带 `mcpp.toml`,描述文件(Form A)只声明
  元数据与下载地址。
- **第三方 C/C++ 库(`compat`)**:其上游不提供 mcpp 支持,描述文件(Form B)内联构建信息。该类库存在
  header-only、纯 C 源码、C++23 module wrapper 等形态,可选组件经 `features` 门控,并配备 GitCode CN 镜像。

### 参考示例(`.lua` 描述符)

| 形态 | 示例 |
|------|------|
| 原生模块库(Form A) | [`mcpplibs.xpkg`](pkgs/x/xpkg.lua) · [`mcpplibs.tinyhttps`](pkgs/t/tinyhttps.lua) · [`tensorvia-cpu`](pkgs/t/tensorvia-cpu.lua) · [`ffmpeg`](pkgs/f/ffmpeg.lua)(模块层,源码经 `compat.ffmpeg` 直编) · [`opencv`](pkgs/o/opencv.lua)(单仓库:模块层与 OpenCV 5 全源码构建同在包内,索引侧只留本描述符) · [`mcpplibs.grpc`](pkgs/g/grpc.lua)(gRPC 1.83.0 —— 本索引里唯一**无法**做成 compat 描述符的库:上游不发布任何自包含源码产物,其 tag 归档里 abseil/protobuf/re2/boringssl/zlib 全是空 submodule 占位,因此 [grpc-m](https://github.com/mcpplibs/grpc-m) 的 release tarball 才是那个产物。它只 vendor gRPC 自己的源码,五个依赖全取自本索引,故同时直接使用 protobuf 的消费者链进去的是同一份而非两份)|
| C 源码 compat(含 `features`) | [`compat.cjson`](pkgs/c/compat.cjson.lua) · [`compat.zlib`](pkgs/c/compat.zlib.lua) · [`compat.hiredis`](pkgs/c/compat.hiredis.lua)(经典 1.2.0 —— 7 个 C TU;tarball 平铺头经 `generated_files` 补 `hiredis/` 前缀薄包装头,消费者可写 `#include <hiredis/hiredis.h>`,与上游安装布局一致) · [`compat.sqlite3`](pkgs/c/compat.sqlite3.lua)(纯 C 源码、无 feature:单一 `sqlite3.c` amalgamation;3.45.3,部署最广的 3.45.x 线) · [`compat.libuv`](pkgs/c/compat.libuv.lua)(libuv 1.48.0 —— 逐 OS 源清单转录自上游 CMakeLists,因为 `src/unix/*.c` 通配会一次编进所有 OS 的后端;linux/macos 显式列 unix 子集,windows 用 `src/win/*.c` glob) |
| C++ 源码 compat(彼此依赖) | [`compat.abseil`](pkgs/c/compat.abseil.lua)(151 TU;对 `absl/**` 取通配后,按上游自身的 test/benchmark 命名约定裁剪) · [`compat.protobuf`](pkgs/c/compat.protobuf.lua)(libprotobuf 运行时,79 TU 逐条转录自上游 `src/file_lists.cmake`;因 protobuf 公开头文件 include 了 `absl/…`,故显式依赖 `compat.abseil`;`gzip` feature 定义 `HAVE_ZLIB` 并拉入 `compat.zlib`,`upb` feature 则从同一个 tarball 里再编出 protobuf 的 64 TU C 运行时;还以 `kind = "bin"` target 暴露 **`protoc`**,消费者写 `tools = ["protoc"]` 即可从「自己链接的那个包」拿到为本机构建的编译器,使生成器与运行时的版本错配无法表达) · [`compat.re2`](pkgs/c/compat.re2.lua)(22 TU,取自上游自身的 `RE2_SOURCES`) · [`compat.redis-plus-plus`](pkgs/c/compat.redis-plus-plus.lua)(redis++ 1.3.13 —— 同步客户端,17 TU + `patterns/redlock.cpp`,依赖 `compat.hiredis`;CMake 唯一会生成的头 `hiredis_features.h` 用 `generated_files` 快照,async/TLS TU 不收,基座保持两包成对。`async` feature 补齐 libuv 版 `AsyncRedis` 接口(9 个 async TU + `compat.libuv`;`event_loop.cpp` 在后台线程跑 `uv_run`,`<hiredis/adapters/libuv.h>` 经 compat.hiredis 的包装头到达)。两个版本分处源码结构分水岭两侧,共享同一份源列表:1.3.13(现代 17-TU 布局)与 1.3.3(缺 `redis_uri.cpp`/`redlock` 的 15-TU 旧布局)—— 并集之所以成立,是因为 1.3.3 的 TU 是 1.3.13 的严格子集,恰好两个 glob 在 1.3.3 上零命中(仅警告,非错误;与 compat.catch2 同款手法)) |
| C++ 源码 compat(零依赖客户端 + 可选组件) | [`compat.websocket`](pkgs/c/compat.websocket.lua)(IXWebSocket 12.0.1 —— 从上游 `IXWEBSOCKET_SOURCES` 剔掉 4 个 server TU 后直编的纯 RFC 6455 客户端,**基座零外部依赖**:TLS 关闭(OpenSSL/MbedTLS/AppleSSL 三组 TU 均不编),`IXWEBSOCKET_USE_ZLIB` 不定义(gzip codec 编译为 no-op)。两个可选 feature 在基座上叠加:`server`(4 个 server TU —— `IXWebSocketServer`/`IXSocketServer`/`IXHttpServer`/`IXWebSocketProxyServer`,零新增外部依赖,且 **implies `zlib`** —— 因为上游 server 默认就宣称 permessage-deflate,而 transport 的协商不受宏门控)与 `zlib`(依赖 `compat.zlib`,把 codec 变成真正的 permessage-deflate 压缩)。默认构建的测试自带基于 loopback 原始 socket 的最小 RFC 6455 echo server(握手/掩码/分片/关闭全部离线实测);第二个成员 `websocket-features` 跑真实的 `ix::WebSocketServer`,并断言压缩在线路上可观测 —— 64 KiB 重复载荷往返,`wireSize` = 80) |
| header-only(含 `features`) | [`compat.eigen`](pkgs/c/compat.eigen.lua) |
| header-only(无可门控组件) | [`compat.CLI11`](pkgs/c/compat.CLI11.lua)(命令行解析器,全部定义都是 `CLI11_INLINE`,故整包就是 `*/include` 加一个 anchor TU。上游两个额外件都不收:`src/Precompile.cpp` 只有在 `CLI11_COMPILE` 同时到达**消费者** TU 时才有意义 —— 那是 interface define,不是 sources 门控;`src/modules/CLI11.cppm` 属于模块层,是另一种包形态,而非 compat 包的 feature) |
| 运行时 loader compat(纯源码,绕开上游 codegen/asm) | [`compat.vulkan`](pkgs/c/compat.vulkan.lua)(Khronos loader:`loader/generated/` 已签入,汇编路径经 `UNKNOWN_FUNCTIONS_SUPPORTED` 降级为纯 C,故无需 CMake/Python/汇编器;windows 延后)· [`compat.vulkan-headers`](pkgs/c/compat.vulkan-headers.lua) |
| 全源码直编 + 生成 config(仅缺口平台) | [`compat.curl`](pkgs/c/compat.curl.lua)(win32 用上游签入 config,unix 生成) · [`compat.sdl2`](pkgs/c/compat.sdl2.lua)(win/mac 用上游签入 config,linux 生成 + 手工开 X11) · [`compat.c-ares`](pkgs/c/compat.c-ares.lua)(91 TU;release tarball 已自带 `ares_build.h` 与 Windows 配置,故只需按 OS 冻结 `ares_config.h`) · [`compat.msdfgen`](pkgs/c/compat.msdfgen.lua)(msdfgen 1.13 —— 这里的 config 不是可选项:`core/base.h` 开头就是 `#include <msdfgen/msdfgen-config.h>`,不生成它连 `core/` 都编不了。选择生成它而非传 `-D`,还使库与消费者**天然一致** —— `base.h` 被每个公开头间接包含,于是该文件成为「SVG/PNG/Skia 哪些存在」的唯一出处。`ext/` 四个单元只编 `import-font.cpp`,其余三个各需一个本索引没有的库,其声明经由同一份生成 config 一并消失。`MSDFGEN_USE_CPP11` 刻意不开:它给 `Bitmap` 增加移动构造,即改变了跨库边界类型的布局,而包无法保证每个消费者都同样定义它) |
| 上游 amalgamation(单 TU 即整库) | [`compat.harfbuzz`](pkgs/c/compat.harfbuzz.lua)(HarfBuzz 14.3.0 —— 上游用 meson,在此复刻意味着跟踪 ~137 个 `.cc` 加一份生成的 config。`src/harfbuzz.cc` 正是上游自己支持的「只编一个文件」路径,于是 `sources` 只有一行,且不会与 release 脱节。该 amalgamation 同时 `#include` 了 CoreText/DirectWrite/GDI/GLib/Graphite2 各后端,每个都有自己的 `HAVE_*` 门,因此仅声明 `HAVE_FREETYPE` 即可精确选中 FreeType 桥接,其余编译为空。`HB_NO_MT` 刻意不设:它会去掉 HarfBuzz 的原子操作与锁,仅在消费者保证单线程时才成立,而共享包无法替消费者作此承诺)· [`compat.mimalloc`](pkgs/c/compat.mimalloc.lua)(mimalloc 3.4.5 —— 反向的教训:它同样带 amalgamation(`src/static.c`),但用它是错的。`src/*.c` 通配会在三处出错,且每一处都是**链接期**而非编译期报错 —— `static.c` 会让每个符号重复,`free.c` / `alloc-override.c` 则是被 `alloc.c` `#include` 的、并非独立 TU —— 故源列表取上游自己的 `mi_sources`。`MI_MALLOC_OVERRIDE` 保持关闭:让一个依赖悄悄接管进程分配器,不该由包来决定)|
| 上游 codegen 前置冻结进镜像归档 | [`compat.godot-cpp`](pkgs/c/compat.godot-cpp.lua)(两个版本:`4.5.0` 是 `godot-4.5-stable` 的绑定,`10.0.0-rc1` 是 godot-cpp 自己的 10.x 线、对应 Godot 4.6。`gen/` 下约 1000 个 GDExtension 类不在任何上游 tag 归档里,由上游 `binding_generator.py` 在构建时生成。改为离线跑一次,把上游源码树逐字节原样 **加上** `gen/` 一起发布,消费侧就完全不需要 Python;`tools/godot-cpp/repack.sh` 可确定性复现该归档,且上游文件一旦有出入即拒绝打包) |
| 补索引空缺的头文件包 | [`compat.glx-headers`](pkgs/c/compat.glx-headers.lua)(libglvnd 的 `GL/glx.h`,Khronos registry 不含,SDL 的 X11 后端必需) |
| C++ 应用框架 compat(依赖复用索引内既有包) | [`compat.eui-neo`](pkgs/e/compat.eui-neo.lua)(上游 `3rd/` 自带 8 个 vendored 依赖,此处一个不编,全部改指索引内同版本 `compat.*`) |
| 互斥后端(同包多后端二选一) | [`compat.eui-neo`](pkgs/e/compat.eui-neo.lua):`vulkan` / `sdl2` 各自**替换**默认的 OpenGL / GLFW,默认后端由"不点名任何 feature"表达,并不存在 `opengl`/`glfw` feature。`default` feature 表达不了互斥 —— 它自带的 `defines`/`sources`/`deps` 完全不生效,而 `implies` 又恒生效、无法被点名的 feature 覆盖(后者反而正好是本表『恒开的 interface define』一行的解法)。可行解是读 mcpp 本就会传的 `-DMCPP_FEATURE_<NAME>`,在强制包含头里做前置判定。另注意 `cflags` 只作用于 C TU,C++ 需 `cxxflags` —— 只写进 `cflags` 的后端 define 到不了任何 `.cpp` |
| 宿主运行时适配(不 vendor 驱动) | [`compat.glx-runtime`](pkgs/c/compat.glx-runtime.lua) · [`compat.vulkan-runtime`](pkgs/c/compat.vulkan-runtime.lua)(mcpp 产物跑在自带 glibc 下,裸 soname 的 `dlopen` 够不到宿主驱动;用符号链接农场 + `runtime.library_dirs` 打通。注意 farm 只放带版本号的 soname —— `library_dirs` 同时进链接行) |
| 恒开的 interface define | [`compat.curl`](pkgs/c/compat.curl.lua) 的 `CURL_STATICLIB`:`cflags` 恒开但包私有,feature `defines` 可达消费端但需点名 —— `default = { implies = … }` 无条件生效,恰好两者兼得 |
| 单包多 major(形态随版本切换) | [`compat.catch2`](pkgs/c/compat.catch2.lua)(3.x 编 `src/catch2/` 出静态库;2.x 走 `single_include/` header-only) |
| 外部构建系统(`install()` 从源码构建) | [`compat.openblas`](pkgs/c/compat.openblas.lua)(Make) · [`compat.openssl`](pkgs/c/compat.openssl.lua)(Perl Configure + Make,静态 libssl/libcrypto) |
| 全源码直编(config 快照 + 源列表,零外部构建系统) | [`compat.ffmpeg`](pkgs/c/compat.ffmpeg.lua)(2281 TU 含 NASM 汇编,28 个目录 glob 声明) |
| 构建期生成器产物内联进描述符 | [`compat.gmp`](pkgs/c/compat.gmp.lua)(516 TU,三平台齐全。GMP 的构建要**编译并运行**七个表生成器,还要把 `gmp-h.in` substitute 成 `gmp.h` —— 这些都只是 limb=64/nail=0 的纯函数,故用上游自己的生成器跑一次,产物写进 `generated_files`(约 270 KB,其中 `trialdivtab.h` 占 109 KB)。这一步换掉的是 `install()` 钩子、autotools 以及其探针所需的宿主编译器,连带解掉 windows 推迟的理由 —— GMP 的通用 C 内核从来只要一个 GCC 兼容编译器。`generated_files` 里还按源码目录各放一个一行转发头,于是整包编译**不需要任何 `-I`**,`include_dirs` 只暴露 `gmp.h` + `gmpxx.h`,而不是 GMP 的私有头。与同一 tarball 的 `--disable-assembly` autotools 构建对拍:导出符号 598 个完全一致,上游自带 `make check` 对本产物 177/178 通过) |
| 模块层叠在 compat 源码构建之上(外部 Form-A 仓) | [`godotengine.godot-cpp-m`](pkgs/g/godotengine.godot-cpp-m.lua)(两个版本与上游对齐:`10.0.0-rc1` 对应 Godot 4.6,`4.5.0` 对应 Godot 4.5。`import godot_cpp;` 重导出整个 `godot` 命名空间,约 1800 个名字由头文件**生成**而非手工罗列;1022 个 TU 的构建留在 `compat.godot-cpp`,索引侧只留这一个描述符。宏 —— `GDCLASS`、`GDREGISTER_CLASS`、`memnew`、`ERR_*` —— 是具名模块唯一带不走的东西,故包内附一个与 import 并排包含的侧头文件。另外还带一份生成的 `hashfuncs.hpp` 遮蔽头 —— 上游那个头去掉两个函数的 `static`(它们体内声明了匿名 union)—— 否则 GCC 直接拒绝该模块接口,且是任何 `-W` 开关都够不到的硬错误) |
| C++23 module wrapper | [`nlohmann.json`](pkgs/n/nlohmann.json.lua) · [`marzer.tomlplusplus`](pkgs/m/marzer.tomlplusplus.lua) · [`neargye.magic_enum`](pkgs/n/neargye.magic_enum.lua) · [`boost-ext.ut`](pkgs/b/boost-ext.ut.lua)(逐字复用上游自带的 `include/boost/ut.cppm`,仅加一处 Clang-on-MSVC 需要的 `__argc`/`__argv` shim;命名空间取 `boost-ext`,因其并非 boost 官方库) |

### 新增一个包

完整流程定义于 agent skill [`add-mcpp-index-package`](.agents/skills/add-mcpp-index-package/SKILL.md)。可将下列
指令提供给 agent(如 Claude Code),由其调用该 skill 完成描述文件的编写与全流程:

```text
参考本仓 skill `.agents/skills/add-mcpp-index-package`,将 <库名 / 仓库URL> @<版本> 收录进 mcpp-index:
判定形态;配置 CN 镜像(无 mcpp-res 权限时使用 plain-string 上游 url);编写 pkgs/<首字母>/<包名>.lua;
添加 tests/examples/<库>/ 测试工程并登记为 workspace 成员;使用与 CI 同版本的 mcpp 本地执行
`mcpp test -p <成员>` 进行验证;更新 README 与在线索引;提交 PR 并确认 CI 通过。
```

细节文档位于 [`docs/zh/`](docs/zh/),供人工与 agent 共同使用(英文版位于 [`docs/`](docs/)):

- [库形态与描述符模板](docs/zh/package-types.md):各类形态的描述符模板与样例,以及最小工程的写法。
- [CN 镜像闭环](docs/zh/cn-mirror.md):`gtc` 与 gitcode 操作,以及无 `mcpp-res` 权限时的回退方案。
- [仓库结构与 schema 与 CI](docs/zh/repository-and-schema.md):字段速查、选跑机制与本地 lint。
- 字段的**权威判定**是 `mcpp xpkg parse`(CI 用的就是它:未知的 mcpp 段字段直接失败,而不是被静默忽略);
  语义与约束见 mcpp 仓的 [`docs/spec/`](https://github.com/mcpp-community/mcpp/tree/main/docs/spec)。

> 提交 PR 后,`validate` 自动执行 lint 并按改动库选跑对应 workspace 成员(整个测试面是一个 mcpp
> workspace,公开模块包 `imgui`/`ffmpeg`/`opencv`/`tinyhttps` 也是普通成员——`compat` 的重定向声明在
> workspace 根并由成员继承,消费其他命名空间的成员各自覆盖,零 shell 驱动);合并后,`deploy-site`
> 将其发布至在线浏览站。

## 相关链接

| 项目 | 说明 |
|------|------|
| [mcpp](https://github.com/mcpp-community/mcpp) | 现代 C++23 构建与包管理工具 |
| [xlings](https://github.com/d2learn/xlings) | mcpp 底层的包安装引擎与沙箱环境 |
| [xpkg V1 spec](https://github.com/d2learn/xim-pkgindex/blob/main/docs/V1/xpackage-spec.md) | 包描述文件规范 |
| [mcpplibs](https://github.com/mcpplibs) | mcpp 生态的模块化 C++23 库集合 |
| [mcpp-res](https://gitcode.com/mcpp-res) | 包资源的 CN 镜像组织(gitcode) |

## 社区

[mcpp issues](https://github.com/mcpp-community/mcpp/issues) · [d2learn 论坛](https://forum.d2learn.org)

## License

包描述文件采用 CC0;各上游库保留其自身许可证。
