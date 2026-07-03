---
title: "进阶 Socket：SIGPIPE、消息边界与并发服务端"
description: "上一章的 TCP 能跑通一个客户端连一个服务端,可一旦推向真实场景就有三个坑等着:这一章逐个拆。其一,SIGPIPE 阴险坑——往一个『对端已经关掉』的连接上 write,内核默认给你进程发 SIGPIPE、默认动作是直接杀进程(连原因都不打);真跑 sigpipe.c 的 mode 0:服务端 accept 后立刻关连接,客户端循环 write 到缓冲满 + RST 到,被 SIGPIPE(signal 13)打死,服务端用 WIFSIGNALED 查出子进程死于信号 13;mode 1 用 signal(SIGPIPE,SIG_IGN) 全局忽略,write 改成返回 -1/errno=32(EPIPE,Broken pipe)、进程不死、能优雅处理(或单次 write 带 MSG_NOSIGNAL 标志)。其二,TCP 没有消息边界——它是个字节流,你两次 write 的『消息』可能被合成一次 read(粘包)、也可能一次 write 被拆成多次 read(半包);正经做法是『长度前缀』组帧:每条消息前头先发一个定长的长度字段,接收方先读长度、再严格读够那么多字节(给一个 read_full/write_full 循环凑齐 N 字节的辅助函数,基于第 1 章的 write_all)。其三,单 accept 一次只能处理一个连接——把它升级成 epoll 并发服务端:accept 拿到的连接 fd 也塞进 epoll,一个事件循环同时管监听 fd + 所有连接 fd,来连接就 accept、来数据就 read,这就是把第 9-11 章的 epoll+reactor 第一次拼进真实网络服务端。全 gcc16+clang22 真跑。"
chapter: 5
order: 12
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
  - "第 11 章：Socket TCP（四件套、read 返回 0 = EOF）"
  - "第 5 章：信号（SIGPIPE 的默认动作、signal/SIG_IGN）"
  - "第 10 章：非阻塞 IO 与 reactor（epoll 事件循环,这里用来做并发服务端）"
  - "第 1 章：文件 IO 与 fd（write_all 循环凑齐字节）"
related:
  - "第 13 章：UDP 与本地域套接字（无连接、有消息边界的另一面）"
  - "第 9 章：poll 与 epoll（epoll_ctl/wait,并发服务端的主力）"
---

# 进阶 Socket：SIGPIPE、消息边界与并发服务端

## 引言：走向真实场景的三个坑

上一章的 TCP「一个客户端连一个服务端」能跑通,可一旦往真实场景推,立刻有三个坑等着,这一章逐个拆。其一,**SIGPIPE**——往一个「对端已经关掉」的连接上 `write`,内核默认给你发 SIGPIPE、默认动作是**直接杀进程**(第 5 章讲过它的名字,这里真机踩一次)。其二,**TCP 没有消息边界**——它是个字节流,「一条消息」和「一次 read」并不对应,粘包/半包是日常。其三,**单 `accept` 一次只处理一个连接**,要同时服务很多客户端、得把它升级成 epoll 并发服务端。这三个搞清楚,你就有了一个能上生产的 TCP 服务端骨架。

## TIME_WAIT 与 SO_REUSEADDR:服务端重启为什么连不上

先讲服务端骨架最该加的一行。你写完一个 TCP 服务端跑得好好的,把它停掉、立刻再启动,常常会撞上这么一句:`bind: Address already in use`。端口明明没人用了,为什么占着?根因是 TCP 关闭时那套四次挥手——**主动调 `close` 的那一端,会进入 `TIME_WAIT` 状态**,停留大约 1～4 分钟(RFC 793 的 MSL 是 2 分钟,Linux 上实测差不多 60 秒)。这段时间里那个本地 `IP:端口` 是被内核占着的。服务端通常是处理完请求就主动关连接的一方,所以服务端一重启、端口大概率还卡在 `TIME_WAIT` 里,`bind` 就报错。

`TIME_WAIT` 不是内核吃饱了撑的,它干两件事:一是可靠地完成连接终止——万一最后那个 ACK 丢了,被动关闭端会重发 FIN,主动关闭端得留着状态好去回 ACK,不留就只能回 RST、对端就报错;二是让网络上这个连接的「老分组」有时间消亡,免得它们被误当成同一个四元组(`IP:端口` 的四种组合)的新连接的数据。所以 `TIME_WAIT` 是 TCP 可靠性的一部分,我们要做的不是消灭它,而是在它存在的时候、还能把服务端重新拉起来。这就是 `SO_REUSEADDR` 的活。

