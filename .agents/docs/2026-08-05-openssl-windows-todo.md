# TODO:compat.openssl 的 Windows 支持(2026-08-05,未完成)

**状态:未完成。** 尝试见 PR [#150](https://github.com/mcpplibs/mcpp-index/pull/150)(已转草稿,**不可按现状合并**)。
本文记录已经查实的东西,让接手的人不必重走这六轮。

## 1. 为什么需要它

`compat.openssl` 是索引里唯一一个仍标着 "windows deferred" 的关键包,而它挡住的不止自己:

- `mcpplibs.grpc`(#151)因此只能是 linux + macOS。gRPC 的 secure 构建去不掉 TLS,
  所以它的平台面**只能等于** `compat.openssl` 的平台面。Windows 上依赖解析在编译任何 TU 之前就失败:
  ```
  error: xlings install_packages failed for 'compat.openssl@3.5.1'
  E_NOT_FOUND: package 'compat:openssl@3.5.1' not found
  ```
- `compat.curl`、`tests/examples/asio-ssl` 等也都受同一约束。

`grpc-m` 已经把 Windows 所需的编译/链接选项(`_WIN32_WINNT`、`NOMINMAX`、`ws2_32`/`crypt32`/`iphlpapi`)
**预置好了**,本条一旦解决,只需把 `grpc-m` 的 Windows CI leg 与 `pkgs/g/grpc.lua` 的 windows xpm 块加回去。

## 2. 构建形态是确定的(已核实,不必再查)

OpenSSL 3.5.1 在 x64 Windows 上**只有一条路**:

| 事实 | 出处 |
|---|---|
| x64 目标只有 `VC-WIN64A` | `Configurations/10-main.conf` |
| clang-cl 配置只覆盖 **ARM**(`VC-WIN64-CLANGASM-ARM`) | `Configurations/50-win-clang-cl.conf` |
| `VC-WIN64A` 的 `build_scheme` 是 `VC-common` → **NMAKE** makefile,GNU make 驱动不了 | 同上 |

由此带来两个**宿主要求**,包本身无法提供:

- **perl** —— `xim:perl` 只有 linux/macosx 两个 xpm 块,其注释写着
  "windows — not shipped. The Windows answer is Strawberry Perl"。
  ⚠️ **不要**试图在 windows 块里写 `deps = { "xim:perl@latest" }`:声明一个在该平台没有块的依赖会
  `E_INVALID_INPUT: package ... not found`,**在 install() 运行之前**就失败(compat.openssl 自己的注释里
  已记录过 macosx 上 `xim:make` 的同类教训)。
- **Visual Studio C++ 工具集**(为了 nmake)。`xim:make` 是 GNU make 且只有 linux。
  只有 `xim:nasm` 有 windows 构建。

## 3. 卡住的地方(这是接手的重点)

CI 上 **vswhere 能正确找到工具集**:

```
[bat] vswhere=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe
[bat] vspath=C:\Program Files\Microsoft Visual Studio\18\Enterprise
```

**但只要以任何形式运行 vcvars,整条进程链就消失**,后面一行都执行不到。三种写法都试过,行为一致:

| 写法 | 结果 |
|---|---|
| 批处理里 `call "%VCVARS%"` | vcvars 打印 `Environment initialized for: 'x64'` 后脚本消失,无 RESULT |
| 把构建移进 `cmd /c <inner.bat>` 子进程 | 子进程与**外层脚本**一起消失,外层连 `RESULT=%errorlevel%` 都写不下 |
| `cmd /c ""%VCVARS%" & set" > env.txt`(标准的环境 dump 手法) | 同样停在这一行 |

即:这已经**不像是描述符里的代码错误**,而是 xlings 执行子进程的方式与 VS 环境脚本之间的交互。

## 4. 下一步建议

**先做一次手工验证,再写代码。** 在一台 Windows 机器上,于 `mcpp` 的沙箱环境内手工跑一遍
`perl Configure VC-WIN64A ... && nmake`,确认它在该环境下究竟能否完成。这一次手工验证能省掉十几轮
CI 盲调(本次六轮全部是盲调,每轮约 10 分钟)。

若确认 vcvars 在该环境下不可用,剩下的路是**绕开所有 VS 脚本**:用 vswhere 拿到 `installationPath` 后
自行推导并设置 `INCLUDE` / `LIB` / `PATH`(即 vcvars 内部所做的事)——
`<VS>\VC\Tools\MSVC\<ver>\bin\Hostx64\x64` 等。可行但脆(Windows SDK 版本发现是主要麻烦),
所以更值得先确认 nmake 路线整体成立。

## 5. 顺带查实的通用结论(已写入 PR #150 的提交历史)

这些不限于 openssl,任何 `install()` 钩子都适用,建议写新钩子时**先建可观测性再写逻辑**:

1. **`log.error` 的内容到不了 CI 日志**,失败只表现为裸的 `E_INTERNAL: [openssl] failed:`。
   唯一活得下来的通道是自建日志文件;`validate.yml` 的 "Dump install() build logs on failure"
   步骤 `find` 的是 `mcpp_*_build.log`,文件名要匹配这个模式。
2. **xlings 沙箱只暴露 xmake Lua API 的子集,调用子集外的函数会让 hook 静默终止**——不报错、不回溯,
   日志停在上一行。实测不可用:`os.curdir()`、`path.absolute()`。
3. **`os.exec` 的返回值不可信**:一个什么都没做、也无输出的批处理返回了 `ok=true`。
   应让被调脚本自己往日志里写 `RESULT=<code>`,由 Lua 读日志判定。
4. **日志必须在最早时刻创建并逐步追加**。第一版把日志留给子进程写,而失败发生在那之前,
   CI 只打出 `no install() build logs found` —— 零信息。
5. `io.writefile` **按字节原样写**,不会替你转换换行;而 cmd 按**文件偏移**逐行读批处理、
   其记账假定 CRLF。

## 6. 与之相关的既有约束

- 合并 PR #150 的现状会让 Windows **退化**:合入前 `tests/examples/openssl` 在 windows 上是 no-op
  `main()`(绿),合入后会真去构建并失败。因此该 PR 已转草稿。
- 本 TODO 完成后要一并恢复的:`grpc-m` 的 windows CI leg、`pkgs/g/grpc.lua` 的 windows xpm 块、
  `tests/examples/grpc-module` 的 windows 门控。
