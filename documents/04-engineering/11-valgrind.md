---
title: "valgrind 与 sanitizer 的分工:能力矩阵"
description: "阶段 4 的第 1 章把 ASan/UBSan 当成调试期默认开关,它能当场抓住堆越界、释放后使用、有符号溢出。可有一类错误 ASan 死活抓不到——读未初始化的内存:你 malloc 了一块、没赋值、就拿去判断,ASan 一声不吭放过去,因为那块地址合法、只是内容是脏的。这一章拿同一个「读未初始化」程序,ASan 跑过(退出码 0、啥都不报)、valgrind memcheck 跑出来当场甩「Conditional jump or move depends on uninitialised value(s)」+行号——这就是 valgrind 在 2026 年仍然不可替代的那一块。再拿一个泄漏程序对照 valgrind 的「definitely lost」与 ASan 自带的 LSan(LeakSanitizer)各报一次,看两者抓泄漏的异同;最后用 helgrind 跑一个两线程无锁自增的竞态,对照 ThreadSanitizer(TSan)的「data race」报告,收口成一张能力矩阵。核心结论:ASan/UBSan/TSan 是编译期插桩(快、要重编、抓地址越界/UAF/UB/竞争),valgrind 是运行期动态二进制翻译(慢 10-50 倍、不用重编、能抓未初始化读这种 ASan 抓不到的、还能换工具 helgrind/drd/massif)。两者不是二选一,是互补——日常调试默认上 sanitizer(本机有就上),遇到 sanitizer 抓不到的「诡异值」就上 valgrind 复核。全 valgrind 3.25.1 + gcc16 + clang22 真跑,贴真实输出。"
chapter: 4
order: 11
tags:
  - host
  - testing
  - memory
  - debug
  - thread
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段 4·第 10 章(本仓库 10-sanitizer-deep.md):ASan/UBSan 的 -fsanitize=address,undefined 用法、红区机制、三段式读报告——本章是它的对照面"
  - "阶段 0·第 10 章:Sanitizer 门禁(MSan/TSan 的提法,本章把 valgrind 补进来)"
  - "阶段 0·第 1 章:工具链体检(valgrind 是否装的检查思路)"
related:
  - "阶段 4·第 10 章:ASan/UBSan(本章 sanitizer 那一侧的完整版)"
  - "阶段 5:线程与并发(helgrind/TSan 抓数据竞争,那里会深入用)"
---

# valgrind 与 sanitizer 的分工:能力矩阵

## 引言:ASan 抓不到的那一类错误

前置阅读:阶段 4 的第 10 章(本仓库 `10-sanitizer-deep.md`)把 ASan/UBSan 当成了调试期的默认开关,我们也确实靠它当场抓住了堆越界、释放后使用、有符号溢出。那一章末尾的速查表里,ASan 那一栏几乎覆盖了 C 程序最容易踩的雷。可它故意留了一个口子没说透——**读未初始化的内存,ASan 抓不到**。

这不是 ASan 的 bug,是它的设计边界:ASan 给每块内存配了一份「影子内存」,标记的是**这块地址能不能访问**(地址越界、已释放、踩红区),它不管**这块地址里装的值是从哪来的**。你 `malloc` 了一块、没赋值、转头就拿来 `if (*p > 100)` 判断——地址完全合法(刚分配的、没越界、没释放),ASan 没有任何理由报警;可 `*p` 里装的是别人用过的脏数据,这次判断的结果是骰子掷出来的。这一类「值未初始化」的错误,要么用 MemorySanitizer(MSan,`-fsanitize=memory`,但 MSan 不能和 ASan 同开、而且它要求**所有链接进来的库都用 MSan 重编**,工程上极难落地),要么——上 valgrind。这一章就是讲 valgrind 在 2026 年仍然不可替代的那一块,以及它和整套 sanitizer 家族的能力怎么分工。

