# 收录 godot-cpp:compat.godot-cpp(本 PR)与模块层 godotengine.godot-cpp(后续 PR)

日期:2026-08-04
上游:<https://github.com/godotengine/godot-cpp> · 版本 `godot-4.5-stable` → 索引版本 `4.5.0`

## 1. 形态判定

godot-cpp 是 Godot GDExtension 的 C++ 绑定库。表面上属于「C++ 源码 compat」,但它有一个决定性特征:

**上游 tag 归档只是半棵源码树。** 另一半 —— 约 1000 个引擎类与全部 builtin Variant 类型,即
`gen/include/` 与 `gen/src/` —— 由上游自带的 `binding_generator.py` 从
`gdextension/extension_api.json` 在构建时生成,任何上游 tag 归档或 release 都不含它:

```
上游归档:  include/ 67 个 .hpp   src/ 32 个 .cpp
生成之后:  +1010 个 .hpp        +990 个 .cpp     (共 18 MB)
```

于是形态判定的真正问题不是模板选哪个,而是**这一步 codegen 在哪里跑**。三个选项:

| 方案 | 结论 |
|---|---|
| 消费侧 `install()` 钩子跑 python | 否决。会让 Python 成为每个消费者、每个平台的硬依赖,与本索引既有做法相悖(compat.ffmpeg 的 config 快照、opencv 包的 gen/ 快照都是「消费侧不跑 codegen」) |
| 把 gen/ 塞进 `generated_files` | 否决。18 MB 文本进描述符 |
| **离线跑一次,产物随镜像归档发布** | **采用** |

因此下载地址不是上游归档,而是 `xlings-res/godot-cpp` 的重打包归档 —— 与
[[windows-symlink-archives]] 里 asio 的 repack 惯例同源,只是这次加的是 `gen/` 而不是去符号链接。

`tools/godot-cpp/repack.sh` 是该归档的唯一来源与验证器:

1. 拉上游 tag 归档(`ac78539c…e339c`);
2. 跑**未修改**的上游 `binding_generator.py`(默认配置:64 位、`precision=single`、`template_get_node` 开);
3. 把重新解包的上游树与生成后的树做 `diff -r`,**除 `gen/` 之外任何差异即拒绝打包**;
4. 确定性打包(`--sort=name`、固定 mtime、`gzip -n`)。

连跑两次 sha256 相同:`b0c36e77f02c4181352cdd7547b209b93a833be1ad6197f8c650d92987221a00`。

### 版本号

上游 tag 是 `godot-4.5-stable`,索引版本取裸版本 `4.5.0`(lint 拦前导 `v`,且给 4.5.x 留位)。

## 2. 镜像

| 区域 | 地址 |
|---|---|
| GLOBAL | `https://github.com/xlings-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz` |
| CN | `https://gitcode.com/mcpp-res/godot-cpp/releases/download/4.5.0/godot-cpp-4.5.0.tar.gz` |

两侧 sha256 一致,均等于本地打包结果(下载回来逐一核过)。仓库 README 记录了复现方式与上游归档 sha。

三平台共用同一份 OS 中立归档:godot-cpp 是可移植 C++,没有按平台切换的源码集合(平台差异在 Godot 本体里,
被 `gdextension_interface.h` 这层 C ABI 挡住了)。

## 3. 描述符要点

- `include_dirs = { "*/include", "*/gen/include", "*/gdextension" }` —— 与上游 cmake 暴露的三个根一致。
- `sources` 逐层枚举(`*/src/*.cpp`、`*/src/*/*.cpp`、`*/gen/src/*/*.cpp`)而非 `**`:上游自带的
  `test/src/*.cpp` 是它自己的示例扩展,不能被扫进库里。