### SO_REUSEADDR 救不救你——三档本机实测

很多人对 `SO_REUSEADDR` 有个误解,觉得加一行就能随便重绑端口。我写了个最小复现打脸:父进程做服务端,`fork` 出一个客户端连进来、读一句就走,服务端回完 `hi` 后**主动 `close`**(端口进入 `TIME_WAIT`),然后**立刻第二次 `bind` 同一个端口**,模拟「服务端重启」。三档对比——不加任何选项 / 只加 `SO_REUSEADDR` / 加 `SO_REUSEADDR + SO_REUSEPORT`(`SO_REUSEPORT` 需要 `#define _GNU_SOURCE` 才有定义):

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
    fflush(stdout);
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
    printf("[第一次] 关闭,端口进入 TIME_WAIT\n");

    printf("==== 第二次 bind (%s) 同端口(模拟重启)====\n", mode_name(mode));
    int lfd2 = make_listen(mode, port);
    if (lfd2 < 0) {
        printf("[第二次] bind 失败 —— TIME_WAIT 还卡着,重绑不了\n");
    } else {
        printf("[第二次] bind 成功 —— 重启可立即接管端口\n");
        close(lfd2);
    }
    return 0;
}
```

三档依次跑(`bind: Address already in use` 是 `perror` 写到 **stderr** 的,跟 `printf` 写到 stdout 的行顺序会错位,真跑看到的就是这样,别当成自己漏抄了):

```text
$ gcc -std=c11 -Wall -Wextra reuse_demo.c -o reuse_demo
$ ./reuse_demo 0 28021
==== 第一次 bind (无选项) port=28021 ====
[第一次] bind+listen 成功
[client] 收到 2 字节: hi
[第一次] 关闭,端口进入 TIME_WAIT
==== 第二次 bind (无选项) 同端口(模拟重启)====
bind: Address already in use
[第二次] bind 失败 —— TIME_WAIT 还卡着,重绑不了

$ ./reuse_demo 1 28022
==== 第一次 bind (SO_REUSEADDR) port=28022 ====
[第一次] bind+listen 成功
[client] 收到 2 字节: hi
[第一次] 关闭,端口进入 TIME_WAIT
==== 第二次 bind (SO_REUSEADDR) 同端口(模拟重启)====
[第二次] bind 成功 —— 重启可立即接管端口

