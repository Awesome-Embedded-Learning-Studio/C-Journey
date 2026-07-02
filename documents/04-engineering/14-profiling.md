---
title: "性能剖析:clock_gettime 计时、gprof 调用图与 perf 原理"
description: "「我的程序慢」是句没法动手的话——慢是墙钟慢还是 CPU 烧得多?慢在哪个函数?为什么那个函数慢?这一章把这三个问号逐个量成数字。先用 clock_gettime 在 sleep(1) 上当场证明 clock() 量的是 CPU 时间不是墙钟(三种时钟读数 0.000055 vs 1.000182 vs 0.000052),再用 CLOCK_THREAD_CPUTIME_ID 在三线程程序里拆出「单线程 0.08s vs 进程累计 0.25s」;接着上 gprof:用 -O2 -fno-inline -pg 跑一个 main→worker_heavy/light→leaf_accumulate 的调用树,gprof flat+call graph 把 100% self 时间归到 leaf_accumulate、把 90.9%/9.1% 的子时间正确分到两个 worker,而一旦去掉 -fno-inline,三个函数全被内联进 main、profile 假报 100% in main——这就是剖析器被编译器优化骗了的经典坑;最后讲 perf(本机未装,诚实标注,只讲 perf stat/record 原理、perf_event_paranoid=2 的权限墙、硬件 PMU 计数器 vs 软件采样的差别、warmup+重复测量的纪律),配一张「量化→剖析→钻微架构」的三段式方法图。全 gcc16+clang22 双跑、gprof 真跑、贴真实输出+POSIX 条款;perf 段明说本机没装、不编造输出。"
chapter: 4
order: 14
tags:
  - host
  - engineering
  - testing
  - system-programming
  - posix
difficulty: advanced
reading_time_minutes: 18
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段 0·第 9 章:标准与优化(-std/-O/-g,本章 benchmark 全程 -O2,要先懂优化级别)"
  - "阶段 0·第 10 章:Sanitizer 门禁(本章会反复强调「别拿带 -fsanitize 的二进制做 benchmark」)"
  - "阶段 0·第 1 章:工具链体检(本机有没有装 perf/gprof 的检查思路)"
related:
  - "阶段 4·第 1 章:头文件契约(_POSIX_C_SOURCE 怎么解锁 POSIX 符号,本章 clock_gettime 会用到)"
  - "阶段 5:线程与并发(CLOCK_THREAD_CPUTIME_ID 量单线程 CPU 时间的进阶用法,那里深入)"
---

> 🟡 状态:待审核(2026-07-02)

# 性能剖析:clock_gettime 计时、gprof 调用图与 perf 原理

## 引言:从「感觉慢」到「量得出慢在哪」

前置阅读:阶段 0 的第 9 章把 `-O` 优化级别讲透了,第 10 章把 sanitizer 当成正确性那一侧的默认开关。这一章换一个维度——**程序是对的,但它跑得不够快**。正确性靠 sanitizer 当场抓,性能则要靠**计时器**告诉你「这段耗多少毫秒」、靠**剖析器(profiler)**告诉你「这点时间花在哪个函数的哪条语句上」。两边的工具链完全不同,别拿 ASan 的二进制去 benchmark——后文会真跑给你看它慢多少倍。

阶段 0 那几章里,性能只是顺带提了一句「`-O2` 比 `-O0` 快」。这一章是它的工程化深度面:我们要回答三个递进的问题——**慢,到底是墙钟慢还是 CPU 烧得多?慢在哪个函数?为什么那个函数慢?**。三个问题对应三件工具:`clock_gettime` 拆时钟、`gprof` 看调用图、`perf` 钻进微架构事件。本机(gcc 16.1.1 / clang 22.1.6,WSL2 x86_64,AMD Ryzen 7 5800H,L1d 32 KiB/核、L2 512 KiB/核、L3 16 MiB、cacheline 64 字节)上 `clock_gettime` 和 `gprof` 都真跑、贴真实输出;`perf` **没装**(`command -v perf` 找不到),那一段我老实标注「本机未装、只讲原理」,不编一条 perf 的输出糊弄你。诚实地写工具能用与不能用,是这一章的底线。

先说一句贯穿全章的话,Knuth 那句「过早优化是万恶之源」之所以被反复引用,是因为它太好犯了——在你还没量出瓶颈之前就去手写 SIMD、去抠位运算,十有八九白费力气。所以我们的顺序钉死:**先量化,再剖析,最后才动数据布局**。下面三段,就照这个顺序走。

## 第一件:慢,要量化哪一种时间

「我的程序跑得慢」——这句话里藏着两个完全不同的指标,新手最常混。**墙钟时间(wall-clock / real)**是从程序启动到结束、墙上时钟走过的时间,受 CPU 占用、I/O 等待、其他进程抢占共同影响,一个 `sleep(1)` 的程序 wall-clock ≈ 1 秒、但它几乎不烧 CPU。**CPU 时间(user + sys)**是 CPU 真正在你这个进程上花的累计时间,`user` 是用户态跑你的代码、`sys` 是陷进内核(系统调用、缺页处理)的时间。所以「慢」分两种:CPU-bound(算不过来)和 I/O-bound 或 latency-bound(在等)。**优化方向完全不同**——前者要减计算量、对齐 cache,后者要减阻塞、上并发或换算法。量化要做的第一件事,就是把这两种慢区分开。

### `clock_gettime`:把「时钟种类」和「测量粒度」分开选

Linux 下拿高精度时间,首选 POSIX 的 `clock_gettime`(头文件 `<time.h>`,定义在 POSIX.1-1993 / `_POSIX_C_SOURCE 199309L`)。它的关键设计是:**你先选一种「时钟」,再读它此刻的读数**。时钟种类决定了你量的是墙钟还是 CPU 时间,这是第一性的选择:

- `CLOCK_MONOTONIC`:单调递增的墙钟,不受 `settimeofday`/`adjtime` 改系统时钟影响、不会被 NTP 往回拨,精度通常是纳秒级。**量「这段代码墙钟跑多久」永远用它**。
- `CLOCK_PROCESS_CPUTIME_ID`:整个进程(含所有线程)的 CPU 时间,墙上时钟走过不算、只有 CPU 真在为你烧才走。量「这段纯计算烧了多少 CPU」用它。
- `CLOCK_THREAD_CPUTIME_ID`(Linux 扩展,`_GNU_SOURCE`):仅当前线程的 CPU 时间,多线程程序里拆「每个线程各烧了多少」靠它。

