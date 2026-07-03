---
title: "UDP 与本地域套接字：无连接与同机高速 IPC"
description: "TCP 讲完了,这一章看 socket 的另两副面孔。先是 UDP(SOCK_DGRAM 数据报):跟 TCP 的『字节流』完全相反——它无连接(sendto 直接指定目的地、不用 connect/accept)、不可靠(丢包不重传、乱序)、但『有消息边界』(一次 sendto 对应一次 recvfrom,不会粘包)。真跑 udp.c:服务端 socket(AF_INET,SOCK_DGRAM)+bind,客户端两次 sendto 发『hello udp』『second』,服务端 recvfrom 两次分别收到完整两条——和 TCP 那种粘包/半包形成鲜明对照,UDP 天然按『一条一条消息』收。然后是本地域套接字 AF_UNIX:它用文件系统里的一个路径当『门牌号』(sockaddr_un 的 sun_path),服务端 bind/listen/accept、客户端 connect,接口跟 TCP 一模一样,但只在同机内走、不经网络协议栈,所以比走 127.0.0.1 的 TCP 快不少;真跑 unix_stream.c 在 /tmp/cj/p5ch13/unix.sock 上自连自、收到 hello unix。AF_UNIX 还有个独家本事——能在 socket 上『传 fd』(SCM_RIGHTS 辅助消息,把一个进程的 fd 发给另一个进程),这是 TCP 做不到的。最后散文化对比 TCP 字节流 vs UDP 数据报(可靠/无边界 vs 不可靠/有边界)。全 gcc16+clang22 真跑。"
chapter: 5
order: 13
tags:
  - host
  - system-programming
  - posix
  - networking
  - socket
  - ipc
difficulty: advanced
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 11 章：Socket TCP（socket/bind/listen/accept/connect 四件套）"
  - "第 12 章：进阶 Socket（TCP 字节流没有消息边界——UDP 正好相反）"
  - "第 6 章：IPC 上（同机 IPC 的需求,AF_UNIX 是 socket 族的同机方案）"
related:
  - "第 14 章：getaddrinfo（协议无关的地址解析,UDP/TCP 通用）"
  - "第 7 章：IPC 下（共享内存,AF_UNIX 是另一种同机高速 IPC）"
---

# UDP 与本地域套接字：无连接与同机高速 IPC

## 引言：TCP 之外的两副面孔

TCP 讲完了,可 socket 家族不止它。这一章看另外两个常用成员。一个是 **UDP**(`SOCK_DGRAM`,数据报)——它跟 TCP 的「字节流」哲学正好相反:**无连接、不可靠、但有消息边界**,适合 DNS 查询、视频流、游戏这种「丢一两个包无所谓、但要快」的场景。另一个是**本地域套接字**(`AF_UNIX`)——接口跟 TCP 几乎一样,但只在**同一台机器**内通信、走文件系统路径而非网络,是同机 IPC 里又快又好用的选择,还能干一件 TCP 干不了的事:在 socket 上**传 fd**。

## UDP:无连接、有消息边界的数据报

UDP 的接口比 TCP 简单得多,因为它**没有连接**这个概念——你不用 `connect`/`accept`,直接 `sendto(目标地址)` 把数据报扔出去、对方 `recvfrom` 收。服务端只要 `socket(AF_INET, SOCK_DGRAM, 0)` + `bind`,然后 `recvfrom` 阻塞等数据报进来;客户端连 `bind`/`connect` 都不用,直接 `sendto` 指定目的地。真跑一遍,服务端收两个数据报:

