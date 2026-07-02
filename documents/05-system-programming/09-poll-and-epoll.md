---
title: "poll 与 epoll：从 O(n) 到 O(1) 的演进"
description: "上一章的 select 有两个结构性短板:fd_set 位图有 FD_SETSIZE=1024 上限、且每次调用内核和用户各扫一遍位图是 O(n)。这一章讲它的两个继任者。poll(POSIX)用『结构体数组』替掉固定位图——数组要多大开多大,fd 编号再大也不越界,摆脱了 1024 上限;真跑 poll_demo 同时盯两个 pipe,子 1 秒后写 B,poll 返回 1、报 fds[1] 可读、读出 hi。但 poll 仍是 O(n)(内核还得扫整个数组、用户也得遍历找 revents)。真正把复杂度降到 O(就绪数) 的是 Linux 特有的 epoll:epoll_create1 建一个 epoll 实例、epoll_ctl 把关心的 fd 注册进去(注册一次、不用每次重设)、epoll_wait 只返回『当前就绪』的那些 fd(内核维护一条就绪链表,有 fd 就绪时直接把它挂上去);真跑 epoll_demo,epoll_wait 返回 1、fd=5 就绪、读出 hi,跟 poll 同样的效果但内部是 O(1)。再讲水平触发(LT,默认)与边沿触发(ET)的本质区别:LT 只要 fd 还有数据可读就持续通知(省心),ET 只在状态『新变化』那一次通知(必须配非阻塞 IO + 一直 read 到 EAGAIN 否则漏读)。最后点 epoll 是 Linux 专有(跨平台靠 libuv/libevent 抽象),高并发服务端(C10k+)的绝对主力。全 gcc16+clang22 真跑。"
chapter: 5
order: 9
tags:
  - host
  - system-programming
  - posix
  - networking
difficulty: advanced
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 8 章：IO 多路复用 select（fd_set、FD_SETSIZE 上限、O(n) 代价）"
  - "第 6 章：IPC 上（pipe,多路复用盯的典型 fd）"
  - "第 1 章：文件 IO 与 fd（fd 是小整数、read 的返回值）"
related:
  - "第 10 章：非阻塞 IO 与 reactor 引子（把 epoll 包成事件循环）"
  - "第 12 章：进阶 Socket（用 epoll 把服务端推向并发）"
---

> 🟡 状态:待审核(2026-07-02)

# poll 与 epoll：从 O(n) 到 O(1) 的演进

## 引言：select 的两个痛点

上一章的 `select` 能用,但有两个结构性毛病,连接数一上去就露怯:其一,`fd_set` 是个**固定位图**,受 `FD_SETSIZE`(默认 1024)限制,fd 编号超 1024 直接越界 UB;其二,每次 `select` 内核都得**从 0 扫到 nfds**、用户态返回后还得**再扫一遍**找 `FD_ISSET`,两趟都是 **O(n)**(n = 你盯的 fd 总数)。这一章讲它的两个继任者——`poll` 解决第一个毛病(摆脱 1024 上限)、`epoll` 把第二个毛病也治了(复杂度降到 O(就绪数))。

## poll：用结构体数组摆脱 1024 上限

`poll`(POSIX,`<poll.h>`)的思路跟 select 类似(也是「给你一堆 fd、告诉你哪个就绪」),但它把固定位图换成了**结构体数组**——你想盯几个 fd、就开几个元素的数组,要多大有多大,fd 编号再大也不会越界。每个数组元素是个 `struct pollfd`,里头三个字段:`fd`(盯哪个)、`events`(关心什么事件,如 `POLLIN` 可读)、`revents`(返回时内核填的「实际就绪了什么」)。真跑一遍,跟上一章 select 的例子结构一样、只是换了 poll 接口:

