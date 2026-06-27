---
title: "进阶 Socket：地址复用、SIGPIPE、消息边界与并发服务端"
description: "在基础 TCP 客户端/服务端之上，亲手踩掉 SO_REUSEADDR、SIGPIPE、粘包、getaddrinfo 这几个真正会让你调到怀疑人生的坑，并用 select 把服务端推向并发。"
chapter: 5
order: 1
tags:
  - host
  - system-programming
  - networking
  - posix
  - socket
  - concurrency
difficulty: advanced
reading_time_minutes: 18
platform: host
c_standard: [99, 11]
prerequisites:
  - "Socket 编程：用 TCP 从零写一个客户端/服务端"
  - "ROADMAP 阶段 5：系统编程"
related:
  - "Socket 编程：用 TCP 从零写一个客户端/服务端"
  - "IO 多路复用：select / poll / epoll"
  - "多线程：pthread"
---

# 进阶 Socket：地址复用、SIGPIPE、消息边界与并发服务端

## 引言

如果你跟着 [上一章](0-socket-programming.md) 把最朴素的 TCP 服务端和客户端跑通了，大概率会觉得"也就那样嘛，socket 四件套 + 两件套，跑通了就完事"。说实话我当年也是这么想的——直到我开始在真实场景里反复折腾：服务端一重启就报 `Address already in use`，客户端写到一半进程莫名其妙消失，明明发了两条消息对端只收到一条……这些坑没有一个是 `socket/bind/listen/accept` 那套骨架能救你的，它们全都长在"TCP 是一条字节流"和"内核对 socket 的特殊处置"这两个根因上。

所以这一篇要做的事情很明确：我们把上一章那个能跑的玩具服务端，逐步改造成"在真实环境下不会炸"的服务端。要解决的具体问题，每一个我都用本机实测的真实输出来回答——

- 端口为什么重启就占住（`TIME_WAIT`），`SO_REUSEADDR` 到底救不救得了你
- 往已关闭的连接上 `write`，进程为什么会被一个信号无声干掉（`SIGPIPE`），怎么治
- TCP 没有消息边界，所谓的"粘包/半包"怎么用"长度前缀 + 循环读写"彻底解决
- 旧时代的 `gethostbyname` 为什么别再写，`getaddrinfo` 怎么用
- 一个进程怎么用 `select` 同时伺候监听 socket 和一堆已连接 socket

这些主题在 Stevens 的《UNIX Network Programming》里散落在好几章，我当年啃那本书做笔记的时候笔记记了一大摞，但真正记住的全是"被坑过一次、自己跑过一遍"的那些。这篇就是把那些"跑过一遍"的产物整理出来，配上真实的客户端/服务端输出。所有代码都是纯 C，全部在 `/tmp/` 里 `gcc -std=c11 -Wall -Wextra` 实测过。

## 先把字节序这件事钉死

上一章已经提过字节序，这里我们用一段小程序把它彻底坐实，免得后面所有代码里那堆 `htonl`/`htons` 看得你犯迷糊。`htonl` 是 "host to network long"，`htons` 是 "host to network short"，反过来的 `ntohl`/`ntohs` 是回程。规则只有一条：**凡是多字节整数（端口、IP）要进/出地址结构体，一律走网络字节序**。下面这段小程序把端口 `0x1234`（十进制 4660）来回转一遍：

```c
#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint16_t port = 0x1234;        /* 端口 4660 */
    uint16_t net  = htons(port);   /* 转网络字节序 */
    printf("host    port = 0x%04x\n", port);
    printf("network port = 0x%04x  (htons 后)\n", net);
    printf("ntohs 还原    = 0x%04x\n", ntohs(net));
    printf("若忘记 htons，对端收到的是 0x%04x = %u（而不是 %u）\n",
           ntohs(port), ntohs(port), port);
    uint32_t ip = 0x01020304; /* 1.2.3.4 */
    printf("IP htonl(0x%08x) = 0x%08x\n", ip, htonl(ip));
    return 0;
}
```

编译跑一下（这台机器是小端 x86）：

```text
$ gcc -std=c11 -Wall -Wextra byteorder.c -o byteorder && ./byteorder
host    port = 0x1234
network port = 0x3412  (htons 后)
ntohs 还原    = 0x1234
若忘记 htons，对端收到的是 0x3412 = 13330（而不是 4660）
IP htonl(0x01020304) = 0x04030201
```

你看，主机序的 `0x1234` 经过 `htons` 变成 `0x3412`（字节顺序对调了，这就是"小端转大端"），对端再 `ntohs` 又转回 `0x1234`，完美闭合。但如果你忘了 `htons`、直接把主机序塞进结构体，对端拿 `ntohs` 一解读就变成 `0x3412`，也就是 `13330`——**你以为自己监听 4660，实际上内核收到的是 13330**。这种 bug 最恶心：本机自测（客户端服务端同一台机器、同一套字节序）能连上，换台不同架构的机器就死活连不上，能坑你一整天。所以这条铁律没有例外：端口、IP 入结构体前，先转。

