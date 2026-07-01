---
title: "进程的诞生：fork、写时复制与 stdio 缓冲陷阱"
description: "这一章我们用 fork() 真机生出第二个进程。fork 是 POSIX 里创造进程的主力手段,最反直觉的地方是它『一次调用、两次返回』——父进程拿到子的 pid、子进程拿到 0、失败才返回 -1。真跑 fork_basic 复现这个二义性返回,顺便看清 pid/ppid 的关系(子的 ppid 就是父的 pid)。然后是写时复制(COW):fork 并不真的把父进程的内存抄一份,而是让父子先共享只读页、谁要写时内核才临时复制那一页——真跑 cow.c 让子进程改全局/栈/堆三块内存,父进程眼里的值纹丝不动,证明 fork 之后父子内存各自独立(内核的优化、POSIX 只保证『独立』不保证『怎么实现』)。本章的真正主角是两个 stdio 缓冲陷阱,正好接上一章 _exit 吞缓冲的伏笔:其一,fork 会把 stdio 用户态缓冲连同里面的字节一起复制给子进程,于是 fork 前没 flush 的那一行会被父子各打印一次(真跑 BEFORE-FORK 出现两次、fflush 后一次);其二,子进程用 _exit 退出时不刷 stdio,一旦 stdout 是全缓冲(重定向到文件/管道),子进程的输出会凭空消失(真跑重定向:子 _exit 文件 13 字节、子 exit 文件 41 字节)。顺带一个真跑推翻计划的小发现:『漏 include <sys/types.h> 直接编译失败』并不准——有 _POSIX_C_SOURCE 时 <unistd.h> 单独就够,真正栽的是忘了宏(pid_t 被门控、报 unknown type name)。全 gcc16+clang22+ASan 真跑。"
chapter: 5
order: 2
tags:
  - host
  - system-programming
  - posix
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 1 章：文件 IO 与 fd（_exit 与 exit 的差别、fflush 刷哪一层、两套缓冲）"
  - "阶段 2·第 12 章：内存布局与生命周期（.data / 栈 / 堆 的落位）"
  - "第 9 章：作用域、存储期与 static（全局与局部变量的存储期）"
related:
  - "第 3 章：exec 家族与 wait（fork+exec 标准模式、wait/waitpid 收尸、僵尸进程的完整处理）"
  - "阶段 0·第 10 章：Sanitizer 门禁（ASan/UBSan，本章用它复核父子内存）"
---

> 🟡 状态:待审核(2026-07-02)

# 进程的诞生：fork、写时复制与 stdio 缓冲陷阱

## 引言：从一个进程到两个

上一章我们手里的程序还是「一个进程从头跑到尾」——`main` 开始、一路调函数、`return` 结束,自始至终只有内核给的那一个进程在干活。可真实的系统程序几乎没几个是单干的:一个 shell 要一边等用户输入、一边跑子命令;一个服务器要一边接新连接、一边处理老连接。靠「一个进程串完所有事」既别扭又容易阻塞,所以操作系统给了我们一条根本出路——**再造一个进程出来**,让它去干另一摊活。这一章讲的就是 POSIX 创造新进程的主力手段:**`fork`**。

`fork` 的思路简单到近乎粗暴:**把当前进程整个复制一份**。原来一个进程,调完 `fork` 之后变成两个——一个叫**父进程**(原来的那个)、一个叫**子进程**(新复制出来的)。两个进程各自有独立的 pid、独立的内存、独立的执行流,从 `fork` 那一刻起各跑各的。听起来像「复制粘贴一份代码再跑」,但它是内核层面真的复制进程,代价远比你想的小(下面讲写时复制时会解释为什么)。这一章我们先搞清 `fork` 那个古怪的「两次返回」,再用真机实验看清父子内存到底怎么个「独立」法,最后撞上两个跟 stdio 缓冲有关的经典陷阱——它们正好接上一章末尾 `_exit` 吞 stdio 缓冲那条伏笔。

## fork()：一次调用，两次返回

`fork` 的原型朴素得不像话(`<unistd.h>`):

```c
pid_t fork(void);
```

