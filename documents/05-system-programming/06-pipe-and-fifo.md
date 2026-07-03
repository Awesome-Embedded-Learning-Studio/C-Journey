---
title: "IPC 上：pipe 与 FIFO,一切皆流的边界"
description: "前面几章我们有了多个进程,可它们各跑各的、没法传数据——这一章开始讲进程间通信(IPC),先上最朴素的 pipe(匿名管道)。pipe() 给一对 fd:写端写进去的字节、读端能按顺序读出来,像一根单向的水管。匿名 pipe 只能在 fork 出来的父子进程间用(子进程继承父的 fd),所以真跑 pipe_basic:父写『hello via pipe』、子读出来、父关写端后子再读返回 0(EOF——所有写端都关了,不是错误)。这里有个高频坑:父子都得关掉自己『不用的那一端』,否则 read 永远等不到 EOF(对端还握着写端,read 以为还有数据、一直阻塞)。讲清 PIPE_BUF 原子性边界:真跑 PIPE_BUF=4096,写到管道的字节数 <= 它内核保证原子(多写者并发也不会交错),超过它就可能被拆开交错。然后是 FIFO(mkfifo 命名管道):它在文件系统里有个名字,所以无亲缘关系的两个进程也能通过『约定路径』用管道通信;真跑 fifo.c,但要注意 open 默认阻塞——只读 open 会卡到有写者、只写 open 会卡到有读者(O_NONBLOCK 能打破)。顺带埋两个伏笔:子进程要 printf 就用 exit 或 _exit 前 fflush(否则输出被吞,第 2 章那个坑在 IPC 里又冒头);往读端已关的管道写会触发 SIGPIPE 默认杀进程(第 5 章信号的延续)。全 gcc16+clang22+ASan 真跑。"
chapter: 5
order: 6
tags:
  - host
  - system-programming
  - posix
  - ipc
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 2 章：进程的诞生（fork、_exit 与 stdio 缓冲陷阱）"
  - "第 3 章：exec 家族与 wait（fork+wait 标准模式）"
  - "第 5 章：信号（SIGPIPE 的伏笔、EINTR 与慢系统调用）"
related:
  - "第 7 章：IPC 下（共享内存与信号量,另一种 IPC 模型）"
  - "第 8 章：IO 多路复用（用 select 同时盯多个 pipe/socket）"
---

# IPC 上：pipe 与 FIFO,一切皆流的边界

## 引言：进程之间怎么传数据

到此我们手里的进程已经能 fork、能 exec、能收信号,可它们彼此**完全隔离**——第 2 章讲写时复制时强调过,fork 之后父子内存各自独立、改了互不可见。那两个进程之间要**传点数据**怎么办?比如 shell 里 `ls | grep`,要把 `ls` 的输出喂给 `grep`;比如父进程派一个子进程去干活、要把结果拿回来。这就需要**进程间通信**(Inter-Process Communication,IPC)。IPC 有好几套手段,这一章讲最朴素也最常用的一种——**管道**(pipe):一根**单向的字节流**,写端写进去什么、读端按写入顺序读出来,像一个先进先出的水管。管道分两种:**匿名管道**(`pipe`,只在有亲缘关系的父子进程间能用)和**命名管道**(`FIFO`/`mkfifo`,在文件系统里有个名字、无亲缘的任意进程都能用)。

## 匿名 pipe:父子间的一根单向水管

