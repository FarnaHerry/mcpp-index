# 收录 concurrentqueue 1.0.5(compat.concurrentqueue)

日期:2026-08-29 · 分支:`feat/add-concurrentqueue` · 状态:本地验证通过

## 1. 来源与形态判定

concurrentqueue 属于来源 (a):第三方上游库,上游不提供 mcpp 支持。

- 上游:<https://github.com/cameron314/concurrentqueue>(Cameron Desrochers;
  即 `moodycamel::ConcurrentQueue`)。
- 最新版本:`git ls-remote --tags` 排序后最大的是 **`v1.0.5`**(本地工作副本 checkout 在
  v1.0.5 之后 6 个 commit,未成 tag,不予采用;版本键用裸版本 `"1.0.5"`,下载 URL 保留
  上游的 `refs/tags/v1.0.5` 拼写)。
- License:**双许可** —— 文件头声明 Simplified BSD(BSD-2-Clause)或 Boost 软件许可
  (BSL-1.0)二选一,故 `licenses = {"BSD-2-Clause", "BSL-1.0"}`(compat.libarchive 已有
  多值先例)。
- 源码布局:`concurrentqueue-1.0.5/` 包一层。库本体是 tarball **根**下三个头:
  `concurrentqueue.h`(核心)、`blockingconcurrentqueue.h`(阻塞变体,依赖
  `lightweightsemaphore.h`)、`internal/concurrentqueue_internal_debug.h`(调试头)。
  另有 `c_api/`(2 个 `.cpp` + 1 个 `extern "C"` 头)、`tests/`、`benchmarks/`。

**形态 = B(header-only)**,叠加一个 source-gated 的 `c-api` feature。

## 2. 版本与下载源

`sha256 = 4d6368a27492d86011fde5ca0cf386dce7c49cd425aa3d9b063ca6ec373a6ef3`
(1567167 字节,连算两次一致)。GitHub 的 tag 归档字节稳定,可直接作 GLOBAL。

## 3. CN 镜像

本机**没有** `GITCODE_TOKEN`(gitcode 公开读 API 可用,但建仓/release 需要写权限)。
按 `docs/cn-mirror.md` 的回退方案:描述符采用**纯字符串 url**(只填上游 release),
lint 的 `check_mirror_urls.lua` 对纯字符串不施加镜像约束,CN 用户回退上游源,
镜像由维护者后续补充(`mcpp-res` 组织下尚无 `concurrentqueue` 仓)。

## 4. 实现决策

### 4.1 `include_dirs = {"*"}`:公开头就在 tarball 根

三个公开头与 LICENSE、CMakeLists 并列在归档顶层,没有 `include/` 一层可挑
(compat.gtl 的「只取 `*/include`」在这里无处安放),`*` 即上游 README 让你放进
include 路径的那个目录。anchor TU 照 compat.eigen / compat.plf-hive 先例,
给 mcpp 一个可构建的 lib 目标。

一个需要点破的名字重叠:根下的 `concurrentqueue.h`(C++ 主头)与
`c_api/concurrentqueue.h`(C API 头)**同名**。它不构成遮蔽危害,因为 include 路径上
只有归档根一个目录 —— `<concurrentqueue.h>` 永远命中前者,
`<c_api/concurrentqueue.h>` 是唯一能命中后者的拼法,两条路径互不混淆。

### 4.2 `c-api` feature:包里唯一可编译的可选组件

feature 准则(「该组件是否为额外的可编译源码」)在本包只有一个命中:`c_api/` 的两个
`.cpp`(把两个队列封装成 `moodycamel_cq_*` / `moodycamel_bcq_*` 平面 C 接口)。
默认关闭,请求后编进同一个 `concurrentqueue` lib 目标(compat.cjson 的 `utils` 先例)。

**其余一概不收**:`tests/`、`benchmarks/` 自带 `main()`,lib 目标的对象会急切进入消费者
链接,包里不能带 main(compat.libaio 同一推理);`unsupported` 一类纯头内容不存在 ——
这库没有纯头形式的可选项,因此没有「无从门控」的遗憾。

### 4.3 Windows 的 `MOODYCAMEL_STATIC`:`cxxflags` 与 feature `defines` 各管一半

`c_api/concurrentqueue.h` 自己挑 `MOODYCAMEL_EXPORT` 的拼法:未定义
`MOODYCAMEL_STATIC`/`DLL_EXPORT` 时按 **DLL 客户端**处理,全部函数声明成
`__declspec(dllimport)`。这在两边同时出错:

- 包自身的 `.cpp` 要**定义**这些函数 —— 定义一个 dllimport 函数是硬错误;
- 静态库消费者本要链接符号,却被声明成导入。

于是包级 `cxxflags = { "-DMOODYCAMEL_STATIC" }` 覆盖包自身 TU(c_api 两个 TU 全是
C++,`cflags` 到不了 —— compat.msdfgen 的教训),`c-api` feature 的
`defines = { "MOODYCAMEL_STATIC" }` 覆盖消费者 TU(Feature System v2 的接口 define)。
该宏只在 `#ifdef _WIN32` 内被读取,linux/macos 上处处定义等于无效操作,故不按平台拆分。
已核对本地产出的 `compile_commands.json`:四个 TU(两个 c_api 源、anchor、消费者测试)
都带上了 `-DMOODYCAMEL_STATIC`。

### 4.4 Linux 侧无需任何 `-D`/`-l`

`lightweightsemaphore.h` 在 Linux 走 POSIX `sem_t`(`<semaphore.h>`),不碰 futex
syscall,也就不触发 compat.libaio 踩过的 `__STRICT_ANSI__` 藏符号问题;
`_GNU_SOURCE` 在该头里只控制一个 monotonic-clock 优化分支,不开只是退化不报错。
`sem_*` 自 glibc 2.34 起在 libc 本体,本机验证未链 -lpthread 即通过。

## 5. 测试成员

- `tests/examples/concurrentqueue`(默认构建):单线程 FIFO、bulk 往返、ProducerToken、
  move-only 元素、阻塞队列(空队列 `wait_dequeue_timed` 须**真的等满** 50ms 才返回
  false —— 把「信号量提前 post」与「真等待」区分开的断言)、4 生产 × 4 消费的 MPMC
  竞争,逐值核验「恰好收到一次」(丢失/重复/ABA 都会表现为计数 ≠1)。
- `tests/examples/concurrentqueue-c-api`(`features = ["c-api"]`):C API 的
  create/enqueue/try_dequeue/size_approx/destroy 与 bcq 的 wait_dequeue 顺序性;
  值以整数经 `void*` 往返,校验和必须分毫不差 —— 这正是「句柄类型装错队列」会暴露的地方。
- 依赖一律**限定拼写** `[dependencies.compat]`(eigen 成员的推理:裸名会按请求的命名空间
  去公开远端索引解析,静默脱离本 checkout)。
- 两个成员都已登记进根 `mcpp.toml` 的 `[workspace].members`。

编写期自摆乌龙两处,恰好证明断言可失败:`wait_dequeue` 在 1.0.5 返回 `void`(只有
`_timed` 拼写报告成败);`enqueue_bulk` 返回 `bool`(只有 `try_dequeue_bulk` 返回数量)。

## 6. 验证结论

mcpp 用**本地已有**的 2026.8.27.1(CI 锁 2026.8.27.2,差一个 patch;宿主到
github.com 主站可达、release CDN 超时,CI 绿灯为最终裁决):

- `mcpp test -p concurrentqueue` → `test result ok`(冷启动,自干净 target/.mcpp);
- `mcpp test -p concurrentqueue-c-api` → `test result ok`(独立重编包,feature 生效);
- **负向验证**:引用 `moodycamel_cq_create` 的 TU 链接默认构建的包对象 →
  `undefined reference to moodycamel_cq_create`,证明门控真实;默认构建的包对象目录里
  也只有 `concurrentqueue_anchor.o`,无任何 `c_api/*.o`;
- 定位/查找:`include_dirs = {"*"}` 下 `<concurrentqueue.h>` 命中根主头,
  `<c_api/concurrentqueue.h>` 命中 C API 头,同名不冲突。

## 7. 与 skill 流程的两处偏差

- **README 未改**:#277 之后描述符目录已从 README 移入 `docs/descriptor-examples.md`,
  README 只留每形态一行的示例表(该表无需新增行)。本 PR 按 #277 后的现状,把条目写进
  `docs/descriptor-examples.md` 与 `docs/zh/descriptor-examples.md` 的
  「header-only(with features)」行。
- **mcpp 版本**:本地 2026.8.27.1 vs CI 2026.8.27.2,见第 6 节。