注意一个本机就会咬人的小坑:严格 `-std=c11`(不是 `gnu11`)下,`clock_gettime` 和 `CLOCK_MONOTONIC` 这些 POSIX 符号会被 `<time.h>` 藏起来——因为严格模式等于 `__STRICT_ANSI__`,glibc 默认不暴露 POSIX 扩展。修法是在**包含任何头文件之前**定义 `#define _POSIX_C_SOURCE 199309L`(用到 `CLOCK_THREAD_CPUTIME_ID` 则再加 `#define _GNU_SOURCE`),告诉 glibc「我要 POSIX 1993+ 的接口」。这一条对应阶段 4 第 1 章讲过的 feature test macro 纪律,这里不再展开。

`clock_gettime` 的签名是 `int clock_gettime(clockid_t clk_id, struct timespec *tp)`,把读数写进调用者给的 `timespec`(`tv_sec` 秒 + `tv_nsec` 纳秒),成功返 0、失败返 -1 并设 `errno`(POSIX 条款:`clock_gettime` 见 POSIX.1-2017 base specifications §2.4/Clock Selection;`struct timespec` 见 ISO C 7.27.1,因为 C11 把它从 POSIX 收编进了 `<time.h>`,但 `clock_gettime` 本身仍是 POSIX、不在 ISO C 里)。下面这个 `now_sec` 是全章 benchmark 的计时骨架:

```c
#define _POSIX_C_SOURCE 199309L /* 严格 -std=c11 下解锁 clock_gettime/CLOCK_MONOTONIC */
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); /* 单调时钟, 纳秒分辨率 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
```

### 为什么别用 `clock()`

`<time.h>` 里还有个看起来更简单的 `clock()`,新手最爱拿来「测这段多久」。它是 ISO C 标准函数(7.27.2.1),返的是「进程使用的**处理器时间**的近似值」,单位是 `clock_t`、要除以 `CLOCKS_PER_SEC` 才是秒。问题在于:**它量的根本不是墙钟,而是 CPU 时间**——一个 `sleep(1)` 的程序,`clock()` 几乎不走。我把它和 `clock_gettime` 的两种时钟一起包在 `sleep(1)` 外面跑,本机真实读数:

```c
/* clock_compare.c -- 对比 clock() 与 clock_gettime 三种时钟在一个 sleep 上的读数 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <unistd.h> /* sleep */

static double monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); /* 墙钟, 单调 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double proc_cpu_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts); /* 整个进程的 CPU 时间 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    /* 用 clock() 包住一段 sleep, 看它报多少 */
    clock_t c0 = clock();
    double m0 = monotonic_sec();
    double p0 = proc_cpu_sec();

    sleep(1); /* 阻塞 1 秒墙钟, 几乎不烧 CPU */

    clock_t c1 = clock();
    double m1 = monotonic_sec();
    double p1 = proc_cpu_sec();

    printf("sleep(1) 后三种时钟的读数:\n");
    printf("  clock()              : %.6f s  (CLOCKS_PER_SEC=%ld)\n",
           (double) (c1 - c0) / CLOCKS_PER_SEC, (long) CLOCKS_PER_SEC);
    printf("  CLOCK_MONOTONIC      : %.6f s  (墙钟)\n", m1 - m0);
    printf("  CLOCK_PROCESS_CPUTIME: %.6f s  (进程 CPU)\n", p1 - p0);
    return 0;
}
```

gcc 16 和 clang 22 都跑一遍,读数几乎一样(gcc 这边):

```text
$ gcc -std=c11 -Wall -Wextra clock_compare.c -o clock_compare
$ ./clock_compare
sleep(1) 后三种时钟的读数:
  clock()              : 0.000055 s  (CLOCKS_PER_SEC=1000000)
  CLOCK_MONOTONIC      : 1.000182 s  (墙钟)
  CLOCK_PROCESS_CPUTIME: 0.000052 s  (进程 CPU)
```

读这张表的方式:`CLOCK_MONOTONIC` 老老实实报了 1.000 秒(我们 `sleep(1)` 的墙钟),而 `clock()` 和 `CLOCK_PROCESS_CPUTIME_ID` 都只走了大约 0.00005 秒——因为 `sleep` 是阻塞系统调用,CPU 这 1 秒里根本没在为这个进程烧(它把 CPU 让给了内核调度器和其他进程)。这就把 `clock()` 的真面目钉死了:**它量的是 CPU 时间、不是墙钟**。所以凡是「我想知道这段代码用户体验上花了多久」「这段是 I/O 密集,在等网络/磁盘」的场景,用 `clock()` 全是错的——要墙钟就用 `CLOCK_MONOTONIC`,要进程 CPU 时间就用 `CLOCK_PROCESS_CPUTIME_ID`,把「量哪一种时间」这个第一性的选择做对,比纠结计时精度重要得多。`clock()` 还有个隐藏的精度问题:`CLOCKS_PER_SEC` 在本机是 100 万,意味着标称微秒分辨率,但它在 glibc 上底层走的也是 `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)`,所以精度其实够;真正的毛病不在精度、在它**名字误导**——`clock` 这个词让人以为是墙钟时钟,实际是 CPU 时钟。这一条坑就地记牢:名字像墙钟、实际是 CPU。

### 拆线程:`CLOCK_THREAD_CPUTIME_ID`

多线程程序里,光知道「进程总 CPU 时间」还不够——三个线程并行跑,你想知道哪个线程吃得多、哪个在划水,就要用 `CLOCK_THREAD_CPUTIME_ID`。它在 Linux 上是 GNU 扩展(需 `_GNU_SOURCE`),量的**仅是调用它的那个线程的 CPU 时间**。下面这个小程序起两个 worker 线程,加上主线程,三路并行各烧同样多的 CPU,然后对比「主线程视角」和「worker 线程视角」的读数:

```c
/* thread_cpu.c -- CLOCK_THREAD_CPUTIME_ID 量单线程的 CPU 时间, 对照进程 CPU 时间 */
#define _GNU_SOURCE /* CLOCK_THREAD_CPUTIME_ID 在 Linux 上需 _GNU_SOURCE */
#define _POSIX_C_SOURCE 199309L
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static double thread_cpu_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); /* 仅本线程的 CPU 时间 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double proc_cpu_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts); /* 整个进程所有线程累计 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* 纯 CPU 烧时间的循环, 用 volatile 防止优化掉 */
static void burn(long iters) {
    volatile long s = 0;
    for (long i = 0; i < iters; i++) {
        s += i;
    }
    (void) s;
}

static void* worker(void* arg) {
    long iters = (long) arg;
    double t0 = thread_cpu_sec();
    burn(iters);
    double t1 = thread_cpu_sec();
    printf("  worker 线程: CLOCK_THREAD_CPUTIME_ID 报 %.4f s\n", t1 - t0);
    return NULL;
}

int main(void) {
    const long iters = 300L * 1000 * 1000; /* 约 0.2s CPU/线程 */
    pthread_t t1, t2;

    double p_start = proc_cpu_sec();
    double th_start = thread_cpu_sec();

    pthread_create(&t1, NULL, worker, (void*) iters);
    pthread_create(&t2, NULL, worker, (void*) iters);
    /* 主线程也烧同样多 */
    burn(iters);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    double p_end = proc_cpu_sec();
    double th_end = thread_cpu_sec();

    printf("主线程视角(烧完 3 路并行后):\n");
    printf("  主线程 CLOCK_THREAD_CPUTIME_ID : %.4f s (只算主线程的 CPU)\n", th_end - th_start);
    printf("  进程  CLOCK_PROCESS_CPUTIME_ID : %.4f s (三个线程累计)\n", p_end - p_start);
    return 0;
}
```

本机实测(gcc 16,`-O2 -pthread`):

```text
$ gcc -std=c11 -Wall -Wextra -pthread -O2 thread_cpu.c -o thread_cpu
$ ./thread_cpu
  worker 线程: CLOCK_THREAD_CPUTIME_ID 报 0.0820 s
  worker 线程: CLOCK_THREAD_CPUTIME_ID 报 0.0865 s
主线程视角(烧完 3 路并行后):
  主线程 CLOCK_THREAD_CPUTIME_ID : 0.0803 s (只算主线程的 CPU)
  进程  CLOCK_PROCESS_CPUTIME_ID : 0.2489 s (三个线程累计)
```

读这张表的方式:两个 worker 各报 ~0.082-0.087 秒、主线程报 0.0803 秒——**三个线程各自的 CPU 时间几乎相等**(都烧了同样多的 `iters`),而进程累计 `CLOCK_PROCESS_CPUTIME_ID` 报 0.2489 秒,正好是三个 0.08 量级相加。这就坐实了两个时钟的分工:`THREAD` 只算本线程、`PROCESS` 把所有线程加起来。线程化性能调优时,你想找「哪个线程是瓶颈」,就挨个打 `CLOCK_THREAD_CPUTIME_ID`,而不是看那个会被所有人摊薄的进程总数。

### 用 `time` 拆 wall / user / sys,判断 CPU-bound 还是 I/O-bound

进程内的 `clock_gettime` 给你某段代码的耗时;但你想看「整个程序的总耗时里,墙钟、用户态 CPU、内核态 CPU 各占多少」,shell 的 `time` 一次给齐。zsh 内置 `time` 跑我们后面要剖析的程序:

```text
$ gcc -std=c11 -Wall -Wextra -O2 -fno-inline -pg work_tree.c -o work_tree_pg
$ (time ./work_tree_pg 80000000) >/dev/null
./work_tree_pg 80000000 > /dev/null  0.60s user 0.00s system 99% cpu 0.601 total
```

读法:`user` 0.60 秒是循环在用户态烧的 CPU;`system` 0.00 秒说明几乎没陷内核(没有系统调用、没有缺页);`total` 0.601 秒是墙钟;`99% cpu` 说明这几乎是个**纯 CPU-bound** 程序,瓶颈就是算力本身。如果这里 `cpu` 只有 5%,那说明程序大部分时间在阻塞(等磁盘、等网络),优化方向就完全不是「让它算更快」,而是「减少等待」。bash 的 `time -p` 给的是 `real/user/sys` 三行格式,语义一样:real 是墙钟、user+sys 是 CPU 时间。看到 `user+sys` 远小于 `real`,就去找阻塞点;看到 `user+sys ≈ real` 且占比高,才值得往剖析和 cache 那一侧走——也就是下面两段的主题。

## 第二件:剖析——慢在哪个函数

计时器告诉你「整体多慢」,但 0.60 秒里有 0.55 秒花在哪个函数?这就是剖析器(profiler)要回答的。Linux 上有两条路,从轻到重:**gprof**(GCC/clang 自带、插桩、入门级)和 **perf**(内核级、采样、能看微架构事件)。先讲 gprof,因为它不需要装额外工具、能让你最快上手;perf 留到第三段,而且要老实说本机没装。

### gprof 是怎么工作的:采样 + 插桩

gprof 不是魔法,它的机制是两件事的叠加——**插桩(instrumentation)**和**采样(sampling)**。你用 `-pg` 编译,编译器就在每个函数的入口插一段代码(`mcount`/`__gprof_mcount`),这段代码在运行期记录「谁调用了谁、调用了多少次」——这是**精确计数**,所以 gprof 的「调用次数(call count)」那一列是准的、不是估的。与此同时,glibc 在程序启动时还挂了一个基于 `SIGPROF`(或 `setitimer`)的周期性中断(默认约 100Hz,每 10 毫秒一次),中断一来就采样「此刻 CPU 正在哪个函数里执行」,把那个函数的采样计数加一——这是**统计采样**,所以 gprof 的「自身耗时(self time)百分比」是基于采样次数估出来的、有统计噪声,对短函数会漏报。两份数据合起来,程序正常退出时(`exit()` 或 `main` return)把累计的统计写进当前目录的 `gmon.out`,再用 `gprof` 读出来,就得到 flat profile(平铺图,按 self time 排序)和 call graph(调用图,谁调用了谁、各占多少)。这套机制决定了 gprof 的全部脾气,记住三条:**调用次数是准的、自身耗时是估的、最短函数会被采样漏掉**。

代价也清楚:`-pg` 插桩本身有开销(每个函数入口多一段代码),程序会慢一些,所以**绝对时间不可信、只看相对比例**;而且**别在正式发布里带 `-pg`**,只用来量。还有个本机就遇到的现实细节:gprof 要在程序**正常退出**时才能写出 `gmon.out`——如果你的程序被 `SIGKILL` 杀、或者 `_exit()` 直接走、或者崩在 sanitizer 手里 abort 掉,`gmon.out` 不会生成。所以跑 gprof 时,程序得能正常 return。

