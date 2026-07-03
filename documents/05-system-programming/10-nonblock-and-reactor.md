---
title: "非阻塞 IO 与 reactor 引子：把多路复用包成事件循环"
description: "epoll 告诉你『哪个 fd 就绪了』,但去 read 它时还有两个新问题:read 可能只读到一部分(短读)、或者 read 会卡住整个循环。这一章先把 fd 设成『非阻塞』——用 fcntl 给它或上 O_NONBLOCK,这之后 read 没数据就立刻返回 -1/errno=EAGAIN(不再阻塞),有几读几。真跑 nonblock.c:空 pipe 上非阻塞 read 返回 -1、errno=11(Resource temporarily unavailable),写入数据后再读返回 2 字节 hi。然后把这章的主角 reactor 拼出来:epoll(等就绪)+ 非阻塞 fd(read 不卡)+ 每个 fd 一个回调函数 + 一个事件循环 —— 真跑 reactor.c,子进程往两个 pipe 分别写 hello-A/hello-B,父进程的 reactor 事件循环 epoll_wait 到就绪就分派给对应回调读出,清爽。但这一路真踩到一个能烧爆 CPU 的坑:LT 模式下,EOF 的 fd 会被 epoll 无限次报『就绪』(因为 read 返回 0 不阻塞、内核认为它一直可读),所以回调发现 EOF 时必须从 epoll 里 EPOLL_CTL_DEL 移掉这个 fd 并 close —— 我第一次写漏了这步,程序疯狂刷『EOF』几百万行、跑爆 2 分钟超时(68MB 输出),修成『回调返回 remove 标志、循环 DEL+close』后才 5 行干净退出。最后散文讲为什么高并发服务端都用 reactor 而不是『一连接一线程』(线程有栈开销、几万连接就几万线程、调度开销爆炸;reactor 一个线程管万级连接)。全 gcc16+clang22 真跑。"
chapter: 5
order: 10
tags:
  - host
  - system-programming
  - posix
  - concurrency
difficulty: advanced
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 9 章：poll 与 epoll（epoll_create/ctl/wait、LT vs ET）"
  - "第 1 章：文件 IO 与 fd（read 的阻塞语义、短读）"
  - "第 8 章：IO 多路复用 select（多路复用的基本概念）"
related:
  - "第 12 章：进阶 Socket（用 reactor + epoll 把服务端推向并发）"
  - "第 5 章：信号（EINTR 与慢系统调用、非阻塞的另一面）"
---

# 非阻塞 IO 与 reactor 引子：把多路复用包成事件循环

## 引言：epoll 还差最后一块

上一章的 epoll 已经能高效地告诉你「哪些 fd 就绪了」,可你去 `read` 那个就绪的 fd 时,还有两个问题没解决。其一,`read` 默认是**阻塞**的——万一就绪通知来了、但内核这次只给你读出一小部分、你又想再来一次 `read`,默认的 `read` 在「暂时没更多数据」时会**卡住**,整个事件循环就被这一个 fd 拖死了。其二,epoll 只负责「通知就绪」,**拿到通知之后怎么处理**(读?写到哪?断开怎么办?)是你自己的事,得有一套组织方式。这两个问题,前者靠**非阻塞 IO** 解决,后者靠 **reactor 模式**(事件循环 + 回调)组织。这一章把这两块拼上,我们就有了写高并发服务端的完整骨架。

## 非阻塞 IO:O_NONBLOCK 与 EAGAIN