```c
#define _POSIX_C_SOURCE 200809L
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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
        sleep(1);
        write(p2[1], "hi", 2);
        sleep(2); /* 持住 p1[1],免得 p1 EOF 搅乱 demo */
        close(p2[1]);
        close(p1[1]);
        _exit(0);
    }

    close(p1[1]);
    close(p2[1]);

    /* poll 用结构体数组,每个元素一个 fd + 关心的事件 + 返回的就绪事件 */
    struct pollfd fds[2];
    fds[0].fd = p1[0];
    fds[0].events = POLLIN; /* 关心「可读」 */
    fds[0].revents = 0;
    fds[1].fd = p2[0];
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    printf("[父] poll 盯两个 pipe(数组式,无 1024 上限)...\n");
    fflush(stdout);
    int n = poll(fds, 2, -1); /* -1 = 永远阻塞 */
    printf("[父] poll 返回 %d,就绪情况:\n", n);
    for (int i = 0; i < 2; i++) {
        if (fds[i].revents & POLLIN) {
            char buf[16];
            ssize_t k = read(fds[i].fd, buf, sizeof(buf) - 1);
            if (k > 0) {
                buf[k] = '\0';
                printf("  fds[%d](fd=%d) 可读 → %s\n", i, fds[i].fd, buf);
            }
        }
    }

    waitpid(pid, NULL, 0);
    close(p1[0]);
    close(p2[0]);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall poll_demo.c -o poll && ./poll
[父] poll 盯两个 pipe(数组式,无 1024 上限)...
[父] poll 返回 1,就绪情况:
  fds[1](fd=5) 可读 → hi
```

子进程 1 秒后往 pipe B 写 `"hi"`,`poll(fds, 2, -1)`(`2` 是数组长度、`-1` 是无限超时)返回 `1`,然后我遍历数组、查每个元素的 `revents & POLLIN`,发现 `fds[1]`(就是 pipe B)就绪、读出 `"hi"`。效果跟 select 一模一样,但**没有 1024 上限**——你开个 10 万元素的 `struct pollfd` 数组也行。不过 poll 的复杂度**仍是 O(n)**:内核每次得扫整个数组(因为不知道哪些 fd 你真关心),用户态返回后也得遍历数组查 `revents`(因为 poll 不会把就绪的「聚拢」给你、而是原地标记在数组里)。连接数几千时,这趟 O(n) 又成了瓶颈——这就轮到 epoll 出场了。

## epoll：内核就绪链表,O(就绪数)

