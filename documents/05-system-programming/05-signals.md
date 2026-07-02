---
title: "信号：sigaction、异步信号安全与 EINTR"
description: "信号是操作系统给的『软件中断』——进程跑着跑着,内核随时可能打断它、让它去跑一段你预先装好的『信号处理器』函数。这一章用 sigaction(替代过时且行为不一致的 signal())正确安装处理器:真跑一个 SIGINT 处理器,处理器只做最轻的事——设一个 volatile sig_atomic_t 标志,真正的处理留给主循环(处理器跑在不可预知的时刻,只能碰 async-signal-safe 的东西)。讲清处理器里最致命的约束:printf/malloc/free 都不在 async-signal-safe 清单里(它们可能持有锁、被信号再次中断就死锁),只能用 write + volatile sig_atomic_t;真跑一个用 write 安全打印的处理器,并演示处理器必须先存 errno、退出前恢复(不然会弄乱主流程的 errno 现场)。然后是慢系统调用被信号打断返回 EINTR 的经典坑:真跑 alarm(2) + 阻塞 read(空 pipe),read 被SIGALRM 打断返回 -1 errno=4(EINTR);要么循环重试、要么装处理器时设 SA_RESTART 让内核自动重启。最后点几条:signal() 为什么过时(跨实现行为不一致、可能自动重置 handler,生产代码必须用 sigaction+SA_RESTART)、SIGKILL/SIGSTOP 不能被捕获、几个常用信号(SIGINT/SIGTERM/SIGCHLD/SIGALRM/SIGPIPE)。全 gcc16+clang22 真跑。"
chapter: 5
order: 5
tags:
  - host
  - system-programming
  - posix
difficulty: intermediate
reading_time_minutes: 17
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 4 章：守护进程与孤儿（daemon 需要响应 SIGTERM 优雅退出）"
  - "第 1 章：文件 IO 与 fd（write、errno、read 的阻塞语义）"
  - "第 3 章：exec 家族与 wait（SIGCHLD 收尸的伏笔）"
related:
  - "第 6 章：IPC 上（pipe 写端被关触发 EOF、SIGPIPE 的进一步处理）"
  - "第 7 章：IPC 下（信号量配合共享内存的同步）"
---

> 🟡 状态:待审核(2026-07-02)

# 信号：sigaction、异步信号安全与 EINTR

## 引言：信号是「软件中断」

上一章的 daemon 能在后台跑了,可它现在是个「哑巴」——你 `kill` 它、它崩了、子进程死了、用户按 Ctrl-C,它一律没反应、只能被硬生生终止。让进程能**响应这些异步事件**的机制,就是**信号**(signal)。信号本质上是操作系统给进程的「**软件中断**」:进程正跑着某段代码,内核随时可能**暂停它、转去跑一段你预先注册的「信号处理器」(signal handler)函数**,处理器跑完再回来继续原来的执行流(或系统调用被打断返回错误,下面专讲)。Ctrl-C 之所以能终止前台进程,就是因为终端把 Ctrl-C 翻译成 `SIGINT` 信号发给了进程;`kill <pid>` 默认发的是 `SIGTERM`;子进程死时内核给父进程发 `SIGCHLD`。这一章讲怎么**正确**地接住和处理信号——这里头的坑比前面几章都密集,因为处理器跑在「任何时刻、任何主代码点」,稍不留神就是未定义行为。

## 用 sigaction 装一个处理器

ISO C 给了一套最朴素的信号接口(`signal()`/`raise()`,头 `<signal.h>`),但它**行为跨平台、跨实现不一致**(同一套代码在 BSD 风格和 SysV 风格的 libc 上语义不同,有的还会「触发一次后自动重置成默认处理」),所以**生产代码绝不用 `signal()`**——一律用 POSIX 的 **`sigaction()`**。它显式、可控、行为一致。来装一个最朴素的处理器,接住 `SIGINT`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t got_sigint = 0;

