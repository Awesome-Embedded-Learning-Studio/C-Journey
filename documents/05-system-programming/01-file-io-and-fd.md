---
title: "文件 IO 与 fd：open/read/write/dup 与缓冲那点事"
description: "这一章我们绕开 stdio（fopen/fread 那套 C 标准库封装），直接用 POSIX 的 open/read/write/close/lseek/dup 跟内核打交道。先说清一件尴尬事：这几个函数不是 ISO C 标准函数，它们是 POSIX（IEEE 1003.1）规定的——所以我们真跑一下 -std=c11 下到底会发生什么（strdup 直接编译失败、open 却恰好漏网，别凭记忆下结论）。然后是文件描述符 fd：它就是一个不起眼的小整数，0/1/2 被 stdin/stdout/stderr 占走、新打开的文件从 3 开始拿最小空闲号。真跑 open 的各种 flags（O_RDONLY/O_WRONLY/O_RDWR 叠 O_CREAT/O_TRUNC/O_APPEND/O_EXCL，漏写第三参 mode 是经典坑）、errno 只在出错时才有意义（成功路径不会清零，实测残留）、read/write 的短读短写（要 4096 实得 6，所以必须循环凑——给一个 write_all 包装）、忘了 close 会撑满 fd 表实测报 EMFILE=24、lseek 在文件里跳着读写与稀疏文件的洞、dup2 把 stdout 改道进文件。最后是本章另一个核心：两套缓冲——stdio 在用户态有一层（fflush 只刷到内核）、内核里还有一层页缓存（fsync(fd) 才真落盘），用 _exit 实测吞掉未 flush 的 stdio 缓冲（文件 0 字节）。全 gcc16+clang22+ASan 真跑。"
chapter: 5
order: 1
tags:
  - host
  - system-programming
  - posix
  - file-io
difficulty: intermediate
reading_time_minutes: 18
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 12 章：基础 IO（三标准流 stdin/stdout/stderr、printf/scanf 是变参）"
  - "阶段 0·第 9 章：标准与优化（-std=c11 与 gnu11 的 __STRICT_ANSI__ 分水岭）"
  - "第 2 章：整型家族与 sizeof（size_t、定宽整型、sizeof 是编译期运算符）"
related:
  - "第 2 章：进程的诞生（fork、写时复制，_exit 与 stdio 缓冲的进一步交锋）"
  - "阶段 0·第 10 章：Sanitizer 门禁（ASan/UBSan，本章用它复核内存安全）"
---

> 🟡 状态:待审核(2026-07-02)

# 文件 IO 与 fd：open/read/write/dup 与缓冲那点事

## 引言：从 stdio 走到系统调用

第 12 章我们用 `fopen`/`fread`/`fwrite`/`fclose` 跟文件打过照面，那会儿它们来自 `<stdio.h>`，是 **C 标准库**给的一套封装。可那套东西底下真正在干活的是谁？是操作系统提供的**系统调用**（system call，syscall）——`open`、`read`、`write`、`close` 这些。`fopen` 帮你把 fd 包成 `FILE*`、加上一层用户态缓冲、替你处理短读短写；好处是省心，代价是你离「机器真正在干什么」隔了一层棉花。这一章我们要把棉花掀掉，直接用最原始的系统调用跟内核说话。

掀掉之前，有一件尴尬事必须先摊开说：**`open`/`read`/`write`/`close` 不是 ISO C 标准函数**，它们是 **POSIX**（IEEE Std 1003.1，Unix 系操作系统的可移植性标准）规定的。ISO C 只认 `<stdio.h>` 那一套（`fopen`/`fread`/`fwrite`），它压根不知道什么叫「文件描述符」。所以从这一章起，我们正式走出 ISO C 的地界、迈进 POSIX——这是系统编程绕不过去的一步，也是为什么这章的代码顶上都多了一行 `#define _POSIX_C_SOURCE 200809L`（先记住这个悬念，马上真跑给你看它到底管什么用）。

换句话说，这一章的代码不再保证在「任何一个号称实现了 C 的环境」里都能跑——比如 Windows 的 MSVC 就没有 `open`/`read`（它有自己一套 `_open`/`_read`）。本章的所有真跑都在 Linux（WSL2）+ glibc 上完成，gcc 16.1.1、clang 22.1.6。这是系统编程的题中之意：一旦你开始跟内核打交道，「平台相关」就成了常态，诚实交代比假装可移植重要得多。

## 文件描述符：一个不起眼的小整数

掀开 stdio 之后，第一个撞进眼里的概念就是**文件描述符**（file descriptor，简称 **fd**）。它是什么？说白了就是一个**小非负整数**——不是什么神秘指针、不是句柄对象，就是 `3`、`4`、`7` 这种数字。内核给每个进程都维护一张「已打开文件」的小表，fd 就是这张表里的下标：你 `open` 一个文件，内核在某格里登个记，把格子的编号（下标）还给你；之后你 `read`/`write`/`close` 都只拿这个编号说话，内核凭编号反查回那张表。