## TIME_WAIT 与 SO_REUSEADDR：服务端重启为什么连不上

### TIME_WAIT 到底卡的是什么

先把根因讲清楚。TCP 关闭是四次挥手，**主动调用 `close` 的那一端，会进入 `TIME_WAIT` 状态**，停留大约 1～4 分钟（RFC 建议的 MSL 是 2 分钟，Linux 默认 60 秒）。这段时间里，那个本地 `IP:端口` 是被内核"占着"的。服务端通常是主动关闭的一方（处理完请求就关连接），所以服务端重启的时候，端口很可能还卡在 `TIME_WAIT` 里，`bind` 直接报错。

`TIME_WAIT` 存在不是内核吃饱了撑的，它干两件事：一是可靠地完成连接终止——万一最后那个 ACK 丢了，被动关闭端会重发 FIN，主动关闭端必须留着状态好去回 ACK，不留的话就只能回 RST，对端就报错；二是让网络上这个连接的"老分组"有时间消亡，免得它们被误当成同一个 `IP:端口四元组` 的新连接的数据。所以 `TIME_WAIT` 是 TCP 可靠性的一部分，我们要做的不是消灭它，而是在它存在的时候，**还能把服务端重新拉起来**。

### SO_REUSEADDR 到底救不救你——本机实测

很多人对 `SO_REUSEADDR` 有个根深蒂固的误解，觉得"加一行就能随便重绑端口"。我一开始也这么以为，直到自己写了个最小复现去打脸。下面这个自包含程序，父进程是服务端，`fork` 出一个客户端连进来，服务端回一句后**主动关闭**（端口进入 `TIME_WAIT`），然后**立刻第二次 `bind` 同一个端口**，模拟"服务端重启"。三档对比：不加任何选项 / 只加 `SO_REUSEADDR` / 加 `SO_REUSEADDR + SO_REUSEPORT`：

```c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const char* mode_name(int m) {
    return m == 0 ? "无选项" : m == 1 ? "SO_REUSEADDR" : "REUSEADDR+REUSEPORT";
}

static int make_listen(int mode, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int one = 1;
    if (mode == 1 || mode == 2) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (mode == 2) setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 5) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

int main(int argc, char** argv) {
    int mode = (argc > 1) ? atoi(argv[1]) : 0;
    int port = (argc > 2) ? atoi(argv[2]) : 19020;
    signal(SIGPIPE, SIG_IGN);

    printf("==== 第一次 bind (%s) port=%d ====\n", mode_name(mode), port);
    int lfd = make_listen(mode, port);
    if (lfd < 0) return 1;
    printf("[第一次] bind+listen 成功\n");

    pid_t pid = fork();
    if (pid == 0) { /* 客户端连进来读一句就走 */
        int c = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        if (connect(c, (struct sockaddr*)&a, sizeof(a)) == 0) {
            char buf[16] = {0};
            ssize_t n = read(c, buf, sizeof(buf) - 1);
            printf("[client] 收到 %zd 字节: %s\n", n, buf);
        }
        close(c);
        _exit(0);
    }

    int cfd = accept(lfd, NULL, NULL);
    if (cfd >= 0) {
        write(cfd, "hi", 2);
        close(cfd); /* 服务端主动关闭 -> TIME_WAIT */
    }
    waitpid(pid, NULL, 0);
    close(lfd);
    printf("[第一次] 关闭，端口进入 TIME_WAIT\n");

    printf("==== 第二次 bind (%s) 同端口（模拟重启）====\n", mode_name(mode));
    int lfd2 = make_listen(mode, port);
    if (lfd2 < 0) {
        printf("[第二次] bind 失败 —— TIME_WAIT 还卡着，重绑不了\n");
    } else {
        printf("[第二次] bind 成功 —— 重启可立即接管端口\n");
        close(lfd2);
    }
    return 0;
}
```

三档依次跑（注意 `bind: Address already in use` 是写到 stderr 的，和 stdout 的 `printf` 顺序会错位，是正常现象）：

```text
$ ./reuse_demo 0 19021
bind: Address already in use
==== 第一次 bind (无选项) port=19021 ====
[第一次] bind+listen 成功
[第一次] 关闭，端口进入 TIME_WAIT
==== 第二次 bind (无选项) 同端口（模拟重启）====
[第二次] bind 失败 —— TIME_WAIT 还卡着，重绑不了

$ ./reuse_demo 1 19022
==== 第一次 bind (SO_REUSEADDR) port=19022 ====
[第一次] bind+listen 成功
[client] 收到 2 字节: hi
[第一次] 关闭，端口进入 TIME_WAIT
==== 第二次 bind (SO_REUSEADDR) 同端口（模拟重启）====
[第二次] bind 成功 —— 重启可立即接管端口

$ ./reuse_demo 2 19023
==== 第一次 bind (REUSEADDR+REUSEPORT) port=19023 ====
[第一次] bind+listen 成功
[client] 收到 2 字节: hi
[第一次] 关闭，端口进入 TIME_WAIT
==== 第二次 bind (REUSEADDR+REUSEPORT) 同端口（模拟重启）====
[第二次] bind 成功 —— 重启可立即接管端口
```

