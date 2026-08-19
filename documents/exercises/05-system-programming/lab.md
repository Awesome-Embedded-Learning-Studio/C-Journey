---
title: "阶段 5 Lab：一个 socket 服务的完整调试"
description: "阶段 5 动手实验：把一个 TCP 服务端从「能跑的最小对答」一路调试成「单进程 epoll 并发 + 长连接多消息 + 优雅关闭」的服务——六个步骤从 ss 观察连接状态、字节序体检、粘包半包组帧、SIGPIPE 解剖、epoll 并发化，走到非阻塞与 LT EOF 陷阱，最后附一道带优雅关闭的 L5 挑战。"
chapter: 5
order: 2
tags:
  - host
  - system-programming
  - socket
  - networking
  - concurrency
difficulty: advanced
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 第 9~13 章"
related:
  - "阶段 5 Homework"
  - "阶段 5 Project"
---

# 阶段 5 Lab：一个 socket 服务的完整调试

## 实验目标

这个 Lab 走一条真实服务端的成长路线：先让 TCP 四件套跑起来、用 `ss` 看连接的一生；再体检字节序、亲手复现粘包半包并用长度前缀治它；然后解剖 SIGPIPE——一个客户端就能干掉一个服务端的阴险坑；接着把「一次一个连接」的玩具升级成 epoll 单进程并发；最后把连接设非阻塞，撞一遍第 10 章那个「EOF 空转烧 CPU」的陷阱，并给服务端装上「空闲踢人」。做完你会对「socket 服务端 = fd + 多路复用 + 非阻塞 + 生命周期管理」有肌肉记忆。

