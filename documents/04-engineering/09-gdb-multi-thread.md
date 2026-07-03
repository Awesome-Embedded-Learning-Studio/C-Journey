---
title: "gdb 实战:多线程栈、catch signal 与 -O2 的变量失踪"
description: "阶段 0 第 13/14 章那套「断点 + 单步 + watchpoint + core dump」是地基,但有三类场景它没碰:多线程程序崩了,崩在哪个线程你根本不知道;信号 SIGSEGV 把进程打死之后才看到现场,但「信号刚到那一刻」的更早现场你想抓却抓不到;还有 -O2 优化一开,GDB 满屏 <optimized out>、连个变量都 print 不出来。这一章把这三件阶段 0 没碰的深度真跑一遍。先写一个两线程程序,worker_bad 线程释放了共享缓冲再往里写,触发 SIGSEGV——gdb run 进去崩了,用 thread apply all bt 一眼看到三个线程各自的栈(出事线程停在 worker_bad,worker_good 在 usleep,main 在 pthread_join 阻塞),再用 thread N / info threads 切到出事线程读它的局部变量(round=3、shared_buf=0x0)。然后 catch signal SIGSEGV 把「信号到达」变成显式 catchpoint,和默认的「事后 bt」对照,看它能在更早的瞬间停下。最后讲 -O2 下变量失踪的根因——编译器把函数内联进调用方、把可常量折叠的局部变量在编译期算掉、不留栈槽,GDB 自然读不到;真跑 optgone.c 对照 -O0 能 print 出 temp=107/data={1,2,3,4}、-O2 同一段却 data=<optimized out>、weighted_sum 整个函数符号都没了(完全被内联),修法是局部加 volatile 强制留内存位置、或干脆回到 -O0 调试版。全 gcc16/clang22 双跑 + ASan 互补,gdb 17.2 真跑。"
chapter: 4
order: 9
tags:
  - host
  - gdb
  - debug
  - thread
  - concurrency
  - volatile
difficulty: advanced
reading_time_minutes: 17
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 13 章:GDB 基础(run/break/next/step/print/bt 那套地基)"
  - "阶段 0·第 14 章:GDB 进阶(条件断点、watchpoint、core dump、generate-core-file)"
  - "阶段 0·第 9 章:标准与优化(-g/-O0/-O2/-DNDEBUG,本章变量失踪的根因在它)"
  - "阶段 4·第 1 章:头文件契约(本章是工程化调试深度,接它定下的工程化基调)"
related:
  - "阶段 0·第 10 章:Sanitizer 门禁(ASan 报 UAF 给出的栈,和 gdb 是「自动报 vs 交互查」两条互补路)"
  - "阶段 4·第 1 章:Sanitizer asan/ubsan(本章多线程 UAF 用 ASan 复核根因)"
---

# gdb 实战:多线程栈、catch signal 与 -O2 的变量失踪

## 引言:单线程那套 gdb,撞上多线程和 -O2 就不够用了

阶段 0 第 13、14 章我们把 gdb 那套「断点 + 单步 + watchpoint + core dump」真跑了一遍——单线程程序崩了,`gdb run` 进去 `bt` 看栈、`print` 读变量,根因立马浮现。可一旦程序长大,两堵墙会同时砸上来。第一堵,**多线程**:你的程序跑着好几个线程,其中一个段错误了,直接跑你只看到一句 `Segmentation fault` 和 139 退出码,**根本不知道是哪个线程炸的**——更别说其他线程当时正在干嘛、是不是它们间接导致了崩溃。第二堵,**优化**:`-O2` 一开,你满怀信心 `gdb run` 进去 `print temp`,屏幕冷冷地回你一句 `<optimized out>`——变量「失踪」了,而且这次不是你写错了,是编译器把变量优化没了。

这一章就专治这两堵墙,顺手补一件阶段 0 没碰的「更早抓现场」的兵器。前置阅读是阶段 0 第 13、14 章(那套断点/单步/watchpoint/core dump 的地基)和第 9 章(`-g`/`-O0`/`-O2` 的含义)——本章默认你已经会,只讲它们没碰的深度。三件事分别对应:**多线程调试**(`thread apply all bt` 看全线程栈、`thread N`/`info threads` 切到出事线程)、**catch signal**(把信号到达变成显式 catchpoint,比事后 `bt` 抓得更早)、**-O2 变量失踪**(为什么 `print` 显示 `<optimized out>`、怎么用 `volatile` 或 `-O0` 调试版兜底)。gdb 17.2 全程真跑。