结论很清楚：**不加任何选项，服务端重启会被 `TIME_WAIT` 卡死（第二次 bind 失败）；加了 `SO_REUSEADDR`，立刻就能重新接管端口**。所以 `SO_REUSEADDR` 确实是你"服务端重启"场景下必须加的那一行。

### 两个必须知道的细节

但是——事情到这里还没完，`SO_REUSEADDR` 有两个边界细节我专门实测过，踩过的人都懂：

> **细节 1：`setsockopt` 必须在 `bind` 之前调用。** 这是高频翻车点。选项是设置在 socket 上的，而 `bind` 是去占用地址的动作，顺序反了不生效。养成肌肉记忆：`socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen`。

> **细节 2：`SO_REUSEADDR` 不等于"能让两个监听 socket 同时绑同一个端口"。** 很多人（包括我当年）以为它有 BSD 那种"端口共享"的能力。我在本机用一个最小双进程测试打过脸：两个进程都开 `SO_REUSEADDR` 去监听同一端口，第二个照样 `bind` 失败。要真正让多个监听 socket 共享一个端口（比如多进程负载均衡），Linux 上需要 `SO_REUSEPORT`（内核 3.9+），而且两个进程都得开它才行。一句话记住——`SO_REUSEADDR` 解决的是"我自己重启能不能绑回来"，`SO_REUSEPORT` 解决的是"好几个进程能不能一起监听"。

所以写服务端的肌肉模板就是这一段，背下来：

```c
int lfd = socket(AF_INET, SOCK_STREAM, 0);
int one = 1;
setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)); /* 必在 bind 前 */
/* ... bind / listen ... */
```

## SIGPIPE：往死连接上 write，进程被无声干掉

`SIGPIPE` 是线上服务最阴险的杀手之一，它的触发条件是：**你往一个已经被对端关闭的连接上 `write`**。TCP 协议规范说这种情况下对方会回一个 RST，而内核收到 RST 后的处理是——给你的进程发 `SIGPIPE`。这个信号的**默认行为是直接终止进程**，连个遗言、连个 core 都不留。

你在本地写个回声服务测着好好的，因为数据量小、时序对得上；一旦上了真实网络，对端先关连接、你后写的概率大得很，进程说没就没，日志里只剩一行"被信号 13 杀死"。下面这个自包含程序还原了整个现场：服务端把客户端发来的数据读空后**优雅关闭**（走 FIN 而不是 RST），客户端继续往这条已半关闭的连接上写——

```c
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int ignore = (argc > 1) ? atoi(argv[1]) : 0;
    int port = (argc > 2) ? atoi(argv[2]) : 19030;
    if (ignore) {
        signal(SIGPIPE, SIG_IGN);
        printf("[main] SIGPIPE 已被忽略\n");
        fflush(stdout);
    } else {
        printf("[main] 不处理 SIGPIPE（默认行为：杀死进程）\n");
        fflush(stdout);
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(lfd, (struct sockaddr*)&a, sizeof(a));
    listen(lfd, 5);

    pid_t pid = fork();
    if (pid == 0) { /* 客户端：连上后疯狂写 */
        int c = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
        if (connect(c, (struct sockaddr*)&sa, sizeof(sa)) < 0) _exit(1);

        usleep(100000); /* 给服务端时间读一个字节然后关掉 */
        int total = 0;
        for (int i = 0; i < 100000; i++) {
            ssize_t n = write(c, "x", 1);
            if (n > 0) total += (int)n;
            else if (n < 0) {
                printf("[client] write 返回 -1, errno=%d (%s)\n", errno, strerror(errno));
                fflush(stdout);
                break;
            }
        }
        printf("[client] 共写入 %d 字节后退出 (mode=%d)\n", total, ignore);
        fflush(stdout);
        close(c);
        _exit(0);
    }

    int cfd = accept(lfd, NULL, NULL);
    /* 把客户端发来的数据尽量读空，保证 close 走优雅 FIN（而不是缓冲区满触发的 RST），
     * 这样客户端再 write 才稳定触发 SIGPIPE/EPIPE。用带超时的 select 读，避免阻塞。 */
    ssize_t got = 0;
    char drain[4096];
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(cfd, &rfds);
        struct timeval tv = {.tv_sec = 0, .tv_usec = 50000};
        int r = select(cfd + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) break;
        ssize_t n = read(cfd, drain, sizeof(drain));
        if (n <= 0) break;
        got += n;
    }
    printf("[server] 已读完 %zd 字节，优雅关闭连接（发 FIN）\n", got);
    close(cfd);
    close(lfd);

    int status;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status)) {
        printf("[parent] 客户端被信号 %d (SIGPIPE) 杀死！\n", WTERMSIG(status));
    } else {
        printf("[parent] 客户端正常退出, exit code=%d\n", WEXITSTATUS(status));
    }
    return 0;
}
```

