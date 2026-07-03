---
title: "IO 多路复用：select 的接口与 fd_set 上限"
description: "前面几章我们一个进程一次只盯一个 fd——read 一个 pipe 没数据就阻塞死等。可真实的服务器要同时处理几十上百个连接(每个一个 fd),总不能为每个 fd 开一个进程/线程死等吧?这一章讲 IO 多路复用的鼻祖 select:一次调用同时盯住一堆 fd,谁先有数据就先返回、告诉你「哪一个就绪了」,你再去 read 那一个。真跑 select_two:父进程同时盯两个 pipe 的读端,子进程 1 秒后往 pipe B 写『hi』,select 阻塞到 B 有数据、返回 1、报出『pipe B 可读』(A 没数据就不报)——这就是事件驱动编程的起点。把 select 的几个坑一次讲清:其一,fd_set 是个位图、select 返回时会被改写成『只剩就绪的 fd』,所以循环里每次调 select 前都得 FD_ZERO+FD_SET 重新建一遍(不能复用);其二,第一个参数 nfds 是『最大 fd 编号 +1』,容易漏算那个 +1;其三,超时用的 struct timeval 会被 select 改写成『剩余时间』(真跑 2 秒超时跑完、tv.tv_sec 变 0),所以 timeval 也不能跨调用复用、每次重设;其四,fd_set 位图有 FD_SETSIZE 上限(默认 1024),fd 编号超过它直接越界是 UB,大量连接场景必须提限或改用 poll/epoll。最后点 select 的 O(n) 代价(内核每次扫整个位图),为后面 poll/epoll 的改进埋伏笔。全 gcc16+clang22 真跑。"
chapter: 5
order: 8
tags:
  - host
  - system-programming
  - posix
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 6 章：IPC 上（pipe 的读端、EOF 语义）"
  - "第 1 章：文件 IO 与 fd（fd 是小整数、read 阻塞语义）"
  - "第 5 章：信号（EINTR 与慢系统调用——select 同样会被信号打断）"
related:
  - "第 9 章：IO 多路复用进阶（poll/epoll 克服 select 的 fd_set 上限与 O(n) 代价）"
  - "第 6 章：pipe（select 监听的典型 fd 之一）"
---

# IO 多路复用：select 的接口与 fd_set 上限

## 引言：一个进程,怎么同时盯一堆 fd

到此我们手里的 `read`/`write` 都是**阻塞**的——`read` 一个暂时没数据的 pipe,进程就**卡在那里死等**,直到对端写进来。一个 fd 死等还好,可想想一个服务器:它得同时处理几十上百个客户端连接,每个连接一个 fd,哪个客户端先发数据是不确定的。要是它 `read` 第一个 fd 时阻塞了、第二个 fd 的数据就永远处理不到;为每个 fd 单独开一个进程或线程去死等,又太重(几千连接就开几千线程?)。出路就是**IO 多路复用**(IO multiplexing):让内核帮你**同时盯住一堆 fd**,只要有任意一个「就绪」(有数据可读、或可写、或有异常),就唤醒你;你拿到「是哪一个就绪」后、再去处理那一个。这样**一个线程就能管海量 fd**,这就是事件驱动、单线程高并发服务器的基石。这一章讲最早、最经典的多路复用接口——**`select`**。

## select 的接口与 fd_set

`select` 的原型(`<sys/select.h>`)看着参数挺多,其实就四个意思:

```c
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
```

`readfds`/`writefds`/`exceptfds` 是三组你关心的 fd 集合(分别盯「可读」「可写」「异常」),不关心的传 `NULL`;`nfds` 是「**最大 fd 编号 + 1**」(告诉内核只要扫到第 nfds-1 号就行,省得扫满整个表);`timeout` 控制等多久(`NULL` = 永远等、`{0,0}` = 立刻返回非阻塞、`{正数}` = 等那么多秒)。返回值是就绪 fd 的个数、`0` 表示超时、`-1` 表示出错(被信号打断时 `errno=EINTR`,呼应第 5 章)。