`pipe()`(`<unistd.h>`)接受一个两元素的 `int` 数组,给你一对 fd:`pfd[0]` 是**读端**、`pfd[1]` 是**写端**(这对 fd 编号谁小谁大的方向别记反——0 读 1 写)。往 `pfd[1]` `write` 的字节,能从 `pfd[0]` `read` 出来。可光 `pipe` 了还没用——同进程自己写自己读没意义,所以标准用法是 `pipe` 之后**立刻 `fork`**:fork 让子进程**继承这对 fd**,于是父子的 `pfd[0]/pfd[1]` 指向同一根管道,一个写、一个读就行。真跑一遍:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子进程:只读,先关掉自己那份写端 */
        close(pfd[1]);
        char buf[64];
        ssize_t n = read(pfd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[子] 收到 %zd 字节: %s", (long) n, buf);
        }
        /* 再读一次:父把写端关了 → 所有写端都没了 → read 返回 0(EOF) */
        ssize_t n2 = read(pfd[0], buf, sizeof(buf) - 1);
        printf("[子] 再读返回 %zd(0 = EOF,所有写端都关了)\n", (long) n2);
        close(pfd[0]);
        exit(0); // 用 exit 不用 _exit:子要 printf,_exit 会吞 stdio(呼应第 2 章)
    }

    /* 父进程:只写,先关掉自己那份读端 */
    close(pfd[0]);
    const char* msg = "hello via pipe\n";
    write(pfd[1], msg, strlen(msg));
    close(pfd[1]); /* 关写端 → 子的 read 才等得到 EOF */
    waitpid(pid, NULL, 0);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall pipe_basic.c -o pb && ./pb
[子] 收到 15 字节: hello via pipe
[子] 再读返回 0(0 = EOF,所有写端都关了)
```

父进程往写端 `write` 了 `"hello via pipe\n"`(15 字节),子进程从读端 `read` 出来、原样收到;接着子进程**再读一次**,这次返回 `0`——这就是管道的 **EOF 语义**:`read` 返回 `0` 不是「出错」,而是「**所有写端都被关了**、管道没数据了、以后也不会有了」。这个 `0` 之关键,是因为它让你能写出一个干净的读循环:`while ((n = read(rd, buf, sizeof buf)) > 0) { 处理 buf; }`——读到 `0` 就知道对端写完关了、循环自然结束。把 `read` 返回 `0` 当成错误处理,是新手写管道程序的高频 bug。

注意代码里子进程一进来 `close(pfd[1])`、父进程一进来 `close(pfd[0])`——**各自关掉不用的那一端**,这件事至关重要,正是上面 EOF 能成立的根。原因:内核判断「`read` 该不该返回 `0`(EOF)」的依据是「**还有没有任何进程握着写端**」。父写完数据 `close(pfd[1])` 之后,只要还有**任何**一个进程持有写端(哪怕它根本不写),`read` 就会**继续阻塞**、以为还可能有数据来——EOF 永远等不到。而 fork 把 fd 复制了一份,所以**父子各自都有一份写端**;父关了自己的写端,可如果子进程**没关自己那份写端**,写端计数还没归零,子的 `read` 就会**永远卡住**等一个永远不会来的 EOF。所以规矩是:**fork 之后,父子立刻把各自不用的那端关掉**(读端不读就关读端、写端不写就关写端),既为了避免这种死等,也为了让 EOF 能正确传递。

顺带一个第 2 章教训在 IPC 里的重现:子进程结尾我用的是 **`exit(0)`、不是 `_exit(0)`**。因为子进程里有 `printf`,而 `_exit` 不刷 stdio 缓冲——输出重定向到文件/管道时(全缓冲),子进程的 `printf` 就会被吞掉(我第一次跑这个程序就一片空白、啥都没打出来,正是这个坑)。父进程在 fork 之前没 `printf`,所以子进程的 stdio 缓冲里只有它自己的输出,用 `exit` 刷掉它**不会**重复打印父进程的内容(否则就要 `fflush(NULL)` 后再 `_exit`)。子进程要打印,就用 `exit`、或 `_exit` 前先 `fflush`。

## PIPE_BUF:原子性的边界

管道还有一个不大显眼、但多写者场景会咬人的属性——**单次 `write` 的原子性边界**,由宏 `PIPE_BUF`(`<limits.h>`)规定。规则是:**一次 `write` 的字节数 `<= PIPE_BUF` 时,内核保证这次写是原子的**——也就是说,如果有多个进程同时往**同一个管道的写端**写,每次写 `<= PIPE_BUF` 字节,各次写的内容**不会互相交错**(要么整块先到、要么整块后到);而**超过 `PIPE_BUF` 的写,内核不再保证原子**,数据可能被拆成几块、和别的写者交错在一起。真跑看一眼这个值:

```c
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("PIPE_BUF = %d(写到管道的字节数 <= 它,内核保证原子)\n", PIPE_BUF);

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("pipe");
        return 1;
    }
    char buf[PIPE_BUF];
    for (int i = 0; i < PIPE_BUF; i++) {
        buf[i] = 'A';
    }
    ssize_t n = write(pfd[1], buf, PIPE_BUF); /* 一次写 PIPE_BUF,原子 */
    printf("一次写 PIPE_BUF 字节: write 返回 %zd\n", (long) n);
    close(pfd[0]);
    close(pfd[1]);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall pipe_buf.c -o pbuf && ./pbuf