两档跑一遍，对比极其直白：

```text
$ ./sigpipe_demo 0 19061
[main] 不处理 SIGPIPE（默认行为：杀死进程）
[server] 已读完 0 字节，优雅关闭连接（发 FIN）
[parent] 客户端被信号 13 (SIGPIPE) 杀死！

$ ./sigpipe_demo 1 19062
[main] SIGPIPE 已被忽略
[client] write 返回 -1, errno=32 (Broken pipe)
[client] 共写入 233 字节后退出 (mode=1)
[server] 已读完 0 字节，优雅关闭连接（发 FIN）
[parent] 客户端正常退出, exit code=0
```

不处理时，客户端进程被信号 **13（SIGPIPE）** 直接干掉，`write` 那一行的返回值你根本没机会看到；忽略之后，`write` 改成返回 `-1`，`errno` 是 **32（`EPIPE` / Broken pipe）**——这才是你能在代码里检查、能打日志、能走正常错误处理的形态。

这里有个我亲自踩过的坑值得多说一句：要让 SIGPIPE **稳定**触发，服务端那侧的关闭必须走"优雅 FIN"。如果你像第一版那样让服务端只读一个字节就 `close`，客户端那个紧密的写循环会把服务端接收缓冲区顶满，内核这时候发的就不是 FIN 而是 **RST**（连接重置），于是客户端 `write` 拿到的不是 SIGPIPE/EPIPE，而是 `errno=104`（`ECONNRESET`）。同样是"写坏连接"，触发的错误却完全不一样——这就是为什么服务端那一段我特意加了"先把数据读空再关"的逻辑。你在真实代码里排查"为什么我的进程没被 SIGPIPE 杀、却收到了 ECONNRESET"的时候，记得往这个方向想。

治法有两条路，我推荐组合用：一是在程序最开头 `signal(SIGPIPE, SIG_IGN)`（更现代的写法是 `sigaction`），全局忽略；二是对于单次关键的 `write`/`send`，用带 `MSG_NOSIGNAL` 标志的 `send(fd, buf, n, MSG_NOSIGNAL)`，让这一次发送"写坏连接不触发信号"而只返回错误。两者都做最稳。忽略之后别忘了：`write` 返回 `-1` 你得检查 `errno`，`EPIPE` 表示对端关了，`ECONNRESET` 表示对端重置了连接，`EINTR` 表示被信号打断要重试。

## 消息边界：TCP 是字节流，不是消息

### 粘包和半包从哪来

`SIGPIPE` 解决了"写坏连接不挂进程"，但没解决"我怎么知道一条消息读完了"。这是 TCP 最根本的特性：**它是字节流，没有消息边界**。你一次 `write` 100 字节，对端可能分两次 `read` 才收齐（半包），也可能把这次 100 字节和下次 50 字节粘在一次 `read` 里收回来（粘包）。上一章那个 SC1 "发一句就关连接"的例子之所以没翻车，是因为它根本不依赖边界——发完就 close，对端读到 EOF 就结束。一旦你的协议是"客户端发一条、服务端回一条、再来一条"这种多轮交互，边界问题立刻爆开。

解决办法只有一条：**在应用层自己定边界**。最通用、最好实现的方式是"长度前缀"——每条消息前面固定 N 个字节存这条消息有多长（用网络字节序，因为它是要跨网络的整数），后面跟那 N 个字节的载荷。这样接收端永远先读定长的"长度字段"，再按那个长度去读正好那么多的字节，一条消息就严丝合缝。

### readn / writen：循环读写直到凑够 N 字节

定边界还差一块拼图：`read`/`write` 本身可能返回**不足量**——你让它读 100 字节，它可能这次只给你 80 字节就返回了，这不是错误，是内核缓冲区的限制。所以我们必须自己包一层"循环读写，直到凑够 N 字节"的函数，这就是 UNP 里经典的 `readn`/`writen`。注意它俩都把 `void*` 转成 `char*` 再 `+n` 偏移，因为 C 不允许对 `void*` 直接做算术：