那个 `fd_set` 是个**位图**(bitmap)——每个 fd 编号对应一位,位是 1 表示「关心这个 fd」。配套四个宏操作它:`FD_ZERO(&set)` 清空、`FD_SET(fd, &set)` 把 fd 加进去、`FD_CLR(fd, &set)` 拿掉、`FD_ISSET(fd, &set)` 测某 fd 是否在集合里(返回后用它判断「这个 fd 是不是就绪了」)。真跑一遍最经典的用法——父进程同时盯两个 pipe 的读端:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
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
        /* 子:不读,关掉两个读端 */
        close(p1[0]);
        close(p2[0]);
        /* 故意不关 p1[1]:留着它,免得 p1 读端 EOF 让 select 立刻返回 */
        sleep(1);
        write(p2[1], "hi", 2); /* 1 秒后往 B 写 */
        sleep(2);              /* 再睡 2 秒,期间一直握着 p1[1] → p1 不 EOF,只有 B 就绪 */
        close(p2[1]);
        close(p1[1]);
        _exit(0);
    }

    /* 父:不写,关掉两个写端,只留两个读端给 select 监听 */
    close(p1[1]);
    close(p2[1]);
    int rda = p1[0];
    int rdb = p2[0];

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(rda, &rfds);
    FD_SET(rdb, &rfds);
    int maxfd = (rda > rdb ? rda : rdb) + 1;

    printf("[父] select 同时盯 pipe A(fd=%d) 和 pipe B(fd=%d),等谁先有数据...\n", rda, rdb);
    fflush(stdout);
    int n = select(maxfd, &rfds, NULL, NULL, NULL); /* 无超时,阻塞到有数据 */
    printf("[父] select 返回 %d,就绪情况:\n", n);
    if (FD_ISSET(rda, &rfds)) {
        printf("  pipe A 可读\n");
    }
    if (FD_ISSET(rdb, &rfds)) {
        printf("  pipe B 可读 → 读它:");
        char buf[16];
        ssize_t k = read(rdb, buf, sizeof(buf) - 1);
        if (k > 0) {
            buf[k] = '\0';
            printf(" %s\n", buf);
        }
    }

    waitpid(pid, NULL, 0);
    close(rda);
    close(rdb);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall select_two.c -o st && ./st
[父] select 同时盯 pipe A(fd=3) 和 pipe B(fd=5),等谁先有数据...
[父] select 返回 1,就绪情况:
  pipe B 可读 → 读它: hi
```

父进程 `FD_ZERO` 清空 `rfds`、`FD_SET` 把 pipe A 和 pipe B 的读端都加进去,然后 `select(maxfd, &rfds, ...)`(写端、异常端、超时全 `NULL`,意为「只盯可读、永远等」)阻塞等待。子进程 1 秒后往 pipe B 写了 `"hi"`,内核发现 B 的读端有数据了、立刻唤醒 select——`select` 返回 `1`(就绪了 1 个 fd),而且**返回时 `rfds` 已经被内核改写**成「只剩就绪的 fd」:用 `FD_ISSET` 一测,pipe B 在、pipe A 不在。父进程据此去 `read` B、读出 `"hi"`。这就是多路复用的全套节奏:**把关心的 fd 塞进集合 → select 等就绪 → 用 FD_ISSET 查谁好了 → 处理那一个 → 回去再等**。

这里子进程那段 `sleep(2)` 不是凑数——它是为了让 demo 干净。如果不睡,子进程 write 完 B、立刻 `close(p1[1])` 再 `_exit`,这几步一气呵成、赶在父进程被调度前就做完了;等父进程真正从 select 醒来一看,pipe A 的所有写端也都关了、A 也「就绪」了(EOF 对 select 来说也算可读),结果 select 会返回「A、B 都就绪」,把「到底是哪个 fd 先来数据」这个 demo 主旨搅混。让子进程多睡 2 秒、把 p1 的写端握住,就保证 select 醒来时只有 B 就绪,demo 才说得清楚。真实程序里,你得自己想清楚 EOF 在你的协议里意味着什么(通常是「对端关连接」)。

## select 的几个坑

`select` 的接口有几个坑,踩过一次才记得住。

**第一,`fd_set` 每次循环都要重建**。`select` 返回时,会把传入的 `fd_set`**改写**——只保留「就绪」的那些 fd 的位、把没就绪的全清掉(这就是为什么上面能用 `FD_ISSET` 判断谁好了)。所以**不能**在循环外建一次 `fd_set` 然后反复用——第二次调 select 时,集合里只剩上轮就绪的 fd、其它你想盯的全丢了。正确写法是**每轮循环里都 `FD_ZERO` + `FD_SET` 重新建一遍集合**再调 select(上面的程序因为是单次演示没体现循环,真实事件循环里这是铁律)。

**第二,`nfds` 是「最大 fd 编号 + 1」**。`nfds` 告诉内核「扫到第几号为止」,要传「集合里最大的 fd 编号 + 1」(上面 `(rda>rdb?rda:rdb)+1`)。这个 **+1 容易漏**——漏了的话内核就少扫一位、可能错过最大那个 fd 的就绪。为什么是 +1 而不是最大值?因为它是「个数意义上的上界」(类似区间 `[0, nfds)`,左闭右开),内核循环 `for (fd=0; fd<nfds; fd++)` 这样写最自然。

**第三,`timeout` 的 `struct timeval` 会被改写**。这点很隐蔽:在 Linux 上,`select` 返回时会把 `tv` 改成「**剩余的等待时间**」(超时跑完就是 0);所以你**不能**把同一个 `timeval` 跨多次 select 复用——第二轮用的就是「上一轮剩下的零头」了。真跑一个 2 秒超时、但没人写数据的 demo:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

int main(void) {
    int p[2];
    if (pipe(p) < 0) {
        perror("pipe");
        return 1;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(p[0], &rfds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0; /* 2 秒超时 */

    printf("[主] select 等 2 秒,没人写,该超时返回 0...\n");
    int n = select(p[0] + 1, &rfds, NULL, NULL, &tv);
    printf("[主] select 返回 %d(0 = 超时),剩余 tv.tv_sec=%ld\n", n, (long) tv.tv_sec);

    close(p[0]);
    close(p[1]);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall select_timeout.c -o sto && ./sto
[主] select 等 2 秒,没人写,该超时返回 0...
[主] select 返回 0(0 = 超时),剩余 tv.tv_sec=0
```

