---
title: "阶段 5 Lab 实验参考"
description: "阶段 5 Lab（一个 socket 服务的完整调试）的实验参考：六个步骤加 L5 挑战的逐步解答，每步标注知识点链接，所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。"
chapter: 5
order: 3
tags:
  - host
  - system-programming
  - socket
  - networking
  - concurrency
difficulty: advanced
reading_time_minutes: 45
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 Lab 题面"
related:
  - "阶段 5 各章"
---

# 阶段 5 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。端口号、fd 编号每次运行不同——对结构（LISTEN/TIME-WAIT、errno 编号、次数关系）而不是对数字。

## 步骤 1：最小对答 + 用 ss 看连接的一生 {#lab-1}

**思路**：`ss -tan` 是内核连接表的快照；服务端 `close` 连接的一方进入 `TIME_WAIT`，所以两次抓包正好一前一后夹住一条连接的出生和余烬。

1. 服务端四件套 + 客户端两件套是最小闭环：`socket`/`bind`/`listen`/`accept` 对 `socket`/`connect`，数据面还是 `read`/`write`。→ 知识点：[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)「服务端四件套」「客户端两件套」两节
2. 第一次 `ss` 抓到 `LISTEN`：监听 fd 已经就位、还没人连；客户端连上、收发、走人，服务端 `accept` 的那条连接由**服务端主动 `close`**——主动关的一方进 `TIME_WAIT`，所以第二次 `ss` 抓到 `TIME-WAIT` 一行。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「TIME_WAIT 与 SO_REUSEADDR」一节
3. 这就是「服务端重启被 TIME_WAIT 卡住」的现场版：那条 60 秒的余烬里，这个端口对重启的 `bind` 来说还是「占着的」。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（`SO_REUSEADDR` 解决的就是这个）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab01_srv.c -o lab01_srv
$ gcc -std=c11 -Wall -Wextra lab01_cli.c -o lab01_cli
$ ./lab01_srv 39321 > srv.log 2>&1 &
$ sleep 0.4
$ ss -tan | grep 39321
LISTEN    0      1           127.0.0.1:39321      0.0.0.0:*
$ ./lab01_cli 39321
CLIENT_GOT: pong
$ sleep 0.3
$ ss -tan | grep 39321
TIME-WAIT 0      0           127.0.0.1:39321    127.0.0.1:47794
$ cat srv.log
SERVER_READY 39321
SERVER_ACCEPTED
SERVER_GOT: ping
```

## 步骤 2：字节序与地址体检 {#lab-2}

**思路**：小端机内存里低字节在前，`0x1234` 存成 `34 12`；网络序是大端，`htons` 把它翻成 `12 34`——sockaddr 里的每个多字节字段都必须是后者。

1. 字节 dump 实锤：主机序 `34 12`、`htons` 后 `12 34`，正好相反——`htons` 在 x86 上是一次字节翻转，在大端机上则是空操作，所以「必须写」而不是「看心情写」。→ 知识点：[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)「字节序」一节
2. 手填的 `127.0.0.1:80` 是 `00 50`（端口 80 = 0x0050 大端）加 `7F 00 00 01`（IP 逐字节）——和 `getaddrinfo` 解析出的数字地址完全对得上。→ 知识点：[第 14 章：getaddrinfo](/05-system-programming/14-getaddrinfo)「getaddrinfo：填 hints、拿链表」一节
3. `AF_UNSPEC` 给了两条结果：`family=10`（AF_INET6，`::1`）和 `family=2`（AF_INET，`127.0.0.1`）——协议无关代码就是在这两条里逐条试。→ 知识点：[第 14 章](/05-system-programming/14-getaddrinfo)（遍历结果逐条试）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab02.c -o lab02 && ./lab02
主机序 0x1234 内存字节: 34 12
htons   0x1234 内存字节: 12 34
手填 sockaddr_in 的端口字节: 00 50
手填 sockaddr_in 的地址字节: 7F 00 00 01
getaddrinfo: family=10 socktype=1 地址=::1 端口=80
getaddrinfo: family=2 socktype=1 地址=127.0.0.1 端口=80
```

## 步骤 3：粘包、半包与长度前缀组帧 {#lab-3}

**思路**：TCP 只保证字节按序到达，不保证「几次 write 对几次 read」——粘包是两条消息挤进一次 read，半包是一条消息被拆进几次 read；长度前缀让接收方自己算出边界。