```c
#define _DEFAULT_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

/* 从 fd 精确读 n 字节（处理 EINTR 与 EOF）；返回实际读到的字节数 */
static ssize_t read_n(int fd, void* buf, size_t n) {
    size_t left = n;
    char* p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) { r = 0; } /* 被信号打断，重试 */
            else return -1;
        } else if (r == 0) {
            break; /* EOF：对端关闭 */
        }
        left -= (size_t)r;
        p += r;
    }
    return (ssize_t)(n - left);
}

/* 向 fd 精确写 n 字节（处理 EINTR 与短写）；成功返回 n */
static ssize_t write_n(int fd, const void* buf, size_t n) {
    size_t left = n;
    const char* p = buf;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) { w = 0; }
            else return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)n;
}
```

你看这两个函数里都处理了 `EINTR`——`read`/`write` 是可能被信号打断的慢系统调用，正确做法是重试而不是直接报错退出。`read_n` 返回值还有个细节：**EOF 也返回已读到的字节数（而不是 -1）**，调用方据此判断"对端关闭时这条消息读全了没"。

### 完整的长度前缀回声服务

把这俩函数和"长度前缀协议"拼起来，下面这个自包含程序就是一个真正能抗粘包的回声服务。父进程做服务端，`fork` 出客户端连发三条不同长度的消息，服务端逐条收、逐条原样回射：

```c
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* read_n / write_n 实现见上一段，此处省略 */

static void echo_server(int cfd) {
    for (;;) {
        uint32_t net_len = 0;
        ssize_t r = read_n(cfd, &net_len, sizeof(net_len));
        if (r == 0) { printf("[server] 客户端关闭连接\n"); return; }
        if (r != (ssize_t)sizeof(net_len)) { perror("read len"); return; }
        uint32_t len = ntohl(net_len);          /* 网络序 -> 主机序 */
        char buf[256];
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        if (read_n(cfd, buf, len) != (ssize_t)len) { perror("read body"); return; }
        buf[len] = '\0';
        printf("[server] 收到 %u 字节: \"%s\"\n", len, buf);

        /* 原样回射：先发长度，再发载荷 */
        write_n(cfd, &net_len, sizeof(net_len));
        write_n(cfd, buf, len);
    }
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 19050;
    signal(SIGPIPE, SIG_IGN);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(lfd, (struct sockaddr*)&a, sizeof(a));
    listen(lfd, 5);

    pid_t pid = fork();
    if (pid == 0) { /* 客户端：连发 3 条不同长度消息 */
        setvbuf(stdout, NULL, _IONBF, 0);
        int c = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
        connect(c, (struct sockaddr*)&sa, sizeof(sa));

        const char* msgs[] = {"ping", "Hello-World-Frame", "abc"};
        for (int i = 0; i < 3; i++) {
            uint32_t len = (uint32_t)strlen(msgs[i]);
            uint32_t net_len = htonl(len);
            write_n(c, &net_len, sizeof(net_len));
            write_n(c, msgs[i], len);
        }
        for (int i = 0; i < 3; i++) {
            uint32_t net_len = 0;
            if (read_n(c, &net_len, sizeof(net_len)) != (ssize_t)sizeof(net_len)) break;
            uint32_t len = ntohl(net_len);
            char buf[256];
            if (read_n(c, buf, len) != (ssize_t)len) break;
            buf[len] = '\0';
            printf("[client] 回射 %u 字节: \"%s\"\n", len, buf);
        }
        close(c);
        _exit(0);
    }

    int cfd = accept(lfd, NULL, NULL);
    echo_server(cfd);
    close(cfd);
    close(lfd);
    waitpid(pid, NULL, 0);
    return 0;
}
```

跑一次，三条长度各异的消息全部精确地一来一回，没有任何粘连、没有任何截断：

```text
$ ./frame_demo 19052
[client] 回射 4 字节: "ping"
[client] 回射 17 字节: "Hello-World-Frame"
[client] 回射 3 字节: "abc"
[server] 收到 4 字节: "ping"
[server] 收到 17 字节: "Hello-World-Frame"
[server] 收到 3 字节: "abc"
[server] 客户端关闭连接
```

你看，服务端清清楚楚地分三次各收到 `4 / 17 / 3` 字节，长度完全对得上——这正是"长度前缀 + readn/writen"的威力。不管内核这次把数据切成几块、粘了几块，协议自己保证一条就是一条。这也是为什么上一章埋的那个"SC1 多发了一个 `\0`"的伏笔在这里会咬人：一旦你按长度收发，多一个字节都会让长度对不齐，所以发之前务必想清楚"这条消息到底算几个字节"。

> **踩坑预警**：别用 stdio（`FILE*`、`fread`/`fgets`）来缓冲网络 socket。stdio 的缓冲区状态对你不可见，会把"什么时候算一条消息"彻底搅乱，UNP 里专门花了篇幅警告这个。文本行协议（HTTP、SMTP、FTP 控制连接那种按 `\n` 分界）看着诱人，但防御性编程要求你能检测并丢弃非预期数据，而 stdio 缓冲会让你"看不清缓冲区里到底剩了什么"。按字节缓冲、自己读、按长度或分隔符切，才是稳的。