PIPE_BUF = 4096(写到管道的字节数 <= 它,内核保证原子)
一次写 PIPE_BUF 字节: write 返回 4096
```

Linux 上 `PIPE_BUF` 是 **4096**。单写者(像本程序父进程一个写)根本不用担心原子性——数据本就只有一路。原子性只在**多个写者并发写同一个管道**时才关键:那种场景下,如果你写的一「条」消息超过 `PIPE_BUF`,它就可能被别人的写拆散,读端就会读到错乱的半截消息。所以多写者协议要么**把每条消息控制在 `PIPE_BUF` 以内**(靠它保证原子)、要么自己上锁/用消息边界(比如每条消息前缀长度)来分块。`PIPE_BUF` 是 POSIX 给的一条「免费的原子性保险线」,单写者无视、多写者守好。

## FIFO:命名管道,无亲缘也能用

匿名 `pipe` 的限制是:它的两个 fd 只能靠 `fork` 传给后代,**没亲缘关系的两个独立进程没法共享**一个匿名管道(它没有名字、不在文件系统里、没法被「打开」)。要让任意两个进程通信,就得用 **FIFO**,也叫**命名管道**——用 `mkfifo(path, mode)`(`<sys/stat.h>`)在文件系统里建一个特殊文件,大家都能 `open(path, ...)` 拿到它的 fd;但它本质还是个管道,读写语义跟匿名 pipe 一模一样。真跑一遍,父写、子读:

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    const char* path = "/tmp/cj/p5ch6/myfifo";
    mkfifo(path, 0644); /* 创建命名管道(文件系统里有个入口) */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子进程:reader。open 只读默认阻塞,直到有写者打开同一个 FIFO */
        int rd = open(path, O_RDONLY);
        if (rd < 0) {
            _exit(1);
        }
        char buf[64];
        ssize_t n = read(rd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[子 reader] 通过 FIFO 收到 %zd 字节: %s", (long) n, buf);
        }
        close(rd);
        exit(0); // 用 exit 不用 _exit:子要 printf,_exit 会吞 stdio(呼应第 2 章)
    }

    /* 父进程:writer。open 只写也阻塞,直到有读者打开 */
    int wr = open(path, O_WRONLY);
    if (wr < 0) {
        perror("open fifo");
        return 1;
    }
    const char* msg = "hello via FIFO\n";
    write(wr, msg, strlen(msg));
    close(wr);
    waitpid(pid, NULL, 0);
    unlink(path); /* 用完删掉 fifo 文件 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fifo.c -o fifo && ./fifo
[子 reader] 通过 FIFO 收到 15 字节: hello via FIFO
```

`mkfifo` 在 `/tmp/cj/p5ch6/myfifo` 建了个 FIFO 文件(`ls -l` 能看到它权限位前头是 `p`,表示 pipe 类型);子进程 `open(path, O_RDONLY)` 当 reader、父进程 `open(path, O_WRONLY)` 当 writer,数据从父流到子。这里我图省事在**同一个程序里 fork 出 reader 和 writer**(免得开两个终端);真实场景下,FIFO 的威力在于 reader 和 writer 是**两个完全独立的程序**——比如一个 `cat /tmp/myfifo`(读)和一个你的程序(写),只要双方约定好路径,就能跨进程通信。