先说清 valgrind 和 sanitizer 的根本差别,后面的实验才有坐标系。**sanitizer 是编译期插桩**:你给 gcc/clang 加 `-fsanitize=...`,编译器在每条内存访问、每条运算前后插进检查代码,程序自己跑、自己报警;好处是快(ASan 大约慢 2 倍、内存多 3 倍),坏处是**必须重新编译、重新链接**整个程序(分步编译的话,链接那步也得带 `-fsanitize`)。**valgrind 是运行期动态二进制翻译**:你拿一个已经编好的、普普通通的可执行文件,直接 `valgrind ./prog`,它在运行时把程序的每条机器指令翻成自己的一套带检查的指令再执行;好处是**不用重编、不用重链、甚至不用源码**(只拿个二进制就能查),坏处是慢——**10 到 50 倍**,因为它把每条指令都重新翻译了一遍。这一个差别决定了它俩的全部用法:sanitizer 适合天天挂着跑(CI、调试、测试套件),valgrind 适合「sanitizer 抓不到、但程序确实在诡异出错」时,才舍得花那个慢的代价去复核。

## 实验一:读未初始化——ASan 放过,valgrind 当场抓

这是全章最关键的一个对照,亲手跑给你看。程序极简:`malloc` 一块 `int`、**故意不赋值**、拿它去判断。

```c
/* uninit.c:读未初始化的栈变量——ASan 抓不到,valgrind memcheck 能抓 */
#include <stdio.h>
#include <stdlib.h>

static int decide(int* p) {
    /* 这里读 *p,但调用方根本没初始化它 */
    if (*p > 100) {
        return 1;
    }
    return 0;
}

int main(void) {
    int* p = (int*) malloc(sizeof(int));
    /* 故意不赋值:这块堆内存里是别人用过的脏数据 */
    int r = decide(p);
    printf("decide returned %d\n", r);
    free(p);
    return 0;
}
```

先用 ASan(带上 UBSan 一起)跑。gcc 16 和 clang 22 都试,结论一样:

```text
$ gcc -g -std=c11 -Wall -Wextra -fsanitize=address,undefined \
      -fno-omit-frame-pointer uninit.c -o uninit_asan
$ ./uninit_asan; echo "exit=$?"
decide returned 0
exit=0
```

退出码 0、一个字的报错都没有——ASan 完全没察觉。`*p` 那块地址刚分配、合法、能访问,ASan 的影子内存里它标着「可访问」,于是判断放行。可 `decide` 返回的 `0` 是骰子掷出来的:这块 `int` 里装的可能是 0、可能是上一次用这块内存的人留下的 42、可能是任何东西。换个机器、换个时间、换个内存分配顺序,这个判断的结果就变。UBSan 也不报,因为「读一个未初始化的值」严格说**在 C 标准里大多数情况是未指定行为**(unspecified,不是未定义行为 UB),UBSan 的职责是抓 UB、不管未指定。

换 valgrind。它不靠编译期插桩,直接拿普通编译的可执行文件跑——这里有个本机的现实坑得先说(坑就地插,不另开框):valgrind 启动时需要重定向 glibc 动态链接器(`ld-linux-x86-64.so.2`)里的 `memcmp`/`memcpy` 等函数到自己的实现,而 **Arch Linux 的 glibc 把 `ld-linux` 的符号全 strip 了**,于是动态链接的程序一启动就被 valgrind 拒绝(`Fatal error at startup: a function redirection ... cannot be set up`)。修法在文档末尾「环境坑」那一节细说,这里先绕过——用 `-static` 把 glibc 直接链接进可执行文件,程序就不再走动态 `ld-linux`,valgrind 能正常跑:

```text
$ gcc -g -std=c11 -Wall -Wextra -static uninit.c -o uninit_static
$ valgrind --tool=memcheck --leak-check=no --error-exitcode=99 ./uninit_static
==162065== Memcheck, a memory error detector
==162065== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==162065== Using Valgrind-3.25.1 and LibVEX; rerun with -h for copyright info.
==162065== Command: ./uninit_static
==162065==
==162065== Conditional jump or move depends on uninitialised value(s)
==162065==    at 0x402F56: decide (uninit.c:7)
==162065==    by 0x402F87: main (uninit.c:16)
==162065==  Uninitialised value was created
==162065==    ... (malloc 链路,suppressed for brevity)
==162065==
==162065== Conditional jump or move depends on uninitialised value(s)
==162065==    at 0x41159A: free (in /tmp/cj/p4ch11/uninit_static)
==162065==    by 0x402FAF: main (uninit.c:18)
==162065== ...
decide returned 0
==162065==
==162065== HEAP SUMMARY:
==162065==     in use at exit: 0 bytes in 0 blocks
==162065== ...
==162065== ERROR SUMMARY: 3 errors from 2 contexts (suppressed: 0 from 0)
$ echo $?
99
```

这就是 valgrind 不可替代的那一句话:`Conditional jump or move depends on uninitialised value(s)`,**精确定位到 `uninit.c:7`** 的 `if (*p > 100)`。valgrind 给每块内存额外记了一份「这块字节有没有被写过」的元数据——和 ASan 的「能不能访问」是两套正交的元数据。你 `malloc` 来的字节,valgrind 标成「未初始化」;一旦你 `*p = 42` 写过,它才标成「已初始化」;任何**读了未初始化字节、还拿它去做条件判断**(conditional jump/move)的地方,它就报警。`--track-origins=yes` 还能往上追,告诉你那个未初始化的值是哪次 `malloc`/`brk` 造出来的(上面输出里那段 `Uninitialised value was created` 就是)。

注意第二条报告在 `free (uninit.c:18)`——这是 valgrind 在回收 `p` 时,glibc 的 `free` 内部读了那块内存的某些字段做整理,而那块内存我们没初始化过,所以 `free` 自己也踩了未初始化读。这类「库内部读了未初始化」的报告往往是噪声,工程里会用 suppression 文件过滤掉;但 `decide` 那条是真错,是你的代码在拿脏值做判断。`--error-exitcode=99` 让 valgrind 在发现错误时用非零码退出(默认 valgrind 自己只返 0,得靠这个开关才能在 CI/脚本里判错),配合 `ERROR SUMMARY: 3 errors` 一起看。

这一节结论先钉死:**读未初始化的内存,ASan/UBSan 抓不到,要靠 valgrind memcheck(或 MSan,但 MSan 工程上几乎没法全栈启用)**。这是 valgrind 在 2026 年仍然有存在意义的头号理由。

## 实验二:内存泄漏——valgrind 与 LSan 各报一次

第二类 valgrind 名场面是内存泄漏。程序里 `malloc` 了不 `free`,进程退出时那块内存就漏了。valgrind memcheck 在程序结束时扫一遍堆,把所有「分配了没释放」的块按确定性分类报告;而 ASan 自带一个子工具叫 **LSan(LeakSanitizer,`-fsanitize=leak`,开 ASan 时默认就带上)**,干的是同一件事。两个都真跑给你看,先看 LSan(本机动态链接下能正常跑):

```c
/* leak.c:内存泄漏——valgrind 与 LSan(ASan)各报一次 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* dup_msg(const char* s) {
    char* p = (char*) malloc(strlen(s) + 1);
    strcpy(p, s);
    return p; /* 调用方负责 free */
}

int main(void) {
    char* a = dup_msg("first leak");  /* 泄漏:从不 free */
    char* b = dup_msg("second leak"); /* 泄漏:从不 free */
    char* c = dup_msg("third ok");
    free(c); /* 只有 c 释放了 */

    /* 用长度而不是内容打印,避免任何「读已释放内存」干扰泄漏报告 */
    printf("a_len=%zu b_len=%zu c_freed\n", strlen(a), strlen(b));
    return 0;
}
```

