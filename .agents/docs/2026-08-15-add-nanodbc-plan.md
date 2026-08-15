# 新增 compat.nanodbc / compat.unixodbc

> 日期：2026-08-15 · 验证工具链：mcpp `2026.8.15.1`(本地最新)+ parse 底线 mcpp `2026.8.10.3`(与 `validate.yml` 的 `MCPP_VERSION` 对齐)+ clang `22.1.8`

## 1. 动机

nanodbc 是原生 C ODBC API 的薄 C++ 封装(RAII connection/statement/result),上游已冻结
(v2.14.0,2022-03-23,此后无提交),正适合一次性收录:没有需要追踪的版本演进。ODBC
仍是接入大量既有数据源(Access、SQL Server、各类国产库)的通用门,索引里此前没有任何
ODBC 相关的包。

## 2. 形态判定

| 包 | 形态 | 判定依据 |
|---|---|---|
| `compat.nanodbc` | **A**(C++ 源码 compat) | 一头一实现:`nanodbc/nanodbc.h` + `nanodbc/nanodbc.cpp` |
| `compat.unixodbc` | **E 叠 A**(冻结 config + 纯 C 源码) | unixODBC 2.3.14 dist tarball,DM + odbcinst + ini/log/lst + libltdl |

版本与 sha256(均双次下载复核):

- nanodbc `2.14.0` = `56228372042b689beccd96b0ac3476643ea85b3f57b3f23fb11ca4314e68b9a5`
  (GitHub tag 归档)
- unixODBC `2.3.14` = `4e2814de3e01fc30b0b9f75e83bb5aba91ab0384ee951286504bb70205524771`
  (unixodbc.org **dist** tarball;GitHub 镜像停在 2.3.12 且归档内没有 bootstrap 过的
  `libltdl/`,只有 dist tarball 自带)

CN 镜像:本贡献者无 `mcpp-res` 写权限,两个包均按 SOP 回退为纯字符串 `url = "<GLOBAL>"`
(lint 允许,先例 `compat.libmysqlclient`),由维护者后续补镜像。

## 3. 关键设计:驱动管理器从哪来

nanodbc 只是封装,真正干活的是平台的 ODBC driver manager。三平台答案不同:

| 平台 | 管理器 | 依据 |
|---|---|---|
| windows | SDK 自带 odbc32(`-lodbc32`) | 始终在 |
| macOS | OS 自带 iODBC(`-liodbc`) | 始终在 |
| linux | **`compat.unixodbc` 源码构建** | 见下 |

linux 最初按"系统依赖 + `-lodbc`"实现,编译链接都过了,但 **mcpp 的运行时闭包检查拒绝**
产物:

```
runtime closure for .../error_path cannot be satisfied: libodbc.so.2 not found on the
search path this artifact will actually use. Its PT_INTERP is a private loader, so the
host's /usr/lib is NOT consulted
```

这与 mcpp#352(libGL)是同一堵墙。宿主符号链接农场(glx-runtime 旧方案)已被生态放弃,
xim 侧没有 unixodbc 包,于是只剩与 conan/vcpkg 相同的结论:**linux 的 ODBC 管理器必须
从源码静态构建**。静态链接让消费者二进制不带 `libodbc.so.2` NEEDED,闭包检查天然通过,
也不再有宿主 glibc 版本错配的风险。

## 4. compat.unixodbc 的三个非常规点

### 4.1 无 libtool 复刻 ltdl 的 dlopen loader 注册

DM 通过 libltdl `dlopen` 数据库驱动。静态构建下 libtool 用 `-dlpreopen` 生成符号表完成
注册;没有 libtool 时需要手工复刻。从 libtool 产物的重定位记录还原出它生成的表:

```c
const lt_dlsymlist lt_libltdlc_LTX_preloaded_symbols[] = {
    { "libltdlc", 0 }, { "dlopen.a", 0 },
    { "dlopen_LTX_get_vtable", &dlopen_LTX_get_vtable }, { 0, 0 }
};
```

配合 `-DLTDLOPEN=libltdlc`(ltdl.c 经 `LT_CONC3` 拼出表名)与编译进包的
`loaders/dlopen.c`,注册链路与 libtool 构建完全一致。**对拍验证**:同一 tarball 的
libtool 静态构建 vs 本描述符直编 —— 句柄分配、驱动枚举、IM002 错误路径、以及
`lt_dlopen("libm.so.6")` + `lt_dlsym("sin")` 全链路行为一致。

### 4.2 两份 config.h 合并为一份

DM 的 122 个 TU 无条件 `#include <config.h>`(顶层 configure 产物);ltdl 的 TU 需要的是
libltdl 子目录**另一份** configure 产物。上游用 `-DLT_CONFIG_H='<config.h>'` 区分,但带
引号的 define 无法穿过 描述符 → mcpp → 命令行 的管道(实测 `#include LT_CONFIG_H` 展开成
无引号标识符直接报 `expected "FILENAME"`)。两份 config 的冲突宏只有 PACKAGE*/VERSION
一组,而 ltdl 源码从不读它们(已逐一核实),于是把 ltdl 独有的宏并入顶层 config.h,一个
文件喂所有 TU。