## 靶子:两线程,一个释放了缓冲还往里写

先写一个真能崩的多线程程序当靶子。两个线程共用一个堆上分配的 `shared_buf`:老实线程 `worker_good` 不停读它求和;事故线程 `worker_bad` 跑几个回合后把 `shared_buf` 给 `free` 掉、再把指针置 `NULL`,然后**继续往 `NULL` 指针里写**——典型的 use-after-free 演变成的 NULL 解引用,必然 SEGV,稳定复现:

```c
/* mt_crash.c */
/* _DEFAULT_SOURCE:让 -std=c11 严格模式下 usleep 等 POSIX/BSD 接口可见
 * (glibc 头里 usleep 在 _DEFAULT_SOURCE 下声明)。 */
#define _DEFAULT_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* 会被两个线程反复读的全局缓冲。事故线程会 free 掉它、置 NULL 再写 -> SEGV。 */
static int* shared_buf = NULL;
static const int BUF_LEN = 8;

/* worker_good:老老实实读 shared_buf,跑得越久越好(好让事故线程先动手) */
static void* worker_good(void* arg) {
    (void) arg;
    for (int round = 0; round < 2000; ++round) {
        long sum = 0;
        for (int i = 0; i < BUF_LEN; ++i) {
            sum += shared_buf[i];
        }
        printf("[good] round=%d sum=%ld\n", round, sum);
        fflush(stdout);
        usleep(1000);
    }
    return NULL;
}

/* worker_bad:跑几个 round 之后,free 掉 shared_buf、置 NULL,然后继续往里写。
 * 写 NULL 指针 -> 必然 SEGV,稳定复现(直接 UAF 写已释放页未必每次都炸)。 */
static void* worker_bad(void* arg) {
    (void) arg;
    for (int round = 0; round < 10; ++round) {
        printf("[bad ] round=%d about to misbehave\n", round);
        fflush(stdout);
        usleep(5000);
        if (round == 3) {
            /* 事故现场:释放后置 NULL,再写就是 NULL 解引用 -> SEGV */
            free(shared_buf);
            shared_buf = NULL;
            for (int i = 0; i < BUF_LEN; ++i) {
                shared_buf[i] = i * round;
            }
        }
    }
    return NULL;
}

int main(void) {
    shared_buf = malloc(sizeof(int) * BUF_LEN);
    if (!shared_buf) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < BUF_LEN; ++i) {
        shared_buf[i] = i + 1;
    }

    pthread_t t_good, t_bad;
    pthread_create(&t_good, NULL, worker_good, NULL);
    pthread_create(&t_bad, NULL, worker_bad, NULL);

    pthread_join(t_good, NULL);
    pthread_join(t_bad, NULL);

    free(shared_buf);
    return 0;
}
```

这里有两个工程细节得交代清楚,免得你后面踩。第一,`usleep` 需要给 `_DEFAULT_SOURCE`(或等价的 `_BSD_SOURCE`/`_XOPEN_SOURCE`)——严格 `-std=c11` 模式下 glibc 头里这个函数是藏起来的,不加这个宏,gcc 直接报 `implicit declaration of function 'usleep'`;这正呼应阶段 0 第 9 章那条「严格标准模式会收窄可见接口」。第二,我故意没让事故线程直接写「free 但还悬空」的指针,而是 free 完**立刻置 NULL 再写**——纯悬空指针写已释放内存,能不能炸取决于那块页有没有被回收/重用,时灵时不灵;置 NULL 后写就是铁定 SEGV 的 NULL 解引用,演示稳定。真实代码里的 use-after-free 不会这么乖,但调试技法是同一套。

照例 `-g -O0 -pthread` 编出来,先直接跑一次看现象:

```text
$ gcc -std=c11 -Wall -Wextra -g -O0 -pthread mt_crash.c -o mt_crash
$ ./mt_crash; echo "exit=$?"
...
[bad ] round=3 about to misbehave
[good] round=14 sum=36
...
Segmentation fault
exit=139
```

直接跑,你的全部线索就是末尾一句 `Segmentation fault` 加 139 退出码(128 + SIGSEGV 的 11)。崩在哪个线程?当时别的线程在干嘛?`shared_buf` 当时是什么值?一概不知。这正是单线程那套「崩完看 bt」不够用的地方——这里有两个 worker 线程,你都不知道该怪谁。

## 多线程栈:thread apply all bt 一眼看穿全线程

把程序交给 gdb,`run` 进去让它崩:

```text
$ gdb -q ./mt_crash
(gdb) run
...
[New Thread 0x7ffff7bff6c0 (LWP 157649)]
[New Thread 0x7ffff73fe6c0 (LWP 157650)]

Thread 3 "mt_crash" received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x7ffff73fe6c0 (LWP 157650)]
worker_bad (arg=0x0) at mt_crash.c:43
47	                shared_buf[i] = i * round;
```

( gdb 启动那几行 debuginfod 提示我略掉了——它问要不要联网下载调试符号,选 `n` 跳过,不影响。)关键信息全在这句:`Thread 3 "mt_crash" received signal SIGSEGV`,而且 gdb 已经自动 `[Switching to Thread ...]` 把当前线程切到了 Thread 3,告诉你它崩在 `worker_bad` 的第 43 行——也就是 `shared_buf[i] = i * round;` 那一行。比起直接跑时那句干巴巴的 `Segmentation fault`,现在你明确知道「是 worker_bad 这个线程、第 43 行」。

但光看出事线程还不够——多线程 bug 的狡猾之处在于,**真正的根因常常在另一个线程**:这里 worker_bad 是直接凶手(它 free 了缓冲又写 NULL),可 worker_good 也在并发地读同一个 `shared_buf`,它读到 `NULL` 之后照样会段错误。事实上重跑几次,崩在 worker_good 第 21 行(`sum += shared_buf[i]`)的概率不低——这就是时序竞争。所以多线程调试的第一条铁律是:**别只看出事线程,要看全部线程**。命令就是 `thread apply all bt`:

```text
(gdb) thread apply all bt

Thread 3 (Thread 0x7ffff73fe6c0 (LWP 157650) "mt_crash"):
#0  worker_bad (arg=0x0) at mt_crash.c:43
#1  0x00007ffff7c981b9 in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7d1d21c in ?? () from /usr/lib/libc.so.6

Thread 2 (Thread 0x7ffff7bff6c0 (LWP 157649) "mt_crash"):
#0  0x00005555555551f3 in worker_good (arg=0x0) at mt_crash.c:21
#1  0x00007ffff7c981b9 in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7d1d21c in ?? () from /usr/lib/libc.so.6

Thread 1 (Thread 0x7ffff7f98740 (LWP 157646) "mt_crash"):
#0  0x00007ffff7ca0a52 in ?? () from /usr/lib/libc.so.6
#1  0x00007ffff7c94abc in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7c94e07 in ?? () from /usr/lib/libc.so.6
#3  0x00007ffff7c9a00d in ?? () from /usr/lib/libc.so.6
#4  0x00005555555553da in main () at mt_crash.c:64
```

这一条命令把所有线程的调用栈一次性全打出来,你看下去立刻就有了全貌。**Thread 3**(worker_bad):栈顶是 `worker_bad` 第 43 行,就是它直接触发了 SEGV——这是案发现场。**Thread 2**(worker_good):栈顶是 `worker_good` 第 21 行,它正卡在 `sum += shared_buf[i]` 的内层循环里——也就是说崩溃那一刻,这个老实线程正在老老实实读那个马上要被 free 的缓冲。**Thread 1**(main):栈顶在 libc 里(`??` 是没调试符号的库函数),最底下能看见 `main` 第 64 行——它正卡在 `pthread_join` 阻塞,等其他线程跑完。三个线程各居其位,谁在干活、谁在睡觉、谁是凶手,一眼穿透。`thread apply all bt` 这一句就是多线程调试的看家命令,崩了之后第一件事该敲的就是它。

知道了是 Thread 3 出的事,接下来想读它的局部变量(`round`、`i`、`shared_buf`)。`info threads` 先把所有线程列一遍,看看当前在哪个、出事的那个编号是多少:

```text
(gdb) info threads
  Id   Target Id                                     Frame
  1    Thread 0x7ffff7f98740 (LWP 157646) "mt_crash" 0x00007ffff7ca0a52 in ?? () from /usr/lib/libc.so.6
  2    Thread 0x7ffff7bff6c0 (LWP 157649) "mt_crash" 0x00005555555551f3 in worker_good (arg=0x0) at mt_crash.c:21
* 3    Thread 0x7ffff73fe6c0 (LWP 157650) "mt_crash" worker_bad (arg=0x0) at mt_crash.c:43
```

