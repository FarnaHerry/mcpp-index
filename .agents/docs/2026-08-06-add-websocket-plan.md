# 新增 compat.websocket 12.0.1(2026-08-06)

产出:`compat.websocket@12.0.1`(IXWebSocket,上游 MIT),32 个 client TU 的 header-compat
Form B 包,workspace 成员 `tests/examples/websocket`,以及一份自带 echo server 的离线测试。

## 1. 形态判定:C++ 源码 compat,纯客户端,零依赖

IXWebSocket 上游**没有 mcpp 支持**(CMake 构建、C++11、`#include <ixwebsocket/IXWebSocket.h>`
的头文件式消费),属第三方上游库 → Form B。上游源码是扁平的 `ixwebsocket/` 目录,头文件与源码同处一室;
`include_dirs = { "*" }` 把 verdir 根暴露出去,消费端 include 路径与上游安装布局一致
(`<includedir>/ixwebsocket/…`)。

**编译开关三项全关,是零外部依赖的关键**:

- **TLS 关**(不定义 `IXWEBSOCKET_USE_TLS`):TLS socket 三组 TU(OpenSSL/MbedTLS/AppleSSL)根本不编入;
  `IXSocketFactory.cpp` 只在 `#ifdef IXWEBSOCKET_USE_TLS` 下引用它们。计划稿"关掉 TLS 就不拉 OpenSSL"由此兑现。
- **zlib 关**(不定义 `IXWEBSOCKET_USE_ZLIB`):`IXGzipCodec.cpp` 与 per-message-deflate codec 的所有
  zlib 调用都包在这个宏后,不定义则编译为透传 no-op —— 客户端在握手里不再提议压缩扩展,是保守且合法的默认。
- **server 不提供**(见下节)。

**源列表 = 上游 CMake `IXWEBSOCKET_SOURCES` 的 36 项去掉 4 个 server TU**,余 32 项逐条列出
(与 `compat.zlib`/`compat.protobuf` 的显式列法一致;`compat.abseil` 用通配是因为上游文件命名可裁剪,
这里没有可裁剪的通配,显式列反而可审)。

## 2. server TU 排除有链接层论证,不是拍脑袋

剔除的四个 TU:`IXWebSocketServer.cpp`、`IXWebSocketProxyServer.cpp`、`IXHttpServer.cpp`、
`IXSocketServer.cpp`。排除前先在整个 client 侧做了符号引用检查 —— 除这 4 个文件自身外,**没有任何
保留的 `.cpp` 引用这些类名**(`grep -l 'IXWebSocketServer\|IXHttpServer\|IXSocketServer\|IXWebSocketProxyServer'
` 只命中那 4 个文件自己)。这是"client 不反依赖 server"的直接证据,链接期不会 undefined reference。

`IXGetFreePort.cpp` **保留**:它是独立工具(找空闲端口),不属于 server 那一侧,且 32 个保留 TU 的
basename 全局唯一,不会触发 mcpp#233/#240 的 flat-obj 撞名。

## 3. 关键决定(含两处与计划稿的偏离)

| 项 | 计划稿 | 本包实际 | 理由 |
|---|---|---|---|
| 版本 | 未指定 | `12.0.1` | 上游最新稳定 tag(`git ls-remote --tags` 取 `sort -V \| tail`) |
| 目录 | `pkgs/w/` | **`pkgs/c/`** | 仓库约定是**完整包名首字母**(`compat.*` → `c/`,见 skill 与 docs);`pkgs/e/` 的 `compat.eui-neo` 是唯一用短名首字母的例外。放错目录会让本地 path index 报 not found |
| `language` | `c++17` | **`c++23`** | 全仓 59 个描述符统一 `c++23`(本仓 floor);IXWebSocket 是 C++11 源码,在 CI 工具链 clang 22.1.8 的 `c++23` 下 **32/32 TU 零错误**通过,无 char8_t/弃用 API 问题,没必要为它开 c++17 特例 |
| CN 镜像 | GLOBAL+CN 双 mirror | **plain-string url** | 本机无 `gtc`/gitcode 配置、无 `mcpp-res` 写权限,按 [docs/cn-mirror.md](../../docs/cn-mirror.md) 回退为纯字符串上游 url(与 `eui-neo` 0.5.5 同做法);sha 不受影响,维护者有权限后补 `{ GLOBAL, CN }` 即可 |

## 4. 平台开关