```c
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int sfd = socket(AF_INET, SOCK_DGRAM, 0); /* UDP 数据报 */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(sfd, (struct sockaddr*) &addr, sizeof(addr));

    socklen_t alen = sizeof(addr);
    getsockname(sfd, (struct sockaddr*) &addr, &alen);
    int port = ntohs(addr.sin_port);
    printf("[server] UDP 绑定 127.0.0.1:%d\n", port);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {
        close(sfd);
        sleep(1);
        int cfd = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(port);
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        /* UDP 无连接:直接 sendto 指定目的地,不用 connect */
        sendto(cfd, "hello udp", 9, 0, (struct sockaddr*) &saddr, sizeof(saddr));
        sendto(cfd, "second", 6, 0, (struct sockaddr*) &saddr, sizeof(saddr));
        close(cfd);
        _exit(0);
    }

    /* 服务端:recvfrom 阻塞收数据报。一次 sendto 对应一次 recvfrom(有边界) */
    for (int i = 0; i < 2; i++) {
        char buf[64];
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        ssize_t n = recvfrom(sfd, buf, sizeof(buf) - 1, 0, (struct sockaddr*) &peer, &plen);
        if (n > 0) {
            buf[n] = '\0';
            printf("[server] recvfrom 第 %d 个数据报: %s\n", i + 1, buf);
        }
    }
    close(sfd);
    waitpid(pid, NULL, 0);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall udp.c -o udp && ./udp
[server] UDP 绑定 127.0.0.1:47744
[server] recvfrom 第 1 个数据报: hello udp
[server] recvfrom 第 2 个数据报: second
```

客户端连发两个 `sendto`(`"hello udp"` 和 `"second"`),服务端两个 `recvfrom` 各收到一条、**完整、不粘连**——这就是 UDP 「**有消息边界**」的可观测后果:每个 `sendto` 是一个独立的数据报、对端一次 `recvfrom` 整个拿走那条、不会跟下一条粘到一起。回想上一章 TCP 的粘包/半包烦恼——UDP 天然没这问题,因为它本来就是「按消息」收发的。代价是它的另两个属性:**无连接**(没有 TCP 那种三次握手、建立连接的状态)和**不可靠**——数据报到不到、到不到顺序、会不会重复,UDP 都不保证(丢就丢了、内核不重传)。所以 UDP 适合「我自己应用层处理可靠性也行」或「丢点数据无所谓」的场景(DNS 一个请求一个响应、视频流丢一帧无所谓、游戏状态同步),不适合传文件这种「一个字节都不能少」的场景(那老老实实用 TCP)。`recvfrom` 的最后两个参数还能把**发送方的地址**带回来(代码里的 `peer`),所以服务端能知道「这个数据报是谁发来的」、回信时 `sendto` 回那个地址——UDP 常用这种「一个 socket 跟很多客户端来回」的模式,不用像 TCP 那样每个客户端一条连接。

## 本地域套接字 AF_UNIX:同机高速 IPC

本地域套接字(`AF_UNIX`,头 `<sys/un.h>`)是 socket 家族里专门给**同一台机器**上进程间通信用的一员。它的接口跟 TCP 几乎一模一样(同样 `socket`/`bind`/`listen`/`accept`/`connect`/`read`/`write`),唯一的区别是:**地址不是 IP+端口,而是文件系统里的一个路径**(`struct sockaddr_un` 的 `sun_path` 字段)。`bind` 会在那个路径上创建一个特殊的 socket 文件(标志是「类型 p」),客户端 `connect` 这个路径就连上。因为数据根本不进网络协议栈、就在内核里搬,所以**比走 `127.0.0.1` 的 TCP 快不少**(少了 TCP/IP 协议头处理、校验、协议栈层级)。真跑一遍:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    const char* path = "/tmp/cj/p5ch13/unix.sock";
    unlink(path); /* 清掉残留 */

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr));
    listen(listen_fd, 1);

    pid_t pid = fork();
    if (pid == 0) {
        close(listen_fd);
        sleep(1);
        int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un saddr;
        saddr.sun_family = AF_UNIX;
        strncpy(saddr.sun_path, path, sizeof(saddr.sun_path) - 1);
        connect(cfd, (struct sockaddr*) &saddr, sizeof(saddr));
        write(cfd, "hello unix", 10);
        close(cfd);
        _exit(0);
    }

    int cfd = accept(listen_fd, NULL, NULL);
    char buf[64];
    ssize_t n = read(cfd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[server] 通过 AF_UNIX 收到: %s\n", buf);
    }
    close(cfd);
    close(listen_fd);
    unlink(path); /* 清理 socket 文件 */
    waitpid(pid, NULL, 0);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall unix_stream.c -o us && ./us
