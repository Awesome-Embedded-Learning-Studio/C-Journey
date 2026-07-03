---
title: "Socket TCP：从零写客户端/服务端四件套"
description: "前面所有 IPC 都只能在同一台机器上的进程间传数据(pipe、共享内存),这一章把 fd 推广到网络——socket。socket 就是『网络版 fd』,既能像 pipe 一样收发字节、又能跨机器跨网络。这一章亲手走通 TCP 服务端的『四件套』+ 客户端的『两件套』,全程在 127.0.0.1 回环上自连自(一个程序 fork 出服务端子进程和客户端子进程)。服务端四件套:socket(AF_INET,SOCK_STREAM,0) 建监听 fd、bind 把它绑到 127.0.0.1:端口(端口传 0 让内核挑空闲的、再用 getsockname 查回来)、listen 把它标成被动套接字愿意接连接、accept 阻塞等连接进来、返回一个新 fd 专门跟这个客户端聊。客户端两件套:socket 建 fd、connect 连到服务端地址。真跑 tcp_basic:服务端监听 127.0.0.1:45547(端口每次跑不同)、客户端 sleep 1 秒后 connect 上来发 10 字节「hello tcp」、服务端 read 收到、再 read 返回 0——TCP 上 read 返回 0 就是『对端关连接了』(EOF,跟 pipe 一样)、不是错误。重点讲字节序:网络规定『网络字节序』(大端),而你的机器很可能是小端,所以端口和地址塞进 sockaddr 前必须 htonl/htons 转一下、取出来 ntohs/ntohl 转回来,否则连的端口就是错的(端口 12345 会被当成 11565 去连)。全 gcc16+clang22 真跑。"
chapter: 5
order: 11
tags:
  - host
  - system-programming
  - posix
  - networking
  - socket
difficulty: advanced
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 6 章：IPC 上（pipe、fd 上的 read/write、read 返回 0 = EOF）"
  - "第 1 章：文件 IO 与 fd（fd 抽象、read/write 接口）"
  - "第 2 章：进程的诞生（fork,这里用来一个程序里同时跑服务端和客户端）"
related:
  - "第 12 章：进阶 Socket（SIGPIPE、消息边界、用 epoll 推向并发服务端）"
  - "第 13 章：UDP 与本地域套接字（无连接 socket、同机高速 IPC）"
---

# Socket TCP：从零写客户端/服务端四件套

## 引言：socket 是「网络版 fd」

到此我们手里的 IPC——pipe、共享内存——都只能在**同一台机器**上的进程之间传数据。可真正的网络程序(浏览器、聊天、数据库远程连接)要在**不同机器**之间传。这就需要 **socket**:它是个「**网络版 fd**」,既继承了普通 fd 的 `read`/`write` 接口(用起来跟 pipe 一样),又能跨机器、跨网络收发字节。socket 还分「**流式**」(`SOCK_STREAM`,就是 TCP,可靠、有序、面向连接)和「**数据报**」(`SOCK_DGRAM`,就是 UDP,下一章讲)。这一章我们只搞 TCP,亲手把服务端和客户端的最小骨架敲出来——为了能在**一个程序里**演示,我 fork 出一个子进程当客户端、父进程当服务端,两边都在 `127.0.0.1`(回环地址,只走本机网卡、不出去)上自连自。

## 服务端四件套:socket / bind / listen / accept

一个 TCP 服务端要经历四步,缺一步都跑不起来。先用 `socket` 建一个监听 fd,用 `bind` 把它绑到一个地址(IP+端口)上,用 `listen` 把它标成「被动套接字」(愿意接连接),最后用 `accept` 阻塞等客户端来连、每来一个连接就 `accept` 返回一个**新 fd** 专门跟那个客户端聊。四步的接口和它配合的客户端,真跑给你看(为了篇幅,客户端那段也塞在同一个程序里):