每一行一个线程:Id 是 gdb 内部编号(1/2/3),Target Id 是内核视角的 LWP(轻量级进程,Linux 上线程就是共享地址空间的 LWP),Frame 是它当前停在哪个函数哪一行。前面带 `*` 的是「当前线程」——也就是 gdb 接下来的 `bt`/`print`/`info locals` 默认作用的那一个。这里 `*` 标在 Thread 3 上(因为 SEGV 时 gdb 自动切过去了),所以直接读它的现场就行:

```text
(gdb) bt
#0  worker_bad (arg=0x0) at mt_crash.c:43
#1  0x00007ffff7c981b9 in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7d1d21c in ?? () from /usr/lib/libc.so.6
(gdb) info locals
i = 0
round = 3
(gdb) print shared_buf
$1 = (int *) 0x0
```

`info locals` 给出 `i = 0`、`round = 3`——崩溃发生在事故线程跑到 `round == 3` 那一回合(正好是代码里 `if (round == 3)` 那个事故分支)、第一次写 `shared_buf[0]` 的时候。`print shared_buf` 是 `(int *) 0x0`——指针已经被 `free` 后置成了 NULL,对它解引用必然 SEGV。整个事故链条彻底清楚了:worker_bad 在第 3 回合 `free` 了共享缓冲、置了 NULL,然后第 43 行 `shared_buf[0] = 0 * 3` 这一句对 NULL 写,当场炸。

要是当前线程不在 Thread 3(比如你想去读 Thread 2 的现场,看 worker_good 当时在干嘛),用 `thread N` 显式切过去:

```text
(gdb) thread 2
[Switching to thread 2 (Thread 0x7ffff7bff6c0 (LWP 157649))]
#0  0x00005555555551f3 in worker_good (arg=0x0) at mt_crash.c:21
24	            sum += shared_buf[i];
(gdb) bt
#0  0x00005555555551f3 in worker_good (arg=0x0) at mt_crash.c:21
#1  0x00007ffff7c981b9 in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7d1d21c in ?? () from /usr/lib/libc.so.6
```

`thread 2` 一切过去,后续的 `bt`/`info locals`/`print` 全都作用在 Thread 2 上了——这就是「切到出事线程」的姿势。多线程调试的常规流程就这么三步:**`thread apply all bt` 看全貌 → `info threads` 找出事线程编号 → `thread N` 切过去读现场**。注意切线程时,该线程的寄存器、栈指针都被 gdb 一起换好了,你 `bt`/`print` 看到的就是那个线程的真实视角,不会串。

## catch signal:在信号刚到的那一刻停,比事后 bt 更早

上一节是「程序崩了、信号把进程停下来之后,我们事后看现场」——绝大多数调试就这么干,够用。但有时候你想抓得更早:在**信号刚到达、还没被默认动作(段错误就把进程打死)处理掉之前**就停下,看一眼「信号投递那一刻」更完整的现场。gdb 默认其实就是在信号投递时停的(所以你才能看到 `bt`),但你可以用 `catch signal` 把它变成一个**显式的 catchpoint**(捕获点),和断点一样有编号、能在 `info break` 里看到、能配条件——这对「我想专门在某种信号上停、别的信号放过」特别有用。

`catch signal SIGSEGV` 就是这么一句。我们在 `run` 之前先下这个 catchpoint,再跑:

```text
(gdb) catch signal SIGSEGV
Catchpoint 1 (signal SIGSEGV)
(gdb) run
...
[New Thread 0x7ffff7bff6c0 (LWP 158110)]
[New Thread 0x7ffff73fe6c0 (LWP 158111)]
[Switching to Thread 0x7ffff73fe6c0 (LWP 158111)]

Thread 3 "mt_crash" hit Catchpoint 1 (signal SIGSEGV), worker_bad (arg=0x0) at mt_crash.c:43
47	                shared_buf[i] = i * round;
```

注意这一行:`Thread 3 "mt_crash" hit Catchpoint 1 (signal SIGSEGV)`——它告诉你停下来的原因是「命中了第 1 号 catchpoint(我们刚设的 SIGSEGV 捕获点)」,而不是默认的那种 `received signal SIGSEGV`。停下来之后,后面的 `info threads`、`bt`、`info locals`、`print shared_buf` 全都和上一节一模一样(`shared_buf = 0x0`、`round = 3`),因为本质上信号投递那一刻的现场,默认停和 catchpoint 停抓到的是同一帧。