- linux:`-lpthread`(上游 `Threads::Threads`);无 `-ldl`(全库无 `dlopen`/`dlsym`)。
- macosx:`-lpthread`;TLS 关则无需 Foundation/Security。
- windows:`-lws2_32 -lwsock32` + `-D_CRT_SECURE_NO_WARNINGS`。`shlwapi` **不链**:它只被剔除的
  `IXSocketOpenSSL.cpp` 用到(其 `PathFileExists` 等);保留的 TU 里没有任何 shlwapi 引用。`NOMINMAX`
  也不需要:唯一 include `windows.h` 的保留 TU 是 `IXSetThreadName.cpp`,其源码不用 min/max。

## 5. 测试设计:自带最小 RFC 6455 echo server,全程离线

包只编 client,测试要实测"握手/掩码/分片/关闭"就不能依赖外部服务。`tests/examples/websocket/tests/ws_test.cpp`
在**进程内**用原始 loopback socket 起了一个最小 echo server,并刻意做成**与被测库相互独立**:

- `Sec-WebSocket-Accept` 用测试自带的 SHA-1 + base64 计算,`HTTP/1.1 101` 响应手写;
- 帧协议手写(掩码位/长度扩展/控制帧),server 端能验证**客户端的掩码位确实置位**;
- 收到特殊载荷 `FRAG` 时回一条**分片消息**(text FIN=0 + 两个 continuation),验证客户端重组;
- 收到 `PINGME` 时回一条 ping,验证客户端默认 `enablePong` 的自动 pong;
- close 回显 code 后关闭连接。

断言 11 项,全部带超时:`initNetSystem`、握手打开、text echo、**客户端掩码位**、binary echo 逐字节、
客户端 ping→pong、服务端 ping→客户端自动 pong、分片重组、分片帧先于重组消息、close 握手完成、
server 收到 close。CI 三平台离线可跑,不依赖 runner 网络。

**两个实测踩坑**(都已修进最终测试):

1. **IXWebSocket 的 `_automaticReconnection` 默认是 `true`**(构造函数 `_automaticReconnection(true)`),
   不是关闭。close 握手完成后状态转 `Closed`,若测试在收到 Close 消息与 `ws.stop()` 之间留空窗,
   run 循环会**再次 `checkConnection` 并重连** —— 而本测试的 server 线程在 close 后已退出,重连的
   握手读响应会永久阻塞,表现为进程挂死。修法:close 测试直接调 `ws.stop()`(它内部先 `close()` 再置
   `_stop`),run 循环因 `_stop` 而退出,Close 消息在 `stop()` 返回前已投递到 collector。
2. **"客户端掩码位"断言过早**:Open 消息之后客户端还没发过任何数据帧,server 看不到掩码位。把该断言
   挪到 text echo 之后(echo 成功 = server 必然已收到客户端数据帧)。

## 6. 验证结论

两轮验证都通过:

**与 CI 完全一致的一轮(mcpp 2026.8.3.3 + gcc@16.1.0 + `MCPP_INDEX_MIRROR=GLOBAL` +
`MCPP_BUILD_CACHE=local`,冷删 `target/`/`.mcpp/`):**

```
mcpp xpkg parse pkgs/c/compat.websocket.lua  →  parse OK(compat.websocket / 三平台 12.0.1 /
                                                c++23 / import_std=false / 32 sources / target websocket)
mcpp test -p websocket                       →  test result ok. 1 passed; 0 failed
```

该轮实际下载了 CI 的 gcc@16.1.0 工具链并以之编译 compat.websocket 的 32 个 TU,再编译并运行
`ws_test` —— **11 项断言全部通过**,`ALL WEBSOCKET ASSERTIONS PASSED`,exit 0。

**独立的一轮(clang 22.1.8,standalone):** 32/32 TU 在 `c++23` 下逐 TU 编译零错误;同一测试二进制的
11 项断言全过。

lint:`lua` 语法、`check_mirror_urls`(plain-string url 不触发镜像约束)、`check_package_name` 全过;
`mcpp xpkg parse` 在本地较新 mcpp(2026.8.4.1)与 CI pin(2026.8.3.3)上均 OK。

## 7. 后续待办

- **CN 镜像**:有 `mcpp-res` 写权限后,`gtc` 建 `mcpp-res/websocket`、传与 GLOBAL 字节一致的
  `IXWebSocket-12.0.1.tar.gz`,把三平台 url 改写为 `{ GLOBAL, CN }`(sha 不变)。
- **TLS feature**(可选):索引里有 `compat.openssl`/`compat.mbedtls`,若要支持 wss,可加 feature 把对应
  TLS TU 与依赖编入 —— 本包刻意先做零依赖纯客户端。
- **C++23 薄封装头**(可选):用户计划稿提到的 `websocket.hpp`(span/format/RAII 薄层)不阻塞本包,可后续单独加。