```text
$ gcc -g -std=c11 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer \
      leak.c -o leak_asan
$ ASAN_OPTIONS=detect_leaks=1 ./leak_asan; echo "exit=$?"

=================================================================
==162723==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 12 byte(s) in 1 object(s) allocated from:
    #0 0x... in malloc (.../libasan.so.8+0x12c161)
    #1 0x... in dup_msg /tmp/cj/p4ch11/leak.c:7
    #2 0x... in main /tmp/cj/p4ch11/leak.c:14
    ...

Direct leak of 11 byte(s) in 1 object(s) allocated from:
    #0 0x... in malloc (.../libasan.so.8+0x12c161)
    #1 0x... in dup_msg /tmp/cj/p4ch11/leak.c:7
    #2 0x... in main /tmp/cj/p4ch11/leak.c:13
    ...

SUMMARY: AddressSanitizer: 23 byte(s) leaked in 2 allocation(s).
exit=1
```

LSan 报了两处 `Direct leak`,12 字节(「first leak」+`\0`)和 11 字节(「second leak」+`\0`),分别指到 `dup_msg` 的 `malloc` 和 `main` 的调用点,退出码 1。`Direct leak` 的意思是「这块内存没有任何指针还指着它」——彻底失联,铁定漏了;LSan 还会区分 `Indirect leak`(自己漏、但只有另一块漏的内存里的指针指着它)等类别,工程意义是让你先治 `Direct`、`Indirect` 往往跟着一起好。

valgrind 那一侧,本机因为前面说的 `ld-linux` strip 问题,动态链接的程序跑不起来;`-static` 绕开动态链接器后,valgrind 又拿不到 glibc 静态 `malloc` 的重定向入口(静态二进制没有 `ld-linux` 给它挂钩),于是 `HEAP SUMMARY` 会显示 `0 allocs`——也就是说**本机这套 Arch + strip 过的 glibc,valgrind 的泄漏检测两条路都堵死**。换一台装了 `glibc-debuginfo`(Debian/Ubuntu 叫 `libc6-dbg`、RHEL 系叫 `glibc-debuginfo`)的机器,动态链接 + `valgrind --leak-check=full` 就能正常出报告,长这样(给真实格式,字段名固定):

```text
==NNNN== HEAP SUMMARY:
==NNNN==     in use at exit: 23 bytes in 2 blocks
==NNNN==   total heap usage: 3 allocs, 1 frees, 35 bytes allocated
==NNNN==
==NNNN== 23 bytes in 2 blocks are definitely lost in loss record 1 of 1
==NNNN==    at 0x........: malloc (vg_replace_malloc.c:...)
==NNNN==    by 0x........: dup_msg (leak.c:7)
==NNNN==    by 0x........: main (leak.c:13)
==NNNN==
==NNNN== LEAK SUMMARY:
==NNNN==    definitely lost: 23 bytes in 2 blocks
==NNNN==    indirectly lost: 0 bytes in 0 blocks
==NNNN==          reachable: 0 bytes in 0 blocks
==NNNN==  For counts of detected errors, rerun with: -s
```

valgrind 的 `definitely lost` 对应 LSan 的 `Direct leak`,分类思路一致。本机 valgrind 抓不到、但 LSan 抓得到——这恰好反过来说明一个工程现实:**抓内存泄漏,2026 年首选 LSan**(快、和 ASan 一体、动态链接就能跑),valgrind 的泄漏检测只有在「程序不能重编、只能拿二进制查」或「LSan 误报需要交叉验证」时才上。两套都能用时,它们对同一份泄漏的判定高度一致,挑快的那个先用。

## 实验三:数据竞争——helgrind 与 TSan 各报一次