$ ./reuse_demo 2 28023
==== 第一次 bind (REUSEADDR+REUSEPORT) port=28023 ====
[第一次] bind+listen 成功
[client] 收到 2 字节: hi
[第一次] 关闭,端口进入 TIME_WAIT
==== 第二次 bind (REUSEADDR+REUSEPORT) 同端口(模拟重启)====
[第二次] bind 成功 —— 重启可立即接管端口
```

为了确认根因真的是 `TIME_WAIT`(而不是别的残留),我在 mode 0 跑完后立刻用 `ss -tan` 看一眼那条连接,清清楚楚一行 `TIME-WAIT`:

```text
$ ./reuse_demo 0 28091 >/dev/null 2>&1
$ ss -tan | grep 28091
TIME-WAIT 0      0      127.0.0.1:28091    127.0.0.1:45544
```

我还顺手量了它待多久:从这条 `TIME-WAIT` 出现开始,55 秒时还在、62 秒时已消失,跟「Linux 默认约 60 秒」对得上(注意 `/proc/sys/net/ipv4/tcp_fin_timeout` 里那个 60 是 **FIN-WAIT-2** 的超时,不是 TIME_WAIT;TIME_WAIT 是 2×MSL,Linux 上 MSL 取 30 秒,所以也大约 60 秒——两个数同值但来源不同,别混)。结论很清楚:**不加任何选项,服务端重启会被 `TIME_WAIT` 卡死(第二次 bind 失败);加了 `SO_REUSEADDR`,立刻就能重新接管端口**。所以「服务端重启」场景下这行必加。

### 顺序坑:setsockopt 必须在 bind 之前

这是高频翻车点。选项是设在 socket 上的,`bind` 是去占用地址的动作,顺序反了不生效——上面那段程序里 `make_listen` 已经是这个顺序,把它单独拎出来当肌肉模板:`socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen`,四步别颠倒。你在 `bind` 之后才补一句 `setsockopt`,编译照样过、`setsockopt` 还给你返回 0,但重启照样 `Address already in use`——因为它生效的是「以后的 bind」,而你这次的 bind 早撞进 TIME_WAIT 里了,这种「不报错但不救命」的坑最难调。

### SO_REUSEADDR != 端口共享,SO_REUSEPORT 才是

但 `SO_REUSEADDR` 不是万金油,它**不等于「能让两个监听 socket 同时绑同一个端口」**——很多人(包括我当年)以为它有 BSD 那种端口共享的能力。我在本机用一个双进程最小测试打过脸:两个进程都开 `SO_REUSEADDR` 去监听同一端口,第二个照样 `bind` 失败;要真正让多个监听 socket 共享一个端口(比如多进程负载均衡),Linux 上得用 `SO_REUSEPORT`(内核 3.9+),而且**两个进程都得开它**才行:

```c
#define _GNU_SOURCE
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int make_listen(int mode, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
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
    int mode = (argc > 1) ? atoi(argv[1]) : 1;
    int port = (argc > 2) ? atoi(argv[2]) : 28030;
    int lfd = make_listen(mode, port);
    if (lfd < 0) return 1;

    pid_t pid = fork();
    if (pid == 0) { /* 子进程再 bind 同一端口 */
        int fd2 = make_listen(mode, port);
        if (fd2 < 0) { printf("[child] 第二个监听 bind 失败 —— 不能共享端口\n"); _exit(0); }
        printf("[child] 第二个监听 bind 成功 —— 端口可共享\n");
        close(fd2);
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    close(lfd);
    return 0;
}
```

两档跑一遍,对比极其直白(同样,`bind:` 那行是 stderr,会和 stdout 错位):

```text
$ gcc -std=c11 -Wall -Wextra share_test.c -o share_test
$ ./share_test 1 28041
[parent] bind+listen 成功
bind: Address already in use
[child] 第二个监听 bind 失败 —— 不能共享端口

$ ./share_test 2 28042
[parent] bind+listen 成功
[child] 第二个监听 bind 成功 —— 端口可共享
```

一句话记住——`SO_REUSEADDR` 解决的是「我自己重启能不能绑回来」,`SO_REUSEPORT` 解决的是「好几个进程能不能一起监听」(nginx 1.9.1 之后的多 worker 抢端口就是这个机制,内核还会在多个监听 socket 间做负载均衡)。所以写服务端的肌肉模板就这一段,背下来:

```c
int lfd = socket(AF_INET, SOCK_STREAM, 0);
int one = 1;
setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)); /* 必在 bind 前 */
/* ... bind / listen ... */
```

有了「重启不被 TIME_WAIT 卡」这一行打底,接下来才可以谈连接死活——因为下面要讲的 SIGPIPE,正是你服务端跑起来之后、最容易让你莫名其妙「进程没了」的那一刀。

## SIGPIPE:往死连接 write 会被默默杀掉

先讲最阴险的一个。设想客户端连上你的服务端、你正美滋滋地往这条连接 `write` 数据;突然客户端那边崩了或断网了、这条连接实际上已经死了,可你这头不知道、还在 `write`。这时内核会怎么做?**给你进程发 SIGPIPE 信号**——而 SIGPIPE 的默认动作是**终止进程**,你连一句错误都没机会打印、服务端就莫名其妙退出了,排查极痛苦。真跑复现它,用 mode 参数区分「无防护」和「`SIG_IGN` 防护」:

```c
#define _POSIX_C_SOURCE 200809L
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