### 真跑:一个有调用层次的小程序

光说不练是嘴炮。我写一个最小的「待剖析程序」,有意识地造出**调用层次**和**调用次数的不均衡**:`main` 调用两个中间层 `worker_heavy`/`worker_light`,它们各自以不同次数调用同一个叶子 `leaf_accumulate`。heavy 调 20 次、light 调 2 次,这样总调用 22 次,heavy 应该吃掉绝大部分时间:

```c
/* work_tree.c -- 给 gprof 准备的有调用层次的小程序: main -> worker_a/b -> leaf */
#include <stdio.h>
#include <stdlib.h>

/* 叶子: 纯算术循环, 迭代数从 argv 进来以防编译期折叠 */
static long leaf_accumulate(long iters) {
    volatile long s = 0;
    for (long i = 0; i < iters; i++) {
        s += i * 3;
    }
    return s;
}

/* 中间层: 两个分支, 一个重一个轻, 调用次数不同 */
static long worker_heavy(long iters) {
    long acc = 0;
    for (int k = 0; k < 20; k++) {
        acc += leaf_accumulate(iters);
    }
    return acc;
}

static long worker_light(long iters) {
    long acc = 0;
    for (int k = 0; k < 2; k++) {
        acc += leaf_accumulate(iters);
    }
    return acc;
}

int main(int argc, char** argv) {
    long iters = 5000000L;
    if (argc > 1) {
        iters = atol(argv[1]);
    }

    long h = worker_heavy(iters);
    long l = worker_light(iters);

    printf("heavy=%ld light=%ld (iters=%ld)\n", h, l, iters);
    (void) (h + l);
    return 0;
}
```

注意两件事:其一,**迭代数从命令行 `argv` 传进来**,不是写死成常量——写死了 `-O2` 会把循环折叠成编译期常量、机器码里根本没循环可测(阶段 0 第 9 章细讲过这个坑,这里不重复);其二,累加结果赋给了 `volatile long s` 再 return,这一步是**防优化**的命根子,不保住结果编译器会判定「这个循环没副作用」直接删掉。这两步在 benchmark 里不是装饰,是让测量诚实的底线。编译时除了 `-O2 -pg`,还得加一个关键的 `-fno-inline`——原因下一段当场演给你看:

```text
$ gcc -std=c11 -Wall -Wextra -O2 -fno-inline -pg work_tree.c -o work_tree_pg
$ ./work_tree_pg 80000000   # 跑完生成 gmon.out
heavy=191999997600000000 light=19199999760000000 (iters=80000000)
$ gprof -b ./work_tree_pg gmon.out   # -b 去掉冗长说明
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  ms/call  ms/call  name
100.00      0.48     0.48       22    21.82    21.82  leaf_accumulate
  0.00      0.48     0.00        1     0.00   436.36  worker_heavy
  0.00      0.48     0.00        1     0.00    43.64  worker_light
```

先读 **flat profile**(平铺图,按 self time 降序排):`leaf_accumulate` 的 self time 是 100%(0.48 秒全在它身上),调用 22 次、每次平均 21.82 毫秒;两个 worker 的 self time 是 0——因为它们除了调用 leaf 几乎不干别的活,自身代码就一个 for 循环加 acc,采样几乎落不到它们头上。注意 `calls` 那一列:leaf 被调 22 次(20+2),heavy 和 light 各被调 1 次——**这列是插桩精确数出来的、不是估的**,和我们的代码完全吻合。

但 flat profile 只告诉你「谁自身烧 CPU 多」,不告诉你「调用关系」。后者要看 **call graph**(调用图),`gprof` 默认在 flat profile 之后输出:

```text
			Call graph


granularity: each sample hit covers 4 byte(s) for 2.08% of 0.48 seconds

index % time    self  children    called     name
                0.04    0.00       2/22          worker_light [4]
                0.44    0.00      20/22          worker_heavy [3]
[1]    100.0    0.48    0.00      22         leaf_accumulate [1]
-----------------------------------------------
                                                 <spontaneous>
[2]    100.0    0.00    0.48                 main [2]
                0.00    0.44       1/1           worker_heavy [3]
                0.00    0.04       1/1           worker_light [4]
-----------------------------------------------
                0.00    0.44       1/1           main [2]
[3]     90.9    0.00    0.44       1         worker_heavy [3]
                0.44    0.00      20/22          leaf_accumulate [1]
-----------------------------------------------
                0.00    0.04       1/1           main [2]
[4]      9.1    0.00    0.04       1         worker_light [4]
                0.04    0.00       2/22          leaf_accumulate [1]
```

这张图才是 gprof 的精华。读法:每个方框是一个函数,`self` 是它自身耗时、`children` 是它调用的所有子函数累计耗时、`% time` 是 self+children 占总时间的比例。看 `[3] worker_heavy` 这一行:self 0、children 0.44 秒、占比 90.9%——它把 90.9% 的时间「花在了它调用的函数上」(就是 leaf);而 `[4] worker_light`:self 0、children 0.04、占比 9.1%。两者比例 90.9 : 9.1 ≈ 20 : 2,正好等于它们调用 leaf 的次数比。这一行就是「找热点」的答案:**优化 worker_heavy 里那个被调 20 次的 leaf 调用,比优化 worker_light 划算 10 倍**。clang 22 的 `-pg` 在本机同样能跑出结构一致的 flat profile(`leaf_accumulate` self 100%、22 次调用),证明 gcc 和 clang 的 `-pg` 在这台机器上都可用——双编译器一致,这里就不重复贴 clang 那一份了。

### 真坑:gprof 被 inline 骗了

上面那份漂亮的 profile,前提是加了 `-fno-inline`。现在我把这个旗标去掉,只用 `-O2 -pg`,**代码一行不改**,重跑:

```text
$ gcc -std=c11 -Wall -Wextra -O2 -pg work_tree.c -o work_tree_inline
$ ./work_tree_inline 80000000 >/dev/null
$ gprof -b ./work_tree_inline gmon.out
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  Ts/call  Ts/call  name
100.00      0.45     0.45                             main
```