所有实验在 `/tmp` 下独立目录做。每步有验收标准；卡住先回[题面标注的章节链接](#lab-1)读教材，再不行看[实验参考](lab-solutions)。

## 步骤 1：最小对答 + 用 ss 看连接的一生 {#lab-1}

**目标**（难度 **L1**）：跑通 TCP 服务端四件套与客户端两件套，再用 `ss -tan` 抓住一条 TCP 连接的「LISTEN → ESTABLISHED → TIME-WAIT」三态。

1. 写服务端 `lab01_srv.c`：`socket(AF_INET, SOCK_STREAM, 0)` → `bind` 到 `127.0.0.1` 的固定端口（比如 39321）→ `listen` → 打印 `SERVER_READY` → `accept` → 读客户端发来的 `ping` → 回 `pong` → 关连接退出。
2. 写客户端 `lab01_cli.c`：`socket` → `connect` 同一个端口 → 发 `ping` → 读回 `pong` 打印。
3. 后台跑服务端，`ss -tan | grep 39321` 抓连接建立前的状态；跑客户端；再抓一次连接关闭后的状态。

**验收标准**：贴出两次 `ss` 的完整输出（能看到 `LISTEN` 和 `TIME-WAIT`）和服务端日志；一句话说清「服务端主动关连接会怎样」。

[实验参考 →](lab-solutions#lab-1)

## 步骤 2：字节序与地址体检 {#lab-2}

**目标**（难度 **L2**）：把「本机是小端」和「sockaddr 里全是网络序」这两件事亲眼看到。

1. 把 `uint16_t host = 0x1234;` 的内存字节逐个 `%02X` 打出来；再打 `htons(0x1234)` 的字节。对照两份输出，确认本机端序。
2. 手填一个 `struct sockaddr_in`（`127.0.0.1:80`），打出它的端口字节和地址字节。
3. 用 `getaddrinfo("localhost", "80", AF_UNSPEC|SOCK_STREAM)` 解析，`getnameinfo` 打出每条结果的 family、数字地址、数字端口，和手填的对一对。

**验收标准**：贴出全部输出；说清 `htons` 前后字节排布为什么正好相反、`getaddrinfo` 给出的结果有几条。

[实验参考 →](lab-solutions#lab-2)

## 步骤 3：粘包、半包与长度前缀组帧 {#lab-3}

**目标**（难度 **L2**）：亲手复现 TCP 的粘包与半包，再用长度前缀把消息边界找回来。

1. 实验一（粘包）：客户端 `write("msg1")`、`write("msg2")` 紧挨着发；服务端 accept 后**等 200ms 再 read 一次**，打印这次 read 收到几个字节、内容是什么。
2. 实验二（半包）：客户端发 11 字节 `hello-world`；服务端先只 `read` 3 字节，打印拿到什么。
3. 实验三（组帧）：实现 `read_full`/`write_full`，客户端发「4 字节网络序长度 + 载荷」，服务端先严格读 4 字节长度、再严格读载荷，打印还原结果。

**验收标准**：贴出三个实验的输出；说清「粘包/半包不是 bug，是字节流的本质」以及「边界是接收方算出来的」这两句话分别指什么。

[实验参考 →](lab-solutions#lab-3)

## 步骤 4：SIGPIPE 解剖 {#lab-4}

**目标**（难度 **L3**）：复现「往死连接写被杀」，并验证三种防护的真伪。

1. 服务端做成子进程、父进程旁观：客户端 connect 后立刻 `close` 走人；服务端 accept 后睡 1 秒（给 RST 回程时间）再循环 `write` 4KB 块。
2. mode 0 无防护：观察服务端怎么死——父进程用 `WIFSIGNALED`/`WTERMSIG` 打出信号号。
3. mode 1 `signal(SIGPIPE, SIG_IGN)`：看 `write` 改成返回什么、errno 是什么、第几次 write 才失败。
4. mode 2 `send(..., MSG_NOSIGNAL)`：单次防护版，同样的死连接。

**验收标准**：贴出三个 mode 的输出；解释「前几十次 write 为什么还能成功」和「为什么服务端 main 开头必须防 SIGPIPE」。

[实验参考 →](lab-solutions#lab-4)

## 步骤 5：epoll 并发化 {#lab-5}

**目标**（难度 **L3**）：把「一次一个连接」的服务端升级成单进程 epoll 事件循环。

1. 监听 fd 设非阻塞、注册进 epoll；事件循环里「监听 fd 就绪 → 循环 accept 到 EAGAIN → 新连接设非阻塞并注册」「连接 fd 就绪 → read → 回显 → EOF 则 DEL+close」。
2. main 开头 `signal(SIGPIPE, SIG_IGN)`、`setvbuf` 无缓冲。
3. fork 三个客户端，错峰 0.3 秒连上、各发一条消息、读回显、打印、关闭。

**验收标准**：贴出完整输出（三次 accept、三次回显、三次 EOF 摘除）；观察连接 fd 编号的变化，用第 1 章的规则解释。

[实验参考 →](lab-solutions#lab-5)

## 步骤 6：非阻塞 + LT 的 EOF 陷阱 + 空闲踢人 {#lab-6}

**目标**（难度 **L4**）：量化「漏 DEL 烧 CPU」的坑，学会修，再给服务端装上超时踢人。

1. mode 0：最小 reactor（一个管道），回调遇到 EOF 只默默计数不要求移除，跑 1 秒墙钟后强制停车，打印 EOF 回调被空转的次数。
2. mode 1：回调 EOF 返回「移除」，事件循环 DEL+close，干净退出。
3. mode 2：socket 版——两个客户端，一个发 `hello` 收完回显就关（EOF 路径），一个连上后**什么都不发**（空闲路径）；服务端记录每条连接的最后活跃时间，`epoll_wait` 用 500ms 超时做心跳，空闲超过 2 秒的连接踢掉。

**验收标准**：贴出三个 mode 的输出；解释「EOF 的 fd 为什么在 LT 下永远就绪」和「空闲踢人为什么依赖 epoll_wait 的超时参数」。

[实验参考 →](lab-solutions#lab-6)

## 附加挑战（L5）：长连接多消息 + 优雅关闭 {#lab-l5}

**目标**（难度 **L5**）：把前面所有手艺合成一个「带优雅关闭的组帧 echo 服务」——长连接、多消息、信号驱动的干净退场。

1. 服务端 `labl5_srv.c`：epoll + 非阻塞 + 长度前缀组帧；每个连接**不读一次就关**，而是持续收消息、原样回显；收到消息 `QUIT` 回 `BYE` 后关闭该连接；安装 `SIGTERM`/`SIGINT` 处理器（只设 `volatile sig_atomic_t` 标志）。
2. 客户端 `labl5_cli.c`：连上后循环发 N 条组帧消息（每条之间歇 120ms，让两个客户端的消息在服务端交错）、每条都读回显，最后发 `QUIT` 收 `BYE`。
3. 后台跑服务端，同时跑两个客户端（一个发 5 条、一个发 3 条）；两个客户端都退场后 `kill -TERM` 服务端，观察它的优雅关闭：摘掉所有连接、关 epoll、关监听 fd、打印统计后退出码 0。

**验收标准**：贴出服务端完整日志和客户端输出；确认「8 条消息全部回显、2 条连接全部关闭、优雅关闭时退出码 0」；说清「信号处理器里为什么只设标志、收尾为什么留给事件循环」。

[实验参考 →](lab-solutions#lab-l5)

## 提交物清单

一个目录装下全部源码、每步终端记录（`stepN.log`）、以及 200 字以内的小结——用你自己的话说清这个 Lab 里哪一步让你对「socket 就是 fd，服务端就是管理一堆 fd 的生命周期」体会最深。