## getaddrinfo：别再写 gethostbyname 了

地址解析这块，旧代码里满眼都是 `gethostbyname`——它返回一个 `hostent`，只能解析 IPv4，而且是个返回静态缓冲区的非可重入函数，多线程里一调就出幺蛾子。现代做法是 `getaddrinfo`，它一次性解决三件事：**协议无关（IPv4/IPv6 都能出）、可重入、把"填地址结构体"这件烦事直接帮你做了**。你给它一个主机名（或 IP 串）和一个服务名（或端口串），加一个 `hints` 说明你想要什么样的结果，它就吐回一串链表，每一条都是一块现成可用的 `addrinfo`（含 `ai_family`、`ai_socktype`、`ai_addr`、`ai_addrlen`），你直接拿去 `socket` + `connect` 就行。

下面这个小程序就是 `getaddrinfo` 的标准用法——`hints` 里把 `ai_family` 设成 `AF_UNSPEC`（IPv4/IPv6 都要）、`ai_socktype` 设成 `SOCK_STREAM`（只要 TCP），然后把解析出的每条候选地址打印出来：

```c
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
    const char* host = (argc > 1) ? argv[1] : "localhost";
    const char* serv = (argc > 2) ? argv[2] : "19060";

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* IPv4 或 IPv6 都行 */
    hints.ai_socktype = SOCK_STREAM;   /* 只要 TCP */

    struct addrinfo* res = NULL;
    int err = getaddrinfo(host, serv, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    printf("解析 %s:%s 的候选地址：\n", host, serv);
    char ipstr[INET6_ADDRSTRLEN];
    int idx = 0;
    for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
        void* addr;
        const char* fam = (p->ai_family == AF_INET) ? "IPv4" : "IPv6";
        if (p->ai_family == AF_INET)
            addr = &((struct sockaddr_in*)p->ai_addr)->sin_addr;
        else
            addr = &((struct sockaddr_in6*)p->ai_addr)->sin6_addr;
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        printf("  [%d] %s %s  socktype=%d\n", idx++, fam, ipstr, p->ai_socktype);
    }

    freeaddrinfo(res); /* 用完一定要释放，否则内存泄漏 */
    return 0;
}
```

几个真实场景的输出，注意它对 IPv6 字面量、服务名（`http` = 80）、解析失败都处理得很干净：

```text
$ ./addrinfo_demo localhost 19060
解析 localhost:19060 的候选地址：
  [0] IPv4 127.0.0.1  socktype=1

$ ./addrinfo_demo ::1 19060
解析 ::1:19060 的候选地址：
  [0] IPv6 ::1  socktype=1

$ ./addrinfo_demo localhost http
解析 localhost:http 的候选地址：
  [0] IPv4 127.0.0.1  socktype=1

$ ./addrinfo_demo nohost.invalid.example 19060
getaddrinfo: Name or service not known
```

几个要点记一下：`getaddrinfo` 返回 0 才是成功，非 0 是错误码，用 `gai_strerror` 转成人话（**注意它不走 `errno`**，和大多数系统调用不一样）；返回的是个链表，可能有多个候选（一个主机名对应多个 IP、或同时有 TCP/UDP），实践中通常循环尝试，哪个 `connect` 成功就用哪个；用完必须 `freeaddrinfo` 释放。`hints` 里那个 `AI_PASSIVE` 标志也值得知道——服务端想绑 `INADDR_ANY` 的话，`getaddrinfo(NULL, port, &hints, &res)` 配合 `AI_PASSIVE` 会直接给你返回适合 `bind` 的通配地址，省得自己手写。

## 并发服务端入门：一个 select 循环伺候多个连接

到这里我们手里已经有了能抗坑的服务端骨架，但它还是**一次只能服务一个连接**——`accept` 一个，慢慢处理，处理完回去再 `accept` 下一个。这种"迭代服务器"在真实负载下根本没法用：一个慢客户端就能把后面所有人卡死。并发的路子有好几条（`fork` 每个连接一个子进程、`pthread` 每个连接一个线程、IO 多路复用），其中 `fork` 在 UNP 里讲得最经典——`accept` 返回后立刻 `fork`，子进程拿着已连接 socket 服务客户，父进程 `close` 掉已连接 socket（只把引用计数从 2 减到 1，真正的关闭要等子进程也关）然后回去继续 `accept`。但 `fork` 在高并发下进程开销大，而且还有 `SIGCHLD` 僵尸回收的麻烦，所以现代服务端更常用的是**单进程 + IO 多路复用**。