没有参数,返回一个 `pid_t`(进程 id 类型,POSIX,`<sys/types.h>`)。真正反直觉的是它的返回值——**一次 `fork` 调用,会返回两次**:一次回到父进程、一次回到子进程,而且两边拿到的值还不一样。规则是:**父进程拿到子进程的 pid(一个大于 0 的整数)**,**子进程拿到 0**;如果 `fork` 失败(比如系统资源不够、或进程数撞了上限),它返回 `-1`、且只返回这一次(压根没创建出子进程)。这是 C 里极其少见的「一个函数返回两个不同值」的特例,理解它的钥匙就一句话:**父子的执行流在 `fork` 之后分岔了,各自拿到自己的那一份返回值继续往下跑**。

所以 `fork` 之后的标准写法,是立刻用 `if` 把父子的路径分开:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        /* 子进程:fork 在它这里返回 0 */
        printf("[子]  pid=%d, ppid=%d, fork 返回=%d\n", (int) getpid(), (int) getppid(), (int) pid);
    } else {
        /* 父进程:fork 在这里返回子的 pid */
        printf("[父]  pid=%d, ppid=%d, fork 返回=%d(=子的 pid)\n", (int) getpid(), (int) getppid(),
               (int) pid);
        wait(NULL); /* 等子跑完,免得它变僵尸;wait 下一章细讲 */
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fork_basic.c -o fb && ./fb
[子]  pid=36850, ppid=36849, fork 返回=0
[父]  pid=36849, ppid=36834, fork 返回=36850(=子的 pid)
```

先把丑话说前头:**那几个 pid 数字,你每跑一次都不一样**(内核按顺序发号,ASLR 之类的也会让进程的栈、堆地址每次不同,这点后面阶段 2 的内存布局已经见过)。所以我盯的不该是具体数字、而是几个**结构上的不变量**,这些才是每次跑都成立的:其一,子进程那一行 `fork 返回=0`,父进程那一行 `fork 返回=` 一个正数、而且那个正数**正好等于子进程的 pid**(`36850`);其二,子进程的 `ppid`(父进程 pid,`getppid()` 拿)是 `36849`,**正好等于父进程的 `pid`**——这就是「父子关系」在 pid 上的体现,子进程总能通过 `getppid()` 找到生它的父。

代码里那个 `wait(NULL)` 先简单交代一句(完整语义是下一章 `exec` 与 `wait` 的事):它让父进程**停下来等子进程跑完再继续**。这里用它有两个目的——一是避免子进程变「僵尸」(下一节末尾讲);二是让父子输出的顺序稳定下来(子先打、父后打),省得你看着乱。你把 `wait(NULL)` 去掉多跑几次就会看到,父子两行的顺序**偶尔会反过来**——这是因为 `fork` 之后**谁先被内核调度上 CPU 跑是不指定的**,想依赖顺序必须用管道或信号显式同步(别靠 `sleep` 赌,那是给自己埋定时炸弹)。

顺带一个和上一章同源的真跑发现。计划里写着「漏 include `<sys/types.h>` 直接编译失败」,可我实测——**有 `_POSIX_C_SOURCE` 时,光一个 `<unistd.h>` 就够、`pid_t` 跟着漏进来**(和上一章 `open` 漏网同款,glibc 2.43 对这些核心符号放得松)。真正会栽的是**忘了 `_POSIX_C_SOURCE` 宏**:这时 `pid_t` 被门控,编译器压根不认识它:

```text
$ gcc -std=c11 -Wall pid_probe.c    # 没 _POSIX_C_SOURCE、只 include <unistd.h>
pid_probe.c: In function 'main':
pid_probe.c:3:5: error: unknown type name 'pid_t'
    3 |     pid_t p = fork();
      |     ^~~~~
```

所以规矩跟上一章一模一样:**每个文件顶上老老实实写 `#define _POSIX_C_SOURCE 200809L`**;至于 `<sys/types.h>`,portable 写法是永远 include 它(别去赌哪个 libc 让 `<unistd.h>` 把 `pid_t` 带进来)——我本章所有程序两个头都 include,稳字当头。

## 写时复制：fork 不真的复制内存

`fork` 说「把进程复制一份」,你可能马上会担心:要是一个进程占了几个 G 的内存(比如跑了个大数据库),`fork` 一次就整出一份几个 G 的副本,那也太费了。内核当然没这么傻——它用的是**写时复制**(Copy-On-Write,简称 **COW**):`fork` 的那一刻,内核并不真把父进程的内存页抄一份给子进程,而是让**父子共享同一份物理内存页,但都标记成只读**;等到其中某一方要**写**某个页时,内核这才临时把那个页**复制一份**、让写者用副本、读者继续共享原来的。换句话说,「复制」被**推迟到了真正写入的那一刻**,而且**只复制真正被动到的页**——`fork` 因此极其便宜,哪怕父进程占了几 G 内存,只要子进程不怎么改它,`fork` 几乎是瞬时的。

COW 是内核的优化手段,**C 标准不管这事**(C 连进程的概念都没有),**POSIX 也只承诺「fork 之后父子拥有各自独立的内存」、并不规定内核怎么实现这个独立**。所以从 C 程序的视角,我们该记的就一句:**fork 之后,父子对各自内存的修改,互不可见**。这一点用三种存储(全局、栈、堆)一起真跑最清楚——它们正好对应阶段 2 第 12 章那张内存地图里的 `.data`、栈、堆:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int global = 10; /* 全局,已初始化 → 落 .data */

int main(void) {
    int local = 20; /* 局部 → 栈 */
    int* heap = malloc(sizeof(int));
    *heap = 30; /* 堆 */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 子进程:把三块内存都改掉 */
        global = 111;
        local = 222;
        *heap = 333;
        printf("[子]  改完:  global=%d  local=%d  *heap=%d\n", global, local, *heap);
    } else {
        wait(NULL);
        /* 父进程:看看自己眼里的值——子改的是子的副本,这边纹丝不动 */
        printf("[父]  子改后:global=%d  local=%d  *heap=%d\n", global, local, *heap);
    }

    free(heap);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall cow.c -o cow && ./cow
[子]  改完:  global=111  local=222  *heap=333
[父]  子改后:global=10  local=20  *heap=30
```

子进程把 `global`、`local`、`*heap` 三块全改成了 `111/222/333`,可父进程眼里它们**还是原来的 `10/20/30`**,纹丝没动。这就是 COW 的可观测后果:子进程「写」的那一刻,内核给子进程分配了**它自己的副本**,父进程手里的原件不受影响——无论这块内存在 `.data`(全局)、栈(局部)还是堆(`malloc` 来的),都一样独立。fork 之后**没有一块内存是父子共享、能互相改的**;想跨进程共享一段内存,得用专门的 IPC(`mmap`/`shm_open`,阶段后面的 IPC 章会讲),普通 `fork` 给不了你。

## stdio 缓冲陷阱（上）：同一行被打两遍

内存独立的事讲完了,现在撞上本章真正的两个主角——它们都跟上一章末尾埋的「stdio 用户态缓冲」直接相关。先回忆一条上一章的结论:`FILE*`(stdio)在**用户态内存里**维护着一个小缓冲区,你 `printf` 的数据先攒在这儿、攒够或遇 `\n` 才送进内核。问题来了——`fork` 复制进程时,会把**用户态内存整个复制**,这自然就**包括 stdio 那块缓冲区连同里面还没送出去的字节**。后果是:如果 `fork` 之前 stdio 缓冲里还卡着没 flush 的数据,`fork` 之后**父子各持有一份副本**,等到它们各自退出(或主动 flush)时,这份数据就会被**打印两遍**。

听上去抽象,复现它只要一行不带 `\n` 的 `printf`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int do_flush = (argc > 1) ? atoi(argv[1]) : 0;

    printf("BEFORE-FORK "); /* 无 \n:留在 stdio 用户态缓冲里 */
    if (do_flush) {
        fflush(stdout); /* fork 前把缓冲倒干净 */
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        printf("[子]\n");
    } else {
        wait(NULL);
        printf("[父]\n");
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fork_buf_double.c -o fbd && ./fbd 0    # mode 0:fork 前 不 flush
BEFORE-FORK [子]
BEFORE-FORK [父]
$ ./fbd 1                                                    # mode 1:fork 前 fflush
BEFORE-FORK [子]
[父]
```

`mode 0` 那次,`printf("BEFORE-FORK ")` 没有 `\n`,这串字符**卡在 stdio 用户态缓冲里没送出去**;接着 `fork`,父子**各继承了一份装着 `BEFORE-FORK` 的缓冲区**;子进程后面 `printf("[子]\n")` 把 `\n` 一带、缓冲被刷出去时连同前面的 `BEFORE-FORK` 一起送进内核,父进程同理——结果 `BEFORE-FORK` 在屏幕上**出现了两次**。`mode 1` 那次,我在 `fork` 之前加了 `fflush(stdout)`,把那块缓冲**提前倒干净**,fork 时子进程继承的就是**空缓冲**了,`BEFORE-FORK` 只出现**一次**。

这就是 fork 编程里最常踩的第一个 stdio 坑,纪律也只有一条:**`fork` 之前,凡是往任何 stdio 流写过东西的,统统先 flush 一遍**。最省事的是 `fflush(NULL)`——它会把你打开的**所有**输出流都刷一遍(不光 stdout)。这个坑在输出直连终端时容易被 `\n` 的行缓冲顺带遮掉(终端是行缓冲,遇 `\n` 就送),可一旦 stdout 被重定向到文件或管道(全缓冲,没有 `\n` 就不送),它就一定咬你——而服务器、守护进程的 stdout 几乎总是被重定向的,所以这是写 daemon 时的高频事故。

## stdio 缓冲陷阱（下）：子进程的 _exit 会吞掉输出

第二个坑正是上一章 `_exit` 那条的 fork 版。上一章我们真跑过:`_exit`(POSIX)是**裸 syscall 退出**,它**不刷 stdio 缓冲**;而 `exit`(ISO C §7.22.4.4)是 C 库退出,会走 `atexit` 注册的清理、**把所有 stdio 流都 flush 一遍**再调 `_exit`。放到 fork 场景里,这件事会被放大成「子进程的输出整个凭空消失」——因为子进程常常用 `_exit` 退出(为什么用 `_exit` 下一章讲 `exec` 时会看到:fork+exec 的标准模式里子进程 exec 失败后必须 `_exit`,免得跑回父进程的后半段代码),一旦它用 `_exit`、且它的 stdout 又是全缓冲(管道或文件),那它在缓冲里攒着的输出就**永远不会被送进内核**,程序一结束就蒸发了。真跑给你看,用「重定向到文件」来强制全缓冲:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int use_exit = (argc > 1) ? atoi(argv[1]) : 0;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        /* 子进程:打印两行 */
        printf("child: line A\n");
        printf("child: line B\n");
        if (use_exit) {
            exit(0); /* mode 1:C 库 exit → 刷 stdio */
        }
        _exit(0); /* mode 0:裸 syscall → 不刷 stdio */
    }
    wait(NULL);
    printf("parent: done\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall child_stdio_exit.c -o cse
$ ./cse 0 > out.txt && cat out.txt    # mode 0:子 _exit,重定向(全缓冲)
parent: done
$ wc -c < out.txt
13
$ ./cse 1 > out.txt && cat out.txt    # mode 1:子 exit,重定向(全缓冲)
child: line A
child: line B
parent: done
$ wc -c < out.txt
41
```

两组对照把问题钉死了。`mode 0` 那次,子进程用 `_exit(0)` 退出,它的两行 `child:` 还卡在 stdio 用户态缓冲里**没送内核**就被连带蒸发了——`out.txt` 里**只剩父进程的 `parent: done`**(13 字节),子的两行**全没**。`mode 1` 把子进程的退出换成 `exit(0)`,`exit` 帮它把 stdio 缓冲 flush 进内核,子的两行这才落进文件(41 字节,父子的输出都在)。这里千万别手滑把这个现象归结成「`printf` 没成功」——`printf` 完全成功,数据进了 stdio 缓冲,是 `_exit` 没刷缓冲把它扣下了。这个坑在子进程 stdout 直连终端时同样会被行缓冲遮掉(每行 `\n` 当场送),**只在 stdout 全缓冲时发作**,可真实服务器的日志几乎都是重定向到文件、或经管道交给日志收集器的——全是全缓冲,所以它一咬一个准。

两条 stdio 坑合起来,给 fork 编程定下一条朴素纪律:**fork 前把 stdio 缓冲清干净(`fflush(NULL)`),fork 后子进程要用 `exit` 退出、或者 `fflush` 之后再 `_exit`**——别让 stdio 缓冲跟 fork 的内存复制撞在一起出幺蛾子。

## 僵尸进程：子死了父没收

本章的程序里你大概注意到了,父进程一律会调 `wait(NULL)` 等子进程跑完——这不只是为了稳定输出顺序,更有一个硬原因:**子进程先死、父进程却没 `wait` 它,子进程就会变成「僵尸」**(zombie,Linux 进程状态里标成 `Z`,ps 里显示 `<defunct>`)。原因是内核**不能自作主张把一个刚死的子进程彻底抹掉**——它的退出状态(正常退出码、还是被信号打死)还得留着,等父进程来「收尸」(reap)时读取;父进程不来收,这个已死、却还占着 pid 和一点点内核记账信息的子进程就卡在「僵尸」状态赖着不走。要是父进程长期 fork 出子、又不 `wait`,僵尸就会越堆越多、最终耗尽进程表。

完整的「怎么收尸」——`wait`/`waitpid`、`WIFEXITED`/`WEXITSTATUS`/`WIFSIGNALED`/`WTERMSIG` 这套退出状态解析——是**下一章 `exec` 与 `wait`** 的主题,这里只先认下「僵尸」这个词和它的成因,知道 `wait` 是收尸的动作就行。本章所有例子都让父进程 `wait(NULL)`,就是图个干净、不留僵尸;真到了下一章,我们会亲手造一个僵尸再把它收掉。

## vfork：别碰它

最后用一句话打发一个历史包袱:`vfork`。它长得跟 `fork` 一模一样,语义却是个**地雷**——POSIX 给它的规定是:`vfork` 出来的子进程**不能 `return`、不能修改父进程的任何变量**,而且**必须用 `_exit` 或 `exec` 退出**,否则就是**未定义行为**。它存在的唯一理由是远古时代 `fork` 还没 COW 时、用来省那一份内存复制的极端优化;如今 `fork` 有了 COW 已经几乎和 `vfork` 一样便宜,`vfork` 那点性能优势早就不值得拿 UB 去换。所以本阶段**只用 `fork`、不碰 `vfork`**——你在老代码里看到 `vfork` 知道有这么个东西就行,新代码千万别写。

## 小结

`fork` 是 POSIX 造进程的主力:一次调用、两次返回,父进程拿子的 pid、子进程拿 0、失败才 -1;fork 之后父子立刻分岔、各跑各的,`getpid`/`getppid` 能让它们彼此相认(子的 ppid 就是父的 pid)。fork 的内存复制有**写时复制**兜底——父子先共享只读页、谁写才临时复制那一页,所以从程序视角看就是「fork 之后父子内存各自独立、互不可见」,全局、栈、堆无一例外。本章真正的两个坑都出在 stdio 用户态缓冲上:其一,fork 会把 stdio 缓冲连同里面的字节复制给子进程,fork 前没 `fflush` 的那行会被父子各打一遍;其二,子进程用 `_exit` 退出时不刷 stdio,在 stdout 全缓冲(文件/管道)时子的输出会凭空消失——这两条合起来要求我们「fork 前 `fflush`、子进程用 `exit` 或先 flush 再 `_exit`」。再记两条边角:fork 后谁先跑不定、别用 `sleep` 赌顺序;子进程死了父不 `wait` 会变僵尸(收尸的完整手艺下一章讲);`vfork` 是 UB 地雷、只用 `fork`。

到这里我们有了「两个进程」,但它们跑的是**同一份代码**——子进程不过是把父进程的程序又跑了一遍。真正让多进程有用的下一步,是让子进程**换一个程序去跑**(比如 shell fork 出来、再 `exec` 成 `ls`),以及父进程**正确回收**那个跑完的子进程(不然就是僵尸)。这就是下一章 `exec` 家族和 `wait` 要解决的事,本章埋的僵尸伏笔、子进程用 `_exit` 退出的伏笔,到那儿一并兑现。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 8 章「进程控制」讲 fork / vfork / wait,fork 的两次返回与 COW 写得最透。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 24 章「创建进程」逐条拆 fork 的返回值、内存语义与 Linux 上的 COW 实现细节。
- **man 页**：`fork(2)`、`vfork(2)`、`getpid(2)`、`getppid(2)`、`wait(2)`、`_exit(2)`、`exit(3)`——本章每条行为都对齐 man 页原文;`credentials(7)` 讲 pid/ppid 关系。
- **ISO C**：§7.22.4.4（`exit`，会 flush 所有 stdio 流）；`_exit`/`fork`/`pid_t`/`wait` 都不在 ISO C 里,它们是 POSIX。
- **POSIX**：IEEE Std 1003.1-2008,`fork` 定义里写明「the child process shall be an exact duplicate of the parent process」(独立内存的承诺),Linux 用 COW 实现这个承诺属于内核优化、不在标准文本里。