这张表的前三格是**预先占好**的，每个进程一启动就有：**0 号是 stdin、1 号是 stdout、2 号是 stderr**——正好对上第 12 章那三个标准流。所以你新打开的文件，fd 不会是 0/1/2，从 **3** 开始；并且内核的分配规则是「**挑最小的空闲号给你**」。这两条加起来，跑一遍就全明白了：

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int a = open("/tmp/cj/p5ch1/fa.txt", O_CREAT | O_WRONLY, 0644);
    int b = open("/tmp/cj/p5ch1/fb.txt", O_CREAT | O_WRONLY, 0644);
    printf("第一个文件 fd = %d\n", a); /* 0/1/2 被 stdin/out/err 占,新 fd 从 3 起 */
    printf("第二个文件 fd = %d\n", b);
    close(a); /* 释放 3 号 */
    int c = open("/tmp/cj/p5ch1/fc.txt", O_CREAT | O_WRONLY, 0644);
    printf("关掉第一个后再开,fd = %d\n", c); /* 最小空闲号回收 → 3 */
    close(b);
    close(c);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fd_intro.c -o fd_intro && ./fd_intro
第一个文件 fd = 3
第二个文件 fd = 4
关掉第一个后再开,fd = 3
```

头两个文件依次拿到 `3`、`4`，毫不意外；好戏在第三步——我把 `3` 号 `close` 掉之后再开一个新文件，它拿到的又是 `3`，因为 3 号格子刚空出来、是当前最小空闲号，内核就把它回收复用了。这个「最小空闲号」规则不是花架子，它正是 shell 做 `2>&1`、`> file` 这类**重定向**的底层原理——把某个已占用的编号 `close` 掉、再 `dup` 一个别的 fd 占到那个号上，后面对那个编号的读写就改道了（后面 `dup2` 那节亲手玩一次）。

顺带一提，fd 的数量**不是无限的**。每个进程有一个 fd 上限，可以用 `getrlimit(RLIMIT_NOFILE, ...)` 查、`setrlimit` 调，shell 里对应的命令是 `ulimit -n`。我这台机器默认上限是 1048576（一百万出头，够壕）；等下面讲 `close` 的坑时，我会把这个上限**故意压到 8**，让你亲眼看看「忘了 close」会怎样撑爆这张表。

## open(2)：开门拿 fd

`open` 的活儿就一件：把一个路径**打开**（或新建），换成 fd 交给你。原型长这样（POSIX 版，头 `<fcntl.h>`）：

```c
int open(const char* path, int flags, ...);
int open(const char* path, int flags, mode_t mode); /* 当 flags 含 O_CREAT 时用这版 */
```

`path` 是路径；`flags` 是一组标志位，决定「怎么开」；`mode` 是权限（只在你给了 `O_CREAT`、要新建文件时才用得上）。返回值是 fd（一个 `>= 0` 的小整数），失败返回 `-1`、并且把 `errno` 设上（`errno` 马上专讲）。

`flags` 分两类，拼的时候用按位或 `|` 叠起来。第一类是**访问模式**，三者互斥、必选其一：`O_RDONLY`（只读）、`O_WRONLY`（只写）、`O_RDWR`（读写）。第二类是**修饰位**，按需叠加，常用的有 `O_CREAT`（不存在就建）、`O_TRUNC`（如果文件已存在、且是写打开，就先把它截成长度 0）、`O_APPEND`（每次写都追加到文件末尾，后面多进程写日志就靠它）、`O_EXCL`（跟 `O_CREAT` 配合：文件已存在就**反而失败**，专门用来「保证只有我来建它」）。

`mode` 是权限位，写成八进制比如 `0644`（ owner 读写、其他人只读）。这里有个真正的坑就地提醒一句：**`mode` 只在 `flags` 里有 `O_CREAT` 的时候才被读**——可 `open` 是变参的，你不写第三个参数它**不会报错**，于是「`O_CREAT` 加了、`mode` 忘写了」就会拿一个**未指定的随机权限**去建文件，轻则权限乱七八糟，重则在某些实现上建出谁都改不了的怪文件。所以纪律是：**只要写了 `O_CREAT`，第三个参数 `mode` 就老老实实跟在后面**，一眼都不能省。另外实际落地的权限还会被进程的 `umask` 削掉一层（你写 `0666`、umask 是 `022`，最后文件是 `0644`），这是 shell 的事，这里知道有这么个减法就够了。

把上面这些凑起来，真跑三个典型场景——只读打开一个不存在的文件、用 `O_EXCL` 首次创建、再 `O_EXCL` 同一个文件第二次：

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    /* (a) 只读打开一个不存在的文件,又不给 O_CREAT —— 必败 */
    int fd1 = open("/tmp/cj/p5ch1/no_such_file", O_RDONLY);
    if (fd1 < 0) {
        printf("只读打开不存在: 失败, errno=%d (%s)\n", errno, strerror(errno));
    }

    /* (b) O_CREAT|O_WRONLY|O_EXCL:不存在就建、存在就拒绝 */
    int fd2 = open("/tmp/cj/p5ch1/excl.txt", O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd2 >= 0) {
        printf("首次 O_EXCL 创建: 成功, fd=%d\n", fd2);
        close(fd2);
    }

    /* (c) 文件已经在,再 O_EXCL 一次 —— 这次该栽在 EEXIST 上 */
    int fd3 = open("/tmp/cj/p5ch1/excl.txt", O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd3 < 0) {
        printf("再次 O_EXCL:     失败, errno=%d (%s)\n", errno, strerror(errno));
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall open_flags.c -o of && ./of
只读打开不存在: 失败, errno=2 (No such file or directory)
首次 O_EXCL 创建: 成功, fd=3
再次 O_EXCL:     失败, errno=17 (File exists)
```