把一个 fd 设成非阻塞,用 `fcntl`(`<fcntl.h>`)给它「或」上 `O_NONBLOCK` 标志:`int fl = fcntl(fd, F_GETFL); fcntl(fd, F_SETFL, fl | O_NONBLOCK);`(先取当前标志位、再加上 `O_NONBLOCK` 设回去,别直接设、会把别的标志冲掉)。设完之后,这个 fd 上的 `read`/`write` 行为就变了:**没有数据可读时,它不再阻塞、而是立刻返回 `-1`、`errno` 被设成 `EAGAIN`(11,Resource temporarily unavailable)**——意思是「现在没数据,你待会儿再来问」。真跑给你看:

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    int p[2];
    if (pipe(p) < 0) {
        perror("pipe");
        return 1;
    }
    /* 把读端设成非阻塞:fcntl 取当前 flags、或上 O_NONBLOCK 再设回 */
    int flags = fcntl(p[0], F_GETFL);
    fcntl(p[0], F_SETFL, flags | O_NONBLOCK);

    char buf[16];
    ssize_t n = read(p[0], buf, sizeof(buf)); /* 没数据又非阻塞 → 立刻返回 */
    printf("[主] 没数据时非阻塞 read 返回 %zd, errno=%d (%s)\n", (long) n, errno, strerror(errno));

    /* 写点数据进去,再读就有了 */
    write(p[1], "hi", 2);
    n = read(p[0], buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[主] 写入数据后再读: 返回 %zd, 内容 %s\n", (long) n, buf);
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall nonblock.c -o nb && ./nb
[主] 没数据时非阻塞 read 返回 -1, errno=11 (Resource temporarily unavailable)
[主] 写入数据后再读: 返回 2, 内容 hi
```

空 pipe 上非阻塞 `read` 立刻返回 `-1`、`errno=11`(`EAGAIN`,文本是 "Resource temporarily unavailable")——它**没卡住**、把控制权立刻还给了你。等写入数据后再读,就正常返回 `2`、读到 `hi`。这就是非阻塞的核心:**`read` 永远不会卡,要么给你数据、要么告诉你「现在没有」(EAGAIN)**。在 reactor 里这至关重要——事件循环不能被任何一个 fd 拖住,EAGAIN 让你「没数据就先放下、回去处理别的 fd、下次轮到再说」。

## reactor:epoll + 非阻塞 + 回调 + 事件循环

有了非阻塞,就能拼 **reactor** 了。reactor 是个模式,骨架就四样:**一组被监听的 fd(都设非阻塞)+ 每个 fd 绑一个「回调函数」+ 一个 epoll 实例 + 一个事件循环**。事件循环干的事就一句:「`epoll_wait` 等就绪 → 拿到就绪 fd → 查它的回调 → 调回调处理 → 回去继续等」,如此反复。一个最小的 reactor 长这样(子进程往两个 pipe 各写一行、父进程的 reactor 负责收):

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*handler_t)(int fd); /* 返回 0=保留,1=移除(EOF/错误)*/

static int on_pipe_a(int fd) {
    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1); /* 非阻塞,有几读几 */
    if (n > 0) {
        buf[n] = '\0';
        printf("[回调 A] 读到 %zd 字节: %s", (long) n, buf);
        return 0;
    }
    printf("[回调 A] EOF/错误(n=%zd) → 要求移除\n", (long) n);
    return 1;
}

static int on_pipe_b(int fd) {
    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[回调 B] 读到 %zd 字节: %s", (long) n, buf);
        return 0;
    }
    printf("[回调 B] EOF/错误(n=%zd) → 要求移除\n", (long) n);
    return 1;
}

int main(void) {
    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        close(p1[0]);
        close(p2[0]);
        write(p1[1], "hello-A\n", 8);
        sleep(1);
        write(p2[1], "hello-B\n", 8);
        sleep(1);
        close(p1[1]);
        close(p2[1]);
        _exit(0);
    }

    close(p1[1]);
    close(p2[1]);
    /* reactor 约定:被监听的 fd 一律设非阻塞 */
    fcntl(p1[0], F_SETFL, fcntl(p1[0], F_GETFL) | O_NONBLOCK);
    fcntl(p2[0], F_SETFL, fcntl(p2[0], F_GETFL) | O_NONBLOCK);

    handler_t handlers[64] = {0};
    handlers[p1[0]] = on_pipe_a;
    handlers[p2[0]] = on_pipe_b;

    int epfd = epoll_create1(0);
    struct epoll_event ev = {.events = EPOLLIN};
    ev.data.fd = p1[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, p1[0], &ev);
    ev.data.fd = p2[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, p2[0], &ev);

    int active = 2;
    printf("[reactor] 进入事件循环\n");
    fflush(stdout);
    while (active > 0) {
        struct epoll_event events[4];
        int n = epoll_wait(epfd, events, 4, 3000);
        if (n == 0) {
            printf("[reactor] 3 秒无事件,退出\n");
            break;
        }
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd < 64 && handlers[fd] && handlers[fd](fd)) {
                /* 回调要求移除(EOF/错误):必须 EPOLL_CTL_DEL + close,
                   否则 LT 模式下 EOF 的 fd 会无限就绪、空转烧 CPU */
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                handlers[fd] = NULL;
                active--;
            }
        }
        fflush(stdout);
    }

    waitpid(pid, NULL, 0);
    close(epfd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall reactor.c -o rtor && ./rtor
[reactor] 进入事件循环
[回调 A] 读到 8 字节: hello-A
[回调 B] 读到 8 字节: hello-B
[回调 A] EOF/错误(n=0) → 要求移除
[回调 B] EOF/错误(n=0) → 要求移除
```

子进程往 pipe A 写 `hello-A`、1 秒后往 pipe B 写 `hello-B`、再 1 秒后关掉两个写端退出。父进程的 reactor:`epoll_wait` 醒来 → 拿到就绪 fd → 用 `handlers[fd]` 查它的回调 → 调回调 `read`+打印 → 回去继续等。回调返回 `0` 表示「这个 fd 还要继续盯」、返回 `1` 表示「EOF/出错了、把它摘掉」。最后两个 pipe 的写端被子进程关闭,父这边 `read` 返回 `0`(EOF),回调返回「移除」,reactor 把它们从 epoll 里 `EPOLL_CTL_DEL` 掉、`close` 掉、`active` 减到 0,循环干净退出。**整个流程没有一个 `read` 阻塞过**——因为 fd 都是非阻塞的,这就是 reactor 能用单线程管很多 fd 的前提。

## LT 模式下 EOF 的坑:必须 DEL

上面那段「回调返回移除 → 循环 `EPOLL_CTL_DEL`」看着不起眼,其实是我真机踩出来的血泪坑,值得专门说一下。我第一版 reactor 漏了这步——回调发现 EOF 只打印一下、不返回「移除」,结果程序跑起来**疯狂刷 `[回调] EOF` 几百万行、2 分钟跑超时、输出 68MB**。原因正是 LT 模式的语义:**一个 fd 只要不发生 `read` 阻塞,LT 的 epoll 就会一直把它报成「就绪」**。EOF 的 fd,`read` 返回 `0` 立刻返回(不阻塞),所以 epoll 认为「它一直可读」、每次 `epoll_wait` 都立刻返回它、回调每次都 `read` 到 EOF、打印、回来再 `epoll_wait` 又立刻返回它……死循环烧满一个 CPU 核。

解法就是代码里那两步:**回调发现 EOF/错误时,必须把这个 fd 从 epoll 里 `EPOLL_CTL_DEL` 摘掉、并 `close` 它**。摘掉之后 epoll 不再监听它、死循环就断了。这是 LT 模式 reactor 的铁律——**每条连接的生命周期你得自己管:建连时 ADD、断开(EOF 或错误)时 DEL+close**,漏了 DEL 就是你服务器 100% CPU 空转的常见原因。(另一条路是上一章提过的 **ET 边沿触发**:ET 只在「状态变化」时通知一次,不会无限报,但代价是回调里必须循环 `read` 到 `EAGAIN` 否则漏读,更难写。)

## 为什么是 reactor 而不是「一连接一线程」

最后说说为什么高并发服务端几乎都用 reactor、而不是早年流行的「为每个客户端连接开一个线程」(thread-per-connection)。线程不是免费的——每个线程要占**一段栈**(默认 8MB 虚拟地址空间、几 MB 实际内存)、要内核调度开销、要缓存局部性损失;你要是来一万连接就开一万线程,光线程的内存和上下文切换开销就能把机器拖垮(C10k 问题当年就是被这种模型卡住的)。reactor 把「等事件」和「处理事件」彻底分开:**一个线程(或少数几个)跑事件循环,用 epoll 同时盯所有连接的 fd,哪个来事件就处理哪个、处理完立刻回去等下一个**——没有「一个连接一个线程死等」的浪费,几万、几十万连接都能用一两个线程扛下来。nginx、Redis、Node.js、Nginx 之类全是这套(具体实现可能是 reactor 或更进一步的 proactor,但地基都是 epoll + 非阻塞 + 事件循环)。这一章只是个引子——把 epoll、非阻塞、回调拼成了最小可用的 reactor;后面 socket 章会把它第一次用进真实网络服务端。

## 小结

`fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)` 把 fd 设非阻塞,`read` 没数据就立刻返回 `-1`/`EAGAIN`(errno=11)、不再卡住。reactor = **epoll + 非阻塞 fd + 每 fd 一个回调 + 事件循环**:`epoll_wait` 拿到就绪 fd → 查它的回调 → 调回调处理 → 回去继续等,全程无阻塞。回调要返回「保留/移除」标志,**遇到 EOF 或错误必须 `EPOLL_CTL_DEL` + `close`**——否则 LT 模式下 EOF 的 fd 会被无限报就绪、CPU 空转烧满(我真跑踩过、68MB 输出)。reactor 比「一连接一线程」强在:单线程就能管万级连接,省掉了每线程的栈和调度开销,是现代高并发服务端的标准骨架。

到此,系统编程里「IO 多路复用 + 非阻塞 + 事件循环」这套事件驱动的地基就铺好了。下一章开始讲**网络 socket**——把「fd」从 pipe 推广到网络连接,亲手走通 TCP 服务端的 `socket`/`bind`/`listen`/`accept` 和客户端的 `connect`,届时这个 reactor 就能第一次跑在真实的网络连接上了。

## 参考资源

- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 63 章「另类 IO 模型」讲非阻塞 IO 与 reactor 思路,第 61 章讲 `fcntl`/`O_NONBLOCK`。
- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 14 章「高级 IO」非阻塞 IO 一节;reactor 模式可参看 Stevens 的《UNP》卷 2。
- **man 页**：`fcntl(2)`(`O_NONBLOCK`)、`open(2)`(同样支持 `O_NONBLOCK` 标志)、`epoll(7)`(LT/ET 与 EOF 行为)、`errno(3)`(EAGAIN/EWOULDBLOCK)。
- **POSIX**：`fcntl`/`O_NONBLOCK`/`EAGAIN` 是 IEEE Std 1003.1-2008;reactor 是一种设计模式(最初由 Douglas Schmidt 在 ACE 框架提出),不是标准 API。