- 目标名 `godot-cpp`,kind = lib,共 1022 个 TU。
- `src/core/object.cpp` 与 `gen/src/classes/object.cpp` **basename 撞名**。mcpp ≥ 0.0.98 的 obj 路径消歧
  (mcpp#233/#240)已覆盖这种同包内撞名,本地实测链接正常;这是本包对客户端版本下限的隐含依赖,
  index.toml 现有 floor 远高于它。

### -fPIC

**GDExtension 本身就是一个 shared library**,所以这个静态库的 .o 几乎必然要链进 .so/.dylib。
不加 `-fPIC` 时链接直接失败(`relocation R_X86_64_32 against '.rodata' can not be used when
making a shared object`),等于这个包做不了它唯一存在的意义 —— 上游 SCons/CMake 也正是为此传 `-fPIC`。
故 `linux`/`macosx` 的 `cxxflags` 加 `-fPIC`;windows 不需要(PE 无此区分,clang-cl 只会报 flag 未使用)。

这条是写 examples/summator(真 GDExtension,`kind = "shared"`)时被链接器抓出来的,不是推断的。

### define 与 feature 评估

`GDEXTENSION` 是上游 cmake 挂在 target INTERFACE 上的 **PUBLIC** define,库与消费者 TU 必须一致,
所以走 `default = { implies = { "gdextension" } }`(feature 的 `defines` 才到得了消费端,而
`default.implies` 无条件生效 —— 与 compat.curl 的 `CURL_STATICLIB` 同一解法)。

以下 define **有意不做成 feature**:

- `DEBUG_ENABLED` / `DEV_ENABLED`:额外检查,上游 release 默认关。
- `HOT_RELOAD_ENABLED`:会改 `Wrapped` 的布局 —— 属于 ABI,不是开关。
- `REAL_T_IS_DOUBLE`:需要用 `precision=double` 重新生成的另一棵 `gen/` 树,归档里没有,
  所以它根本不可能是本包的 feature;真要支持是另一个版本/包。

共同理由:每个都会把 store 重新 key 一次,等于把同一个库的 1000 TU 再全量编一遍。

## 4. 测试成员

`tests/examples/godot-cpp/`(根 `[indices] compat` 继承,成员不再声明)。断言分两类,都能真失败:

- **链接类**:`Vector2::length()`、`Basis::orthonormalized()`、`Color::to_rgba32()`、`AABB::get_volume()`
  在头里只有声明,定义在 `src/variant/*.cpp` —— 跑通即证明这 1022 个 TU 真的编了并链进来了
  (对照 [[verify-obj-count-not-green-ci]]:绿 CI 不等于包被编译)。
- **生成绑定类**:`<godot_cpp/classes/node.hpp>`、`Node::PROCESS_MODE_*`、`godot::OK`、
  `godot::ERR_FILE_NOT_FOUND`、`Variant::OBJECT` —— 这些只存在于 `gen/`,缺了就编不过。

**不能测什么**:`String`/`Array` 等一切要走 `gdextension_interface_*` 函数指针的 API,以及类注册
(`GDREGISTER_CLASS`),都需要一个已加载该扩展的 Godot 进程。纯数学那半边不需要,所以断言全落在那里。

## 5. 本地验证

与 CI 同版本(`.github/workflows/validate.yml` 的 `MCPP_VERSION = 2026.8.3.3`),冷验证:

```
$ mcpp test -p godot-cpp
   Compiling compat.godot-cpp v4.5.0
     Running bin/godot_cpp
vec2=1 vec3=1 basis=1 color=1 aabb=1 gen=1
godot_cpp ... ok (0.12s)
 test result ok. 1 passed; 0 failed; finished in 106.54s (build 55.60s + run 0.02s)
```

另外用 gcc 13 单独全量编过一遍 1022 个 TU(`-std=c++23`,零告警失败),并把全部 .o 与测试 main
直接链接跑通 —— 用来提前确认「依赖 .o 全量入链」下没有未解析符号(不需要 `-ldl`/`-lpthread`)。
CPU 时间约 16 分钟,4 核 runner 上折合 4~5 分钟。

## 6. 后续:模块层 godotengine.godot-cpp-m

模块包**不**放在本索引里内联(避免索引变重,也避免把 1000 TU 再编一遍):按 Form A 走外部仓
`mcpplibs/godot-cpp-m`(版本号**与上游对齐**,即 4.5.0 —— 只做 API 重导出的模块层没有自己的版本值得编,
消费者从 `godot-cpp = "4.5.0"` 需要知道的就是它对着哪个 Godot),其 `mcpp.toml` 依赖 `compat.godot-cpp`,提供 `import godot_cpp;`
(单段模块名,与 `#include <godot_cpp/...>` 和库名对应;点号在本索引里留给子模块,如 `opencv.cv`)。

**顺序是硬约束**:模块成员的测试工程只能声明一条 `[indices]`,给 `godotengine`;它的传递依赖
`compat.godot-cpp` 于是从**已发布**的远端索引解析(tests/examples/ffmpeg-module 的注释写的就是这件事)。
所以必须先合并本 PR、`publish-artifact` 重新发布 artifact,第二个 PR 的 CI 才可能绿。

### 已完成(mcpplibs/godot-cpp-m 0.0.1)

`src/godot_cpp.cppm` 由 `tools/gen_module_cppm.py` **生成**:扫描头文件(带花括号/命名空间状态机),
把 `godot` 命名空间作用域的全部声明批量产出为 `export using ::godot::X;`,约 1750 个名字 ——
全部引擎类、全部 builtin Variant 类型、全局枚举**连同枚举量**(所以 `godot::OK` 拼法不变)、
`godot::Math`、模板。手工罗列这个面积不现实,且每个 Godot 版本都会变,故生成 + 编译器校验。

踩到的两类坑:

- **宏带不走**。`GDCLASS`、`GDREGISTER_CLASS`、`GDVIRTUAL_*`、`D_METHOD`、`memnew`、`ERR_*` 是预处理器
  构造,而 GDExtension 代码正是用它们写的。解法照搬 ffmpeg-m:附一个侧头文件
  `<godot-cpp-m/macros.h>`,与 import 并排包含。godot-cpp 的头在模块的 GMF 里,两种拼法指向同一批
  global-module 实体,混用是良构的 —— `tests/godot_cpp_macros.cpp` 就是这个形状,通过。
- **TU-local 暴露是硬错误,不是告警**。`HashMap`/`HashSet` 及其默认 hasher 的内联体会走到
  `hash_murmur3_one_float/double`,这两个既是 `static`,体内又声明了匿名 union;从模块接口暴露一个
  TU-local **类型**是 error(不是 `-Wexpose-global-module-tu-local` 那条 warning)。用二分法在 1759 个
  导出名里定位到 7 个(含 `PairHash`),排除即可 —— 它们是 godot-cpp 内部容器,扩展代码用
  Dictionary/Array/TypedArray,头文件里也仍然拿得到。另外那 3300 条 warning 级暴露用
  `-Wno-expose-global-module-tu-local` 静掉,是上游头的形态,包一层改变不了。

验证:`mcpp test` 两条全过(模块面 + 宏面),`examples/summator`(真 GDExtension,`kind = "shared"`)
构建出 `libsummator.so` 且导出 `summator_library_init`。

### 本 PR(索引侧)可验证到哪一步

`godotengine.godot-cpp` 是 Form A,索引侧只有描述符;成员的 `[indices]` 给了 `godotengine`,
所以传递依赖 `compat.godot-cpp` 必须从**已发布**的索引解析。合并前本地伪造 published index 来验证
是走不通的:mcpp 在解析不到时会主动 refresh 一次,把注入的状态直接冲掉(参见
[[stale-global-index-masks-descriptor-bugs]] 的反面)。因此这一步的唯一真实验证就是
compat PR 合并 + artifact 重新发布之后的 CI。
