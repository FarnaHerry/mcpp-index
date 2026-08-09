# compat.gmp:三平台支持 + 去掉 install()/外部构建系统

**日期**: 2026-08-10
**本仓**: `mcpplibs/mcpp-index`
**issue**: [#171 增加libgmp科学库](https://github.com/mcpplibs/mcpp-index/issues/171)
**前身**: [PR #194](https://github.com/mcpplibs/mcpp-index/pull/194) / `.agents/docs/2026-08-09-add-gmp-plan.md`(linux+macOS,install() 驱动 autotools;本文取代其 §2/§4/§5)

**结论**:`compat.gmp` 改为**纯描述符**形态 —— 没有 `install()` 钩子、没有 autotools/cmake/make、
没有任何构建期依赖,mcpp 用**消费端自己的工具链**直接编译 GMP 的可移植 C 内核,**linux / macOS /
windows 三平台同一条路径**。同时补上 GMP 的 C++ 绑定(`gmpxx` feature)。

---

## 1. 起点:合入版本的三个问题

### 1.1 gcc ≥ 15 上 `configure` 直接失败(真 bug,非 windows 相关)

GMP 6.3.0 的 `configure` 会**编译并运行**探针程序。其中 "long long reliability test 1" 里有

```c
void g(){}                       /* 旧式:参数未指定 */
...
g(i,d[i].src,d[i].n,got,d[i].want,9);   /* 传 6 个实参 */
```

C23 起 `()` 等价于 `(void)`,于是这是硬错误。gcc 15 起默认 `-std=gnu23`,configure 遂报:

```
configure: error: could not find a working compiler, see config.log for details
```

本机 gcc 16.1.0(`__STDC_VERSION__ 202311L`)实测复现。合入版本的 CI 之所以是绿的,只是因为
`ubuntu-latest` 还停在 gcc 13(默认 gnu17)。也就是说:**这个包在任何较新的发行版上装不上**,
而 CI 看不见。加 `CFLAGS=-std=gnu17` 可以修,但这只是把问题挪后一步 —— 根子在"必须有一个宿主
编译器,而且它的行为由宿主决定"。

### 1.2 install() 钩子被迫使用**宿主**编译器

configure 要运行探针、`make` 要运行 `gen-*` 构建工具,而链接到 xim glibc payload 的可执行文件
在钩子环境里跑不起来(PR #194 首轮 CI 实测)。于是钩子只能钉 `/usr/bin/gcc`,产物由一个**与消费端
无关**的编译器构建 —— 这也是 `gmpxx` 一直没法收录的原因(见 §3.4)。

### 1.3 windows 无解

GMP 上游没有 MSVC 构建路径。issue #171 的评论建议用
[skeeto/gmp-cmake](https://github.com/skeeto/gmp-cmake)(1 star,给 GMP 加了一个 `CMakeLists.txt`)。
这条路调研并**实现验证过**(见 §5.1),可行但要引入 `xim:cmake` + `xim:ninja` + `xim:llvm` 三个
构建期依赖,并依赖 clang 能在宿主上找到 MSVC SDK。

---

## 2. 方案选择:为什么最后不用 cmake

维护者提问"能不能直接用 `mcpp = {}` 描述"。把 GMP 的构建拆开后,答案是**能**,只卡在一处:

| 需求 | 描述符能否表达 |
|---|---|
| 源码清单(root 15 + `mpn/generic` 180 + mpz/mpf/mpq/printf/scanf/rand 302) | ✅ glob + `!` 取反 |
| 16 个 multiplex TU(`logops_n.c` 按 `-DOPERATION_*` 编 8 次等) | ✅ `generated_files` 生成 stub |
| 跨目录 quoted include(实测只缺 `gmp-impl.h`/`longlong.h`/`config.h` 等 24 处) | ✅ 一行转发头 |
| `-D__GMP_WITHIN_GMP`、`mp_clz_tab.c` 的单文件宏 | ✅ `cflags` + stub TU |
| **9 张生成表 + `gmp.h`** | ❌ 要**编译并运行**生成器 |

最后一项是唯一的硬约束,而它是 **(GMP_LIMB_BITS=64, GMP_NAIL_BITS=0) 的纯函数** —— 于是产物内联进
描述符(约 270 KB,其中 `trialdivtab.h` 109 KB)。参照物:仓内 `compat.ffmpeg.lua` 已是 335 KB。

换来的是:

- **零构建期依赖**。旧版 linux 带 `xim:make@latest`、macOS 什么都不带(这种按平台不对称的声明正是
  只会在红 job 里现形的那类问题);cmake 方案要再加三个。现在一个都不要。
- **windows 自然成立**。GMP 的通用 C 内核只需要一个 GCC 兼容编译器 —— 而本索引在 windows 上用的
  正是 clang++(MSVC ABI,见 `compat.sdl2.lua` 的注释),`__GNUC__`/`__builtin_alloca`/GNU inline asm
  全都在。
- **产物由消费端工具链编译**,ABI 天然一致,`gmpxx` 才成为可能(§3.4)。
- `install()` 钩子那一整套盲区(`log.error` 被吞、沙箱越界静默杀进程、`os.exec` 返回值、构建日志
  找不着)全部消失 —— 见 `.agents/docs/` 里 openssl/mysql 两次的记录。

代价:**只支持 64-bit limb**。`config.h` 里用 `#error` 硬失败,而不是默默套错表算错数。

---

## 3. 描述符设计

生成器:`tools/gmp/generate_descriptor.py`(`write` / `check` / `stage`)。它下载并校验上游 tarball、
用上游自己的 `gen-*.c` 生成 9 张表、再吐出整份 `pkgs/c/compat.gmp.lua`。与
`tools/godot-cpp/repack.sh` 同一形态:**内联的字节要附带能再造它的脚本**。`check` 会重新生成并对拍,
所以"绕过脚本手改描述符"是可见的。

### 3.1 源码清单

与配置过的 6.3.0 树的 `.lo` 集合逐条对拍(198 个 mpn 对象 = 180 个 `mpn/generic` + 18 个
wrapper/表)。上游**不编**的三个也照样排除:`udiv_w_sdiv`(只给有符号除但无无符号除的 CPU)、
`div_qr_1{n,u}_pi2`(只由本构建从不设置的 `HAVE_NATIVE_mpn_*` 选中)。

### 3.2 零 `-I`

上游靠 `-I<srcroot>` 让各目录都能 `#include "gmp-impl.h"`。mcpp 只用包的**公共** `include_dirs`
编包自己的源码,所以这里改成:每个源码目录放一个一行转发头,quoted include 按"包含它的文件所在
目录"解析。结果是整包编译**不带任何 `-I`**,而 `include_dirs` 只暴露 `gmp.h` + `gmpxx.h` ——
GMP 的私有头(尤其是 `config.h` 这种名字)不会进任何消费者的搜索路径。

### 3.3 `config.h` 刻意做成平台无关

每一项要么是 C89/C99 保证的,要么**刻意答"没有"**,于是三平台编出同一个库:

| 关掉的 | 代价 |
|---|---|
| `HAVE_UNISTD_H` | 只挡 `getpid()`,而它只是 `raise()` 缺失时的兜底;`raise` 是 C89 |
| `HAVE_LANGINFO_H` / `HAVE_NL_LANGINFO` | 找小数点改用 C89 的 `localeconv()`。在 `-std=c11` 下伸手要 POSIX 声明正是 implicit declaration 的来源 |
| `HAVE_QUAD_T` / `HAVE_SYS_TYPES_H` | 少一个 BSD 的 `%q`。glibc 只在 `__USE_MISC` 下声明 `quad_t`,`-std=c11` 会关掉它,所以答"有"本来就不可移植 |
| `HAVE_OBSTACK_VPRINTF` | 少 `gmp_obstack_printf`;glibc 专属且需要 `_GNU_SOURCE` |
| `HAVE_ALLOCA_H` | 多余:`gmp-impl.h` 在看 `<alloca.h>` 之前先用 `__builtin_alloca` |
| `HAVE_HIDDEN_ALIAS` | ELF 专属的链接期优化,在 Mach-O/PE 上是错的 |

**`NO_ASM` 特意不定义**:`longlong.h` 的 `umul_ppmm`/`add_ssaaaa`/`count_leading_zeros` 由编译器
预定义宏选通、够不着就自动退回 C。上游 `--disable-assembly` 会连它们一起关掉,乘除关键路径要慢
好几倍,却换不来任何可移植性。随之要定义 `LSYM_PREFIX`(Mach-O 是 `L`,ELF/COFF 是 `.L`)——
`MPN_IORD_U` 的 inline asm 要用;写错是响亮的汇编器错误,不是静默错算。

`__clz_tab` 单独用一个 wrapper TU 强制生成:开了 inline asm 之后 x86_64/arm64 用不到它,但上游的
NO_ASM 构建总是带这个符号,GMP 自己的测试套件就直接引用它 —— 129 字节买"导出符号是 stock
libgmp.a 的超集"。

`gmp.h` 里的 `_LONG_LONG_LIMB` 不再由构建期烧死,而是从 `<limits.h>`(`gmp-h.in` 在那一行之前已经
包含)判定。这样头文件和库**不可能**对 `mp_limb_t` 的宽度产生分歧 —— 一个 LP64 烧死的头文件跑到
LLP64 的 windows 上正是这类错误。

### 3.4 `gmpxx` feature

GMP 的 C++ 绑定(`mpz_class`/`mpq_class`/`mpf_class`,RAII + 运算符重载 + iostream 运算符)。

**必须由消费端工具链编译**:这些符号的名字里带 `std::ostream`/`std::string`,即 C++ 标准库的 ABI。
在钩子里用宿主 g++ 编一份 `libgmpxx.a`,在 libstdc++ 腿上能链、在每一条 libc++ 腿上都是 undefined
reference。所以做成 feature:11 个 `cxx/*.cc` 合并成**一个** TU(`mcpp_gmpxx.cc`)。

合并的理由是 **feature 表只能门控 sources、带不了 define** —— `__GMP_WITHIN_GMPXX` 必须写在文件里,
所以无论如何都要一个 wrapper,一个比十一个简单。**不是**因为 basename 撞名:那是 2026.8.3.4 之前的
情况,现版本对象路径无条件嵌套,本包自己就有三个 `add.c` 各自落在
`obj/compat_gmp/gmp-6.3.0/{mpf,mpn/generic,mpz}/add.o`(实测)。

合并踩到一个真坑并已修:`gmp.h` 用"`<stdio.h>`/`<stdarg.h>` 是否已被看见"来决定要不要声明
`FILE*`/`va_list` 入口(`_GMP_H_HAVE_FILE` / `_GMP_H_HAVE_VA_LIST`),`gmp-impl.h` 又拿同一个答案
去 gate `struct gmp_asprintf_t`。分开编时 `osdoprnti.cc` 自己包含 `<stdarg.h>` 所以没事;合并后是
**第一个** `.cc` 先摸到 `gmp-impl.h`,答案被 include guard 冻结给整个 TU —— 表现为 libstdc++ 上
`aggregate gmp_asprintf_t has incomplete type`,而在 `<iostream>` 恰好拖进 `<cstdarg>` 的标准库上
一切正常。解法是 wrapper TU 顶部先包含那两个头。

### 3.5 `gmp.h` 的一处补丁:MSVC ABI 上的 `__GMP_EXTERN_INLINE`

除 configure 的 8 处 substitute 外,生成的 `gmp.h` 还打了**一个**补丁,由首轮 windows CI 抓出来。

`gmp.h` 里约十二个小函数(`mpz_get_ui`、`mpn_add`、`mpn_cmp` …)有两份定义:头里一份走
`__GMP_EXTERN_INLINE`,库里一份由带 `-D__GMP_FORCE_<fn>` 的那个 TU 提供
(`mpz/get_ui.c`、`mpn/generic/add.c` …;那里 gmp.h 会**手工抑制** `__GMP_EXTERN_INLINE`,
所以那份一定是普通外部定义)。这套分工成立的前提是:**头里那份永远不能变成独立符号**。
GCC 分支用 `extern __inline__ __attribute__ ((__gnu_inline__))` 保证了这点。

`_MSC_VER` 分支给的是裸 `__inline`,MS 语义下**每个**包含 gmp.h 的 TU 都会发射一份外部定义;
而且它是整串厂商分支里**唯一没有** `! defined (__GMP_EXTERN_INLINE)` 守卫的(SunPro / SCO 两条都有)。
本索引 windows 用的 clang 打 MSVC ABI **定义 `_MSC_VER` 但不定义 `__GNUC__`**(实测:先按"两个都定义"
的假设只补 `! defined (__GNUC__)` 守卫,CI 结果一字未变),于是落到 MSVC 那条,链接死在十个重复符号:

```
lld-link: error: duplicate symbol: __gmpz_get_ui
>>> defined at gmp.h:1800   obj/.../mpz/pprime_p.o
>>> defined at              obj/.../mpz/get_ui.o
```

**解法是把这条分支改成 `static __inline`** —— 正是 GMP 自己给另外两个"extern inline 会漏出全局符号"
的编译器(DEC C、SCO OpenUNIX)的答案。关键耦合在于 `__GMP_INLINE_PROTOTYPES`:GCC 分支会把它设为 1
(于是发 extern 原型,与 gnu_inline 定义相容),而 MSVC 分支不设 → 后面默认 0 → **不发 extern 原型**,
`static` 定义才不会和它打架。第一次本地模拟只换 `__GMP_EXTERN_INLINE` 不动 `__GMP_INLINE_PROTOTYPES`,
立刻得到 `static declaration of '__gmpz_abs' follows non-static declaration` —— 忠实复现 MSVC 分支
(两者都换)之后本地实测:非 FORCE TU 里那些符号**一个都不发射**(全内联),FORCE TU 发射唯一的全局,
六个 TU 合并链接无重复。守卫照旧补上,这样将来若有既 GNU 兼容又设 `_MSC_VER` 的编译器,仍走
gnu_inline 那条。

生成器里对补丁点做了"必须恰好命中一次"的断言,上游改动了这段会直接报错而不是静默漏打。

值得记一笔的是**首轮 windows CI 的 516 个 TU 全部编译通过**,只挂在链接 —— 也就是说
`config.h`/`gmp.h`/内联表在 LLP64 + MSVC ABI 上本来就是对的。另外由于 windows 上 `__GNUC__` 未定义,
`config.h` 里 `HAVE_ATTRIBUTE_*` 与 `longlong.h`/`MPN_IORD_U` 的 inline asm 在那边自动全关,
走纯 C 路径 —— 正确但比 linux/macOS 慢,这是可接受的取舍。

### 3.6 `-fPIC`

`linux`/`macosx` 加 `-fPIC`(`cflags` + `cxxflags`)。静态库经常被链进消费者的 `.so`;而且 clang++
默认 PIE 时链接本包就直接
`relocation R_X86_64_32 against '__gmp_digit_value_tab' ... recompile with -fPIC` ——
本地跑 gmpxx 测试时抓到的。windows 不加:PE/COFF 没这个区分。

顺带修掉一处旧描述符的错误:`linux` 的 `-lm`。实测 `libgmp.a` 的未定义符号里**没有任何 libm 函数**
(autotools 产物同样没有)—— `mpf_sqrt` 是 GMP 自己用 mpn 实现的。

---

## 4. 测试成员

- `tests/examples/gmp` —— 默认(C API)构建,**三平台**,不再有 `cfg` 门控和 no-op main。
  每个被编译的源码目录各有一组断言:mpz(100!、`7^1234 mod 2^127-1`、gcd/lcm、
  secp256k1 素数的 16 进制往返 + `probab_prime_p`(会走到 `mpn_trialdiv` → 内联的
  `trialdivtab.h`)、import/export(限位序))、mpq、mpf、`gmp_snprintf`、`gmp_sscanf`、
  `gmp_randinit_mt`。参考值由 Python 独立预算。另外断言 `GMP_LIMB_BITS == 64` 且
  `mp_bits_per_limb` 与之一致 —— 头与库对 limb 宽度分歧时,其余断言可能照样过。
- `tests/examples/gmp-gmpxx` —— `gmpxx` feature 路径,**三平台**。feature 的负向检查是结构性的
  (同 `catch2-main`):`gmpxx.h` 恒发布,但 `operator<<` 的实现是 feature;关掉就是
  `undefined reference`,不会静默通过。本地实测:把该成员的源码在**不带** `mcpp_gmpxx.cc` 的情况下
  链接,得到 4 处 undefined reference —— `operator<<(std::ostream&, __mpz_struct const*)`、
  `__mpq_struct const*`、`__mpf_struct const*` 与 `operator>>(std::istream&, __mpz_struct*)`。

`mpz_get_str` 的结果改用 `mp_get_memory_functions` 报告的 free 归还,而不是 `std::free` ——
那才是文档写的用法,也是消费者换了自定义分配器之后唯一还正确的写法。

---

## 5. 验证证据

### 5.1 与上游构建对拍

同一 tarball 的 `--disable-assembly` autotools 构建作为参照:

- **导出符号 598 个,集合完全一致**(既不少也不多)。
- **上游自带测试套件**(`make check`,178 个测试程序)对本产物运行:
  `TOTAL=178 PASS=177 FAIL=0 ERROR=0 SKIP=1` —— gcc 与 clang 各一遍。

(cmake 路线也做过同样的对拍并同样通过;它作为备选被放弃的原因见 §2,不是因为不工作。)

### 5.2 生成器可复现

`python3 tools/gmp/generate_descriptor.py check` 从零重新下载 tarball、重跑上游生成器、重新生成
整份描述符,与仓内文件**逐字节一致**。

### 5.3 mcpp 端到端(CI 同版本 mcpp 2026.8.8.2)

| 腿 | gmp | gmp-gmpxx |
|---|---|---|
| linux gcc@16.1.0(冷启动,含工具链下载) | ok,68.3s | ok,5.5s |
| linux llvm@22.1.8(libc++/lld) | ok,4.2s | ok,5.2s |

对象数:`gmp` 517 个 `.o`(516 GMP TU + 1 测试 TU),`gmp-gmpxx` 518 个 —— feature 恰好多一个 TU。
`readelf -p .comment` 确认 llvm 腿的依赖对象确实由 clang 22.1.8 编、LLD 链接(不是缓存串味)。
另注意 mcpp 的默认工具链就是 **gcc 16.1.0**,即 §1.1 里让 autotools 崩掉的那个 C23 编译器。

### 5.4 lint

`validate.yml` 的 8 项本地全过(lua 语法 / 必填字段 / 无前导 v / `check_mirror_urls` /
`check_package_name` / c++fly / `check_cross_package_refs` / `check_platform_version_parity`),
`mcpp xpkg parse` 正常输出三平台 6.3.0、49 sources、1 include、59 generated。

---

## 6. 已知边界与后续

- **32-bit**:不支持,`config.h` 与 `gmp.h` 各有一处 `#error`。要支持得再内联一套 limb=32 的表,
  或者退回运行生成器。索引现有的目标平台全是 64-bit。
- **手写汇编内核**:不编。选中它们需要 m4 + CPU 匹配,正是本包要摆脱的宿主依赖。通用 C 内核 +
  `longlong.h` inline asm 已经拿到大部分收益。
- **CN 镜像**:仍是纯字符串上游 url。补镜像需要往 `mcpp-res` 上传同字节 tarball(维护者动作),
  之后把 url 改成 `{ GLOBAL=…, CN=… }`,sha256 不变。
- **少掉的次要 API**:`gmp_obstack_printf` / `gmp_obstack_vprintf`(glibc 专属)与 `%q` 长度修饰符
  (BSD)。理由见 §3.3;需要时可按平台条件放开。
- **`compat.openssl` 同病**:它的 windows 也卡在"钩子里跑外部构建系统"。本包的形态(生成产物内联 +
  转发头 + 零 `-I`)是否能推广过去,值得单独评估。