```c
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    /* 服务端四件套之 1、2:socket + bind(绑 127.0.0.1,端口 0 = 让内核挑) */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);                      /* 端口 0:内核分配一个空闲端口 */
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1,只接受本机连接 */
    if (bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    /* 3:listen,把 fd 标成「被动套接字」,愿意接连接 */
    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    /* bind 端口 0 后,getsockname 查内核到底分了哪个端口(要告诉客户端) */
    socklen_t alen = sizeof(addr);
    getsockname(listen_fd, (struct sockaddr*) &addr, &alen);
    int port = ntohs(addr.sin_port);
    printf("[server] 监听 127.0.0.1:%d\n", port);
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子进程 = 客户端 */
        close(listen_fd);
        sleep(1); /* 等服务端 listen 就绪 */
        int cfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(port); /* 网络字节序 */
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(cfd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
            perror("connect");
            _exit(1);
        }
        write(cfd, "hello tcp\n", 10);
        close(cfd); /* 关写端 → 服务端 read 拿到 EOF */
        _exit(0);
    }

    /* 父进程 = 服务端。4:accept 阻塞等连接进来,返回一个新 fd 专门跟这个客户端聊 */
    int cfd = accept(listen_fd, NULL, NULL);
    if (cfd < 0) {
        perror("accept");
        return 1;
    }
    char buf[64];
    ssize_t n = read(cfd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[server] 收到 %zd 字节: %s", (long) n, buf);
    }
    n = read(cfd, buf, sizeof(buf) - 1); /* 客户端关了 → read 返回 0(EOF) */
    printf("[server] 再读返回 %zd(0 = 客户端关连接了)\n", (long) n);

    close(cfd);
    close(listen_fd);
    waitpid(pid, NULL, 0);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall tcp_basic.c -o tcp && ./tcp
[server] 监听 127.0.0.1:45547
[server] 收到 10 字节: hello tcp
[server] 再读返回 0(0 = 客户端关连接了)
```

逐条拆这四件套。`socket(AF_INET, SOCK_STREAM, 0)` 建一个 IPv4 的 TCP socket(`AF_INET` 地址族、`SOCK_STREAM` 流式),返回一个 fd——此刻它还只是个「光杆 socket」,没地址、不能通信。`bind` 把它绑到 `127.0.0.1` 的某个端口上(`struct sockaddr_in` 是 IPv4 地址结构,填地址族、端口、IP)——这一步是「**登记门牌号**」,客户端照着这个 IP+端口才能找到你。`listen` 把这个 fd 从「主动」(默认的、用来 `connect` 的)切到「**被动**」(愿意 `accept` 别人来连),第二个参数是「**等待连接队列**」的长度(我传 1)。最后 `accept` 阻塞——直到有客户端 `connect` 进来,它返回一个**全新的 fd**(叫连接 fd),这个新 fd 才是真正「跟那个客户端一对一聊数据」用的;原来的 `listen_fd` 继续守着、接下一个连接(所以一个服务端能 `accept` 很多次、每次拿一个新连接 fd)。真跑输出里客户端发了 10 字节 `hello tcp`、服务端 `read` 收到,跟 pipe 一模一样——socket 在数据收发层面和普通 fd 没区别,`read`/`write` 直接能用。

服务端绑端口这里有个小技巧值得记:**我把端口写成 0、让内核挑一个空闲的**,然后 `getsockname` 查回内核到底分了哪个端口(真跑里是 `45547`,每次跑都不同)。为啥不写死一个端口(比如 8080)?因为写死的端口可能被别的程序占着、`bind` 会失败(`EADDRINUSE`);端口 0 让内核挑一定成功,代价是端口不定——真实服务端通常写死固定端口(方便客户端找),教学和测试用 0 最省心。

## 客户端两件套:socket / connect

客户端简单得多,只要两步:`socket` 建一个 fd(跟服务端一样的 `AF_INET`/`SOCK_STREAM`),`connect` 连到服务端的地址(IP+端口)。`connect` 的参数和服务端的 `bind` 一样是 `struct sockaddr_in`——填服务端的 IP 和端口。`connect` 成功后,这个 fd 就跟服务端那条 TCP 连接绑定了,直接 `write`/`read` 收发数据就行(代码里客户端 `write(cfd, "hello tcp\n", 10)` 就把数据发出去了)。注意客户端**不用 `bind`**——内核会在 `connect` 时自动给它分配一个临时端口(叫「临时端口」/ephemeral port),客户端不关心自己绑哪个端口、只关心连到服务端的哪个端口。

## 字节序:htonl / htons 别漏