[server] 通过 AF_UNIX 收到: hello unix
```

服务端 `bind` 到 `/tmp/cj/p5ch13/unix.sock`、`listen`/`accept`,客户端 `connect` 同一个路径、`write` —— 完全跟 TCP 一样的用法,只是地址换成了文件系统路径。注意我开头 `unlink(path)` 先清残留、结尾再 `unlink(path)` 清掉——**这个 socket 文件不会随进程退出自动消失**(跟第 7 章的共享内存对象一个脾气),不清理就会留在文件系统里、下次 `bind` 还会因路径已占报错(`EADDRINUSE`)。

AF_UNIX 还有个 TCP 做不到的独家本事:**在 socket 上传 fd**。用 `sendmsg`/`recvmsg` 配合「辅助消息」(`SCM_RIGHTS`),一个进程能把自己打开的一个 fd **发给**另一个进程、对方收到后就能用这个 fd 访问同一个打开文件——这是 `AF_UNIX` 独有的(因为内核得能在两个进程的 fd 表之间搬,跨机器的网络 socket 做不到)。Docker、systemd 都用这套机制把 fd 交给别的进程。这套接口(`struct msghdr`、`CMSG_*` 宏)比较繁琐,这里只认下名字、知道「AF_UNIX 能传 fd」就行,具体用到了再查 `cmsg(3)`。

## TCP vs UDP:字节流 vs 数据报

把 TCP 和 UDP 放一起对照,差别就一句话:**TCP 是字节流、UDP 是数据报**。TCP 给你一条可靠的、有序的、**没有消息边界**的字节管道(你得多写、它会粘包/半包、得自己加边界),适合「数据不能少」的场景(文件、网页、数据库);UDP 给你一个个独立、**有边界**、但不保证到达、不保证顺序的数据报(发一条是一条、收一条是一条),适合「快、丢了无所谓」或「应用层自己管可靠性」的场景。两者 socket 接口都从 `socket()` 起步,只是 type 一个 `SOCK_STREAM` 一个 `SOCK_DGRAM`,后续 `read`/`write` vs `recvfrom`/`sendto` 也对称。本地域套接字 `AF_UNIX` 则是另一维度——它可以用 `SOCK_STREAM`(像 TCP)或 `SOCK_DGRAM`(像 UDP),区别只在于地址是「文件路径」而非「IP+端口」、且只在同机内走、能传 fd。

## 小结

UDP(`SOCK_DGRAM`)无连接、不可靠、但**有消息边界**——`sendto` 一次就是一条数据报、对端 `recvfrom` 一次整条收走,不粘不拆;适合 DNS/视频/游戏等「快、丢得起」的场景,不适合传文件。本地域套接字 `AF_UNIX` 用文件系统路径(`sockaddr_un.sun_path`)当地址,接口同 TCP(socket/bind/listen/accept/connect + read/write),但只在同机内走、不经网络协议栈、**比 127.0.0.1 TCP 快**,还能用 `SCM_RIGHTS` 在 socket 上**传 fd**(TCP 做不到);记得 `unlink` 清理 socket 文件。TCP 字节流 vs UDP 数据报:前者可靠有序无边界、后者独立有边界但不可靠不保序。

socket 这一族到这就基本齐了。最后一章讲**名址解析**——前面所有 demo 都把 IP 写死成 `127.0.0.1`,可真实程序拿到的是「域名」(像 `example.com`)和「服务名」(像 `http`),怎么把它们变成能 `connect` 的地址?`getaddrinfo` 就是干这个的,而且它能做到「协议无关」(IPv4/IPv6 通吃)。

## 参考资源

- **UNP**：《Unix Network Programming, Volume 1》(W. Richard Stevens / Bill Fenner / Andrew M. Rudoff),第 8 章 UDP 基础、第 15 章 Unix 域套接字(含 `SCM_RIGHTS` 传 fd)。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 59 章(`AF_UNIX`)、第 58 章(socket 总览,UDP 一节)。
- **man 页**：`udp(7)`(UDP 语义、不可靠性边界)、`sendto(2)`/`recvfrom(2)`、`unix(7)`(AF_UNIX,`SCM_RIGHTS` 传 fd)、`cmsg(3)`(辅助消息宏)。
- **POSIX**：`SOCK_DGRAM`/`sendto`/`recvfrom`/`AF_UNIX`/`struct sockaddr_un` 全是 IEEE Std 1003.1-2008。