2 秒过去、没人写,`select` 返回 `0`(超时);而且我设进去的 `tv.tv_sec=2` 被**改写成了 0**——这就是「Linux 会把 timeval 改成剩余时间」的实证。纪律:每次调 select 前,**重新给 `tv` 赋值**,别复用上一轮被改过的。(POSIX 对这点描述模糊,有的实现不改、Linux 改,所以跨平台代码一律「重设」最稳。)

**第四,`fd_set` 有 `FD_SETSIZE` 上限**。`fd_set` 是个固定位图,大小由宏 `FD_SETSIZE` 决定,**默认 1024**——也就是说,fd 编号 >= 1024 时,`FD_SET` 会**越界写位图**,是未定义行为。普通程序 fd 编号到不了 1024 没事,可高并发服务器动辄几千几万连接、fd 编号很容易冲破 1024。两条路:进程级提限(编译时 `_GNU_SOURCE` + 在 main 开头 `FD_SETSIZE` 重定义不太干净;更常见的是系统级提限);或者——**别用 select,改用 `poll`/`epoll`**(它们没有这个固定位图上限)。这正是 select 被后来者取代的主要原因之一。

## select 的 O(n) 代价

最后说一个 select 在性能上的结构性短板,为后面的 `poll`/`epoll` 埋伏笔。select 每次调用,内核都得**从 0 号扫到 nfds-1**,逐位检查「这个 fd 你关心不、它就绪没」——这是 **O(n)** 的扫描(n = 你盯的 fd 数)。而且你传给内核的是整个位图、内核返回的也是改写后的整个位图,你用户态还得**再 O(n) 扫一遍**找出哪些 FD_ISSET。连接数一多(几千),这两趟 O(n) 就成了瓶颈。`poll` 稍好(去掉了固定位图、用结构体数组,但仍是 O(n) 扫描);真正解决问题的是 Linux 的 **`epoll`**——它「注册一次、之后每次只返回就绪的 fd」、是 O(就绪数) 而非 O(总fd数),这是现代高并发服务器(C10k、C100k)的主力。select 适合**连接数不多**、**跨平台**(Windows 也有 select)的场景;追求高并发就该上 epoll 了。

## 小结

`select` 让一个进程**同时盯多个 fd**:把关心的 fd 用 `FD_SET` 塞进 `fd_set` 位图,调 `select(nfds, &readfds, NULL, NULL, &tv)`,它阻塞到「有 fd 就绪 / 超时 / 被信号打断(EINTR)」;返回后用 `FD_ISSET` 查哪个 fd 好了、去处理它。真跑里 pipe A 没数据、pipe B 一秒后有数据,select 准确报出「B 可读」。四个坑记牢:**每轮循环重建 `fd_set`**(返回时被改写成只剩就绪的)、`nfds` 是「最大 fd + 1」(那个 +1 别漏)、`timeout` 的 `timeval` 会被改写成剩余时间(每次重设、别复用)、`fd_set` 有 `FD_SETSIZE=1024` 上限(fd 编号超了越界 UB)。性能上 select 是 O(n) 双扫(内核 + 用户各扫一遍位图),连接数大时该上 `poll`/`epoll`。select 是 IO 多路复用的鼻祖,接口笨重但跨平台;下一章我们进到更现代的 `poll` 和 Linux 的 `epoll`,看它们怎么一个个克服 select 的这些毛病。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 14 章「高级 IO」讲 select/poll,fd_set 操作与 timeout 改写行为清楚。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 63 章「另类 IO 模型」,select 的接口、FD_SETSIZE 限制、与 poll/epoll 的对比最系统。
- **man 页**：`select(2)`、`select_tut(2)`(教程式,fd_set/nfds/timeout 全讲)、`fd_set(3)`(`FD_ZERO`/`FD_SET`/`FD_ISSET` 宏)。
- **POSIX**：`select`/`fd_set`/`FD_SETSIZE`/`struct timeval` 是 IEEE Std 1003.1-2008(`<sys/select.h>`);FD_SETSIZE 默认值 1024 是 POSIX 建议的最小下限,具体值由实现定。