代码里那些 `htonl`/`htons`/`ntohs` 不是装饰,漏了就是大坑。背景:网络协议规定传输多字节整数(端口号、IP 地址)时用「**网络字节序**」(network byte order,POSIX 规定是大端 big-endian);可你的 CPU 很可能是小端(x86、ARM 都是小端)。「端口 12345」在小端机器内存里和在大端表示里**字节顺序相反**,你要是把小端的 12345 直接塞进 `sockaddr_in.sin_port`,网络层会把它读成另一个数(小端 12345 = `0x3039`,字节 `39 30`;大端读成 `0x3930` = 14640),结果连到**完全错误的端口**。所以规矩是:**凡是往 `sockaddr_in` 里塞端口、IP,先 `htons`/`htonl`(host to network,主机→网络)转一下;从里面读出来,`ntohs`/`ntohl`(network to host)转回来**。`htons` 转短整数(端口是 16 位 `uint16_t`)、`htonl` 转长整数(IP 是 32 位 `uint32_t`)。代码里服务端 `addr.sin_port = htons(0)`、`addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK)`,客户端 `saddr.sin_port = htons(port)`,都是这个道理;`getsockname` 读回端口时 `ntohs(addr.sin_port)` 转回主机序才能 `printf`。这些函数在 `<arpa/inet.h>`。

## read 返回 0 = 对端关连接

最后讲一个跟 pipe 完全一样的语义:**socket 上 `read` 返回 `0`,表示「对端关连接了」(EOF),不是错误**。真跑里客户端 `write` 完数据就 `close(cfd)`,这关掉了它那一端的连接;服务端第一次 `read` 收到 10 字节数据,第二次 `read` 返回 `0`——这个 `0` 就是 TCP 的「对端发了 FIN、连接要关了」的信号。所以服务端读 socket 的循环和读 pipe 一模一样:`while ((n = read(cfd, buf, sz)) > 0) { 处理; }`——读到 `0` 就知道客户端断开了、循环结束。把 `read` 返回 `0` 当错误处理、或者干脆不处理 EOF,是写 socket 服务端的高频 bug(连接永远清不掉)。

## 小结

TCP 服务端「四件套」:`socket(AF_INET, SOCK_STREAM, 0)` 建监听 fd → `bind` 绑门牌号(IP+端口)→ `listen` 切成被动套接字 → `accept` 阻塞等连接、每来一个返回一个新连接 fd。客户端「两件套」:`socket` + `connect`(连到服务端地址,无需 `bind`、内核自动分临时端口)。socket 上的 `read`/`write` 跟普通 fd 一样;`read` 返回 `0` 表示对端关连接(EOF,不是错误)。字节序铁律:**往 `sockaddr_in` 塞端口/IP 用 `htons`/`htonl`,读出来用 `ntohs`/`ntohl`**,否则小端机器会把端口搞错、连到错误目标。`bind` 端口传 `0` 让内核挑空闲端口(再 `getsockname` 查回),写死端口可能 `EADDRINUSE`。

到这里我们有了「一个服务端接一个客户端」的最小 TCP。可真实服务端要同时接**很多**客户端——而上面这个 `accept` 一次只能处理一个连接、处理完才能接下一个。下一章进阶 socket:处理「往死连接 write 触发 SIGPIPE 杀进程」的阴险坑、TCP 没有消息边界带来的粘包/半包问题(长度前缀)、并用前面学过的 epoll 把服务端推向「单线程并发处理多连接」。

## 参考资源

- **UNP**：《Unix Network Programming, Volume 1》(W. Richard Stevens / Bill Fenner / Andrew M. Rudoff),第 4–5 章是 TCP 客户端/服务端四件套的圣经级讲解,字节序、`sockaddr` 结构全套。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 56–59 章 socket TCP,`socket`/`bind`/`listen`/`accept`/`connect` 每个步骤配图。
- **man 页**：`socket(2)`、`bind(2)`、`listen(2)`、`accept(2)`、`connect(2)`、`getsockname(2)`、`byteorder(3)`(`htonl`/`htons`/`ntohl`/`ntohs`)、`ip(7)`。
- **POSIX**：`<sys/socket.h>` 的 socket/bind/listen/accept/connect、`<netinet/in.h>` 的 `struct sockaddr_in`/`INADDR_LOOPBACK`、`<arpa/inet.h>` 的字节序函数,全是 IEEE Std 1003.1-2008。