**100% 时间全堆在 `main` 头上,`leaf_accumulate`/`worker_heavy`/`worker_light` 一个都不见了。** 这不是 gprof 坏了,而是 `-O2` 下 GCC 把这三个小函数**内联(inline)进了 `main`**——机器码里根本没有独立的 `leaf_accumulate` 函数体可供采样,采样中断一来落到的就是 `main`。`calls` 那一列也空了,因为内联之后不存在「函数调用」这回事了。这是个极其重要的教训:**剖析器的分辨率受编译器优化影响**。函数被内联、被循环展开,gprof 就看不见它们,profile 会把时间归到「调用者」头上,误导你以为「main 慢」,实际慢的是被内联进去的叶子。

这个坑的修法是辩证的,不是无脑加 `-fno-inline`:如果你怀疑某个函数是热点、但它没出现在 profile 里,先加 `-fno-inline`(或更精细的 `-fno-inline-small-functions`)重测,确认它是不是被内联藏起来了;但**正式发布版别带 `-fno-inline`**,因为内联本身是重要的优化(尤其对小叶子函数,内联能省掉调用开销、暴露寄存器分配机会)。也就是说:**剖析时关内联看清结构、发布时开内联拿性能**,两者用不同的编译旗标,这是 gprof 时代的常规操作。perf 那一侧没有这个烦恼(它采的是指令地址、内联进 main 也照样能映射回源码),所以到了 perf 那一段,这个坑自动消失。

### gprof 的两个脾气,记住就行

把上面两段的经验浓缩成两条,够用了。其一,gprof 基于**采样**,对短函数容易漏报(默认 100Hz、10ms 一次采样,跑不到几毫秒的函数采样落不到它头上),所以它擅长抓「少数大头函数」、不擅长抓「被调几百万次但每次很碎」的开销——后者要靠更细的方法(perf 或 instrumentation-based 的 `-finstrument-functions`)。其二,**只看相对比例、别信它的毫秒数**——`-pg` 插桩有开销,绝对时间会比真实慢,profile 里的 `0.48 秒` 不能拿来当「这函数真实耗时」,只能拿来和别的函数的 `0.04 秒` 比「谁是大头」。这两条记牢,gprof 就是个趁手的入门剖析器;真要钻进 cache miss、分支预测失败这种微架构事件,得往下走。

## 第三件:perf——本机没装,只讲原理

讲完 gprof 该讲 perf 了。但**先说实话**:`command -v perf` 在这台 WSL2 机器上找不到,perf 没装。WSL2 默认不带内核的 `tools/perf`,要装得 `apt install linux-perf` 或 `linux-tools-generic`,而 WSL2 的内核是微软自维护的、perf_event 子系统的支持有时还不全。所以下面这一段是「**你装上之后该这么用、该这么读**」的概念指引,不是本机跑出来的输出——**我绝不编一条 perf 的输出糊弄你**。诚实地标注「本机未装、讲原理」,是这一章的底线,也是 AGENTS.md「真实输出铁律」的硬要求。

### perf 是怎么工作的:硬件计数器 + 采样

perf 是 Linux 内核 `perf_event` 子系统的用户态前端(源码在内核树的 `tools/perf`)。它和 gprof 最根本的差别是:**perf 不插桩,它采样**——而且采样靠的是 CPU 的**硬件性能计数器(PMU,Performance Monitoring Unit)**。现代 CPU 内部有一组专用寄存器,能统计「这次运行执行了多少条指令、有多少个时钟周期、cache miss 了多少次、分支预测错了多少次」——这些是 CPU 自己在数,几乎零开销。perf 让你把这些计数器配置成「每数到 N 个事件就触发一次中断」,中断一来记录「此刻的指令地址、调用栈」,跑完汇总成 `perf.data`,再 `perf report` 看热点。因为是硬件计数器,它**不影响代码生成**(不需要 `-pg`、不需要重编),开销可以压到 1% 以下;而且能看见 gprof 看不见的微架构事件——这才是 perf 的真正价值。

perf 有两种用法,对应两种粒度。**`perf stat`** 是统计式剖析,跑一遍程序、给一组汇总计数,不细到指令;**`perf record`** 是采样式剖析,周期性记录「CPU 在哪」,跑完生成 `perf.data` 用 `perf report` 看,粒度可以细到汇编指令。装上之后典型命令长这样(概念,非本机实跑):

```text
# 最常用:统计式剖析, 看 CPU 周期、指令数、cache miss、分支预测失败
perf stat ./work_tree_pg 80000000

# 采样式剖析:周期性记录 CPU 在哪里, 跑完生成 perf.data
perf record -g ./work_tree_pg 80000000   # -g 记录调用栈
perf report                              # 交互式查看热点(类似 gprof 但基于采样)

# 只看缓存相关事件:cache miss 是性能优化的关键信号
perf stat -e cache-misses,cache-references,instructions,cycles ./work_tree_pg 80000000
```

### perf 的权限墙:`perf_event_paranoid`

perf 不是想用就能用的。内核有个 sysctl 叫 `perf_event_paranoid`,控制普通用户能用 perf 的哪些功能,取值是整数:`-1` 全开放(所有 CPU 都能测,包括内核态)、`0` 允许测 CPU 相关的硬件事件但限本进程、`1`(很多发行版默认)限制更多、**`2` 或更高**(部分发行版的默认)普通用户只能测自己的、且不能采内核态。本机即便装上 perf,普通用户常常因为 `perf_event_paranoid >= 2` 而被拒,表现是 `perf stat` 报 `Permission denied` 或 `perf_event_open(...): Operation not permitted`。修法要么 `sudo sysctl kernel.perf_event_paranoid=1`(或更松的 `-1`,但 `-1` 有安全含义、生产环境别乱开),要么把自己加进 `perf` 用户组。这一段我诚实标注:**本机没装 perf,这条权限墙是 Linux perf 的通用知识、不是本机实测的报错**——但你在自己的机器上头一次跑 perf 撞墙,大概率就是它。

### perf 输出该看什么:IPC 与 cache miss

装上 perf、绕过权限墙之后,`perf stat` 的输出长这样(下面是**典型字段示意,不是本机实跑、仅说明读法**——本机 perf 没装,我编不出真实数字):

```text
 Performance counter stats for './work_tree_pg':
       1,234.56 msec task-clock                #    0.995 CPUs utilized
             2,103      context-switches        #    1.702 K/sec
        45,000,000      instructions            #    0.55  insn per cycle
        81,800,000      cycles                  #    0.066 GHz
        12,300,000      cache-misses            #    15.0% of all cache refs
        67,000,000      cache-references
         1,200,000      branch-misses           #    0.8%  of all branches
       1.241234567 seconds time elapsed
```