`epoll`(**Linux 特有**,`<sys/epoll.h>`,非 POSIX)是高并发服务端的绝对主力。它的核心思路是:**把「关心哪些 fd」和「等就绪」这两件事拆开**。你先用 `epoll_create1` 建一个 epoll 实例(它本身也是个 fd),用 `epoll_ctl` 把关心的 fd **逐个注册**进去(注册时内核把这个 fd 挂到内部一个红黑树、并登记它的就绪回调);之后每次 `epoll_wait` 时,内核**只把「当前就绪」的 fd 收集到一条就绪链表里**返回给你——不用扫所有 fd、只处理真正就绪的那几个。所以 `epoll_wait` 的复杂度是 **O(就绪数)**,跟总 fd 数无关。真跑一遍:

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

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
        sleep(1);
        write(p2[1], "hi", 2);
        sleep(2);
        close(p2[1]);
        close(p1[1]);
        _exit(0);
    }

    close(p1[1]);
    close(p2[1]);

    /* epoll:先建一个 epoll 实例(就是个 fd) */
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return 1;
    }
    /* 把关心的 fd 注册进去(一次性,后面不用每次重设) */
    struct epoll_event ev;
    ev.events = EPOLLIN; /* 默认水平触发 LT */
    ev.data.fd = p1[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, p1[0], &ev);
    ev.data.fd = p2[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, p2[0], &ev);

    printf("[父] epoll_wait 盯两个 pipe(就绪链表,O(就绪数))...\n");
    fflush(stdout);
    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, -1); /* 只返回就绪的 fd */
    printf("[父] epoll_wait 返回 %d,就绪:\n", n);
    for (int i = 0; i < n; i++) {
        char buf[16];
        ssize_t k = read(events[i].data.fd, buf, sizeof(buf) - 1);
        if (k > 0) {
            buf[k] = '\0';
            printf("  fd=%d 就绪 → %s\n", events[i].data.fd, buf);
        }
    }

    waitpid(pid, NULL, 0);
    close(p1[0]);
    close(p2[0]);
    close(epfd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall epoll_demo.c -o ep && ./ep
[父] epoll_wait 盯两个 pipe(就绪链表,O(就绪数))...
[父] epoll_wait 返回 1,就绪:
  fd=5 就绪 → hi
```

`epoll_create1(0)` 建出 epoll 实例 `epfd`;两次 `epoll_ctl(epfd, EPOLL_CTL_ADD, ...)` 把 p1、p2 的读端注册进去(注意是 `EPOLL_CTL_ADD`,还有 `MOD` 改、`DEL` 删);然后 `epoll_wait(epfd, events, 4, -1)` 阻塞,子进程写 pipe B 后它返回 `1`,`events[0]` 里装的就是就绪那个 fd 的信息(`events[i].data.fd` 是注册时我塞进去的 fd 编号、`events[i].events` 是就绪的事件类型),直接 `read` 出 `"hi"`。表面效果跟 poll 一样,但**内部天差地别**:epoll 不用扫所有注册的 fd、只返回就绪的,连接数从一百涨到十万,`epoll_wait` 的开销基本不涨(只随「就绪数」涨)。这就是为什么 nginx、Redis 这些高并发服务端都用 epoll。

epoll 还有两个 select/poll 没有的优势:其一,**`fd_set`/数组不用每次重建**——select 每轮要 FD_ZERO+FD_SET、poll 每次要传整个数组,而 epoll 注册一次之后内核自己记着,`epoll_wait` 只填就绪的、原注册集合不动;其二,**fd 集合可以动态增删**(`EPOLL_CTL_ADD`/`DEL`),不用像 select/poll 那样把「当前所有 fd」整个重传。

## LT vs ET:水平触发与边沿触发

epoll 还有个 select/poll 没有的维度:**触发模式**。默认是**水平触发**(LT,Level Triggered)——注册时给 `EPOLLIN` 不带 `EPOLLET` 就是 LT;LT 的意思是「**只要这个 fd 还有数据可读,`epoll_wait` 就会持续通知你**」。另一种是**边沿触发**(ET,Edge Triggered,加 `EPOLLET` 标志)——「**只在 fd 的状态『新变化』那一次**通知你」(从无数据变有数据那一瞬通知一下),之后哪怕 fd 里还有大堆数据没读完,也不再通知了。

LT 是省心的(默认,你 `read` 一次没读完、下次 `epoll_wait` 还会告诉你「还有数据」,不会漏);ET 是高效但**容易漏读**的——因为只通知一次,你必须在那一次把数据**全部读完**(配非阻塞 IO、循环 `read` 直到返回 `-1`/`EAGAIN` 表示「暂时没数据了」),否则剩下的数据就被你「以为读完了」漏掉了、再也没有通知唤醒你。ET 的好处是减少了 epoll_wait 被唤醒的次数(高性能服务端常选 ET),代价是**必须**配非阻塞 IO + 循环读到 EAGAIN,正确性更难写。新手老老实实用 LT,等你把非阻塞 IO(下一章)彻底搞懂了、再考虑切 ET。

## 小结

`poll`(POSIX)用 `struct pollfd` 结构体数组替掉 select 的固定位图,摆脱了 `FD_SETSIZE=1024` 上限,但复杂度仍 O(n);`epoll`(Linux 特有)用「`epoll_create1` 建实例、`epoll_ctl` 注册 fd、`epoll_wait` 只返回就绪 fd」的三段式,靠内核的**就绪链表**把复杂度降到 **O(就绪数)**——连接数再多、就绪少时它几乎不涨,是高并发(C10k+)的绝对主力。epoll 还有「注册一次不用重建集合」「动态增删 fd」的好处。触发模式分 **LT**(默认,有数据就持续通知,省心不漏)和 **ET**(只在状态变化通知一次,必须配非阻塞 IO + 循环 read 到 EAGAIN,高效但易漏读)。select/poll 跨平台、epoll 是 Linux 专有(跨平台高并发代码通常靠 `libuv`/`libevent` 这类抽象层,底下在 Linux 自动用 epoll、别的平台用对应机制)。

到此多路复用的三件套(select/poll/epoll)就齐了。但我们的 fd 都还是**阻塞**的——`read` 没数据就卡住。下一章把「非阻塞 IO」配进来:fd 设 `O_NONBLOCK` 后 `read` 没数据立刻返回 `EAGAIN`(不卡),配合 epoll 就能写出真正的**事件循环**(reactor 模式)——那是单线程高并发的标准骨架,也是后面 socket 服务端的地基。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 14 章「高级 IO」讲 poll;epoll 是 Linux 扩展(APUE 跨平台、覆盖有限)。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 63 章「另类 IO 模型」,poll 与 epoll(epoll_create/ctl/wait、LT/ET)讲得最系统,epoll 的就绪链表机制有展开。
- **man 页**：`poll(2)`、`epoll_create(2)`/`epoll_create1(2)`、`epoll_ctl(2)`、`epoll_wait(2)`、`epoll(7)`(综述,LT/ET 区别、性能模型)。
- **POSIX**：`poll`/`struct pollfd`/`POLLIN` 是 IEEE Std 1003.1-2008(`<poll.h>`);`epoll` **不在 POSIX 里**,是 Linux 内核提供的专有接口(`<sys/epoll.h>`)。