### 4.3 单目标 `odbc`

上游分 libodbc / libodbcinst 两库,但 libodbc.a 本来就吸收 odbcinst/ini/log/lst/ltdl
全部 convenience 对象(实测 241 个 .o)。单目标 `odbc` 就是这个符号集,驱动安装器要的
`SQLInstallDriverEx` 也在其中。

## 5. compat.nanodbc 的两个冻结上游修复

### 5.1 libc++ 没有 `char_traits<unsigned char>`

nanodbc.cpp 有 4 处 `std::char_traits<SQLCHAR>::length()`(SQLCHAR = unsigned char)。
C++23 起主模板只剩声明,libc++ 从未带过该特化 → 硬错误。修复是
`mcpp_nanodbc_char_traits.h`:一份完整的显式特化(标准为用户字符类型预留的定制点),
以 `_LIBCPP_VERSION` 为界,libstdc++/MSVC STL 自带特化不受影响。经 `-include` 强制包含,
**仅作用于本包编译**,消费者不见。

两个已踩的坑,留在描述符注释里:

- `-include` 必须走 `cxxflags`(本包无 C TU,`cflags` 到不了 `.cpp`);
- shim 内**先 `#include <string>` 再判 `_LIBCPP_VERSION`** —— force-include 跑在一切头
  文件之前,此时宏尚未定义。

### 5.2 SQL state 被截掉末字符(上游 off-by-one,不修,测试如实断言)

`recent_error()`(nanodbc.cpp:411-420)复制 `size(sql_state) - 1` 个字符,DM 报的
"IM002" 经 nanodbc 变成 "IM00"。上游冻结,这是该版本的**真实行为**;测试断言
`state()` 是 "IM002" 的非空前缀,并断言 `what()` 含 DM 原文
"Data source name not found"。

### 5.3 GCC 拒绝重复的显式实例化定义(linux default 腿,首轮 CI 红)

C++17 起 string_view 支持默认开,`NANODBC_INSTANTIATE_BIND_STRINGS(std::string)` 与
`(std::string_view)` 展开出**完全相同**的显式实例化定义(`value_type` 都是 char;u16
对同理)。同一 TU、同一特化,语义无碍,但标准称其为 ill-formed,GCC(16 实测,c++23
模式)直接报错,clang 默认静默接受 —— 这正是首轮 CI 只有 linux **default**(GCC)腿红、
llvm 腿绿的原因(linux llvm 腿的存在价值实锤)。修复是 linux `cxxflags` 加
`-fpermissive`:GCC 官方为这类诊断留的降级开关,clang 静默忽略该 flag,一条声明同时
喂两条腿。

### 5.4 generated_files 的写法边界(评审意见引出)

shim 最初是单行 `\n` 转义串,评审认为不可读。改写时确认了两条边界:

- Lua 的 `..` 拼接**不被 mcpp 段解析器支持**(`malformed mcpp segment near key
  'string'`),尽管它能过 Lua 语法检查 —— 段解析器 ≠ Lua 解析器;
- `[==[ ]==]` 长括号在当前解析器下可用(compat.ffmpeg / compat.sdl2 的既有先例,
  且最新 mcpp 与钉住的 2026.8.10.3 均实测通过),shim 最终采用此形式。

## 6. 测试设计

`tests/examples/nanodbc`:**无数据库、无驱动**断言。CI 两样都没有,但有管理器本身 —
— 那正是 nanodbc 封装的层。连接不存在的 DSN 必然抛 `nanodbc::database_error`,该路径
走穿句柄分配、DSN 查找、诊断格式化;IM002 前缀 + DM 原文证明诊断来自真实管理器而非
空壳。成员按 `[target.'cfg(linux)']` 门控(windows/macOS 的 DM 链路在本索引 CI 不可验),
非 linux 编译为 no-op main。

## 7. 验证结论

- `mcpp test -p nanodbc`(冷构建,无任何宿主 include/lib 路径注入):**1 passed / 0 failed**
  —— 全链路自包含,不依赖宿主 unixODBC。
- lint:`check_mirror_urls` / `check_package_name`(两新包)、`check_cross_package_refs` /
  `check_platform_version_parity`(全部描述符)均过。
- `mcpp xpkg parse`:全部 98 个描述符在**钉住的 2026.8.10.3** 下通过(底线先行,语法其后)。
- unixODBC 直编与 libtool 构建对拍:行为一致(见 §4.1)。
- sha256 双次下载复核一致(两包)。

## 8. 后续可做(本 PR 不做)

- CN 镜像:待维护者在 `mcpp-res` 建仓后,把两处纯字符串 url 换成 `{ GLOBAL, CN }` 表。
- windows/macOS 成员测试:当前描述符已声明 `-lodbc32` / `-liodbc`,但索引 CI 不可验;
  若在真实 SDK 环境验证过,可放开成员门控。
- SQLite ODBC 驱动打包:有了它就能做"真连库"的正向测试,而不只是错误路径。