FIFO 有个匿名 pipe 没有的新行为,就埋在 `open` 里:**默认阻塞**。`open(fifo, O_RDONLY)` 如果还没有任何进程以写者身份打开它,这个 `open` 会**卡住**、直到有人 `O_WRONLY` 打开它;反过来 `open(fifo, O_WRONLY)` 也会卡到有读者。这就是为什么本程序里父子双方的 `open` 不会立刻返回、而是「约好了」才一起往下走。这个阻塞行为有时是你要的(自动同步),有时是麻烦(单开 reader 调试、永远卡住等一个不存在的 writer)——加 `O_NONBLOCK` 标志能打破它(`open(path, O_RDONLY | O_NONBLOCK)` 立刻返回,后面用 `read` 配合 `EAGAIN` 做非阻塞轮询,第 8 章 IO 多路复用会用到这套)。用完记得 `unlink(path)` 把 FIFO 文件删掉,免得残留在文件系统里。

## SIGPIPE:往没读者的管道写

最后埋一个伏笔,跟第 5 章的信号接上。如果管道的**读端全关了**(读端进程退了、或没人读),这时你往**写端 `write`** 会发生什么?内核会给你进程发 **`SIGPIPE`** 信号——而 `SIGPIPE` 的默认动作是**直接杀死进程**。所以一个没做防护的网络/管道服务程序,常常因为对端突然关连接、自己还在 `write`,就被 `SIGPIPE` 默默干掉了、连原因都查不到。两种应对:一是 `signal(SIGPIPE, SIG_IGN)` 全局忽略它(然后 `write` 会改成返回 `-1`/`errno=EPIPE`,你可以正常处理);二是更现代的做法——`sendmsg` 带 `MSG_NOSIGNAL` 标志(单次写不触发 SIGPIPE)。这个坑在下一章 IPC(共享内存前的同步)和后面网络 socket 章还会再撞见,这里先认下 `SIGPIPE` 这个名字。

## 小结

`pipe()` 给一对 fd(`pfd[0]` 读、`pfd[1]` 写),fork 让子进程继承,于是父子间有了一根**单向字节流**管道;**父子各自立刻关掉不用的那一端**是必须遵守的规矩(否则 read 永远等不到 EOF);`read` 返回 `0` 表示「所有写端都关了」的 EOF、不是错误,据此写 `while ((n=read(...))>0)` 读循环。`PIPE_BUF`(Linux 上 4096)是单次 `write` 的**原子性边界**——多写者并发时,每次写不超过它就保证整块不交错,超过就可能被拆散。`mkfifo` 建命名管道(FIFO),它在文件系统里有名字、无亲缘的任意进程都能 `open` 使用,代价是 `open` **默认阻塞**(只读卡到有写者、只写卡到有读者,`O_NONBLOCK` 打破)。子进程要 `printf` 就用 `exit` 或 `_exit` 前 `fflush`(否则输出被吞,第 2 章坑的 IPC 重现);往读端已关的管道写会触发 `SIGPIPE` 默默杀进程(第 5 章信号的延续)。匿名 pipe 只够父子这对方向、单工;下一章进 IPC 下半场——**共享内存**让多进程直接共享一段地址、配**信号量**做同步,那是另一种「共享」而非「流」的 IPC 模型。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 15 章「进程间通信」讲 pipe/FIFO,PIPE_BUF 与原子性、SIGPIPE 都覆盖。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 44 章 pipe、第 46 章 FIFO,管道缓冲、`O_NONBLOCK`、`SIGPIPE` 的处理讲得最细。
- **man 页**：`pipe(2)`、`mkfifo(3)`、`fifo(7)`(命名管道的 open 阻塞语义)、`pipe(7)`(PIPE_BUF 原子性、管道容量)、`signal(7)`(SIGPIPE)。
- **POSIX**：`pipe`/`mkfifo`/`PIPE_BUF` 都是 IEEE Std 1003.1-2008;`PIPE_BUF` 的原子性承诺见 `<limits.h>`。