那 `catch signal` 到底比默认多了什么?有三点真正值钱。其一,它是**显式且有编号**的——`info break` 能看见它,你能 `delete 1` 把它删掉、`disable 1` 临时关掉,管理上和断点同构;而默认的「信号到达就停」是一种行为、不是一个可管理对象。其二,它能**精确选信号**——`catch signal SIGSEGV SIGFPE` 只在这两种信号上停,`SIGPIPE` 之类的放过;调试网络程序时这一手特别有用,因为 `SIGPIPE` 经常刷屏,你只想在真正的崩溃信号上停。其三,它常配合 `handle 信号 stop/nostop` 用:`handle SIGSEGV stop` 让 gdb 收到这个信号时停下来(默认就是),`handle SIGSEGV nostop` 则反过来让 gdb 收到也不停、把它直接交给程序的 signal handler——你的程序自己装了 `sigaction` 处理 SIGSEGV 时,这个区分就关键了。这三点合起来,`catch signal` 是「我想把信号投递变成一个可管理的、可选信号的断点」时的正式兵器;事后 `bt` 是默认行为,够用就别加 catchpoint,要精细控制信号时它才出场。

顺带提一句阶段 0 第 14 章讲过的 core dump 在多线程里仍然成立:程序崩了之后 `generate-core-file mt.core` 存一份全进程快照,事后 `gdb ./mt_crash mt.core` 进去照样能 `thread apply all bt`——区别只是 core 是**冻结的死现场**,不能 `continue`、不能改线程,但所有线程的栈和变量都在。多线程偶发崩溃、没法当场复现时,core 是唯一能事后穿越回现场的路子,别忘了这条路。

## -O2 的变量失踪:为什么 print 显示 <optimized out>

前面两节的前提都是 `-g -O0` 编译——阶段 0 第 9、13 章讲过,这是 GDB 调试的标准姿势。可工程里真发布的是 `-O2` 优化版,你拿一个 `-O2` 编出来的程序进 gdb,大概率会撞上这句话:

```text
(gdb) print data
$1 = <optimized out>
```

`<optimized out>` 的字面意思就是「这个变量被优化掉了,GDB 读不到它的值」。它不是 gdb 的 bug、也不是你写错了——是**编译器在 `-O2` 下把这个变量从内存里抹掉了**,自然没地方读。要理解为什么,得看 `-O2` 到底对变量做了什么。我们写一个会触发典型优化的程序:

```c
/* optgone.c */
#include <stdio.h>

/* 一个会被 -O2 内联 + 常量折叠的小函数 */
static int weighted_sum(const int* data, int n, int weight) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += data[i] * weight;
    }
    return acc;
}

int main(void) {
    int data[4] = {1, 2, 3, 4};
    int weight = 10;
    int total = weighted_sum(data, 4, weight); /* 100 */
    int temp = total + 7;                      /* 107,中间值 */
    int final_result = temp * 2;               /* 214 */

    if (final_result % 2 == 0) {
        printf("final_result=%d (even)\n", final_result);
    } else {
        printf("final_result=%d (odd)\n", final_result);
    }
    return 0;
}
```

`main` 里这一串全是编译期可算清的常量:`data` 是个 `{1,2,3,4}`、`weight` 是 10、`weighted_sum` 算出来必是 100、`temp` 必是 107、`final_result` 必是 214、`214 % 2 == 0` 必然走 even 分支。`-O2` 的编译器一眼看穿这一切,做了几件事:把 `weighted_sum` **内联**(inline)进 `main`、把整个循环**常量折叠**(constant folding)成编译期的 100、把 `data`/`weight`/`temp` 这些中间变量**消除**掉(它们只在编译期有用、运行时根本不需要在内存里留位置)。结果就是 `main` 的函数体塌缩成几条指令。我们用 objdump 把 `-O2` 编出的 `main` 反汇编出来看:

```text
$ gcc -std=c11 -Wall -Wextra -g -O2 optgone.c -o optgone_O2
$ objdump -d --no-show-raw-insn optgone_O2 | sed -n '/<main>:/,/^$/p'
0000000000001040 <main>:
    1040:	sub    $0x8,%rsp
    1044:	mov    $0xd6,%esi
    1049:	lea    0xfb4(%rip),%rdi
    1050:	xor    %eax,%eax
    1052:	call   1030 <printf@plt>
    1057:	xor    %eax,%eax
    1059:	add    $0x8,%rsp
    105d:	ret
```

