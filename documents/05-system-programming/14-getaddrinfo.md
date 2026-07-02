---
title: "网络名址解析:getaddrinfo 与协议无关编程"
description: "前面所有 socket demo 都把 IP 写死成 127.0.0.1,可真实程序拿到的是『域名』(example.com)和『服务名』(http/8080),得先解析成能 connect 的二进制地址。这一章讲现代方案 getaddrinfo——它替代了老掉牙、只支持 IPv4、还不可重入的 gethostbyname。用法:填一个 hints(struct addrinfo,告诉它『我要 TCP、IPv4/IPv6 都行』),调 getaddrinfo(域名, 服务, &hints, &res),它返回一条 addrinfo 链表(因为一个域名可能解析出多个地址、IPv4 和 IPv6 各几个);你遍历链表、每条试着 socket+connect、成功就 break——这套『遍历结果逐条试』的写法让同一份代码『协议无关』地通吃 IPv4/IPv6(老代码写死 IPv4 在 IPv6-only 环境就废)。真跑 gai_demo.c 解析 localhost:8080:返回 IPv4(family=AF_INET)/127.0.0.1:8080,socket 能建。讲清几个点:用 getnameinfo 把二进制地址转回可读字符串打印;服务端要通配绑定就把 hints.ai_flags 设 AI_PASSIVE 且 host 传 NULL(getaddrinfo 返回 0.0.0.0/:: 通配地址供 bind);链表是 malloc 出来的、用完必须 freeaddrinfo 整条释放(别像第7章共享内存那样漏成泄漏)。最后散文化收口:这一章是系统编程阶段的收官——从 fd、文件 IO、进程、信号、IPC、多路复用、socket 到名址解析,主机系统编程的主干打通。全 gcc16+clang22 真跑。"
chapter: 5
order: 14
tags:
  - host
  - system-programming
  - posix
  - networking
difficulty: advanced
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 11 章：Socket TCP（sockaddr_in、connect/bind,这里把写死的地址换成 getaddrinfo 给的）"
  - "第 12 章：进阶 Socket（字节序,名址解析结果也是网络字节序）"
  - "第 1 章：文件 IO 与 fd（socket 返回的也是 fd,资源用完要释放的纪律）"
related:
  - "第 11 章：Socket TCP（sockaddr_in、connect/bind,这里把写死地址换成 getaddrinfo 给的）"
  - "第 7 章：IPC 下（freeaddrinfo 的『用完释放』纪律,同共享内存的 shm_unlink）"
---

> 🟡 状态:待审核(2026-07-02)

# 网络名址解析:getaddrinfo 与协议无关编程

## 引言：地址不是写死的

前面三章的 socket demo,地址全是写死的 `127.0.0.1`——本机回环,方便演示。可真实程序拿到的是**域名**(`example.com`)和**服务名**(`http`、或端口号 `8080`),你得先把它们**解析**成 `sockaddr` 能用的二进制地址,才能 `connect`/`bind`。这一章讲现代的解析接口 **`getaddrinfo`**——它替代了老掉牙的 `gethostbyname`(后者只支持 IPv4、还返回静态缓冲不可重入、多线程用就出事),并且能做到**协议无关**:同一份代码,IPv4、IPv6 通吃。

## getaddrinfo:填 hints、拿链表

`getaddrinfo`(`<netdb.h>`)的用法是「填一个 hints、拿一条结果链表」。`hints` 是个 `struct addrinfo`,告诉它「我想要什么样的地址」;`getaddrinfo(域名, 服务名, &hints, &结果链表)` 根据你的要求解析,返回一条**链表**(因为一个域名可能解析出好几个地址——IPv4 和 IPv6 各几个,它都给你)。真跑解析 `localhost:8080`:

```c
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    /* hints 告诉 getaddrinfo「我想要什么样的地址」 */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* 不限 IPv4/IPv6,都要 */
    hints.ai_socktype = SOCK_STREAM; /* 只要 TCP 的 */

    struct addrinfo* res = NULL;
    int err = getaddrinfo("localhost", "8080", &hints, &res);
    if (err) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    /* 返回的是一条 addrinfo 链表,逐条试到能 socket+connect 的为止 */
    int i = 0;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        char host[INET6_ADDRSTRLEN]; /* 46,够装 IPv6 数字地址 */
        char serv[16];               /* 端口字符串最多几位 */
        getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV);
        printf("结果 %d: family=%d(%s) socktype=%d 地址=%s 端口=%s\n", ++i, p->ai_family,
               p->ai_family == AF_INET ? "IPv4" : "IPv6", p->ai_socktype, host, serv);

        /* 真要用就拿这条结果试 socket+connect,成功就 break */
        int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd >= 0) {
            close(fd);
            printf("  ↑ 这条能建 socket,真程序就拿它 connect\n");
        }
    }

    freeaddrinfo(res); /* 链表是 malloc 出来的,用完整条释放 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall gai_demo.c -o gai && ./gai
结果 1: family=2(IPv4) socktype=1 地址=127.0.0.1 端口=8080
  ↑ 这条能建 socket,真程序就拿它 connect
```

逐条拆。`hints.ai_family = AF_UNSPEC` 是「协议无关」的关键——告诉 getaddrinfo「IPv4(`AF_INET`)、IPv6(`AF_INET6`)都接受」,于是它会把这个域名能解析到的所有地址(两种都可能有)全塞进链表;`hints.ai_socktype = SOCK_STREAM` 表示「我只要 TCP 的」(筛掉 UDP)。`getaddrinfo("localhost", "8080", &hints, &res)` 把 `localhost` 解析成地址、把 `8080` 解析成端口(服务名参数也可以传 `"http"` 这种符号名、它查 `/etc/services` 给你端口号)。返回的 `res` 是一条 `addrinfo` 链表,每个节点装着「一个可用的地址」(`ai_addr`/`ai_addrlen`,可以直接喂给 `socket`/`connect`/`bind`)。

遍历链表时,我用 `getnameinfo` 把每个地址的二进制形式**转回可读字符串**(数字 IP + 数字端口)打印出来——真跑里 `localhost` 在这台 WSL2 上解析出一个 IPv4 地址 `127.0.0.1:8080`(`family=2` 就是 `AF_INET`、`socktype=1` 就是 `SOCK_STREAM`,这些是 Linux 上的数值)。每条我还试 `socket(p->ai_family, p->ai_socktype, p->ai_protocol)`——能建就说明这条地址可用,真程序就直接拿它 `connect`、然后 `break` 跳出循环;不能建(比如这个系统不支持 IPv6)就试链表里的下一条。这套「**遍历结果逐条试、成功就用**」的写法就是「协议无关」的落地:你的代码不写死 `AF_INET` 还是 `AF_INET6`,getaddrinfo 给什么你试什么,于是在纯 IPv4、纯 IPv6、双栈环境里同一份代码都能跑。

## AI_PASSIVE:服务端通配绑定 + freeaddrinfo 释放

`getaddrinfo` 还能服务端用。服务端 `bind` 时通常想「绑所有网卡的某个端口」,这时把 `hints.ai_flags = AI_PASSIVE`、且 `host` 参数传 `NULL`(`getaddrinfo(NULL, "8080", &hints, &res)`),它返回的就是**通配地址**(`0.0.0.0` 对 IPv4、`::` 对 IPv6,表示「本机所有网卡」),拿去 `bind` 就能接受从任意网卡进来的连接。`AI_PASSIVE` 不设、`host` 给具体域名时,返回的是「用来 `connect` 的对端地址」——客户端和服务端用同一个 `getaddrinfo`、靠 `AI_PASSIVE` 区分角色。