三行输出把 `open` 的脾气展得明明白白：只读开一个不存在的文件，败在 `errno=2`（`ENOENT`，No such file or directory）；第一次 `O_EXCL` 创建成功，拿到 `fd=3`；第二次对**同一个**文件再 `O_EXCL`，文件已经在那儿了，`O_EXCL` 的职责就是反手拒绝，败在 `errno=17`（`EEXIST`，File exists）。`O_EXCL` 这一手在写「单实例守护进程」「锁文件」「临时文件」时特别有用——它能保证「这个文件是我亲手建的、不是别人先建好的」，避免你一脚踏进别人留下的文件里。

顺便回应开头那个 `_POSIX_C_SOURCE` 的悬念。本章所有程序顶上都写着 `#define _POSIX_C_SOURCE 200809L`，它的意思是「请把 POSIX.1-2008 及其要求的符号都暴露给我」。为什么教程都劝你加它？因为严格 `-std=c11` 模式下编译器会定义 `__STRICT_ANSI__`，glibc 看到它就把一堆 POSIX 符号藏起来。可别凭印象觉得「藏起来了那 `open` 肯定就编译不过」——我自己也是这么以为的，真跑一遍才发现事情没这么干脆。下面是两个**都不带** `_POSIX_C_SOURCE` 的小程序，唯一区别是一个用了 `strdup`、一个用了 `open`：

```c
#include <string.h>
int main(void) {
    char* s = strdup("hi");
    return s != 0;
}
```

```c
#include <fcntl.h>
#include <unistd.h>
int main(void) {
    int fd = open("/tmp/cj/p5ch1/x.txt", O_RDONLY);
    if (fd >= 0)
        close(fd);
    return 0;
}
```

同样 `-std=c11 -Wall -Wextra`、同样没加 macro，命运却截然相反：

```text
$ gcc -std=c11 -Wall -Wextra no_macro_strdup.c -o nms    # 用 strdup
no_macro_strdup.c: In function 'main':
no_macro_strdup.c:3:15: error: implicit declaration of function 'strdup'; did you mean 'strcmp'? [-Wimplicit-function-declaration]
    3 |     char* s = strdup("hi");
      |               ^~~~~~
      |               strcmp
no_macro_strdup.c:3:15: error: initialization of 'char *' from 'int' makes pointer from integer without a cast [-Wint-conversion]
$ gcc -std=c11 -Wall -Wextra no_macro_open.c -o nmo       # 用 open/close
$ echo $?
0                                                          # 居然编过了、零警告
```

`strdup` 被门控得死死的——没有原型、gcc 14 起隐式声明从警告升级成硬 error（呼应阶段 0 第 9 章），编译直接失败、退出码 1。可 `open`/`close` 这两个同样属于 POSIX、同样不在 ISO C 里的调用，却**恰好漏网**、编过了还没半句警告（gcc、clang 都一样，退出码 0）。我把 glibc 在 `-std=c11` 下实际暴露的宏打出来看了一眼：`_POSIX_C_SOURCE`、`_DEFAULT_SOURCE` 都没定义、`__USE_POSIX` 也没开，门确实是关着的——但这几个基础调用就是从门缝里溜出来了（glibc 2.43 对 `<fcntl.h>`/`<unistd.h>` 里这几个历史悠久的核心入口放得格外松）。

这种「有的漏、有的卡」是 libc 实现的细节、**不是标准承诺的**：换 musl、换老版 glibc、换 BSD，漏的不一定还漏。所以正确的姿势依然是**老老实实写 `#define _POSIX_C_SOURCE 200809L`**（或者干脆 `-std=gnu11`）——这样不管哪个 libc、不管用哪个 POSIX 调用，都能稳稳拿到声明。这里压一句教训：**断言 C/POSIX 行为之前一定自己真跑一遍**，我这次就靠记忆差点写错——以为「没 macro 一定编不过」，实际 `open` 偏偏能过。

## errno：只在出错时才有意义

刚才每行失败都配了一个 `errno`，现在把它说清楚。`errno` 是个 `int`，头 `<errno.h>`。它**不是一个真全局变量那么简单**——C11 起（ISO/IEC 9899:2011 §7.5）规定它有**线程存储期**（thread-local），也就是说每个线程各有一份自己的 `errno`，你在线程 A 里调用失败设上的 `errno`，不会串到线程 B 那儿去（这一点对后面阶段讲多线程至关重要，先记下）。它也不是「函数返回值」，而是「**副作用**」：库函数/系统调用在出错时，会顺手把一个能说明「错在哪」的正整数塞进 `errno`，比如 `ENOENT=2`、`EEXIST=17`、`EMFILE=24`。