`main` 总共就 7 条指令:核心是 `mov $0xd6,%esi`——`0xd6` 就是十进制的 214,编译期算好的 `final_result`,直接当成 `printf` 的参数塞进去。没有循环、没有数组、没有 `weighted_sum` 的调用,全在编译期折叠完了。这种情况下,你 `print data` 想看那个数组——可 `data` 这个变量**根本没在运行时存在过**,编译器在编译期把它算成 `1+2+3+4` 之后连内存位置都没分配,GDB 自然只能告诉你 `<optimized out>`。

更要命的是 `weighted_sum` 这个函数整个都不见了。我们看看 `-O0` 和 `-O2` 各自的符号表:

```text
$ gcc -std=c11 -Wall -Wextra -g -O0 optgone.c -o optgone_O0
$ objdump -t optgone_O0 | grep weighted_sum
0000000000001149 l F .text  000000000000004c  weighted_sum
$ objdump -t optgone_O2 | grep weighted_sum
(no output -- 函数被内联,符号没了)
```

`-O0` 编出的可执行文件里 `weighted_sum` 是个有地址、有大小(`0x4c` 字节)的真实函数;`-O2` 一编,`grep weighted_sum` 啥都没有——它被内联进 `main` 之后整个消失了。所以你在 `-O2` 版上 `break weighted_sum`,gdb 会告诉你这函数压根不存在、断点根本设不上:

```text
(gdb) break weighted_sum
Make breakpoint pending on future shared library load? (y or [n])
```

( gdb 在问要不要把这个找不到的断点挂成「待定」,等将来某个动态库加载出来再说——这里根本不会有那么一天,选 `n` 即可。)函数被内联、断点找不到落点,这是 `-O2` 下另一个比 `<optimized out>` 更彻底的「失踪」:连函数本身都没了。

对照一下 `-O0` 同一段代码在 GDB 里能读到什么。先在 `main` 第 18 行(`int final_result = temp * 2;`)下断点跑进去:

```text
(gdb) break optgone.c:18
Breakpoint 1 at 0x11ef: file optgone.c, line 18.
(gdb) run
Breakpoint 1, main () at optgone.c:18
18	    int final_result = temp * 2;               /* 214 */
(gdb) print temp
$1 = 107
(gdb) print data
$2 = {1, 2, 3, 4}
(gdb) info locals
data = {1, 2, 3, 4}
weight = 10
total = 100
temp = 107
final_result = 0
```

`-O0` 下,断点稳稳停在第 18 行(行号没漂),`print temp` 是 107、`print data` 是 `{1,2,3,4}`,`info locals` 把所有局部变量的值都列得清清楚楚——因为 `-O0` 不优化,每个局部变量都老老实实在栈上留了位置,GDB 按调试信息一找就着。

同一份代码、同一行断点,`-O2` 编出来再跑一遍,行为就完全不一样了:

```text
(gdb) break optgone.c:18
Breakpoint 1 at 0x1040: file optgone.c, line 18.
(gdb) run
Breakpoint 1, main () at optgone.c:21
21	        printf("final_result=%d (even)\n", final_result);
(gdb) print data
$1 = <optimized out>
(gdb) info locals
data = <optimized out>
weight = 10
total = 100
temp = 107
final_result = 214
```

三处变化全是 `-O2` 捣的鬼。其一,断点落在「第 18 行」,可程序**实际停在了第 21 行**——`-O2` 把指令和源码行的映射打乱了(编译器重排了指令、内联折叠之后,某一行已经没有对应的独立指令了),gdb 只能就近找一个还能停的位置,于是 18 行的断点跳到了 21 行的 `printf`。其二,`data` 变成了 `<optimized out>`——这个数组在编译期就被算成 `1+2+3+4`、运行时根本没分配内存,GDB 读不到。其三,`weight`/`total`/`temp`/`final_result` 这些标量值居然**还能 print 出来**(10/100/107/214)——这是因为 gdb 在某些情况下能从最终结果反推,或者编译器给这些变量留了位置;但 `data` 这种「完全消失」的就没辙了。哪几个变量失踪、哪几个还能看,取决于编译器的具体优化决策,**不可预测**——这正是 `-O2` 调试让人头疼的地方:你不知道哪个变量还活着。

## 兜底:volatile 强制留位置,或干脆回 -O0 调试版

变量失踪不是没治,有两个常规兜底。第一个是 `volatile`——这个关键字告诉编译器「这个变量可能被编译器看不见的方式改动(硬件寄存器、信号处理函数、别的线程),所以你**每次都真的从它的内存地址读写,不准把它缓存在寄存器、不准常量折叠它**」。结果就是 `volatile` 变量一定在内存里留了位置、GDB 一定能读。我们把刚才那段里的关键变量都标上 `volatile`:

```c
/* optvolatile.c */
#include <stdio.h>

static int weighted_sum(const int* data, int n, int weight) {
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += data[i] * weight;
    }
    return acc;
}

int main(void) {
    /* volatile 兜底:编译器不敢把这些变量折叠成常量,
     * 必须在内存里留位置、运行时真算 -> GDB 能读。 */
    volatile int data[4] = {1, 2, 3, 4};
    volatile int weight = 10;
    volatile int total = weighted_sum((const int*) data, 4, weight);
    volatile int temp = total + 7;
    volatile int final_result = temp * 2;

    int fr = final_result; /* 脱掉 volatile 给 printf 用(printf 要非 volatile) */
    if (fr % 2 == 0) {
        printf("final_result=%d (even)\n", fr);
    } else {
        printf("final_result=%d (odd)\n", fr);
    }
    return 0;
}
```

`-O2` 编出来,同样进 gdb,这一回 `data`/`weight`/`total`/`temp`/`final_result` 全都读得到:

```text
$ gcc -std=c11 -Wall -Wextra -g -O2 optvolatile.c -o optvolatile_O2
$ gdb -q ./optvolatile_O2
(gdb) break optvolatile.c:21
(gdb) run
Breakpoint 1, main () at optvolatile.c:21
21	    int fr = final_result; /* 脱掉 volatile 给 printf 用(printf 要非 volatile) */
(gdb) print data
$1 = {1, 2, 3, 4}
(gdb) print total
$2 = 100
(gdb) print temp
$3 = 107
(gdb) print final_result
$4 = 214
(gdb) info locals
data = {1, 2, 3, 4}
weight = 10
total = 100
temp = 107
final_result = 214
fr = <optimized out>
```

`volatile` 一加,那些原本失踪的变量全回来了——`data = {1,2,3,4}`、`total = 100`、`temp = 107`、`final_result = 214`,`info locals` 列得清清楚楚。对比之下,没标 `volatile` 的 `fr`(我故意只给 `final_result` 加了 volatile、`fr` 没加)还是 `<optimized out>`——这一正一反就是 `volatile` 的全部价值:它强制编译器给变量留内存位置,GDB 才读得到。

但 `volatile` 不是免费午餐,它有代价,所以**别满工程乱撒**。它屏蔽了常量折叠、寄存器分配这些优化,标了 `volatile` 的变量运行时会慢(每次都走内存而不是寄存器);而且 `volatile` 只保证「这个变量的读写不被优化掉」,它**不保证线程同步**(很多人误以为 `volatile` 能当锁用,这是错的——多线程同步要用 mutex/原子操作,`volatile` 在 C 标准里只管「不被优化」,跟可见性/有序性无关,这一点 ISO C11 §6.7.3 和 C11 §5.1.2.4 都没给它背书)。所以正确用法是:**平时别加,只在「这段代码我正在用 GDB 调,某个变量 print 不出来」时局部给它标上 `volatile`、调完再删掉**——它是个调试期的临时拐杖,不是常规生产代码的写法。

真正可靠的兜底是第二个、也是最简单的:**回到 `-O0` 调试版**。优化关掉,所有变量都老老实实在栈上,`break` 行号不漂、`print` 全读得到、`weighted_sum` 也作为一个真实函数存在(可以 `break` 进去单步),就像前面 `-O0` 那段演示的那样。工程上常见的做法是用 CMake 的 `CMAKE_BUILD_TYPE` 开两个 build 目录(阶段 0 第 12 章 / 阶段 4 第 5 章讲过):一个 Debug(`-g -O0`)专供调试、一个 Release(`-O2 -DNDEBUG`)专供发布。出 bug 时用 Debug 版复现、GDB 里看个通透,改完再用 Release 版验证优化版也没问题。`-O0` 调试版是兜底,`volatile` 是「Release 版非调不可时的临时拐杖」,两条路配合。

最后插一句和 sanitizer 的关系。前面那个 `mt_crash.c` 的 UAF 根因,如果你先用 ASan 编一版(阶段 0 第 10 章 / 阶段 4 第 1 章),它**自动就把根因报出来了**,根本不用你手动 `thread apply all bt`:

```text
$ gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address -pthread mt_crash.c -o mt_asan
$ ./mt_asan
==162363==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000 (pc ... T2)
==162363==The signal is caused by a WRITE memory access.
    #0 0x... in worker_bad /tmp/cj/p4ch9/mt_crash.c:43
    ...
SUMMARY: AddressSanitizer: SEGV /tmp/cj/p4ch9/mt_crash.c:43 in worker_bad
```