1. 实验一：等 200ms 再 read 的这次，两条 write 都已经在接收缓冲里，一次 read 拿到 8 字节 `msg1msg2`——粘包现身。不等就直接 read 的话可能只拿到 `msg1`（两条 write 还没都到），那反而演示不了粘包——这个「等」是实验设计的一部分。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「TCP 没有消息边界」一节
2. 实验二：先读 3 字节拿到 `hel`，剩下的 8 字节要再 read 一次——半包是「一条消息被拆几半」的日常。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（半包）
3. 实验三：`read_full` 严格凑 4 字节长度 → `ntohl` → 严格凑 11 字节载荷，`hello-world` 边界清晰还原——此后 TCP 怎么粘怎么拆都无所谓，因为「读多少」是长度字段算出来的。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（长度前缀组帧、`read_full` 基于第 1 章的 `write_all`）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab03.c -o lab03 && ./lab03
[实验1·粘包] 一次 read 收到 8 字节: 'msg1msg2' —— 两条消息被粘成一条
[实验2·半包] 先读 3 字节: 'hel', 剩下的得再读一次才拿得到
[实验3·组帧] 长度字段=11, 载荷 11 字节: 'hello-world' —— 边界清晰还原
```

## 步骤 4：SIGPIPE 解剖 {#lab-4}

**思路**：往「对端已关」的连接写，内核发 SIGPIPE、默认动作杀进程——服务端是子进程的话，父进程正好当法医。

1. mode 0：**死在和 mode 1 同一处——第 28 次写**（前 27 次约 110KB 被发送缓冲吞掉）；差别只在处置：默认动作直接杀进程（信号 13），服务端自己一行日志都没留下，父进程 `WIFSIGNALED`/`WTERMSIG` 才查出死因。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「SIGPIPE：往死连接 write 会被默默杀掉」一节
2. mode 1 和 mode 2：`SIG_IGN` 与 `MSG_NOSIGNAL` 殊途同归——`write`/`send` 改成返回 `-1`、`errno=32`（EPIPE），且都是**第 28 次写**才失败：前 27 次共约 110KB 被 TCP 发送缓冲吞掉了，第 28 次才撞上 RST 之后的 EPIPE。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（小 write 被发送缓冲吞掉、两招防护）
3. 这就是「一个客户端能带走整个服务端」的机制：对端悄悄断网，你的下一次 write 就是最后一击——所以服务端 main 开头防 SIGPIPE 是标配。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（防护二选一或都上）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab04.c -o lab04
$ ./lab04 0
[server] accept 完成, 睡 1 秒等对端 RST
[main] 服务端死于信号 13(SIGPIPE=13)
$ ./lab04 1
[server] accept 完成, 睡 1 秒等对端 RST
[server] 第 28 次写失败: errno=32 (Broken pipe)
[main] 服务端正常退出(防护起了作用)
$ ./lab04 2
[server] accept 完成, 睡 1 秒等对端 RST
[server] 第 28 次写失败: errno=32 (Broken pipe)
[main] 服务端正常退出(防护起了作用)
```

## 步骤 5：epoll 并发化 {#lab-5}

**思路**：监听 fd 和连接 fd 都进 epoll，事件循环统一分派——`fd == listen_fd` 是「来新连接」，否则是「某条连接有动静」；非阻塞保证循环永远不被卡住。

1. 服务端骨架：监听 fd 非阻塞才能「循环 accept 到 EAGAIN」；新连接非阻塞、注册进 epoll；读到的数据 `write_full` 回显；EOF 就 `EPOLL_CTL_DEL + close`。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「epoll 并发服务端」一节、[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)（reactor 四件套）
2. 真跑：三次 accept、三次回显、三次 EOF 摘除，客户端各收各的回显——注意连接 fd 三次都是 5，上一轮关闭腾出的最小空闲号又被下一轮 accept 复用，第 1 章的规则在 socket 上照常生效。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)（最小空闲号回收）
3. `setvbuf` 无缓冲是为了日志实时可见——服务端被重定向时 stdio 全缓冲，不设的话日志都攒在退出前那一刻，调试时看起来像「卡死了」。→ 知识点：[第 1 章](/05-system-programming/01-file-io-and-fd)「setvbuf」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab05.c -o lab05 && ./lab05
[server] 监听 127.0.0.1:44835
[server] accept fd=5(共 1 条)
[server] fd=5 回显 16 字节
[client-0] 回显: client-0-says-hi
[server] fd=5 EOF → 摘除关闭
[server] accept fd=5(共 1 条)
[server] fd=5 回显 16 字节
[client-1] 回显: client-1-says-hi
[server] fd=5 EOF → 摘除关闭
[server] accept fd=5(共 1 条)
[server] fd=5 回显 16 字节
[client-2] 回显: client-2-says-hi
[server] fd=5 EOF → 摘除关闭
[server] 3 条连接全部收尾: 回显 3 次
```

## 步骤 6：非阻塞 + LT 的 EOF 陷阱 + 空闲踢人 {#lab-6}

**思路**：LT 的「就绪」判据是 `read` 不阻塞——EOF 的 fd 上 `read` 永远立刻返回 0，于是 epoll 认为它永远就绪，事件循环退化成死循环；「移除」和「踢人」都是 DEL+close 的不同触发条件。

1. mode 0：1 秒内 EOF 回调被空转 **2,514,538 次**（约 250 万次/秒）——漏 DEL 的后果不是「偶尔多跑一圈」，是实打实的烧 CPU 死循环。→ 知识点：[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)「LT 模式下 EOF 的坑：必须 DEL」一节
2. mode 1：回调返回「移除」、循环 DEL+close，两次回调（数据 + EOF）就干净退场。→ 知识点：[第 10 章](/05-system-programming/10-nonblock-and-reactor)（连接生命周期：建连 ADD、断开 DEL）
3. mode 2 的空闲踢人：`epoll_wait` 的 500ms 超时是「心跳」——每半秒扫一遍连接表，`now - last_active > 2s` 就踢。发完 `hello` 就关的客户端走 EOF 路径正常摘除；只连不发的那个被判定「2 秒没动静」踢掉——没有这个超时参数，事件循环就永远没机会主动「巡检」。→ 知识点：[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)（超时参数是**教材外补充**——第 9 章只用 `-1` 阻塞，此处用超时实现心跳）、[第 10 章](/05-system-programming/10-nonblock-and-reactor)（保活策略）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab06.c -o lab06
$ ./lab06 0
[mode0·漏DEL] 1 秒内 EOF 回调空转 2514538 次
$ ./lab06 1
[mode1·修复] 读到: hi
[mode1·修复] EOF → DEL + close
[mode1·修复] 事件循环干净退出
$ ./lab06 2
[server] accept fd=5(共 1 条)
[server] fd=5 活跃, 回显 5 字节
[client-0] 回显: hello
[server] fd=5 EOF → 摘除关闭
[server] accept fd=5(共 1 条)
[server] fd=5 空闲超 2 秒 → 踢掉
[server] 2 条连接全部收尾
```