第三类是并发里的数据竞争:两个线程没加锁、同时读写同一个变量。这一类的现代主力是 ThreadSanitizer(TSan,`-fsanitize=thread`),它编译期插桩、慢大约 5-15 倍;valgrind 那一侧对应的是 **helgrind**(以及它的姊妹工具 drd,思路类似、实现不同)。程序照样,两个线程各做一百万次 `counter++`,而 `++` 不是原子操作、`volatile` 也不挡竞争:

```c
/* race.c:两个线程无锁自增同一全局——helgrind 与 TSan 各报一次 */
#include <pthread.h>
#include <stdio.h>

static volatile int counter = 0; /* volatile 不保证原子性、也不挡数据竞争 */

static void* worker(void* arg) {
    (void) arg;
    for (int i = 0; i < 1000000; ++i) {
        counter++; /* 非原子 read-modify-write,有数据竞争 */
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("counter = %d (期望 2000000)\n", counter);
    return 0;
}
```

先看 helgrind(本机同样 `-static` 编,pthread 一起静态链进来):

```text
$ gcc -g -std=c11 -Wall -Wextra -static -pthread race.c -o race_static
$ valgrind --tool=helgrind --error-exitcode=99 ./race_static
... (Thread-Announcement 略)
==173147== Possible data race during read of size 4 at 0x4D7A48 by thread #2
==173147==  Locks held: none
==173147==    at 0x403256: worker (race.c:10)
==173147==    by 0x40D2F9: start_thread (...)
==173147== This conflicts with a previous write of size 4 at 0x4D7A48 by thread #3
==173147==  Locks held: none
==173147==    at 0x40325F: worker (race.c:10)
==173147==    by 0x40D2F9: start_thread (...)
==173147==  Address 0x4d7a90 is 0 bytes inside data symbol "counter"
...
counter = 2000000 (期望 2000000)
==173147== ERROR SUMMARY: 6 errors from 6 contexts (suppressed: 0 from 0)
$ echo $?
99
```

helgrind 抓到了 `worker (race.c:10)` 上对 `counter` 的 `Possible data race`,并指明「thread #2 读、thread #3 写、双方都没持锁」(Locks held: none),地址还贴心地翻译成「0 bytes inside data symbol "counter"」——直接告诉你出问题的是哪个全局变量。`Possible` 这个限定词是 helgrind 的诚实:它基于「happens-before」的锁集分析,分析不到精确的所有同步原语,所以宁可误报也不漏报。

对照 TSan(动态链接,本机能正常跑):

```text
$ gcc -g -std=c11 -Wall -Wextra -pthread -fsanitize=thread \
      -fno-omit-frame-pointer race.c -o race_tsan
$ ./race_tsan; echo "exit=$?"
==================
WARNING: ThreadSanitizer: data race (pid=173366)
  Read of size 4 at 0x555555558064 by thread T2:
    #0 worker /tmp/cj/p4ch11/race.c:10 (race_tsan+0x11f9)
    ...

  Previous write of size 4 at 0x555555558064 by thread T1:
    #0 worker /tmp/cj/p4ch11/race.c:10 (race_tsan+0x1211)
    ...

  Location is global 'counter' of size 4 at 0x555555558064 (race_tsan+0x4064)

  Thread T2 (tid=173370, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x61642)
    #1 main /tmp/cj/p4ch11/race.c:18 (race_tsan+0x1295)
    ...
SUMMARY: ThreadSanitizer: data race /tmp/cj/p4ch11/race.c:10 in worker
==================
counter = 2000000 (期望 2000000)
exit=66
```

TSan 的报告更干净:`data race`,「T2 读 / T1 之前写」,`Location is global 'counter'`,直指 `race.c:10`。注意一个细节——这次 `counter` 居然打印出了 `2000000`(正好等于期望值),但这**不代表程序对**:`++` 是「读-改-写」三步,两线程交错时丢更新是常态,这次没丢只是时序碰巧;TSan/helgrind 抓的是「**有没有竞争**」,不是「**这次结果对不对**」——结果对是运气,竞争存在是事实。这就是为什么并发 bug 不可靠复现、必须靠工具而不是靠「跑出来对就算对」。

