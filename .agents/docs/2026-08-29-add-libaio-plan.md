# 收录 libaio 0.3.113(compat.libaio)

日期:2026-08-29 · PR 分支:`feat/add-libaio` · 状态:本地验证通过

## 1. 来源与形态判定

libaio 属于来源 (a):第三方上游库,上游不提供 mcpp 支持。

- 上游:<https://pagure.io/libaio>(Jeff Moyer 维护;GitHub 上只有零散的 fork,无官方镜像)。
- 最新版本:`git ls-remote --tags` 排序后最大的 release tag 是 **`libaio-0.3.113`**。
  注意 tag 命名有两代:老的是 `libaio.0-3-107.1` 这种点划混排形式,`sort -V` 会把它们排在
  `libaio-0.3.113` 之后 —— 只看 `tail` 会误判成 0.3.107。
- License:LGPL-2.1-or-later(`COPYING` 是 LGPL 2.1;各源文件头写 "version 2 of the License,
  or (at your option) any later version")。
- 源码布局:`libaio-0.3.113/` 包一层,库源码全在 `src/`(12 个 TU + 1 个公开头 + 若干私有头),
  另有 `harness/`(测试)与 `man/`。无 configure、无生成步骤、无 submodule、无符号链接。

**形态 = A(C 源码 compat)**,叠加一处「公开头从源码目录里择出来」的处理。

## 2. 版本与下载源

`sha256 = 2c44d1c5fd0d43752287c9ae1eb9c023f04ef848ea8d4aafa46e9aedb678200b`(49980 字节,连算两次一致)。

GLOBAL 用 `https://releases.pagure.org/libaio/libaio-0.3.113.tar.gz`,而**不是** pagure 的
tag 归档 `https://pagure.io/libaio/archive/…`:后者当场返回 404,且即便可用,pagure 与 GitLab 同类,
归档是即时生成的,sha 会漂移。releases.pagure.org 上的是固定发布文件。

## 3. CN 镜像

已在 gitcode 建 `mcpp-res/libaio`,seed 一个 README 后发 `0.3.113` release,上传与 GLOBAL **同一份**
tarball:

    https://gitcode.com/mcpp-res/libaio/releases/download/0.3.113/libaio-0.3.113.tar.gz

回拉校验 sha256 与 GLOBAL 逐字节一致,故描述符里 GLOBAL/CN 共用一个 `sha256`。

## 4. 三个实现决策

### 4.1 只暴露一个公开头(`generated_files` 转发头)

上游 `make install` 只装 `libaio.h` 一个头,但 tarball 把它放在 `src/` 里,与私有头并列。若直接写
`include_dirs = { "*/src" }`,消费者的 include 路径上就会多出 `syscall.h`、`aio_ring.h`、
`vsys_def.h`、`syscall-*.h`。其中 **`syscall.h` 会遮蔽 glibc 的同名头** —— 这是实打实的危害,不是洁癖。

做法沿用 compat.gmp 的先例:`generated_files` 写一个

    libaio-0.3.113/mcpp/include/libaio.h  →  #include "../../src/libaio.h"

`include_dirs` 只指这一个目录。为什么这样够用:

- 包自身的 `.c` 写 `#include <libaio.h>`,命中转发头 → 命中真头文件;
- 它们的 `#include "syscall.h"` / `"aio_ring.h"` 是引号形式,按「包含者所在目录优先」解析,
  就在 `src/` 里,**不需要任何指向 `src/` 的 `-I`**;
- `compat-0_1.c` 用引号形式 `#include "libaio.h"`,同理命中 `src/` 里的真头。

已实测:12 个 TU 全部在只有 `-I .../mcpp/include` 的情况下零警告编过,且消费者侧
`#include <syscall.h>` 拿到的确实是 glibc 的(`SYS_read == 0`)。

### 4.2 `c_standard = "gnu11"` 是个陷阱 —— 用 `-D_GNU_SOURCE`

`-std=c11` 会定义 `__STRICT_ANSI__`,glibc 随之关掉 `_DEFAULT_SOURCE`,于是:

- `<unistd.h>` 不再声明 `syscall()`(每个 TU 都经 `syscall.h` 的 `_body_io_syscall` 用到它);
- `sigset_t` 不可见,连**公开头**都在 `io_pgetevents(…, sigset_t *sigmask)` 处解析失败。

上游没这个问题,因为它的 Makefile 用编译器默认的 gnu 模式。

第一版描述符写了 `c_standard = "gnu11"`,`mcpp xpkg parse` 通过,`mcpp test` 却报出与 c11 完全相同的
两个错误。查产出的 `compile_commands.json`:**mcpp 2026.8.27.2 收下了 `gnu11` 这个字符串,仍然发
`-std=c11`**,静默降级、无任何提示。

真正生效的写法是 `cflags = { "-D_GNU_SOURCE" }`。已实测 gcc 16.1.0 与 clang 22.1.8 下
`-std=c11 -D_GNU_SOURCE -Wall` 12 个 TU 全部零警告。

> 附带影响:`pkgs/c/compat.freetype.lua` 也声明了 `c_standard = "gnu11"`,同样拿不到 gnu 模式。
> 它目前 CI 是绿的(说明 freetype 不依赖 gnu 模式),因此本 PR 不动它,但这里记一笔。

顺带说明另外两处 GNU 扩展为何不用管:`syscall.h` 的具名可变参数宏
`_body_io_syscall(sname, args...)`,以及 `raw_syscall.c` 在非 ia64 架构上整个文件只剩一个文件作用域
的 `;` —— gcc/clang 只在 `-pedantic` 下才诊断这两者。

### 4.3 符号版本(symver)

`io_cancel.c` / `io_getevents.c` / `io_queue_wait.c` 的函数真名是 `io_getevents_0_4` 之类,短名靠
`.symver … @@LIBAIO_0.4` 发布;`compat-0_1.c` 另加三个 `@LIBAIO_0.1` 的老 ABI 别名。

- **链可执行文件**:`@@`(默认版本)会同时定义基名,已在 ld.bfd 与 lld 上各实测通过,
  最终测试二进制里 `nm` 可见 `io_getevents_0_4` 与 `io_getevents@@LIBAIO_0.4` 同址。
  `kind = "lib"` 的对象并进消费者,走的正是这条路径。
- **直接拿这些对象建 `.so`**:失败,`undefined version LIBAIO_0.4`,需要上游的
  `src/libaio.map` 版本脚本。这一点与上游自己的 `libaio.a` 完全相同,不是本描述符引入的。

**为什么不删 `compat-0_1.c`**:一开始考虑过删掉它以消除 `@LIBAIO_0.1`。实测证明没用 ——
把它去掉后 `.so` 链接仍然因三个 `@@LIBAIO_0.4` 失败。既然删了不解决问题,又会让对象集与上游
`libaio.a` 不一致,就保留。

## 5. feature 评估:无

判据是「是否存在额外的、可门控的**可编译源码**」。libaio 没有:

- `src/struct_offsets.c` 是构建期断言(其注释明说 "this code does not end up in the compiled object
  files"),上游也是与库分开编的 —— 不编,也不该做成 feature;
- `harness/` 是测试套件,自带 `main()`。mcpp 的 lib 目标对象是**全量入链**的(非 archive 懒选),
  包里带 `main()` 必与消费者的 `main()` 冲突,所以它连做成 feature 的资格都没有。

故 `features` 整个不声明。

## 6. 测试成员 `tests/examples/libaio`

依赖按 `[target.'cfg(linux)'.dependencies.compat]` 门控,测试源码在非 Linux 上编成 no-op `main()`
(compat.wil 的镜像写法)。断言全部是真实内核 AIO 行为,不 mock:

1. 复刻上游 `struct_offsets.c` 的三条 `static_assert`(本包不编那个 TU,把检查搬到这里);
2. `io_prep_pwrite` 填出的 iocb 字段(opcode / fildes / buf / nbytes / offset);
3. 写路径:提交 → 收事件 → `res == 512`,再用 `pread` 确认字节真的落盘;
4. 读路径:在 offset 512 处异步读回并逐字节比对;
5. 一次提交两个 iocb,用 `data` cookie 区分 —— `data` 是 padded 结构的首成员,
   PADDEDptr 选错在这里就会现形;
6. 错误契约:对已关闭的 fd 提交,断言返回值是 `-EBADF` 且 `errno` 未被改动
   (libaio 不用 errno,这是它与周围 POSIX 调用相反的约定);
7. `io_cancel` 对已完成请求返回负 errno —— 目的是让这个只以 `@@LIBAIO_0.4` 存在的符号真的被链接;
8. `io_queue_init` / `io_set_callback` / `io_queue_run` / `io_queue_release` 的回调层。

## 7. 验证结论(mcpp 2026.8.27.2,与 CI 同版本)

- `mcpp xpkg parse pkgs/c/compat.libaio.lua` → `parse OK`,`sources 12`、`generated 312 bytes`。
- 冷跑(先删 `target/` 与 `.mcpp/`)`mcpp test -p libaio` → `test result ok. 1 passed; 0 failed`。
- **确认包真的被编译**:`obj/compat_libaio/libaio-0.3.113/src/` 下 12 个 `.o`,
  且最终二进制里能看到 `io_setup`/`io_submit`/`io_getevents@@LIBAIO_0.4` 等符号。
- **确认断言可失败**:把一条 `memcmp` 断言反过来,`mcpp test` 报 `FAIL (exit 134)`;改回后重新冷跑仍绿。
- 五个本地 lint(syntax / mirror-urls / package-name / platform-parity / duplicate-versions /
  cross-package-refs)全部通过。

## 8. 描述符解析踩到的一个坑

`generated_files` 的值**不能用 Lua 的 `..` 拼接**。mcpp 的描述符解析器不执行 Lua,只读字面量,
遇到 `..` 会报 `malformed mcpp segment near key '<下一个 token>'` —— 报错位置指向别处,很容易误判。
另外整张表写成一行也不行(`expected '=' in generated_files entry`),每个条目要各占一行。
可用形态两种:单行字符串字面量,或 `[[ … ]]` 长字符串(本包与 compat.gmp 都用后者)。

## 9. 同 PR 内的 README 重构

`README.md` / `README.zh-CN.md` 的「参考示例」整张大表(占英文 README 约 22 KB 中的绝大部分)已移出到
`docs/descriptor-examples.md` 与 `docs/zh/descriptor-examples.md`,README 只留每种常见形态一行、
一句话说明的六行小表加一个链接。libaio 自己的条目写在新文档里。

顺带修掉表里一条早就失效的链接:`pkgs/o/opencv.lua` → `pkgs/o/opencv.opencv.lua`。

README 体积:28693 → 6151 字节(中文 25591 → 5330)。