把 `errno` 翻译成人话有两个常用手段：`strerror(errno)`（头 `<string.h>`）返回错误码对应的字符串；`perror("前缀")`（头 `<stdio.h>`）直接把「前缀: 错误字符串」打到 stderr。但 `errno` 最容易让人栽跟头的地方在于——**它只在「刚才那个调用明确返回了错误」时才有意义**。一个成功的调用**没有义务把 `errno` 清零**。这件事光说不踏实，真跑给你看：

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    int bad = open("/tmp/cj/p5ch1/definitely_not_here", O_RDONLY); /* 失败,errno=ENOENT */
    printf("失败后 errno = %d (%s)\n", errno, strerror(errno));

    int good = open("/tmp/cj/p5ch1/ok.txt", O_CREAT | O_WRONLY, 0644); /* 成功 */
    printf("成功后 errno = %d (%s)\n", errno, strerror(errno));        /* 没被清零! */

    if (bad >= 0)
        close(bad);
    if (good >= 0)
        close(good);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall errno_resid.c -o er && ./er
失败后 errno = 2 (No such file or directory)
成功后 errno = 2 (No such file or directory)
```

第一行 `bad` 那次 open 失败，`errno=2` 合情合理；可第二行 `good` 那次明明成功了，`errno` 居然**还是 2**——成功路径根本没帮我们把上一次的残留抹掉。这件事直接决定了一条写系统编程的铁律：**判断一个调用成不成功，永远只看它的返回值**（`fd >= 0` 还是 `fd == -1`），**绝不能写 `if (errno != 0)` 来判成败**——因为 `errno` 可能正揣着上一次失败留下的旧债。规矩就一句：`errno` 只在返回值告诉你「出错了」之后再去读，读一次就够，别在成功路径上搭理它。

## write 与 read：字节进、字节出

有了 fd 和 `errno`，就可以真正读写文件了。`read` 和 `write` 的原型在 `<unistd.h>`：

```c
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
```

两者的核心约定值得逐字记住。`write`：把 `buf` 里前 `count` 个字节写进 `fd`，返回**实际写入的字节数**（`>= 0`），失败返回 `-1`。`read`：从 `fd` 读最多 `count` 个字节进 `buf`，返回**实际读到的字节数**；返回 `0` 表示「读到文件末尾」（EOF，不是错误！）；失败返回 `-1`。

类型上有个细节：`count` 是 `size_t`（无符号，ISO C §7.19，第 2 章见过），返回值却是 `ssize_t`（**带符号**的 size，POSIX，`<sys/types.h>`）——为什么带符号？因为返回值得既能表示「读了 N 字节」又能表示「出错了 `-1`」，无符号可装不下 `-1`。所以打印这俩类型时，`size_t` 用 `%zu`、`ssize_t` 用 `%zd`，别混（第 12 章说过格式说明符不匹配是 UB）。

来一个最朴素的写完再读：

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    /* --- 写 --- */
    int fd = open("/tmp/cj/p5ch1/hello.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    const char* msg = "hello, file IO\n";
    ssize_t n = write(fd, msg, strlen(msg));
    printf("write 写了 %zd 字节\n", n);
    close(fd);

    /* --- 读 --- */
    fd = open("/tmp/cj/p5ch1/hello.txt", O_RDONLY);
    char buf[64];
    n = read(fd, buf, sizeof(buf) - 1);
    if (n >= 0) {
        buf[n] = '\0'; /* read 不管字符串结尾,自己补 \0 */
        printf("read 读到 %zd 字节: %s", n, buf);
    }
    close(fd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall write_read.c -o wr && ./wr
write 写了 15 字节
read 读到 15 字节: hello, file IO
```

`write` 写了 15 字节（`"hello, file IO\n"` 正好 15 个字节），`read` 又把这 15 字节原样读回来。这段代码把字节搬进栈上的 `buf`，我另外用 `-fsanitize=address,undefined` 复核过一遍，退出码 0、没有内存错误——本章凡是涉及往缓冲区里读写的例子都照此过 ASan。代码里有个**坑就地提醒**：`read` 只负责把字节搬进 `buf`，它**不会**帮你补字符串结尾的 `'\0'`——`buf` 是你开的字符数组，里面装的是裸字节，不补 `'\0'` 就直接当字符串 `printf("%s")` 用，会一路读越过本次读到的数据、踩到 `buf` 里原来的垃圾值直到撞见偶然的 `\0`。所以上面我在 `buf[n]` 处手动补了一个 `'\0'`，这才敢当字符串用。记住：**`read` 给你的是一段字节流，不是 C 字符串**；要当字符串用，自己负责结尾。

## 短读短写：别假设一次到底

刚才那个例子 `write` 一次写了 15 字节、`read` 一次读了 15 字节，正好都「一次到底」。但你千万别据此总结出「调一次 `write` 就一定把我要写的都写完了」——这正是 `read`/`write` 最坑人的地方：**它们都允许「短读短写」**，也就是返回的字节数**可以小于**你请求的 `count`，剩下的你得自己接着读/写。