`select` 是最古老的多路复用接口，思路朴素但能讲清楚所有多路复用的本质：**你把一堆你关心的 fd 塞进一个 `fd_set`，然后调 `select` 去睡觉；这些 fd 里只要任何一个"有事"（可读/可写/异常），`select` 就醒来告诉你有几个 ready，你再去挨个查是谁、处理谁**。服务端要做的事情就变成了维护一个"我关心的 fd 集合"——其中一个是监听 socket（它 ready 表示有新连接），其余是已连接 socket（它们 ready 表示有数据可读、或对端关了）。

下面这个自包含程序就是一个最小的 `select` 并发服务端：父进程跑 select 循环，`fork` 出三个客户端故意错开连接、连上后 hold 住连接制造"多个客户端同时在线"，服务端用一个 `fd_set` 把监听 fd 和所有已连接 fd 一起管，谁可读就处理谁：

```c
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 19070;
    signal(SIGPIPE, SIG_IGN);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(lfd, (struct sockaddr*)&a, sizeof(a));
    listen(lfd, 8);

    const char* msgs[3] = {"from-A", "from-B", "from-C"};
    for (int i = 0; i < 3; i++) {
        if (fork() == 0) { /* 错开连接，制造同时在线 */
            setvbuf(stdout, NULL, _IONBF, 0);
            usleep(20000 + i * 50000);
            int c = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in sa;
            memset(&sa, 0, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
            connect(c, (struct sockaddr*)&sa, sizeof(sa));
            write(c, msgs[i], strlen(msgs[i]));
            usleep(400000); /* 故意 hold 连接 */
            char buf[64] = {0};
            ssize_t n = read(c, buf, sizeof(buf) - 1);
            printf("[client %c] 回射 %zd 字节: %s\n", 'A' + i, n, buf);
            fflush(stdout);
            close(c);
            _exit(0);
        }
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(lfd, &master);
    int maxfd = lfd;
    int active_clients = 0, served = 0;

    for (;;) {
        fd_set readfds = master;
        struct timeval tv = {.tv_sec = 1, .tv_usec = 500000};
        int nready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (nready < 0) { perror("select"); break; }
        if (nready == 0) { if (served >= 3) break; continue; }

        if (FD_ISSET(lfd, &readfds)) { /* 监听 fd 可读 = 有新连接 */
            int cfd = accept(lfd, NULL, NULL);
            if (cfd >= 0) {
                FD_SET(cfd, &master);
                if (cfd > maxfd) maxfd = cfd;
                active_clients++;
                printf("[server] 新连接 fd=%d (在线 %d)\n", cfd, active_clients);
            }
            nready--;
        }

        for (int fd = lfd + 1; fd <= maxfd && nready > 0; fd++) {
            if (!FD_ISSET(fd, &readfds)) continue;
            char buf[128];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            if (n <= 0) { /* EOF 或出错：客户端断了 */
                printf("[server] fd=%d 断开\n", fd);
                close(fd);
                FD_CLR(fd, &master);
                active_clients--;
            } else {
                buf[n] = '\0';
                printf("[server] fd=%d 收到 %zd 字节: %s\n", fd, n, buf);
                write(fd, buf, (size_t)n); /* 回射 */
                served++;
            }
            nready--;
        }
    }

    close(lfd);
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    return 0;
}
```

跑出来的真实输出最能说明问题——**三个客户端被同一个 select 循环同时接住**，在线数一路涨到 3，三条回射各自走通：

```text
$ ./select_demo 19074
[server] 新连接 fd=4 (在线 1)
[server] fd=4 收到 6 字节: from-A
[server] 新连接 fd=5 (在线 2)
[server] fd=5 收到 6 字节: from-B
[server] 新连接 fd=6 (在线 3)
[server] fd=6 收到 6 字节: from-C
[client A] 回射 6 字节: from-A
[client B] 回射 6 字节: from-B
[client C] 回射 6 字节: from-C
[server] fd=4 断开
[server] fd=5 断开
[server] fd=6 断开
```

你看，服务端只有一个进程、一个 select 循环，却同时管着监听 fd（4 之外的）和三个已连接 fd（4/5/6）。`FD_SET(lfd, &master)` 把监听 socket 也纳入监控，它一 ready 就 `accept` 新连接并把新 fd 加进 `master`；已连接 fd 一 ready 就 `read`，读到 0 说明对端关了，`FD_CLR` 把它从集合里摘掉。这就是单线程并发的全部秘密。

> **`select` 的硬伤**：它用 `fd_set`（位图）存关心哪些 fd，**fd 数量受 `FD_SETSIZE` 限制，默认 1024**——超过这个数直接没法监控，这是 `select` 在高并发场景被 `epoll` 淘汰的头号原因。其次是每次调用都得把整个 `fd_set` 从用户态拷到内核态、内核再线性扫一遍，连接多了开销线性涨。`poll` 解了数量限制（用数组代替位图）但仍是线性扫描；Linux 上的 `epoll` 才是真正面向"上万连接"的方案——内核帮你维护就绪表，`epoll_wait` 只返回真正 ready 的那几个。这些我们留到专门讲 IO 多路复用的那一篇展开，这一篇你只要抓住"一个循环 + 一个 fd 集合"的骨架就够了。