这里面最该盯的两行。**`insns per cycle`(IPC)**:理想能到 3-4(取决于流水线深度和乱序执行能力),接近 1 说明 CPU 喂得饱;远低于 1(比如上面示意里的 0.55)通常意味着 CPU 在**等内存**——流水线里有大量空槽,因为下一条指令要的数据还没从 cache/DRAM 取回来。**`cache-misses` 占比**:`cache-misses / cache-references`,这个比例高(比如示意里的 15%),基本可以断定代码「对缓存不友好」,而缓存不友好正是 IPC 低的主要原因——两个指标互相印证。绝对墙钟时间会随机器、负载波动,但 IPC 和 cache-miss 比例这种**微架构事件更稳定、更有诊断价值**:同一份代码,在快机器上跑 0.1 秒、慢机器上跑 0.3 秒,但两台机器上报的 cache-miss 比例应该差不多——那才是「这段代码为什么慢」的真正答案。

### 微基准的纪律:warmup、重复、别拿 sanitizer 二进制测

讲完工具再讲方法,因为工具用错了,数据全是垃圾。benchmark 不是「跑一次拿个数字」,它有自己的纪律。**第一,先 warmup 再测**:第一次访问一段内存会触发缺页(操作系统现分配物理页、建立页表映射),这开销不该算进你的测量;所以正式计时前先跑一遍把内存全触及、把指令 cache 填满,再开始计时。本章的 stride bench 在计时前先 `for` 一遍把数组填满、matrix scan 也一样,就是这个道理。**第二,重复多次取代表值**:单次测量受系统负载、CPU 频率动态调节(`cpufreq` turbo)、其他进程抢占影响,误差可能 20% 以上;跑 5-10 次取中位数或最小值,比单次可信得多。本章的 stride bench 内部跑 5 个 pass,就是这个用意。

**第三条是和阶段 0 第 10 章呼应的红线:别拿带 sanitizer 的二进制做 benchmark。** ASan/UBSan 是编译期插桩,它在每条内存访问前后都加检查代码,程序会显著变慢、行为也不再代表真实发布版。我用本章的 stride bench 真跑对照给你看——同一个 `.c`,一份 `-O2` 编、一份 `-O2 -fsanitize=address,undefined` 编,跑步长 1 的扫描:

```text
$ gcc -std=c11 -Wall -Wextra -O2 stride_bench.c -o stride_plain
$ gcc -std=c11 -Wall -Wextra -O2 -fsanitize=address,undefined stride_bench.c -o stride_asan
$ ./stride_plain | grep 'stride   1'
stride   1 :   0.0077 s   41943040 elems   0.18 ns/elem
$ ./stride_asan 2>/dev/null | grep 'stride   1'
stride   1 :   0.0439 s   41943040 elems   1.05 ns/elem
```

同一个循环,**带 ASan 的版本慢了将近 6 倍**(0.18 → 1.05 ns/elem)。这不是循环变慢了,是 ASan 在每次 `a[i]` 访问前后插的边界检查、影子内存查询在烧时间。所以铁律是:sanitizer 用来**找正确性 bug**(阶段 4 第 1 章的主题),`-pg` 用来**剖析热点**(本章第二段),benchmark 用来**量真实性能**(本章第一段)——三种场景、三份不同的二进制,各管一摊,千万别混。

## 把「为什么慢」量成数字:cache 局部性微基准

讲完工具链,最后用一个真实微基准把「为什么那个函数慢」量成数字,收口这一章的方法论。这一节和阶段 0 第 9 章里提过的「`-O` 级别影响速度」是不同维度:那里讲的是编译器优化,这里讲的是**程序和内存层次结构的契合度**——也就是 cache 局部性。现代 CPU 和内存之间有道越来越宽的鸿沟:一条寄存器运算大概 1 个周期(<1 纳秒),而去主存(DRAM)取一个数据要 200-300 个周期(上百纳秒),差两个数量级。这道鸿沟靠**缓存(cache)**填,本机 Ryzen 7 5800H 的延迟阶梯是:L1d 32 KiB/核 ~1ns、L2 512 KiB/核 ~3ns、L3 16 MiB 共享 ~12ns、主存 ~100ns。cache 按 **cacheline**(本机和绝大多数 x86/ARM 都是 64 字节一行)为单位搬运——CPU 要读地址 `A` 的一个字节,实际把 `A` 所在的整行 64 字节搬进 L1。这一设计决定了「快」的关键是两种局部性:**空间局部性**(顺序访问,搬一次的 64 字节里能用上多个,均摊几乎免费)和**时间局部性**(刚用过的数据接着用,还在 cache 里)。

光说不练是嘴炮,写一个最小的微基准,把「连续访问」和「大步长跳跃访问」的差距**量成每个元素的均摊延迟**。造一个 32 MiB 的 `int` 数组(远超 16 MiB 的 L3,每次访问几乎都得看 cache 命不命中),分别用步长 1(连续,cacheline 搬一次喂 16 个 int)、16(正好每 cacheline 取一个)、128(每 8 个 cacheline 才取一个)扫描:

```c
/* stride_bench.c -- clock_gettime 微基准:连续扫描 vs 大步长扫描 */
#define _POSIX_C_SOURCE 199309L /* 严格 -std=c11 下解锁 clock_gettime/CLOCK_MONOTONIC */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_ELEMS (8 * 1024 * 1024L) /* 8M int = 32 MiB, 远超 16 MiB 的 L3 */
#define PASSES 5

static double now_sec(void) {
    struct timespec ts;
    /* CLOCK_MONOTONIC: 单调递增、不受 adjtime 改系统时钟影响、纳秒分辨率 */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    int* a = malloc(sizeof(int) * N_ELEMS);
    if (!a) {
        perror("malloc");
        return 1;
    }
    /* 先填好数据并触及每一页, 把缺页开销排除在测量之外 */
    for (long i = 0; i < N_ELEMS; i++) {
        a[i] = (int) (i & 0xff);
    }

    volatile long sink = 0; /* 防止累加被优化掉 */

    /* 步长 1: 顺序访问, cacheline 搬一次喂 16 个 int, 对 L1 友好 */
    double t0 = now_sec();
    long s1 = 0;
    for (int pass = 0; pass < PASSES; pass++) {
        for (long i = 0; i < N_ELEMS; i++) {
            s1 += a[i];
        }
    }
    double t_stride1 = now_sec() - t0;

    /* 步长 16: 64B/4B=16, 正好每 cacheline 取 1 个元素 */
    double t1 = now_sec();
    long s16 = 0;
    for (int pass = 0; pass < PASSES; pass++) {
        for (long i = 0; i < N_ELEMS; i += 16) {
            s16 += a[i];
        }
    }
    double t_stride16 = now_sec() - t1;

    /* 步长 128: 每 8 个 cacheline 才取一个, 踩的全是全新行 */
    double t2 = now_sec();
    long s128 = 0;
    for (int pass = 0; pass < PASSES; pass++) {
        for (long i = 0; i < N_ELEMS; i += 128) {
            s128 += a[i];
        }
    }
    double t_stride128 = now_sec() - t2;

    sink += s1 + s16 + s128;
    (void) sink;

    long n1 = (long) PASSES * N_ELEMS;
    long n16 = (long) PASSES * (N_ELEMS / 16);
    long n128 = (long) PASSES * (N_ELEMS / 128);

    printf("array: %ld ints (%.1f MiB), %d passes each\n", N_ELEMS,
           (double) N_ELEMS * sizeof(int) / (1024 * 1024), PASSES);
    printf("stride  %2d : %8.4f s   %ld elems   %.2f ns/elem\n", 1, t_stride1, n1,
           t_stride1 / n1 * 1e9);
    printf("stride %2d : %8.4f s   %ld elems   %.2f ns/elem\n", 16, t_stride16, n16,
           t_stride16 / n16 * 1e9);
    printf("stride %3d : %8.4f s   %ld elems   %.2f ns/elem\n", 128, t_stride128, n128,
           t_stride128 / n128 * 1e9);
    printf("slowdown stride128/stride1: %.1fx\n", (t_stride128 / n128) / (t_stride1 / n1));

    free(a);
    return 0;
}
```

本机实测(gcc 16,`-O2`):

```text
$ gcc -std=c11 -Wall -Wextra -O2 stride_bench.c -o stride_bench
$ ./stride_bench
array: 8388608 ints (32.0 MiB), 5 passes each
stride   1 :   0.0081 s   41943040 elems   0.19 ns/elem
stride  16 :   0.0043 s   2621440 elems   1.64 ns/elem
stride 128 :   0.0014 s   327680 elems   4.19 ns/elem
slowdown stride128/stride1: 21.8x
```

读这张表的方式:注意每元素是**均摊**(已除以各自访问的元素数),所以可以直接比「访问每个元素的代价」。**步长 1 时每个元素只要 0.19 纳秒**——连续访问,cacheline 搬一次喂 16 个 int,L1 的速度;步长 16 时 cacheline 里只用了 1/16,均摊涨到 1.64 纳秒;**步长 128 时每次访问都踩进全新 cacheline,4.19 纳秒一个元素——是连续访问的 21.8 倍**。这个 22 倍,正好落在「L1 命中(~1ns) vs 主存访问(~100ns 含整行搬运)」的延迟阶梯上:跳跃访问每多跨几个 cacheline,你就在为内存带宽而不是 CPU 速度买单。clang 22 的数据结构一致(0.29 / 3.60 / 7.56 ns,慢 26 倍),绝对数随编译器/负载浮动,但**比值(连续 vs 跳跃的倍数)稳定可复现**——benchmark 里要看的常常是这个比值,不是某个绝对数字。

### 对照实验:数据塞进 cache 时差距消失

为了证明上面这 22 倍确实来自 cache、而非别的因素,把同样的代码换一个数据规模重跑。下面这个 `matrix_scan.c` 扫描一个方阵,行优先(内层连续)vs 列优先(内层跨行,每元素踩新 cacheline),矩阵大小从命令行控制——先扫 4000×4000(61 MiB,**远超 L3**),再扫 200×200(0.2 MiB,**塞得进 L2**):

```c
/* matrix_scan.c -- 行优先 vs 列优先扫描, 矩阵大小从命令行控制 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 4000;

    /* 一段连续内存, 当成 n x n 的 int 矩阵(行主序) */
    int* flat = malloc(sizeof(int) * (size_t) n * (size_t) n);
    if (!flat) {
        perror("malloc");
        return 1;
    }
    /* 填好并触及每一页, 排除缺页开销 */
    for (long i = 0; i < n * n; i++) {
        flat[i] = (int) (i & 1);
    }

    volatile long sink = 0;

    /* 行优先: 内层 j 连续, rows[i*n + j] 与 rows[i*n + j+1] 相邻 */
    double t0 = now_sec();
    long sum_row = 0;
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) {
            sum_row += flat[i * n + j];
        }
    }
    double t_row = now_sec() - t0;

    /* 列优先: 内层 i 跨行, flat[i*n + j] 与 flat[(i+1)*n + j] 相隔 n 个 int */
    double t1 = now_sec();
    long sum_col = 0;
    for (long j = 0; j < n; j++) {
        for (long i = 0; i < n; i++) {
            sum_col += flat[i * n + j];
        }
    }
    double t_col = now_sec() - t1;

    sink += sum_row + sum_col;
    (void) sink;

    double mib = (double) n * n * sizeof(int) / (1024.0 * 1024.0);
    printf("matrix %ldx%ld (%.1f MiB)\n", n, n, mib);
    printf("  row-major: %.6f s\n", t_row);
    printf("  col-major: %.6f s\n", t_col);
    if (t_row > 0) {
        printf("  speedup col/row: %.2fx\n", t_col / t_row);
    }

    free(flat);
    return 0;
}
```

本机实测(gcc 16,`-O2`),大矩阵再扫一次、小矩阵换一次规模:

```text
$ gcc -std=c11 -Wall -Wextra -O2 matrix_scan.c -o matrix_scan
$ ./matrix_scan 4000
matrix 4000x4000 (61.0 MiB)
  row-major: 0.005433 s
  col-major: 0.083268 s
  speedup col/row: 15.33x
$ ./matrix_scan 200
matrix 200x200 (0.2 MiB)
  row-major: 0.000011 s
  col-major: 0.000011 s
  speedup col/row: 1.01x
```

