# 新增 GMP 收录(compat,install() 驱动构建)+ linux/macOS 方案

**日期**: 2026-08-09
**本仓**: `mcpplibs/mcpp-index`
**issue**: [#171 增加libgmp科学库](https://github.com/mcpplibs/mcpp-index/issues/171)(open,无 assignee/评论)
**目标**: 收录 GNU GMP 6.3.0 为 `compat.gmp`,用户 `#include <gmp.h>` 开箱即用,链接静态 `libgmp.a`。

---

## 1. 上游调研与形态判定

- 最新发布:**6.3.0**(2023-07-30;2026 年仍为最新,ftp.gnu.org 仅有 6.3.0)。GNU 官方 tarball 自带
  `configure`,**不可用 GitHub snapshot**(缺 configure)。
  GLOBAL = `https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.gz`,sha256(两次计算一致)
  `e56fd59d76810932a0555aa15a14b61c16bed66110d3c75cc2ac49ddaa9ab24c`。
- 许可证:GMP 双许可 **LGPL-3.0-or-later OR GPL-2.0-or-later**(tarball README 声明),描述符取宽松侧
  `LGPL-3.0-or-later`。
- 形态:**autotools 工程**(configure 在构建期生成 `gmp.h`/`config.h`/`gmp-mparam.h`/汇编选择),不属于
  「列出 .c 文件」的直编形态 → 采用与 `compat.openblas`(Make)、`compat.openssl`(Perl Configure + Make)
  相同的 **xpkg `install()` 钩子**模式:钩子跑上游 `configure && make && make install`,把 `libgmp.a` +
  `include/gmp.h` 铺进 install 目录,再 emit anchor TU 触发 mcpp 构建。

## 2. 描述符设计(pkgs/c/compat.gmp.lua)

- 身份:`namespace = "compat"`, `name = "gmp"` → `pkgs/c/`(完整包名首字母)。
- `mcpp` 段:anchor 源 `mcpp_gmp_anchor.c`(由 install() 生成,非 generated_files),`targets.gmp = lib`,
  `include_dirs = {"include"}`,ldflags:linux `-Llib -l:libgmp.a -lm`(libgmp 的 mpf 层用 libm;glibc 不带)、
  macosx `-Llib -lgmp`(ld64 无 `-l:`;libSystem 已含 libm)。
- configure 参数:`--disable-shared --enable-static --disable-assembly` + `--prefix=<install_dir>`
  `--libdir=<install_dir>/lib`。
  - **`--disable-assembly`**:通用 C 内核。与 openblas `TARGET=GENERIC` 同一取舍 —— 可移植、可复现优先,
    牺牲手写汇编加速(后续可逐平台重开;mcpp feature 表无法携带 configure 标志,故这是构建期决策)。
  - **`--libdir` 必须绝对路径**:GMP configure 拒绝相对值(`expected an absolute directory name`),
    与 OpenSSL 不同 —— 首版踩坑,已修复并注释。
- 平台:linux + macosx(install() 内用 host cc 构建);**windows 推迟**(见 §4)。
- 构建期依赖:linux `xim:make@latest` + `xim:glibc@>=2.39` + `xim:linux-headers@5.11.1`(cc_override 复用
  compat.openssl 的 libc payload 方案:裸 `cc` 经 xim shim 会注入指向空 subos 的 `--sysroot`,必须显式给
  payload gcc 传 `-isystem/-B/-L`);macosx 无构建期依赖(make 回退 PATH —— macOS 自带 GNU Make 3.81 满足
  GMP 的 >=3.80 要求;cc 钉 `/usr/bin/cc` 以自带 SDK)。

## 3. 镜像决策

当前无 `mcpp-res` 写权限(gtc 登录为维护者 Sunrisepeak,不擅自发布外部资源),故按 docs/zh/cn-mirror.md
回退:**纯字符串 url**(lint 合规,CN 用户回退上游 ftp.gnu.org)。维护者后续可改写为
`{ GLOBAL=…, CN=… }`(sha256 不变)。

## 4. Windows 推迟

GMP **无官方 MSVC 构建路径,也无官方预编译二进制**(vcpkg 在 Windows 用 MPIR fork/MinGW)。与
`compat.openssl` 现状一致:不声明 windows xpm 块,测试成员用 `cfg(linux)`/`cfg(macos)` 门控 + 其他平台
no-op main。

## 5. feature 评估

无可门控组件:GMP 的可选项(汇编、C++ wrapper gmpxx、静态/共享)均由 **configure 标志**控制,而 mcpp
feature 表只能门控 `sources`,不能携带 configure 参数。`--disable-assembly` 作为确定性构建取舍固定写入
钩子,并在描述符注释说明。

## 6. 消费者测试(tests/examples/gmp/)

- `mcpp.toml`:`[target.'cfg(linux)'/'cfg(macos)'.dependencies.compat] gmp = "6.3.0"` + `-DHAVE_GMP=1`;
  windows 无依赖。成员名 `gmp`,已注册进根 `mcpp.toml` members。
- `tests/gmp.cpp`(TDD 先写断言):覆盖三层算术,参考值用 Python 独立预计算:
  - mpz `mpz_fac_ui(100)` = 158 位十进制串(逐字符比对);
  - mpz `mpz_powm_ui(2^1000, mod 12345)` = 5341;
  - mpz `mpz_gcd(123456789, 987654321)` = 9;
  - mpq `mpq_add(1/2, 1/3)` = 5/6;
  - mpf `mpf_sqrt(2)` ∈ (1, 2)。
  每项独立 return code,失败可定位。

## 7. 本地验证(mcpp 2026.8.8.2 = validate.yml 当前 pin)

```
RED:   mcpp test -p gmp → dependency 'compat.gmp': no package found(包缺失,预期失败)
GREEN: mcpp test -p gmp → gmp ... ok / test result ok. 1 passed; 0 failed
冷跑:  清 target/.mcpp 后完整重跑 45.2s(下载→install() configure+make→anchor→链接→运行)→ 通过
产物:  install 目录含 include/gmp.h(84K)、lib/libgmp.a(908K)、mcpp_gmp_anchor.c;链接行
       -L…/xpkgs/compat-x-gmp/6.3.0/lib -lgmp(确认用本包静态库,非宿主 libgmp)
lint:  lua5.4 语法/必填字段/无前导 v/check_mirror_urls/check_package_name 全过;
       mcpp xpkg parse → compat.gmp,linux+macosx 6.3.0,1 source,1 include,target gmp;
       check_cross_package_refs、check_platform_version_parity(全仓)过
```

## 8. 注意事项 / 后续

- windows:需上游 MSVC 路径或维护者提供预编译产物,方可补 xpm.windows 块与钩子分支。
- 汇编:后续可逐平台尝试默认(带 asm)构建,验证 gas 兼容后去掉 `--disable-assembly`;gmpxx(C++ wrapper)
  同理可在 `--enable-cxx` 下评估。
- CN 镜像:获得 `mcpp-res` 写权限后,用 gtc 建 `mcpp-res/gmp` 并上传同字节 tarball,url 改表形式。