## 常见踩坑汇总

把这一篇散落各处的坑收到一起，方便回头查：

> **坑 1：`SO_REUSEADDR` 加在了 `bind` 之后。** 不生效，服务端重启照样 `Address already in use`。顺序死记：`socket → setsockopt → bind → listen`。

> **坑 2：以为 `SO_REUSEADDR` 能让两个服务进程共享同一端口。** 不能。要端口共享（多进程负载均衡）得用 `SO_REUSEPORT`，而且所有相关进程都得开它。

> **坑 3：不处理 `SIGPIPE`，线上进程被无声干掉。** 往已关闭连接上 `write` 就触发，默认行为是终止进程。开头 `signal(SIGPIPE, SIG_IGN)` + 关键发送用 `send(..., MSG_NOSIGNAL)`，双保险。

> **坑 4：把 TCP 当成"消息"。** 它是字节流，会粘包会半包。多轮交互的协议必须自己定边界——长度前缀是最稳的，配 `readn`/`writen` 循环读写。

> **坑 5：`read`/`write` 被 `EINTR` 打断就当错误退出。** 慢系统调用被信号打断是常态，正确做法是重试。所以循环读写函数里一定要把 `EINTR` 当成"再来一次"。

> **坑 6：还在用 `gethostbyname`。** 只支持 IPv4、返回静态缓冲区不可重入。换成 `getaddrinfo`，协议无关、可重入、还顺手把地址结构体填好了，记得 `freeaddrinfo`。

> **坑 7：`select` 上 fd 超过 1024。** `FD_SETSIZE` 默认 1024，超出会出问题。海量连接直接上 `epoll`。

## 小结

这一篇把一个"能跑的玩具服务端"改造成了"在真实环境下不会炸的服务端"，关键点 checklist：

- [ ] 多字节整数（端口、IP）进结构体前一律 `htonl`/`htons`，否则跨架构必踩
- [ ] 服务端 `bind` 前必加 `SO_REUSEADDR`，重启才不被 `TIME_WAIT` 卡住
- [ ] `TIME_WAIT` 是主动关闭端的 1～4 分钟占位，是 TCP 可靠性的一部分，不是 bug
- [ ] 全局忽略 `SIGPIPE`，或用 `MSG_NOSIGNAL`，防止进程被无声杀死
- [ ] 多轮消息协议用"长度前缀 + `readn`/`writen`"定边界，根治粘包/半包
- [ ] 循环读写里处理 `EINTR`，被信号打断要重试
- [ ] 地址解析用 `getaddrinfo`，告别 `gethostbyname`，用完 `freeaddrinfo`
- [ ] 并发服务端的骨架是"一个循环 + 一个 fd 集合"，`select` 是入门，海量连接上 `epoll`

## 练习

- [ ] 把第一篇的 SC1/SC2 服务端改造成"加了 `SO_REUSEADDR`、忽略 `SIGPIPE`、`read` 检查 `EINTR`"的生产级骨架，验证快速重启不再报错。
- [ ] 把本篇的长度前缀回声协议，从"客户端发、服务端原样回"改成"客户端发一个算式（如 `3+4`）、服务端算出结果按长度前缀回"，体会定边界的协议设计。
- [ ] 把 `select` 例子里的回射，改成"用上一节的长度前缀协议"做回声——你会发现 `select` + `readn` 组合时，一次 `read` 可能只读到半个长度字段，要做一个"按连接累积缓冲区、够 4 字节才解长度、够 N 字节才解载荷"的状态机。这是真实的工业级回声服务雏形。
- [ ] 用 `getaddrinfo` + `AI_PASSIVE` 重写服务端的 `bind` 部分，让服务端也能同时支持 IPv4/IPv6 连入（提示：`AF_INET6` + 关闭 `IPV6_V6ONLY`）。

## 参考资源

- `man 2 setsockopt` / `man 7 socket` / `man 7 tcp`——`SO_REUSEADDR`、`SO_REUSEPORT`、`TIME_WAIT` 的权威出处
- `man 3 getaddrinfo`——现代地址解析的一手资料，`gai_strerror`、`freeaddrinfo` 都在这
- `man 2 select` / `man 3 select_tut`——`fd_set`、`FD_SETSIZE`、超时语义
- W. Richard Stevens 等，《UNIX Network Programming, Volume 1: The Sockets Networking API》——本章主题（`TIME_WAIT`、`readn`/`writen`、`SIGCHLD`/并发服务器、`getaddrinfo`）的原始出处
- Michael Kerrisk，《The Linux Programming Interface》——第 60–61 章把 socket 选项和 `select` 讲得极透

---
*整理自作者笔记，按 C-Journey 写作规范重写；所有输出本机实测捕获。*