最后那条 `freeaddrinfo(res)` 别漏:`res` 这条链表是 `getaddrinfo` 内部 `malloc` 出来的、各节点的 `ai_addr` 也是动态分配的,**用完必须 `freeaddrinfo` 整条释放**,否则就是内存泄漏(跟第 7 章共享内存的 `shm_unlink`、第 6 章 FIFO 的 `unlink` 一个道理——系统编程里「谁分配谁释放」的纪律一以贯之)。`getaddrinfo` 失败时不返回链表(返回非零错误码、`res` 不被设置),用 `gai_strerror(err)` 把错误码翻译成人话(注意它**不**走 `errno`/`strerror`,有自己的一套)。

## 告别 gethostbyname

为什么不用老的 `gethostbyname`?三条硬伤:其一,**只支持 IPv4**——它的 `struct hostent` 里只有 IPv4 地址(`h_addr_list` 是 `in_addr`),IPv6 它根本不认识,你的程序一上 IPv6-only 网络就废;其二,**不可重入**——它返回的是**指向静态缓冲**的指针,多线程同时调就互相踩,是经典的线程不安全 API(POSIX 后来给了 `gethostbyname_r` 可重入版,但接口更难用);其三,它只解析「域名→IP」,不管端口、不管 TCP/UDP,你得自己再 `socket`/`bind` 时分别填。`getaddrinfo` 把这三条全治了:IPv4/IPv6 通吃(`AF_UNSPEC`)、可重入(结果是你传入的链表指针、不用静态缓冲)、一次给全地址+端口+type。所以新代码一律 `getaddrinfo`、别碰 `gethostbyname`。

## 小结:`getaddrinfo` + 协议无关

`getaddrinfo(域名, 服务, &hints, &res)` 把域名+服务名解析成一条 `addrinfo` 链表(每个节点一个可用的二进制地址+端口+协议);`hints.ai_family = AF_UNSPEC` + 遍历结果逐条 `socket`/`connect` 试、成功就用,是**协议无关**编程的标准写法(同一份代码 IPv4/IPv6 通吃)。服务端通配绑定用 `AI_PASSIVE` + `host=NULL`;链表用完 `freeaddrinfo` 释放(别漏、否则泄漏);错误用 `gai_strerror`(不走 errno)。它替代了只支持 IPv4、不可重入的老 `gethostbyname`。

到此,**系统编程阶段收官**。回头看这 14 章铺的主干:第 1 章的 **fd** 是一切的地基(文件 IO、`open`/`read`/`write`);第 2-4 章的 **进程**(`fork`/`exec`/`wait`/daemon)让我们能造进程、换程序、后台化;第 5 章的**信号**让进程响应异步事件;第 6-7 章的 **IPC**(pipe/共享内存/信号量)让进程间传数据;第 8-10 章的**多路复用**(select/poll/epoll + 非阻塞 + reactor)让一个线程管海量 fd;第 11-14 章的 **socket**(TCP/UDP/Unix 域/getaddrinfo)把通信推上网络、并协议无关地解析名址。一条「从 fd 走到能写高并发网络服务」的完整路径就铺通了——后面综合项目阶段,这套骨架会第一次组合进真实工程里。

## 参考资源

- **UNP**：《Unix Network Programming, Volume 1》(W. Richard Stevens / Bill Fenner / Andrew M. Rudoff),第 11 章 `getaddrinfo`/`getnameinfo` 是名址解析的权威讲解,协议无关编程的范式。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 59 章 socket 高级一节讲 `getaddrinfo`,IPv4/IPv6 通吃的实例。
- **man 页**：`getaddrinfo(3)`、`getnameinfo(3)`、`freeaddrinfo(3)`、`gai_strerror(3)`、`hostname(7)`/`protocols(5)`/`services(5)`(名址解析的后台数据源)。
- **POSIX**：`getaddrinfo`/`getnameinfo`/`freeaddrinfo`/`struct addrinfo`/`AI_PASSIVE` 是 IEEE Std 1003.1-2008(`<netdb.h>`),取代了被标记为 obsolete 的 `gethostbyname`。