static void on_sigint(int sig) {
    (void) sig;
    got_sigint = 1; /* 处理器里只做最轻的事:设个标志 */
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* 暂不设 SA_RESTART,下个程序演示 EINTR */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    printf("[主] 装好 SIGINT 处理器,raise 给自己发一个\n");
    raise(SIGINT); /* 自己给自己发:raise 返回前信号已送达、处理器已跑 */

    /* 处理器跑过后,主循环看标志决定怎么走 */
    if (got_sigint) {
        printf("[主] 检测到标志,优雅收尾\n");
    } else {
        printf("[主] 没收到?\n");
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall handler_flag.c -o hf && ./hf
[主] 装好 SIGINT 处理器,raise 给自己发一个
[主] 检测到标志,优雅收尾
```

`sigaction` 的核心是 `struct sigaction` 这个结构:填上 `sa_handler`(处理器函数指针,签名为 `void func(int)`)、用 `sigemptyset(&sa.sa_mask)` 清空「处理器运行期间要额外屏蔽的信号集」(空集表示不额外屏蔽)、`sa_flags` 控制一堆行为位(下面讲 `SA_RESTART`),然后调 `sigaction(信号号, &sa, NULL)` 装上。这里我用 `raise(SIGINT)` **自己给自己发信号**——`raise` 给自身发信号时,信号在 `raise` 返回**之前**就已经送达、处理器已经跑完了,所以下一行 `if (got_sigint)` 看到的就是处理器刚设好的标志。这样不用真去按 Ctrl-C,程序自己就能演示整个流程(真跑里 `[主] 检测到标志` 那行就是证据)。

注意这里跟主循环通信用的是**全局的 `volatile sig_atomic_t got_sigint` 标志**,处理器只管「设标志」、真正的处理(打印、收尾、释放资源)留在主循环里做。这是信号处理器最该遵守的纪律——**处理器里只做最少、最安全的事**,重活全部推迟到主循环。为什么不能在处理器里直接 `printf`、干复杂的事?下一个程序讲。

## 处理器里能做什么:async-signal-safe 与 errno

这是信号这一章最容易出 UB 的地方,务必记牢。信号处理器跑在「**任何时刻**」——它可能打断主代码的**任意一条指令**。这意味着,如果主代码此刻正持有某个锁(比如 `printf` 内部用的 stdio 锁、或 `malloc` 的堆锁),处理器再去调同一个持锁的函数,就会**自己等自己、死锁**。所以 POSIX 圈定了一份极短的「**异步信号安全**」(async-signal-safe)函数清单——**只有这份清单上的函数,才能在处理器里安全调用**。`write`、`read`、`_exit`、`signal` 等少数在清单上;而 `printf`、`malloc`、`free`、`fopen`、绝大多数 stdio 和堆函数**都不在**。处理器里要输出,只能用 `write`(下面就这么干);要处理复杂数据,把活儿丢给主循环。

处理器还有一个容易漏的职责:**先存 `errno`、退出前恢复**。因为处理器里如果调了任何会设 `errno` 的函数(连 `write` 失败都会设),就会把主流程此刻正在用的 `errno` 现场**弄乱**——主流程可能刚收到一个错误、正准备读 `errno`,处理器这一搅,`errno` 就变了。真跑一个用 `write` 安全打印的处理器,并把 errno 的存/恢复做出来:

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void on_sigusr1(int sig) {
    int saved_errno = errno; /* 处理器先存 errno */
    (void) sig;
    const char msg[] = "[处理器] 抓到 SIGUSR1(用 write 安全打印)\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1); /* write 是 async-signal-safe,printf 不是 */
    errno = saved_errno;                        /* 退出前恢复,别污染主流程现场 */
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = on_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    errno = 42; /* 故意把 errno 设成 42,看处理器会不会弄乱它 */
    raise(SIGUSR1);
    printf("[主] 处理器跑完后 errno=%d(应还是 42,处理器存恢复了)\n", errno);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall safe_handler.c -o sh && ./sh
[处理器] 抓到 SIGUSR1(用 write 安全打印)
[主] 处理器跑完后 errno=42(应还是 42,处理器存恢复了)
```

处理器一进来先 `int saved_errno = errno;`,干完活再 `errno = saved_errno;` 恢复——这样无论它在里面调了什么,主流程的 `errno` 现场都毫发无损。真跑里我把主流程的 `errno` 故意设成 `42`,处理器跑完后它**还是 42**,这就是存/恢复的效果。输出那句「抓到 SIGUSR1」是处理器用 `write(STDOUT_FILENO, ...)` 打的——`write` 在 async-signal-safe 清单上,合法;如果你手贱把它换成 `printf`,在本机上「可能」看起来也能跑,但那是**未定义行为**(在多线程、或信号嵌套、或 stdio 正好持锁时就死锁或崩),绝不能写进生产代码。

处理器和主循环之间共享的那个标志变量,类型也很有讲究——必须是 **`volatile sig_atomic_t`**(就像上面两个程序里的 `got_sigint`、`alarmed`)。`volatile` 告诉编译器「别把它优化进寄存器、每次都老老实实从内存读」(不然主循环可能永远看不到处理器写的新值);`sig_atomic_t` 是 ISO C 保证「读和写整体完成、不会读一半」的整数类型(§7.14)。普通 `int` **不保证**处理器和主循环之间的可见性——这点对单线程看似无所谓,一旦多线程或信号嵌套就会咬人。

## 慢系统调用被信号打断:EINTR

信号还有一个副作用,会从另一个方向咬你:**「慢」系统调用被信号打断时,会返回错误 `EINTR`**。所谓「慢」调用,是指那些可能**无限期阻塞**的:`read` 一个暂时没数据的管道/socket、`wait` 一个还没死的子进程、`accept` 一个还没来连接的 socket、`epoll_wait`……这些调用阻塞期间如果**收到一个信号**且处理器跑完了,内核有两种选择:一是**让调用失败返回 `-1`、`errno=EINTR`**(意思是「你被信号打断了,要不要重来自己决定」),二是装处理器时设了 `SA_RESTART` 的话、内核**自动重启**该调用(对你透明、继续阻塞)。真跑一个不设 `SA_RESTART` 的例子——`alarm(2)` 定个 2 秒闹钟、然后 `read` 一个空 pipe(永远没数据、阻塞):

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

volatile sig_atomic_t alarmed = 0;

static void on_alrm(int sig) {
    (void) sig;
    alarmed = 1;
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = on_alrm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* 不设 SA_RESTART:read 被打断就返回 EINTR */
    sigaction(SIGALRM, &sa, NULL);

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("pipe");
        return 1;
    }

    printf("[主] alarm(2) 后阻塞 read(空 pipe,没数据)...\n");
    alarm(2); /* 2 秒后给自己发 SIGALRM */

    char buf[16];
    ssize_t n = read(pfd[0], buf, sizeof(buf)); /* 阻塞,被 SIGALRM 打断 */
    int e = errno;
    printf("[主] read 返回 %zd, errno=%d (%s), alarmed=%d\n", (long) n, e, strerror(e),
           (int) alarmed);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall eintr.c -o ei && ./ei
[主] alarm(2) 后阻塞 read(空 pipe,没数据)...
[主] read 返回 -1, errno=4 (Interrupted system call), alarmed=1
```

`read` 阻塞在空 pipe 上(没人写,永远没数据),2 秒后 `alarm` 到点、内核发 `SIGALRM`,`on_alrm` 处理器跑完(把 `alarmed` 设成 1),`read` 这才**被打断**、返回 `-1`,`errno=4`(`EINTR`,Interrupted system call)。这就是「慢调用被信号打断」的标准表现。处理它有两条路:**要么装处理器时设上 `SA_RESTART`**(`sa.sa_flags = SA_RESTART;`),让 `read`/`write`/`accept` 这类调用被信号打断后**内核自动重启**、对你透明(最省心);**要么调用方循环重试**——`while ((n = read(...)) < 0 && errno == EINTR) continue;`,自己把 EINTR 当「不算错、再来」处理掉(第 1 章 `write_all` 里那段 `if (errno == EINTR) continue;` 就是这个套路)。注意 `SA_RESTART` 并非对所有调用都生效(比如 `poll`、某些系统的 `select` 即便设了也照样返回 `EINTR`),所以严谨的工程代码常常**两者都上**——设 `SA_RESTART` 减少打扰、调用点再兜一层 `EINTR` 重试。

## 几个常用信号 + 两条硬规矩

把日后最常打交道的几个信号认一下(信号名都是 `<signal.h>` 里的宏,数字是 Linux 上的值、其它系统可能不同,所以代码里**永远用宏名、不写数字**):`SIGINT`(2,Ctrl-C 中断)、`SIGTERM`(15,`kill` 默认发的「请退出」、daemon 该接住做优雅退出)、`SIGKILL`(9,`kill -9` 的「立即打死」)、`SIGCHLD`(子进程状态变化,父进程可接住去收尸,呼应第 3 章)、`SIGALRM`(14,`alarm` 定时到)、`SIGPIPE`(13,往读端已关的管道/socket 写,默认直接杀死进程,网络程序常 `SIG_IGN` 忽略它)、`SIGSEGV`(11,段错误)、`SIGABRT`(6,`abort`)、`SIGFPE`(8,算术异常如除零)。

两条硬规矩压在最后,都是能要命的。其一,**`SIGKILL`(9)和 `SIGSTOP` 不能被捕获、不能被忽略**——它们是给内核和管理员的「最后手段」,你没法在处理器里截住它们(所以 `kill -9` 一定能杀掉一个卡死的进程,daemon 也救不了自己);其它的信号(包括 `SIGTERM`、`SIGINT`)都能被你接住做处理。其二,**接住 `SIGSEGV`/`SIGFPE` 这类「硬件异常」信号要极其谨慎**——它们意味着程序已经处于未定义状态(踩了坏内存、除零),处理器里再调任何非 async-signal-safe 的东西都可能二次崩溃;正经做法通常是处理器里只 `_exit`(或 `siglongjmp` 跳出),**绝不在里面继续「正常工作」**。

## 小结

信号是操作系统的「软件中断」:`sigaction`(永远用它、别用过时且不一致的 `signal()`)装一个处理器,信号一到、主代码被打断、处理器跑、跑完回来。处理器里的铁律:**只做最轻的事**——设一个 `volatile sig_atomic_t` 标志、用 `write` 输出(它是 async-signal-safe)、复杂处理全丢给主循环;**绝不在处理器里调 `printf`/`malloc`/`free`**(不在安全清单里,可能死锁,是 UB);**先存 `errno`、退出前恢复**(别弄乱主流程的 `errno` 现场)。慢系统调用(`read`/`wait`/`accept`/`epoll_wait`)被信号打断会返回 `-1`/`EINTR`,要么装处理器时设 `SA_RESTART` 让内核自动重启、要么调用点循环重试(或两者都上)。`SIGKILL`/`SIGSTOP` 不能被捕获(所以 `kill -9` 必杀),其余信号都能接——`SIGTERM` 接住做优雅退出、`SIGCHLD` 接住收尸、`SIGPIPE` 常忽略。

到这里,我们的进程能「懂事」地响应外部事件了。但前面几章反复提到的「进程间通信」——父子之间传字节、跨进程共享数据——还一直没正式讲。下一章开始进 IPC:先用最朴素的 `pipe` 在父子进程间单向传字节、再上 `FIFO`(命名管道)做无亲缘进程间的通信,顺便撞上 `SIGPIPE` 和「写端全关触发 EOF」这两个管道编程的经典边界。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 10 章「信号」是这门话题的权威长篇,`sigaction`、`SA_RESTART`、async-signal-safe、信号屏蔽字全套。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 20–22 章讲信号,第 22 章逐条列 async-signal-safe 函数清单,`EINTR` 与 `SA_RESTART` 的对照清楚。
- **man 页**：`sigaction(2)`、`signal(2)`(为什么不要用)、`kill(2)`、`raise(3)`、`alarm(2)`、`signal-safety(7)`(async-signal-safe 函数清单,本章铁律的依据)、`signal(7)`(信号编号表)。
- **ISO C**：§7.14(`<signal.h>`:`signal`/`raise`/`sig_atomic_t`)——ISO C 给了最朴素的接口,但 `sigaction`、`SA_RESTART`、完整的信号屏蔽字模型都是 POSIX(IEEE Std 1003.1-2008)扩展。