`read` 的短读最好复现——对一个只有 6 字节的文件，我张口要 4096 字节，它只会把实际存在的 6 字节给我：

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* 先建一个只有 6 字节的小文件 */
    int w = open("/tmp/cj/p5ch1/small.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    write(w, "hello\n", 6);
    close(w);

    /* 然后张口要 4096 字节,看实际给多少 */
    int fd = open("/tmp/cj/p5ch1/small.txt", O_RDONLY);
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    printf("要了 %zu 字节, 实际读到 %zd 字节\n", sizeof(buf), n);
    close(fd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall short_read.c -o sr && ./sr
要了 4096 字节, 实际读到 6 字节
```

要了 4096，实得 6——这就是一次短读。这个例子里的短读其实是因为很快撞上 EOF（文件就那么大），读完了自然停；但「短读」真正让人头秃的场景是**还没到 EOF 也只给你一部分**——读管道、读 socket 时，对端先写了一小段、或内核缓冲区暂时只有那么多，`read` 就会先把你**能拿到的**那点给你返回，让你决定要不要继续等。短写同理：往管道、socket 写一大批数据，对端缓冲区快满了，`write` 可能只写进去前一半就返回了，告诉你「这次先写这么多」。

诚实地补一句：对**普通磁盘文件**，Linux 上 `write`/`read` 一般会尽量一次到底（像刚才那两次 15 字节），但这只是「通常」、不是标准给你的承诺。POSIX `read(2)`/`write(2)` 的语义白纸黑字写着「返回值可能小于 `nbyte`」。所以**只要你想保证写满 N 字节，就必须套个循环**，一次没写完就接着写剩下的。下面这个 `write_all` 是工程里几乎人手一份的标配：

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* 把 count 字节完整写下去:处理「短写」和被信号打断的 EINTR。失败返回 -1。*/
ssize_t write_all(int fd, const void* buf, size_t count) {
    const char* p = buf;
    size_t done = 0;
    while (done < count) {
        ssize_t n = write(fd, p + done, count - done);
        if (n < 0) {
            if (errno == EINTR)
                continue; /* 被信号打断:不算错,重试 */
            return -1;    /* 别的真错误 */
        }
        done += (size_t) n; /* n 可能 < 剩余量,继续凑 */
    }
    return (ssize_t) done;
}

int main(void) {
    int fd = open("/tmp/cj/p5ch1/wall.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    const char* msg = "这条字符串故意写长一点,演示即使被拆成几段也能凑完整。\n";
    ssize_t n = write_all(fd, msg, strlen(msg));
    printf("write_all 共写入 %zd 字节\n", n);
    close(fd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall write_all.c -o wa && ./wa
write_all 共写入 80 字节
```

这个循环干两件事：一是**凑够字节数**——`done` 记着已经写下去多少，每次只写「剩下的」`count - done`，只要 `done < count` 就接着写，直到全写完；二是处理 `EINTR`——`write` 有可能被到达的信号打断，这时它返回 `-1` 且 `errno == EINTR`，这**不是真错误**，重试就行（信号是阶段后面专门一章的大题目，这里先认下 `EINTR` 这个名字、知道「被信号打断要重试而不是报错退出」就够了）。`read` 的对应版本叫 `read_all` 或 `readn`，思路完全对称，留给你当练习。

## close 与 fd 泄漏：开门别忘了关

`close(fd)`（`<unistd.h>`）干的事和 `open` 相反：告诉内核「这个 fd 我用完了，把那张表里的格子释放掉」。返回 `0` 表示成功、`-1` 表示失败。看起来最没戏份的一步，却是长跑进程最容易栽的地方——**忘了 `close`，fd 就一直占着格子不还**。普通短命程序无所谓（进程一退出，内核把它打开的所有 fd 统统回收），可一个守护进程、一个服务器，跑着跑着 fd 表就被漏光了。

要把这个后果亲眼复现，不用真去漏一百万个——用 `setrlimit(RLIMIT_NOFILE, ...)` 把本进程的 fd 上限压到 8，再循环 `open` 不 `close`，几下就撑爆：

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

int main(void) {
    /* 把本进程的 fd 上限压到 8,快速复现「不 close 撑满 fd 表」的后果 */
    struct rlimit rl = {8, 8};
    setrlimit(RLIMIT_NOFILE, &rl);

    int count = 0;
    for (;;) {
        int fd = open("/tmp/cj/p5ch1/leak.txt", O_CREAT | O_WRONLY, 0644);
        if (fd < 0) {
            printf("第 %d 次 open 失败: errno=%d (%s)\n", count + 1, errno, strerror(errno));
            break;
        }
        count++; /* 故意不 close,fd 越漏越多 */
    }
    printf("一共泄漏 %d 个 fd 就撑爆了表\n", count);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall close_leak.c -o cl && ./cl
第 6 次 open 失败: errno=24 (Too many open files)
一共泄漏 5 个 fd 就撑爆了表
```

上限压到 8，意味着本进程只能用 fd 0..7；其中 0/1/2 被 stdin/stdout/stderr 占着，所以我每次 `open` 拿到的是 3、4、5、6、7——连漏 5 个之后，8 个格子全满，第 6 次 `open` 直接被内核拒绝，`errno=24`（`EMFILE`，Too many open files）。一个 `ulimit -n 8` 的微缩模型，把「服务器跑半年突然再也开不动文件、连个 socket 都建不起来」的惨剧浓缩成了 5 次 open。纪律也就这一句：**开一个 fd，用完立刻 `close`**，越早越好；尤其在错误处理路径上，别一遇到错就 `return`、把已经打开的 fd 丢在身后（C 没有 RAII，没人替你兜底）。

## 文件偏移与 lseek：在文件里跳着读写

每个打开的 fd，内核都给它记着一个「**当前偏移量**」（offset）——一个表示「下一次 `read`/`write` 从文件的第几号字节开始」的位置。`read`/`write` 每次成功后会自动把这个偏移往后推进实际读写的字节数，所以你连续 `read` 就能依次把文件读完，不用自己记位置。

想**主动**跳到某个位置呢？用 `lseek`（`<unistd.h>`，注意是 `l` 开头不是 `fseek`——`fseek` 是 stdio 那套给 `FILE*` 用的）：

```c
off_t lseek(int fd, off_t offset, int whence);
```

`whence` 决定 `offset` 怎么解释：`SEEK_SET`（从文件开头算，绝对位置）、`SEEK_CUR`（从当前位置算，相对）、`SEEK_END`（从文件末尾算）。返回新的偏移量（类型 `off_t`，POSIX 的文件偏移类型，`<sys/types.h>`）。`lseek` 的一个好玩用法是 `lseek(fd, 0, SEEK_END)`——跳到末尾、返回值就是**文件大小**，不用调别的接口。来一次完整的「写完→跳回开头读→跳到末尾外再写→读洞」：

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int fd = open("/tmp/cj/p5ch1/seek.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    write(fd, "ABCDEF", 6);

    /* 把偏移拉回开头,再读 */
    lseek(fd, 0, SEEK_SET);
    char buf[6];
    read(fd, buf, 6);
    printf("回到开头读: %.6s\n", buf);

    /* 跳到第 15 号位置写一个 X —— 中间 6..14 成了「洞」*/
    lseek(fd, 15, SEEK_SET);
    write(fd, "X", 1);

    /* 用 SEEK_END 量出文件多大 */
    off_t sz = lseek(fd, 0, SEEK_END);
    printf("文件大小 = %lld 字节\n", (long long) sz);

    /* 读洞里那 9 个字节,稀疏文件的洞读出来全是 0 */
    lseek(fd, 6, SEEK_SET);
    char hole[9];
    read(fd, hole, 9);
    printf("洞里的 9 字节:");
    for (int i = 0; i < 9; i++)
        printf(" %d", hole[i]);
    printf("\n");
    close(fd);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall lseek_demo.c -o ls && ./ls
回到开头读: ABCDEF
文件大小 = 16 字节
洞里的 9 字节: 0 0 0 0 0 0 0 0 0
```

这里发生了一件有意思的事：我先写了 6 个字节 `ABCDEF`，然后**直接 `lseek` 跳到第 15 号位置**、再写一个 `X`。中间第 6..14 号那 9 个字节我**根本没写过**——可文件照样有 16 字节长（`SEEK_END` 给出 16），而那 9 个「没写过」的字节读出来全是 `0`。这就是所谓的**稀疏文件**（sparse file）：跨过去没写的部分不占真实磁盘块、只是一个「洞」，读的时候内核用 `\0` 填给你。有些场景（比如数据库、虚拟机磁盘镜像）里头大量连续的零，稀疏文件能省一大笔磁盘。这里把它当 `lseek` 的副产品见识一下，重点是记住**偏移量可以随便跳、不一定连续**。

## dup 与 dup2：复制 fd 就是复制一条路

最后一个 fd 操作——`dup` 和 `dup2`（`<unistd.h>`）。它俩干的事是「**复制一个 fd**」：让一个**新的 fd 编号**指向**同一个**打开文件（同一个偏移、同一份权限）。`dup(oldfd)` 返回最小空闲号当新 fd；`dup2(oldfd, newfd)` 更强——它强制把 `oldfd` 复制到**你指定的 `newfd` 编号**上（如果 `newfd` 已经开着，先默默 `close` 它）。

听起来抽象，可它的杀手级应用就一个词：**重定向**。回想 fd 那一节——0/1/2 是 stdin/stdout/stderr，而 `printf` 默认就是往 fd 1 写。如果我用 `dup2` 把一个普通文件的 fd **复制到编号 1** 上，那此后所有 `printf`（往 fd 1 写）就全改道进了那个文件，这正是 shell 里 `> file` 的底层原理。亲手玩一次：

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* 先偷偷记下真正的 stdout,等会儿好还原 */
    int saved = dup(1);

    int fd = open("/tmp/cj/p5ch1/redirected.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    dup2(fd, 1); /* 把 fd 1(stdout)改指向我们的文件 */
    close(fd);   /* 有了 fd 1 这条路,原 fd 多余了 */

    /* 这句 printf 不上屏幕,改道进了文件 */
    printf("这行被重定向,写进了文件\n");
    fflush(stdout); /* stdio 是有缓冲的,不刷不一定落(下一节细讲) */

    /* 把真正的 stdout 还原回去 */
    dup2(saved, 1);
    close(saved);
    printf("还原 stdout,这行才上屏幕\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall dup2_redirect.c -o dr && ./dr
还原 stdout,这行才上屏幕
$ cat /tmp/cj/p5ch1/redirected.txt
这行被重定向,写进了文件
```

屏幕上只看到「还原 stdout,这行才上屏幕」一句，而「这行被重定向」那句**没上屏幕**——它进了 `redirected.txt`，`cat` 一下文件就能看到。整个戏法的关键就是 `dup2(fd, 1)`：它把 fd 1（stdout）原来指向的终端**顶掉**、换成指向我们开的文件，从此 `printf` 写 fd 1 就是写文件。完事再用 `dup2(saved, 1)` 把一开始用 `dup(1)` 备份的原 stdout 还原回去。注意我中间写了一句 `fflush(stdout)`——这其实是给下一节埋的伏笔：`printf` 是 stdio、它在用户态有缓冲，不刷的话数据可能还卡在缓冲里没进内核，重定向的效果就看不见。这个坑马上讲透。

顺带埋一句（不展开，留给后面进程章）：`dup` 出来的新 fd，默认**没有** `FD_CLOEXEC` 标志——意思是如果你 `fork` 出子进程再 `exec` 换成另一个程序，这个 fd 会**漏给**被 exec 的程序，造成意料之外的 fd 泄漏。要用 `fcntl` 的 `F_DUPFD_CLOEXEC` 或显式设 `FD_CLOEXEC` 来堵这个口子。这里只认下这个名词，等讲 `fork`+`exec` 时再亲手踩。

## 两套缓冲：stdio 用户态 vs 内核页缓存

这一节是本章的另一个核心，也是从 stdio 走到 syscall 以后**必须重新理一遍**的事——缓冲分**两层**，别再混为一谈。

第一层是 **stdio 自己的用户态缓冲**。第 12 章提过，`FILE*`（`fopen` 给你的那个）在用户态内存里维护一个小缓冲区（默认几 KB），你 `fprintf`/`fread` 的数据先进这儿，攒够一批、或遇到换行（行缓冲）、或缓冲满了，才**一次性**通过 `write` 系统调用送进内核。这一层的存在是为了**减少 syscall 次数**——每进一次内核都是开销，攒一批再送划算。普通磁盘文件默认是**全缓冲**（攒满才送），终端是**行缓冲**（遇 `\n` 就送），stderr 是**无缓冲**（立刻送）。

第二层是 **内核里的页缓存**（page cache）。即便你用裸 `write` 系统调用绕开 stdio，数据进了内核也**不是立刻就写到磁盘硬件**——内核先把它收进页缓存这一层，择机（一定时间后、或内存紧张、或你主动要求）才真正落盘。这一层是为了**减少磁盘 IO 次数**、提升读写性能。

理解了这两层，下面这几个常被搅在一起的名字就能分清了。**`fflush(FILE*)`** 只刷**第一层**：它把 stdio 用户态缓冲里还没送的数据，推过 `write` 这一关、送进**内核的页缓存**——但**到此为止**，它**没有**保证数据到了磁盘硬件。**`fsync(fd)`** 才刷**第二层**：它命令内核把这个 fd 对应的、还赖在页缓存里的脏页**真正写到磁盘**设备上、等设备确认落盘了才返回。两者**不可互相替代**：只 `fflush` 不 `fsync`，断电还是丢数据（数据在内核页缓存里、没到磁盘）；只 `fsync` 不 `fflush`，那 stdio 缓冲里还没 `fflush` 的数据连内核都没进、`fsync` 自然也管不着它。真正要保证数据落盘，得**先 `fflush` 再 `fsync`**，缺一不可。

要把这套缓冲「看得见」，最能说明问题的就是 `_exit` 和 stdio 缓冲的交锋。**`_exit`**（`<unistd.h>`，POSIX）是**裸 syscall 退出**——它直接让内核终结进程，**不**走 C 库的退出收尾流程，**不**会帮你 flush 任何 stdio 缓冲。而 **`exit`**（`<stdlib.h>`，ISO C §7.22.4.4）是 C 库的退出——它会先调用所有用 `atexit` 注册的清理函数、**把所有 stdio 流都 flush 一遍**，再调 `_exit`。两者的差别，恰好能把「stdio 缓冲里还没送内核的数据」暴露出来：

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int mode = (argc > 1) ? atoi(argv[1]) : 0;
    FILE* fp = fopen("/tmp/cj/p5ch1/buf.txt", "w"); /* 普通文件 = 全缓冲 */
    fprintf(fp, "还在 stdio 用户态缓冲里的数据\n");

    if (mode == 0) {
        _exit(0); /* 裸 syscall 退出:不刷 stdio,数据原地蒸发 */
    }
    fflush(fp); /* mode==1:先把 stdio 缓冲推进内核,再 _exit */
    _exit(0);
}
```

```text
$ gcc -std=c11 -Wall buffer_exit.c -obe && ./be 0    # mode 0:只 _exit,不 flush
$ wc -c < /tmp/cj/p5ch1/buf.txt
0
$ ./be 1                                              # mode 1:先 fflush 再 _exit
$ wc -c < /tmp/cj/p5ch1/buf.txt
41
```

两组对照干净利落。`mode 0` 那次，`fprintf` 把数据写进了 stdio 的用户态缓冲、但还**没送内核**，紧接着 `_exit` 裸退出、根本不 flush——结果文件 **0 字节**，那句「还在 stdio 用户态缓冲里的数据」**原地蒸发**了。`mode 1` 那次，`_exit` 之前先 `fflush(fp)`，把 stdio 缓冲推进了内核（进了页缓存），再 `_exit` 就没事了——文件 **41 字节**，数据保住了。

这个对照把三件事一次讲透：其一，**stdio 缓冲确实存在**，没 flush 的数据就飘在用户态、没进内核；其二，**`_exit` 不刷 stdio**，而 `exit` 会（你把 `_exit` 换成 `exit` 跑 mode 0，数据照样保得住，因为 `exit` 帮你 flush 了所有流）；其三，前面 `dup2` 那节我特意写的 `fflush(stdout)` 不是多余的——`printf` 是 stdio、有用户态缓冲，重定向到文件后是全缓冲、不会自动按 `\n` 送，不 `fflush` 那句很可能就卡在缓冲里看不见。系统编程里一个频繁踩的坑就是 `fork` 之后子进程用 `_exit` 退出、管道里的输出却莫名其妙少了半截——根子就在这儿，等下一章讲 `fork` 时还会再撞见它。

诚实地收一句关于 `fsync` 的话：这一节我用 `_exit` 把「stdio 缓冲 vs 内核」的边界演示得很清楚了，但「内核页缓存 vs 磁盘硬件」这层（也就是 `fsync` 的主战场）在普通教学环境里很难肉眼区分——`fsync` 真正的价值在断电、在数据库的持久性保证，这些光在我们这种 WSL2 + 普通文件系统上很难「演」出来。所以这里我只把 `fflush`/`fsync`/`_exit` 三者的边界讲准（`fflush` 刷 stdio→内核、`fsync` 刷内核→磁盘、`_exit` 啥都不刷），至于 `fsync` 的真落盘演示，等你写数据库类项目时再去亲手体会——那种「`fsync` 慢得能感觉到」的量级，比我在这里造个假演示有说服力得多。

## 小结

走到这儿，我们把 C 标准库那层棉花真正掀开了一角。文件描述符就是内核给你的一张表里的小整数下标，0/1/2 是 stdin/stdout/stderr、新文件从 3 开始按最小空闲号分配；`open` 拿 fd、`close` 还 fd、`read`/`write` 在 fd 上搬字节、`lseek` 在文件里跳位置、`dup2` 把 fd 复制成另一条路（重定向的底子）。围绕这套接口有三件必须刻进肌肉记忆的事：第一，**`errno` 只在出错返回之后才有意义**，成功路径它不清零、别拿它判成败；第二，**`read`/`write` 允许短读短写**，要保证写满 N 字节就得循环凑、外加处理 `EINTR`，别假设一次到底；第三，**缓冲分两层**，stdio 用户态一层（`fflush` 刷它）、内核页缓存一层（`fsync` 刷它），`_exit` 连 stdio 那层都不刷、数据和它一起蒸发。

更重要的是这一章定下的世界观基调：从现在开始，你打交道的是一个**「每个调用都可能只完成一半、每个错误都得自己查、每个资源都得自己记得还」**的世界——`read` 可能只给你几字节，`write` 可能只吃下去半截，`open` 可能因为你漏写 `mode` 就埋下权限地雷，`close` 漏一次就往 fd 表里漏一个格子。stdio 帮你兜的这些底，syscall 这层统统不兜，全甩给你自己。听着累，但这就是系统编程的真相，也是后面所有章节的共同底色——进程、信号、管道、socket，没一个能绕开这套「自己查错、自己凑齐、自己善后」的纪律。把这一章的 fd 和缓冲吃透，下一章我们就能放心地 `fork` 出一个新进程，去看父子进程各自的 fd 表和 stdio 缓冲又会怎么互相折腾。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago)，第 3、4 章讲文件 IO 与 fd，是这一章的内容骨架。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk)，第 4、5 章对 `open`/`read`/`write`/`lseek`/`dup` 的返回值与短读写讲得最细，`fflush` 与 `fsync` 的分层也清楚。
- **man 页**：`open(2)`、`read(2)`、`write(2)`、`close(2)`、`lseek(2)`、`dup(2)`、`errno(3)`、`feature_test_macros(7)`——本章每条 syscall 行为都对齐 man 页原文。
- **ISO C**：§7.5（`errno`，C11 起线程存储期）、§7.21（`stdio`：`fopen`/`fread`/`fwrite`/`fflush`/`fclose`）、§7.22.4.4（`exit`）。注意 `open`/`read`/`write`/`close`/`_exit`/`ssize_t`/`off_t` 都不在 ISO C 里，它们是 POSIX。
- **POSIX**：IEEE Std 1003.1-2008（即 POSIX.1-2008），`open`/`read`/`write`/`close`/`lseek`/`dup`/`dup2` 的权威定义；`_POSIX_C_SOURCE` 是其 feature test macro 体系。