## 附加挑战（L5）：长连接多消息 + 优雅关闭 {#lab-l5}

**思路**：长连接意味着每条连接的组帧状态要**跨 epoll 事件保活**（长度字段可能分几次到、载荷也可能分几次到）；优雅关闭的钥匙是「处理器只设标志、收尾留给事件循环」。

1. 服务端的组帧状态机：每个连接记住「正在收长度字段还是载荷、已收多少」，每次 `read` 都是非阻塞的「有多少吃多少」，EAGAIN 就等下一轮——这是半包问题在**非阻塞 + 事件驱动**下的标准解法。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)（长度前缀）、[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)（非阻塞递增读取）
2. 真跑：两个客户端共 8 条消息在 fd=5、fd=6 两条连接上交错到达、逐条回显；`QUIT` 触发 `BYE` 应答后摘除该连接；两条连接全部关闭后 `kill -TERM`，服务端打印「共服务 8 条消息、关闭 2 条连接」、退出码 0。→ 知识点：[第 13 章：UDP 与本地域套接字](/05-system-programming/13-udp-and-unix-domain)（对照：UDP 有边界不用组帧）、[第 5 章：信号](/05-system-programming/05-signals)（SIGTERM 优雅退出）
3. 信号处理器里为什么只设 `volatile sig_atomic_t` 标志：处理器可能打断任意一行代码，`printf`/`free` 都不是 async-signal-safe 的——把「关 epoll、close 全部连接、打统计」这些重活留给事件循环，循环下一轮看到标志就优雅退场。→ 知识点：[第 5 章](/05-system-programming/05-signals)「处理器里能做什么」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra labl5_srv.c -o labl5_srv
$ gcc -std=c11 -Wall -Wextra labl5_cli.c -o labl5_cli
$ ./labl5_srv 39351 > l5_srv.log 2>&1 &
$ ./labl5_cli 39351 1 5 & ./labl5_cli 39351 2 3 &
[client-2] 收到回显 8 字节: c2-msg-0
[client-2] 收到回显 8 字节: c2-msg-1
[client-2] 收到回显 8 字节: c2-msg-2
[client-2] 收到 ACK: BYE
[client-1] 收到回显 8 字节: c1-msg-0
[client-1] 收到回显 8 字节: c1-msg-1
[client-1] 收到回显 8 字节: c1-msg-2
[client-1] 收到回显 8 字节: c1-msg-3
[client-1] 收到回显 8 字节: c1-msg-4
[client-1] 收到 ACK: BYE
$ kill -TERM %1; wait %1
$ cat l5_srv.log
SERVER_READY 39351
[server] accept fd=5
[server] accept fd=6
[server] fd=5 收到长度字段: 载荷 8 字节
[server] fd=6 收到长度字段: 载荷 8 字节
...(两条连接的交错回显与摘除,完整日志见运行记录)...
[server] fd=6 摘除关闭
[server] 收到退出信号, 优雅关闭: 共服务 8 条消息, 关闭 2 条连接
```

服务端优雅关闭与处理器骨架：

```c
static volatile sig_atomic_t stop = 0;

static void on_term(int sig) {
    (void) sig;
    stop = 1; /* 处理器只设标志,收尾留给事件循环 */
}

/* main 里装处理器 */
struct sigaction sa;
sa.sa_handler = on_term;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;
sigaction(SIGTERM, &sa, NULL);
sigaction(SIGINT, &sa, NULL);

/* 事件循环:while (!stop) { epoll_wait(...); ... } */

/* 循环退出后优雅关闭:关全部连接、关 epoll、关监听 fd,再 return 0 */
for (int fd = 0; fd < 64; fd++) {
    if (conns[fd].alive) {
        free_conn(&conns[fd]);
        close(fd);
    }
}
close(epfd);
close(listen_fd);
```