大矩阵(61 MiB,超过 L3)时,列优先比行优先慢 **15 倍**——同样的计算量、同样的 checksum,只因为访问顺序不同;把矩阵缩到 200×200(0.2 MiB,整个塞进 L2),**差距消失了**(1.01x ≈ 1.0)。这一升一消,把根因钉死在 cache:数据在 L2 里,无论怎么跳,访问延迟都是 ~3ns,无所谓顺序;数据超过 L3,顺序访问还能靠 cacheline 顺路带、跳跃访问就每次都 miss。如果你写过矩阵乘法、图像处理、数值计算,这 15 倍就是「能用」和「没法用」的差距。把这一节的方法论落到代码上:优先顺序访问数组(C 行主序,`for(i) for(j) a[i][j]` 永远快于反过来),让「一起用的数据」在内存里也挨在一起(把热循环访问的字段排进同一段连续内存),别在热循环里反复 `malloc`/`free`(不光慢、还破坏局部性)——这几条不是抠常数,是改数据布局,收益大、风险低、可读性不降。

## 小结

性能优化的正确顺序是**先量化、再剖析、最后才动手改**,这一章就照这个顺序走了一遍。量化这一段,用 `clock_gettime` 在 `sleep(1)` 上当场证明了 `clock()` 量的是 CPU 时间不是墙钟(本机三种时钟读数 0.000055 vs 1.000182 vs 0.000052),又用 `CLOCK_THREAD_CPUTIME_ID` 在三线程程序里拆出「单线程 0.08s vs 进程累计 0.25s」——把「量哪一种时间」这个第一性的选择做对,比纠结精度重要。剖析这一段,用 gprof 跑一个有调用层次的小程序,flat+call graph 把 100% self 时间归到 `leaf_accumulate`、把 90.9%/9.1% 的子时间正确分到两个 worker;接着去掉 `-fno-inline`,三个函数全被内联进 `main`、profile 假报 100% in main——剖析器被编译器优化骗了,这是 gprof 时代最常见的坑,修法是「剖析时关内联看清结构、发布时开内联拿性能」。perf 那一段,本机没装、我老实说了,只讲了它的硬件计数器采样原理、`perf_event_paranoid` 权限墙、IPC 与 cache-miss 的读法,没编一条假输出。最后用 stride 和 matrix 两个微基准把「为什么慢」量成数字:连续访问每元素 0.19ns、跨 cacheline 跳跃 4.19ns(慢 22 倍),大矩阵列优先比行优先慢 15 倍,而数据塞进 L2 时这些差距统统消失——根因是 cache 局部性,解法是改数据布局。

记牢一句话:**让 CPU 算得快很难,让 CPU 不用等内存、不被插桩干扰、不被编译器骗了眼,常常就够了**。这一章没讲算法复杂度(那是数据结构阶段的事)、没讲 SIMD/向量化(那是更深的优化专题),只讲了「怎么诚实量出慢在哪」——这一步做不对,后面所有优化都是瞎子摸象。

## 练习

1. 把 `clock_compare.c` 改成包住一段纯 CPU 循环(把 `sleep(1)` 换成 `burn(300000000)`),重跑三种时钟,观察这次 `clock()` 和 `CLOCK_PROCESS_CPUTIME_ID` 的读数是不是都接近 `CLOCK_MONOTONIC`——验证「CPU-bound 时三者趋同、I/O-bound 时 clock() 失真」。
2. 在 `work_tree.c` 里把 `worker_heavy` 的调用次数从 20 改成 50、`worker_light` 从 2 改成 5,重跑 gprof,确认 call graph 里两者的 children 时间比例是否随之变成 50:5(即 90.9% : 9.1% 那一栏的数字会变)——亲手验证「gprof 的 calls 列是精确计数」。
3. 把 `work_tree.c` 分别用 `-O0`、`-O1`、`-O2`、`-O3`(都带 `-fno-inline -pg`)编译并跑 gprof,观察四个 profile 的 self time 分布是否一致;再用 `gcc -fopt-info-vec` 看 `-O3` 有没有对 `leaf_accumulate` 的循环做自动向量化(提示:有 `volatile` 阻挡,大概率不会,想想为什么)。
4. 在 `stride_bench.c` 里再加 `stride 4` 和 `stride 32` 两档,画出「步长 → 每元素延迟」的曲线,看它在哪个步长发生跳变——那大概率对应 cacheline 边界(步长 16 附近)。
5. 如果你装得上 perf,对 `matrix_scan 4000` 的行优先和列优先各跑一次 `perf stat -e cache-misses,cache-references`,对比两者的 cache-miss 数量,验证「慢 15 倍」确实对应「cache miss 多了一个数量级」——这就是 perf 相对 gprof 的不可替代之处:它能直接量出微架构事件。

## 参考资源

- [POSIX.1-2017 — `clock_gettime`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_gettime.html)(时钟种类与 `CLOCK_MONOTONIC`/`CLOCK_PROCESS_CPUTIME_ID` 的权威定义)
- [ISO C11 §7.27 — Time components](https://www.iso.org/standard/57853.html)(`struct timespec`、`clock()`、`CLOCKS_PER_SEC` 的标准定义)
- [GNU C Library Manual — CPU Time and Profiling](https://www.gnu.org/software/libc/manual/html_node/Profiling.html)(gprof 与 `mcount`/`_mcount` 插桩机制)
- [Brendan Gregg — Linux Performance](https://www.brendangregg.com/linuxperf.html)(perf、火焰图、性能分析的权威资料集)
- [`perf` Wiki — Tutorial](https://perf.wiki.kernel.org/index.php/Tutorial)(`perf stat`/`record`/`report` 与 `perf_event_paranoid` 详解)
- [Drepper — *What Every Programmer Should Know About Memory*](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)(cache/cacheline/局部性部分的进阶读物,本章微基准的理论背景)
- 本仓库相关章节:[阶段 4·第 1 章头文件契约](`_POSIX_C_SOURCE` 解锁 POSIX 符号的纪律)、[阶段 0·第 9 章标准与优化](`-O2` 与编译期常量折叠,benchmark 必须 `argv` 驱动迭代的原因)、[阶段 0·第 10 章 Sanitizer 门禁](「别拿 sanitizer 二进制做 benchmark」那条红线的来由)

---
*整理自作者笔记,按 C-Journey 写作规范重写;clock_gettime / gprof / 微基准数据均在 AMD Ryzen 7 5800H / gcc 16.1.1 + clang 22.1.6 本机实测捕获。perf 部分为概念指引(本机 WSL2 未安装 perf),其余输出均为真实运行结果。*
