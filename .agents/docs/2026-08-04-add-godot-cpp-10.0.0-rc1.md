# compat.godot-cpp 增加 10.0.0-rc1(Godot 4.6),并补上 MSVC ABI 缺的 define

日期:2026-08-04 · 承接 [2026-08-04-add-godot-cpp-plan.md](2026-08-04-add-godot-cpp-plan.md)(4.5.0,PR #143)

## 1. 版本线:godot-cpp 有了自己的版本号

上游 godot-cpp 的 tag 从「跟 Godot 走」(`godot-4.5-stable`)换成了**自己的版本线**:
[`10.0.0-rc1`](https://github.com/godotengine/godot-cpp/releases/tag/10.0.0-rc1)。它绑定的引擎版本写在
`gdextension/extension_api.json` 的 header 里:

```json
{ "version_major": 4, "version_minor": 6, "version_patch": 0, "version_status": "stable",
  "version_full_name": "Godot Engine v4.6.stable.official" }
```

即 **10.0.0-rc1 = Godot 4.6**。索引里两条版本并存,消费者按需选:

| 索引版本 | 上游 tag | 引擎 |
|---|---|---|
| `4.5.0` | `godot-4.5-stable` | Godot 4.5 |
| `10.0.0-rc1` | `10.0.0-rc1` | Godot 4.6 |

`mcpp xpkg parse` 接受带预发布后缀的 `10.0.0-rc1`,lint 也只拦前导 `v`,无需特殊处理。

## 2. repack 脚本要兼容两代 API

10.x 改了两处,`tools/godot-cpp/repack.sh` 按**实际签名/实际文件**判定而不是按 tag 判定:

- `generate_bindings()` 多了 `interface_filepath` 参数(用 `inspect.signature` 探测);
- `gdextension_interface.h` **不再签入**,改为由 `gdextension/gdextension_interface.json` 生成到
  `gen/include/`(存在哪个就传哪个,与 cmake 的 `GODOTCPP_GDEXTENSION_INTERFACE_FILE` 同逻辑)。

改完后重跑 4.5.0 仍得到同一个 sha `b0c36e77…`,即向后兼容;10.0.0-rc1 连跑两次得
`aaafbf50d4b8469d610fdb2eb76c6f58d758dbabbc6b013f60464d99b20ceb6e`。

## 3. 描述符:两种布局取并集

10.x 的 `gen/src/` 多了一个直接位于其下的 `.cpp`(4.5 只有 `classes/`、`variant/` 两层),故 sources
加一条 `*/gen/src/*.cpp`。**匹配不到的 glob 会被跳过**,这是 compat.catch2 已经在用的做法(v2 走
`single_include`、v3 走 `src`,另一个空着)。`include_dirs` 同时保留 `*/gdextension` 与 `*/gen/include`,
因为那个 C ABI 头在两代里位置不同。

## 4. TYPED_METHOD_BIND —— MSVC ABI 上不是可选项

`godot-cpp-m` 的 Windows CI 暴露出来的:任何 `ClassDB::bind_method` 调用在 clang-cl 下直接编译失败

```
error: cannot reinterpret_cast from member pointer type 'double (TestSprite::*)() const'
       to member pointer type 'double (_gde_UnexistingClass::*)() const' of different size
```

`method_bind.hpp` 在 `#ifndef TYPED_METHOD_BIND` 时把成员指针 cast 成一个**前向声明**的
`_gde_UnexistingClass`;MSVC ABI 下成员指针的大小取决于该类的继承模型,不完整类型只能按最一般形式
假定,于是尺寸对不上、cast 非法。上游 `cmake/windows.cmake` 正是为此在 MSVC 下把
`TYPED_METHOD_BIND` 设为 **PUBLIC**。

本包把它挂在默认 feature 上(与 `GDEXTENSION` 同处),**不按平台分**:它是个改
`MethodBindT` 模板参数表的**头文件开关**,库与消费者必须一致,统一一个答案比按 OS 分更容易保证。
非 MSVC 侧的代价只是多一些模板实例化,无行为差异。上游一起设的 `WINDOWS_ENABLED` / `NOMINMAX`
**不需要** —— 在 4.5 与 10.x 的头文件和源码里都一次都没出现过。

## 5. 测试补了 GDCLASS + bind_method

两个成员(`godot-cpp`、`godot-cpp-v10`)都加了一个 `GDCLASS` 子类,带两个 `ClassDB::bind_method`
绑定和一次对 GDCLASS 生成物的 ODR-use。**这正是之前 Windows 绿得没有意义的原因**:老测试只碰纯数学,
根本没走到 bind_method,所以上面那个必现的编译错误一次都没被 CI 看见。断言是「编得过且链得上」——
不能真调用,ClassDB/StringName 都要走 `gdextension_interface_*` 函数指针。

`godot-cpp-v10` 另外断言 `GODOT_VERSION_MAJOR/MINOR == 4/6` 且 4.6 才有的 `EditorDock` 存在,
用来证明拿到的确实是 10.x 那套绑定而不是 4.5 的。

## 6. 本地验证

```
$ mcpp test -p godot-cpp        # 4.5.0
bind=1 vec2=1 vec3=1 basis=1 color=1 aabb=1 gen=1
 test result ok. 1 passed; 0 failed; finished in 59.15s

$ mcpp test -p godot-cpp-v10    # 10.0.0-rc1
version=1 bind=1 vec2=1 vec3=1 basis=1 color=1 aabb=1 gen=1
 test result ok. 1 passed; 0 failed; finished in 52.72s
```

另外 10.0.0-rc1 的 1075 个 TU 用 gcc 13 `-std=c++23 -fPIC` 全量编过,零失败。

## 7. 镜像

| 区域 | 地址 |
|---|---|
| GLOBAL | `https://github.com/xlings-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz` |
| CN | `https://gitcode.com/mcpp-res/godot-cpp/releases/download/10.0.0-rc1/godot-cpp-10.0.0-rc1.tar.gz` |

两侧下载回来核过 sha,与本地打包一致。