ASan 直接告诉你「地址 0x0、WRITE、`worker_bad` 第 43 行」——根因一目了然。这是「自动报错」和「交互调试」的两条互补路:ASan 适合**第一时间**把根因定位到行,gdb 适合**进一步钻进去**看其他线程、改个变量试假设、单步追逻辑。多线程 UAF 这种场景,真实工作流常常是「ASan 先报、gdb 兜底深挖」,两条一起用比单用哪条都强。

## 小结

阶段 0 第 13、14 章那套断点/单步/watchpoint/core dump 是地基,这一章补上三件它没碰的深度。多线程程序崩了,你不知道是哪个线程炸的——`thread apply all bt` 一条命令把所有线程的栈全打出来,出事线程、并发线程、阻塞在 `pthread_join` 的主线程各居其位,一眼穿透(我们真跑两线程 UAF,gdb 命中 Thread 3 的 `worker_bad` 第 43 行,`info locals` 给出 `round=3`、`print shared_buf` 是 `0x0`);想读别的线程用 `info threads` 列出编号、`thread N` 切过去,后续 `bt`/`print` 全作用在新线程上。`catch signal SIGSEGV` 把信号投递变成一个有编号、可选信号、能配条件的显式 catchpoint,比默认的「事后 bt」更精细——调试网络程序想只在意 SIGSEGV 放过 SIGPIPE 时它出场。`-O2` 一开,GDB 满屏 `<optimized out>`——根因是编译器内联 + 常量折叠把变量从内存里抹掉了(`objdump` 真跑出 `main` 塌缩成 7 条指令、`weighted_sum` 符号整个消失、`break` 设不上),断点行号还会漂(`-O2` 下第 18 行的断点跳到第 21 行的 `printf`);兜底有两条:局部加 `volatile` 强制编译器留内存位置(代价是屏蔽优化、且不保证线程同步,只当临时拐杖),或干脆回到 `-O0` 调试版用 CMake 的 Debug/Release 双 build 切换。多线程 UAF 这种 bug,gdb 和 ASan 是互补的两条路——ASan 第一时间报根因到行,gdb 兜底深挖全线程栈,真实工作流常常两条一起用。下一章我们继续在工程化深度里走,看更复杂的并发原语怎么和这套调试技法配合。

## 参考资源

- **GDB 17.2 手册**(GDB 内 `help` 随时查):`thread apply all bt`、`info threads`、`thread N`、`catch signal`、`handle`(信号处理)、`generate-core-file`、`print`(`<optimized out>` 的说明在 "Debugging Information in Separate Files" 与 "Optimized Code" 章节)。
- **ISO/IEC 9899:2011** §6.7.3(`volatile` 限定符的语义:实现不能对 volatile 对象的读写做优化假设)、§5.1.2.4(多线程执行模型与数据竞争——`volatile` 不替代同步原语的标准依据)。
- **GCC 手册**:`-O0`/`-O2` 优化等级对调试信息与变量位置的影响、`-g` 调试信息级别、`-pthread` 多线程编译链接。
- **`man pthread_create`** / **`man 7 pthreads`**:POSIX 线程模型、LWP 与线程的关系(本章 `info threads` 输出里 `LWP` 的来源)。
- **`man signal`** / **`man 7 signal`**:SIGSEGV 等信号的默认动作(段错误把进程终止),`catch signal` 的语义基础。
- **阶段 0 第 13 章:GDB 基础**——`run`/`break`/`next`/`step`/`print`/`bt`/`info locals` 的地基,本章默认你会。
- **阶段 0 第 14 章:GDB 进阶**——条件断点、watchpoint、core dump、`generate-core-file`,本章多线程段仍可走 core 这条事后路。
- **阶段 0 第 9 章:标准与优化**——`-g`/`-O0`/`-O2`/`-DNDEBUG` 的含义,本章「变量失踪」根因在它。
- **阶段 0 第 10 章 / 阶段 4 第 1 章:Sanitizer(ASan/UBSan)**——多线程 UAF 的自动根因报告,和本章 gdb 交互调试互补。
- **阶段 4 第 5 章:CMake 工程化**——`CMAKE_BUILD_TYPE` 的 Debug(`-g -O0`)/Release(`-O2 -DNDEBUG`)双 build 切换,本章 `-O2` 调试困难时的工程兜底。