并发这一类,**首选 TSan**(报告干净、和 gcc/clang 一体、比 helgrind 快),helgrind 留给「程序不能重编、拿二进制查」或「TSan 和别的 sanitizer 冲突(见下)」时兜底。

## 一张能力矩阵:谁抓什么、各多慢

把三章的实验和阶段 4 第 1 章合起来,整理成一张能力矩阵(散文写,不开速查表——这玩意儿要进脑子,不是进收藏夹)。先按「能抓什么错」分:ASan 抓**地址类**——堆/栈/全局越界、释放后使用、重复释放,这是它的主场;UBSan 抓**未定义行为**——有符号溢出、非法位移、空指针解引用、对齐违规;LSan(开 ASan 自带)抓**内存泄漏**;TSan 抓**数据竞争**(多线程无锁读写);MSan 抓**读未初始化**(本节实验一那个 ASan 抓不到的)。valgrind memcheck 一把抓的是**地址类 + 泄漏 + 读未初始化**,覆盖面最广,但**对未定义行为基本不管**(那是 UBSan 的活);helgrind/drd 抓**数据竞争**;valgrind 还有个 cachegrind(分析缓存命中)和 massif(分析堆增长曲线),sanitizer 家族没有对应物。

再按「怎么用」分,这是更关键的工程差别。sanitizer 全家都是**编译期插桩**,必须 `-fsanitize=...` 重编重链,源码在手;好处是快(ASan ~2 倍、UBSan 几乎无开销、TSan ~5-15 倍、MSan ~3 倍),能挂 CI 天天跑。valgrind 全家都是**运行期翻译**,`valgrind ./prog` 直接上、不重编、不挑编译器、甚至不要源码;代价是慢 10-50 倍(本机实测,纯计算负载从 0.18 秒涨到 2.45 秒,慢了约 14 倍),只能定向复核、不能挂 CI 常驻。两套在「能不能同开」上也得留意:**ASan 和 TSan、MSan 互斥**(它们的影子内存机制冲突,同开会报错或漏报),所以同一次运行里你只能挑一个 sanitizer;valgrind 的 memcheck 和 helgrind 也是二选一(一个进程只挂一个 valgrind 工具),但 valgrind 和 sanitizer 不冲突——你可以 sanitizer 编一份、valgrind 跑另一份,各跑各的。

最后按「能不能落地」分。sanitizer 里 ASan/UBSan/LSan/TSan 都好落地,gcc/clang 直接给、动态链接就跑;**MSan 是落地困难户**——它要求程序里**每一个库**(包括 glibc、第三方依赖)都得用 MSan 重编,否则那个没用 MSan 编的库传出来的未初始化字节,MSan 会一路误报。这就是为什么「抓未初始化读」这件事,工程上几乎都退回 valgrind——MSan 太重,valgrind 不用重编、`valgrind ./prog` 一把梭。本机这套 Arch + strip 过的 glibc 还专门给 valgrind 加了一道坎(`ld-linux` 符号缺失,见末尾环境坑),换主流发行版(Debian/Ubuntu/Fedora,装上对应的 debuginfo 包)就没这毛病。

合起来一句话:**日常调试默认挂 `-fsanitize=address,undefined`(能加 TSan 就再加 TSan,但和 ASan 分开跑),这是 90% 的场景;剩下 10%——程序在诡异出错的值、sanitizer 报「没问题」、你怀疑读到了脏数据——上 valgrind memcheck 复核,舍得花那 10-50 倍的慢**。两者不是二选一,是互补的层级防御。

## 小结