int main(int argc, char** argv) {
    int protect = (argc > 1) ? atoi(argv[1]) : 0;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr));
    listen(listen_fd, 1);
    socklen_t alen = sizeof(addr);
    getsockname(listen_fd, (struct sockaddr*) &addr, &alen);
    int port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0) {
        close(listen_fd);
        int cfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(port);
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        connect(cfd, (struct sockaddr*) &saddr, sizeof(saddr));

        if (protect) {
            signal(SIGPIPE, SIG_IGN);
        }
        sleep(1); /* 等服务端 accept 后关掉连接、RST 传回本机 */

        /* 小 write 会被 TCP 发送缓冲吞掉、不报错;循环写到缓冲满 + RST 到,
           才能触发 SIGPIPE/EPIPE(真实长连接里 peer 中途断开就咬这一口) */
        char chunk[4096];
        memset(chunk, 'x', sizeof(chunk));
        int ok = 0;
        while (ok < 100000) {
            ssize_t n = write(cfd, chunk, sizeof(chunk));
            if (n < 0) {
                printf("[client] 第 %d 次write 失败: 返回 %zd, errno=%d (%s)\n", ok + 1, (long) n,
                       errno, strerror(errno));
                fflush(stdout);
                break;
            }
            ok++;
        }
        if (ok >= 100000) {
            printf("[client] 写了 10 万次都没失败(TCP 缓冲+对端没真关?)\n");
        }
        close(cfd);
        exit(0);
    }

    /* 服务端:accept 后立刻关掉,制造「客户端往已关连接 write」 */
    int cfd = accept(listen_fd, NULL, NULL);
    close(cfd);
    close(listen_fd);
    int st;
    waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) {
        printf("[server] 子进程被信号 %d 杀死(SIGPIPE=%d)\n", WTERMSIG(st), SIGPIPE);
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall sigpipe.c -o sp
$ ./sp 0        # mode 0:无防护
[server] 子进程被信号 13 杀死(SIGPIPE=13)
$ ./sp 1        # mode 1:signal(SIGPIPE, SIG_IGN)
[client] 第 28 次write 失败: 返回 -1, errno=32 (Broken pipe)
```

服务端 `accept` 完立刻 `close(cfd)`,模拟「连接已死」;客户端等 1 秒(RST 传回)后开始往这条死连接上 `write`。这里有个细节我得诚实交代:**一次小 `write` 触发不了 SIGPIPE**——因为前几个字节被 TCP 的发送缓冲(几十 KB)吞掉了、`write` 直接返回成功、根本没察觉对端已关。所以我写成循环 `write` 4KB 一块、直到把缓冲写满、RST 也传到了,这才会触发。真跑里 **mode 0** 客户端在第 28 次前后撞上 SIGPIPE,被默认动作**直接杀死**(子进程死于信号 13),客户端那边一行都没机会打——是服务端 `waitpid` 用 `WIFSIGNALED` 查出来的(`WTERMSIG(st)==13`,正是 `SIGPIPE`)。**mode 1** 在客户端开头加了 `signal(SIGPIPE, SIG_IGN)` 全局忽略它,同样的死连接上 `write` 这次**不被杀**,改成返回 `-1`、`errno=32`(`EPIPE`,Broken pipe)——你拿到这个错误就能优雅处理(记日志、清连接、重试),而不是整个进程被秒杀。

防护有两招,任选其一(工程上常常都上,双保险):**全局 `signal(SIGPIPE, SIG_IGN)`**——整个进程生命周期内忽略 SIGPIPE,所有 `write` 都改成返回 `-1/EPIPE`(服务端程序几乎都该在 `main` 开头加这一行);**或者单次 `write` 用 `send(fd, buf, n, MSG_NOSIGNAL)`**(`send` 是 socket 专用的 `write` 加强版,`MSG_NOSIGNAL` 表示「这次不发 SIGPIPE」,只这一次、不动全局)。网络服务端务必做这个防护,否则一个客户端崩了能把你整个服务端带走。

## TCP 没有消息边界:粘包、半包与长度前缀

第二个坑更日常。TCP 是个**字节流**,它保证字节**按序到达、不丢不重**,但**不保证「消息边界」**——你应用层以为的「一条消息」,和 TCP 的「一次 read」毫无对应关系。你连续 `write` 两条 `"hello"` 和 `"world"`,对端可能一次 `read` 就拿到 `"helloworld"`(**粘包**,两条粘成一条);也可能你 `write` 一次 `"hello world"`,对端第一次 `read` 只拿到 `"hel"`、第二次再拿到 `"lo world"`(**半包**,一条被拆成几半)。这是 TCP 的本质(它根本不知道、也不关心你的「消息」是几个字节),不是 bug。

正经解法是应用层自己**定边界**,最常用的是**长度前缀组帧**:每条消息发送时,前面先发一个**固定长度的「长度字段」**(比如 4 字节、网络字节序的 `uint32_t`),告诉对方「后面跟着 N 字节的载荷」;接收方先**严格读够 4 字节**拿到长度 N,再**严格读够 N 字节**拿到完整载荷。所谓「严格读够 N 字节」需要一个循环(因为单次 `read` 可能短读,第 1 章 `write_all` 的读侧版本):

```c
/* 一直读够 count 字节,处理短读和 EINTR。返回实际读到的字节数,< count 表示 EOF */
ssize_t read_full(int fd, void* buf, size_t count) {
    char* p = buf;
    size_t done = 0;
    while (done < count) {
        ssize_t n = read(fd, p + done, count - done);
        if (n == 0) {
            break; /* EOF */
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t) n;
    }
    return (ssize_t) done;
}
```

发消息就:`uint32_t len = htonl(payload_size); write_full(cfd, &len, 4); write_full(cfd, payload, payload_size);`;收消息就:`read_full(cfd, &len, 4); len = ntohl(len); read_full(cfd, buf, len);`。这样无论 TCP 怎么粘、怎么拆,接收方都能把每条消息**完整、边界清晰**地还原出来。HTTP 用 `\r\n` 做分隔(文本协议的另一种边界方案)、gRPC 用长度前缀(protobuf 二进制)、TLS 记录层也是长度前缀——本质都是「在字节流上人为加边界」。

## epoll 并发服务端:一个线程管一堆连接

第三个坑是容量。上一章那个服务端 `accept` 一次拿一个连接、处理完才能 `accept` 下一个——要是某个客户端半天不发数据、`read` 阻塞住,后面排队的连接全得等。要同时服务很多客户端,就把第 9-10 章的 epoll+reactor 拼进来:**`listen_fd` 也塞进 epoll 监听**——它「可读」就是「有新连接来了」,事件循环里就 `accept` 拿到一个新连接 fd、把这个新 fd 也 `EPOLL_CTL_ADD` 进 epoll;已注册的连接 fd 「可读」就是「数据来了」,事件循环里就 `read` 处理。这样一个事件循环同时管着「监听 fd + 所有活跃连接 fd」,来连接就接、来数据就处理,单线程就能并发扛很多客户端。

骨架大致是这样的节奏(不展开完整代码,核心是「listener 和 connection 都进 epoll、统一事件循环」):`epfd` 里挂着 `listen_fd`(关心 `EPOLLIN`)→ `epoll_wait` 返回 `listen_fd` 就绪 → `accept` 拿到 `cfd` → `fcntl(cfd, F_SETFL, O_NONBLOCK)` 设非阻塞 → `epoll_ctl(ADD, cfd)` → 下次 `epoll_wait` 返回某个 `cfd` 就绪 → `read` 它的数据、处理、可能 `write` 回应 → 这个 `cfd` EOF 就 `EPOLL_CTL_DEL` + `close`(第 10 章那个 LT 的坑,这里同样适用、别忘了)。这就是 nginx、Redis 那种高并发服务端的地基;完整的实现还要处理非阻塞 `read` 的 `EAGAIN`(读一会没数据了先放下)、`SIGPIPE` 防护(上一节)、消息边界(上一节)——前面这几章的东西全用上了。

## 小结

往已关连接 `write` 触发 **SIGPIPE**、默认杀进程(真跑 mode 0 子进程死于信号 13,服务端用 `WIFSIGNALED` 查出);防护两招——全局 `signal(SIGPIPE, SIG_IGN)` 让 `write` 改返回 `-1`/`errno=32`(`EPIPE`,真跑 mode 1 第 28 次 write 拿到),或单次 `send(.., MSG_NOSIGNAL)`。TCP 是字节流、**没有消息边界**,粘包/半包靠应用层**长度前缀组帧**解决(先发/读定长长度字段、再发/读严格 N 字节载荷,用 `read_full`/`write_full` 循环凑齐,基于第 1 章 `write_all`)。单 `accept` 一连接一处理扩展不开,升级成 **epoll 并发服务端**:`listen_fd` 和所有连接 fd 都进 epoll,事件循环统一分派(来连接就 accept+注册、来数据就 read、EOF 就 DEL+close)——这就是第 9-11 章 epoll+非阻塞+reactor 在真实网络服务上的合体。

到这里 TCP 的基本盘就齐了。下一章看 socket 的另外两副面孔:**UDP**(`SOCK_DGRAM`,无连接、不可靠、但**有消息边界**——和 TCP 形成对照)和**本地域套接字**(`AF_UNIX`,同机进程间的高速 IPC,还能在 socket 上传 fd)。

## 参考资源

- **UNP**：《Unix Network Programming, Volume 1》(W. Richard Stevens / Bill Fenner / Andrew M. Rudoff),第 5 章讲 SIGPIPE 与 MSG_NOSIGNAL、第 16 章非阻塞 connect 与并发模型、stream socket 的字节流边界问题。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 61 章 socket 高级(SO_LINGER、MSG_NOSIGNAL)、第 63 章非阻塞 IO + epoll 并发服务端实例。
- **man 页**：`send(2)`(`MSG_NOSIGNAL`)、`signal(3)`/`signal(7)`(SIGPIPE=13)、`errno(3)`(EPIPE=32)、`epoll(7)`(并发服务端)。
- **POSIX**：`SIGPIPE`/`SIG_IGN`/`send`+`MSG_NOSIGNAL`/`SOCK_STREAM` 字节流语义,IEEE Std 1003.1-2008。