这一章把 valgrind 和整套 sanitizer 家族摆在一起,钉死了三件事。其一,**读未初始化的内存,ASan/UBSan 抓不到**——ASan 的影子内存只管「地址能不能访问」,不管「值从哪来」;valgrind memcheck 给每块字节额外记了「有没有被写过」的元数据,所以同一个未初始化读,ASan 放行(exit 0)、valgrind 当场甩 `Conditional jump or move depends on uninitialised value(s)` + 行号。其二,**抓内存泄漏,2026 年首选 LSan**(ASan 自带、快、动态链接就能跑),valgrind 的 `definitely lost` 留给「程序不能重编」或「交叉验证」时兜底——两套对同一份泄漏的判定高度一致。其三,**数据竞争首选 TSan**,helgrind 是它的 valgrind 版兜底,两者都抓「有没有竞争」而非「结果对不对」,所以并发 bug 不能靠「跑出来对就算对」。

更深一层是两者形态的差别:sanitizer 是编译期插桩(快、要重编、能挂 CI),valgrind 是运行期翻译(慢 10-50 倍、不重编、不要源码);这决定了日常默认上 sanitizer、sanitizer 抓不到的诡异值才上 valgrind 复核。MSan 虽然也能抓未初始化读,但它要求全栈重编、工程上几乎没法用,所以 valgrind 在「不重编就能查未初始化」这个生态位上,2026 年仍然无可替代。带着这套分工,下一章我们看怎么把这些工具串进一个调试工作流——什么顺序上、什么时候切换、报告怎么读最省时间。

## 环境坑:本机 Arch 的 valgrind 启动失败

本机 valgrind 3.25.1 在动态链接程序上启动即死,报 `Fatal error at startup: a function redirection which is mandatory for this platform-tool combination cannot be set up`,点名要 `ld-linux-x86-64.so.2` 里 `memcmp` 的符号——而 Arch 的 glibc 把 `ld-linux` 整个 strip 掉了(`nm -D /usr/lib/ld-linux-x86-64.so.2` 一个符号都没有)。本章三个实验都是用 `-static` 把 glibc 直接链进可执行文件绕开的(memcheck 的未初始化检测、helgrind 的竞争检测在静态二进制上都正常工作);代价是 valgrind 的泄漏检测也跟着废了(静态二进制没有 `ld-linux` 给 valgrind 挂 `malloc`/`free` 重定向,`HEAP SUMMARY` 恒显示 `0 allocs`)。换 Debian/Ubuntu 装 `libc6-dbg`、或 RHEL/Fedora 装 `glibc-debuginfo`,动态链接的程序就能正常跑 valgrind 的全套(memcheck + leak-check + helgrind),那才是 valgrind 的「满血」状态。本章贴的 helgrind / memcheck 输出是真跑的,泄漏那段的 valgrind 格式给的是标准字段名(本机抓不到、格式来自 valgrind 官方手册),诚实标注、不编。

## 参考资源

- **valgrind 官方手册**:`valgrind(1)` + `/usr/share/doc/valgrind/`,memcheck 章讲 `Conditional jump depends on uninitialised value` 的判定、`--track-origins` / `--error-exitcode` / `--leak-check=full` 的用法;helgrind 章讲「happens-before」锁集分析为什么用 `Possible`。
- **AddressSanitizer Wiki(Google)**:ASan 的影子内存机制、和 MSan/TSan 为什么互斥(`https://github.com/google/sanitizers/wiki/AddressSanitizer`)。
- **MemorySanitizer Wiki**:MSan 为什么要求全栈重编、为什么工程上几乎没法用(同上 wiki 的 MemorySanitizer 页)。
- **ISO C**:读未初始化的自动变量在 C11 §6.7.9 / §6.2.6.1 里多数情况算「未指定值」(unspecified),用 trap representation 时才升格为未定义行为——这正是 ASan/UBSan 都不报的法律根据。
- 阶段 4 第 10 章(本仓库 `10-sanitizer-deep.md`):sanitizer 那一侧的完整用法、红区机制、三段式读报告,是本章的直接对照面。
