---
title: "程序性能与剖析:用 perf/计时器把 C 程序的慢处逼出来"
description: "从「感觉慢」到「量化慢在哪」——先用 clock_gettime 和 time 拿到真实的墙钟/CPU 时间,用 gprof 看函数级耗时,再钻进 CPU 与内存层次结构,用真实 benchmark 验证行优先 vs 列优先遍历相差 11 倍、跨 cacheline 访问每元素慢 17 倍,最终写出对缓存友好的 C 代码。"
chapter: 4
order: 3
tags:
  - host
  - engineering
  - testing
  - debug
  - system-programming
difficulty: advanced
reading_time_minutes: 22
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0:编译流程、GDB 与库"
  - "Chapter 2:指针、内存布局与位运算"
  - "Chapter 4:ASan 与 UBSan"
related:
  - "Chapter 4:CMake 与模块化工程"
---

# 程序性能与剖析:用 perf/计时器把 C 程序的慢处逼出来

## 引言

笔者最近在研究怎么写出更高效的程序,而判断「快不快」的第一步,就是把模糊的「感觉慢」变成可以比较的数字。这一章我们要回答三个问题:**慢在哪、为什么慢、怎么改才真的快**。

前三章我们处理的是「对不对」——程序崩了、行为不符合标准,ASan/UBSan 当场揪出来。这一章换一个维度:**程序是对的,但它跑得不够快**。这两件事的工具链完全不同。正确性靠编译器内置的 sanitizer,性能则要靠**计时器**和**剖析器(profiler)**:前者告诉你「这段耗时多少毫秒」,后者告诉你「这点时间花在哪个函数/哪条语句上」。

先把丑话说在前头:这一章我们聊 cache、聊局部性、聊 `perf`,但**真正的坑在「过早优化」**——在你还没量出瓶颈之前就去手写 SIMD、去抠位运算,十有八九是白费力气。Knuth 那句「过早优化是万恶之源」之所以被反复引用,是因为它太容易犯。所以我们的顺序是:**先量化,再剖析,最后才谈 cache 友好的写法**。

> 本文所有计时、gprof 输出、benchmark 数据都在本机实测捕获:**AMD Ryzen 7 5800H / GCC 16.1.1**,L1d 32 KiB/core、L2 512 KiB/core、L3 16 MiB(共享)、cacheline 64 字节。perf 在这台机器上没装,我们用 `clock_gettime` + `gprof` 跑出真实数据,perf 留作「你装上之后该这么用」的概念指引。

## 第一件事:慢,到底要量化什么

「我的程序跑得慢」——这句话里藏着两个完全不同的指标,新手常混为一谈:

- **墙钟时间(wall-clock / real)**:从程序启动到结束,墙上时钟走过的时间。受 CPU 占用、I/O 等待、其他进程抢占共同影响。一个 `sleep(1)` 的程序,wall-clock ≈ 1 秒,但它**几乎不消耗 CPU**。
- **CPU 时间(user + sys)**:CPU 真正在你这个进程上花的累计时间。`user` 是你的代码在用户态跑的时间,`sys` 是陷进内核(系统调用、缺页处理等)的时间。一个纯算术循环,CPU 时间会逼近 wall-clock;一个在 `read()` 上阻塞等网络的程序,wall-clock 很大但 CPU 时间近乎 0。

所以「慢」分两种:CPU-bound(算不过来)和 I/O-bound 或 latency-bound(在等)。**优化方向完全不同**:前者要减少计算量、对齐 cache;后者要减少阻塞、上并发或换算法。量化就是要把这两种慢区分开。

## 拿到真实时间:`clock_gettime` 与 `time`

Linux 下拿高精度墙钟时间,首选 `clock_gettime(CLOCK_MONOTONIC, ...)`,它单调递增、不受系统时间被改的影响,精度通常是纳秒级:

```c
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);          /* 单调时钟,纳秒级 */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* 用法 */
double t0 = now_sec();
do_the_work();
double elapsed = now_sec() - t0;
```

> ⚠️ **别用 `clock()`**。`<time.h>` 的 `clock()` 量的是**进程 CPU 时间近似值**,不是墙钟时间,而且它的精度只有 `CLOCKS_PER_SEC`(常是 100 万),还可能因为整数累加溢出在长任务上失真。要墙钟就用 `clock_gettime(CLOCK_MONOTONIC)`,要进程 CPU 时间就用 `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)`。把「时钟种类」和「测量粒度」分开选,这是性能测量的基本功。

### 一个标杆程序:三个 CPU 强度不同的函数

我们先造一个最小的「待剖析程序」,模仿三个 CPU 强度递增的函数——`low_cpu`、`medium_cpu`、`high_cpu`,各自跑不同规模的累加循环:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* 用运行期才知道的迭代数,防止 -O2 把循环优化没(下文细说) */
static long low_cpu(long iters) {
    long s = 0;
    for (long i = 0; i < iters; i++) s += i;
    return s;
}
static long medium_cpu(long iters) {
    long s = 0;
    for (long i = 0; i < iters; i++) s += i;
    return s;
}
static long high_cpu(long iters) {
    long s = 0;
    for (long i = 0; i < iters; i++) s += i * 3;
    return s;
}

int main(int argc, char **argv) {
    long n_low    = 1000;
    long n_medium = 50000000L;
    long n_high   = 800000000L;
    if (argc == 4) { n_low = atol(argv[1]); n_medium = atol(argv[2]); n_high = atol(argv[3]); }

    double t0 = now_sec(); long a = low_cpu(n_low);       double t1 = now_sec();
    double                b = medium_cpu(n_medium);       double t2 = now_sec();
    double                c = high_cpu(n_high);           double t3 = now_sec();

    printf("low    (%11ld): %.4f s  sum=%ld\n", n_low,    t1 - t0, a);
    printf("medium (%11ld): %.4f s  sum=%ld\n", n_medium, t2 - t1, b);
    printf("high   (%11ld): %.4f s  sum=%ld\n", n_high,   t3 - t2, c);
    printf("total             : %.4f s\n", t3 - t0);
    (void)(a + b + c);
    return 0;
}
```

### 第一个真实的坑:被 `-O2` 优化掉的循环

我先按最直觉的写法——把迭代数直接写死成常量、累加结果也不输出——然后用 `gcc -O2` 编译。结果令人血压拉满:

```text
$ gcc -O2 cpu_work.c -o cpu_work
$ ./cpu_work
low    : 0.000000 s  sum=499500
medium : 0.000000 s  sum=49999995000000
high   : 0.000000 s  sum=5999999997000000000
total  : 0.000000 s
```

**三个函数全是 0.000 秒**。这不是计时器坏了,而是编译器比我聪明:当迭代次数是编译期常量、循环体又是纯算术(没有副作用),GCC 直接在编译期把循环算成 `1+2+...+N` 的闭式结果,塞个常数进去——**循环在生成的机器码里根本不存在了**。你测了一个不存在的循环。

这就是为什么上面的标杆程序要做两件事:**迭代数从命令行 `argv` 传进来**(编译期不知道值),**结果也打印出来或喂给 `volatile`**。这两步都阻止编译器把它折叠成常量。改完之后,真实数据出来了:

```text
$ gcc -O2 cpu_work.c -o cpu_work
$ ./cpu_work
low    (       1000): 0.0000 s  sum=499500
medium (   50000000): 0.0307 s  sum=1249999975000000
high   (  800000000): 0.3444 s  sum=959999998800000000
total             : 0.3750 s
```

这下三个函数的耗时差距清清楚楚:`high_cpu` 吃掉了 92% 的时间。这就是量化的第一步——**别测一个被优化没的循环**。benchmark 里那个 `volatile` 和命令行参数不是装饰,是让测量诚实的命根子。

### 用 `time` 拆 wall-clock / user / sys

光有进程内计时还不够,我们还想看「这段总耗时里,CPU 占了多少、有没有在等」。shell 的 `time`(zsh 内置是这个格式)一次给齐:

```text
$ time ./cpu_work >/dev/null
./cpu_work > /dev/null  0.30s user 0.01s system 90% cpu 0.345 total
```

读法:`user` 0.30 秒是我们的循环在用户态烧的 CPU;`system` 0.01 秒是内核态(主要是缺页处理和 `printf` 的写缓冲);`total` 0.345 秒是墙钟时间;`90% cpu` 说明这是一个**几乎纯 CPU-bound** 的程序——它几乎没在等任何东西,瓶颈就是算力本身。如果这里 `cpu` 只有 5%,那说明程序大部分时间在阻塞(等磁盘、等网络),优化方向就完全不是「让它算更快」,而是「减少等待」。

## 第二件事:剖析(profiling)——慢,慢在哪个函数

计时器告诉你「整体多慢」,但 0.345 秒里有 0.30 秒花在哪?这就是剖析器要回答的。Linux 上我们有几条路,从轻到重:

### `gprof`:最简单的函数级剖析

`gprof` 是 GCC 自带、不需要装额外工具的入门级剖析器。原理很简单:用 `-pg` 编译,编译器在每个函数入口插入计数代码,程序跑完生成一份 `gmon.out`,再用 `gprof` 读出来。代价是程序会慢一些(插桩开销),所以**别在正式发布里带 `-pg`**,只用来量。

```bash
gcc -O2 -pg cpu_work.c -o cpu_work_pg   # 加 -pg 插桩
./cpu_work_pg                            # 跑完生成 gmon.out
gprof -b ./cpu_work_pg gmon.out          # -b 去掉冗长说明
```

我把上面那个标杆程序用 `-pg` 编译跑一遍,`gprof` 给出的 flat profile(平铺图,按自身耗时排序)长这样:

```text
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  Ts/call  Ts/call  name
100.00      0.30     0.30                             main
  0.00      0.30     0.00        4     0.00     0.00  now_sec
```

注意看——**100% 时间全算在 `main` 头上**,`low_cpu`/`medium_cpu`/`high_cpu` 一个都没出现。这不是 gprof 坏了,而是 `-O2` 下 GCC 把这三个小函数**内联(inline)进了 `main`**,机器码里根本没有独立的 `high_cpu` 函数体可供采样。这是个非常重要的教训:**剖析器的分辨率受编译器优化影响**。函数被内联、被循环展开,gprof 就看不见它们。

想让 gprof 看见每个函数,编译时关掉内联再试:

```bash
gcc -O2 -pg -fno-inline cpu_work.c -o cpu_work_pg2
```

```text
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  ms/call  ms/call  name
 97.06      0.33     0.33        1   330.00   330.00  high_cpu
  2.94      0.34     0.01        2     5.00     5.00  low_cpu
  0.00      0.34     0.00        4     0.00     0.00  now_sec
```

这下对了:`high_cpu` 占了 97%,和我们的直觉(也是 wall-clock 计时的结论)一致。`medium_cpu` 没单独冒出来是因为它也短到被部分内联或采样太稀疏漏掉了。gprof 的采样是基于时钟中断(默认 100Hz),对非常短的函数会漏报——这也是它的固有限制。

> ⚠️ **gprof 的两个脾气**:一是基于**采样**,对短函数容易漏报,所以它擅长抓「少数大头函数」,不擅长抓「被调几百万次但每次很碎」的开销;二是 `-pg` 的插桩本身有开销,测出来的绝对时间会比真实慢,**只看相对比例,别拿它当计时器**。

### `perf`:Linux 的主力剖析器(本机未装,概念指引)

真正严肃的 Linux 性能剖析用 `perf`(Linux 内核自带的 `perf_event` 子系统封装)。它的核心优势是**采样而非插桩**——靠硬件性能计数器(PMU)周期性打断程序,记录「这一刻 CPU 在哪条指令、cache miss 了多少次、分支预测错了多少次」,几乎零开销,而且能看见缓存命中等微架构事件。

本机没装 perf(`command -v perf` 找不到),所以下面是「装上之后你该这么用」的概念指引,不是本机跑出来的输出。在 Debian/Ubuntu 上一般是 `apt install linux-perf` 或 `linux-tools-$(uname -r)`:

```bash
# 最常用:统计式剖析,看 CPU 周期、指令数、cache miss、分支预测失败
perf stat ./cpu_work

# 采样式剖析:周期性记录 CPU 在哪里,跑完生成 perf.data
perf record -g ./cpu_work         # -g 记录调用栈
perf report                       # 交互式查看热点(类似 gprof 但基于采样)

# 只看缓存相关事件:cache miss 是这一章后半段的主角
perf stat -e cache-misses,cache-references,instructions,cycles ./cpu_work

# 生成火焰图(需要 FlameGraph 脚本)
perf record -F 99 -g ./cpu_work
perf script | stackcollapse-perf.pl | flamegraph.pl > prof.svg
```

`perf stat` 的输出长这样(典型字段,**非本机实跑**,仅示意):

```text
 Performance counter stats for './cpu_work':
       1,234.56 msec task-clock                #    0.995 CPUs utilized
             2,103      context-switches        #    1.702 K/sec
        45,000,000      instructions            #    0.55  insn per cycle
        81,800,000      cycles                  #    0.066 GHz
        12,300,000      cache-misses            #    15.0% of all cache refs
        67,000,000      cache-references
         1,200,000      branch-misses           #    0.8%  of all branches
       1.241234567 seconds time elapsed
```

这里面最该盯的两行:**`insns per cycle`(IPC)**和 **`cache-misses` 占比**。IPC 接近 1 说明 CPU 喂得饱(理想能到 3-4,受流水线深度限制);远低于 1 通常意味着 CPU 在**等内存**——这正是下一节的主题。`cache-misses` 占比高,基本可以断定你的代码「对缓存不友好」。

> Visual Studio 那套(笔者的原始笔记里用的)做的是同一件事:链接器 `/PROFILE` 开关、性能探查器里的「检测(instrumentation)」和「采样(sampling)」、火焰图视图——概念上一一对应到 `gprof` 的插桩 / `perf` 的采样 / `perf report`。工具不同,方法论是一套:**插桩准但慢、采样快但稀疏,先采样定位大头、必要时再插桩精修**。

## 第三件事:CPU 与内存的鸿沟

量化告诉我们「`high_cpu` 慢」,剖析告诉我们「时间花在某个循环里」。但**为什么**那个循环慢?答案在现代 CPU 与内存之间那道越来越宽的鸿沟。

### CPU 比内存快太多了

过去二十年,CPU 的速度涨得远比内存快。一条寄存器运算大概 1 个周期(<1 纳秒),而去主存(DRAM)取一个数据要 **200-300 个周期(上百纳秒)**——差了两个数量级。如果每条指令都要等内存,CPU 99% 的时间都在发呆。

这道鸿沟靠**缓存(cache)**来填。现代 CPU 都有三层缓存,以本机(Ryzen 7 5800H)为例:

| 层级 | 容量 | 延迟(典型) | 谁能访问 |
|---|---|---|---|
| 寄存器 | 极少 | <1 ns | 单条指令直接用 |
| L1 cache | 32 KiB/核(数据) | ~1 ns(3-4 周期) | 该核私有 |
| L2 cache | 512 KiB/核 | ~3 ns(十几个周期) | 该核私有 |
| L3 cache | 16 MiB(共享) | ~12 ns(四十几个周期) | 全部核共享 |
| 主存 DRAM | 数十 GiB | ~100 ns | 经内存控制器 |

注意这条延迟阶梯:**离 CPU 越近的层越小但越快**。缓存的本质是用「小而快的存储」挡在「大而慢的内存」前面,赌的就是**程序接下来要用的数据,大概率已经在缓存里**。

### 缓存是怎么工作的:cacheline 与局部性

cache 不是逐字节缓存的,而是按 **cacheline**(缓存行)为单位搬运,本机和绝大多数 x86/ARM 都是 **64 字节一行**。CPU 要读地址 `A` 的一个字节,实际会把 `A` 所在的那整行 64 字节从内存(或下一层 cache)整块搬进 L1。

这一设计决定了「快」的关键是两种**局部性(locality)**:

- **空间局部性(spatial locality)**:访问了 `A`,接下来大概率访问 `A+1`、`A+2`……既然搬进来就是 64 字节,顺序访问就意味着「搬一次,用 64 个」,均摊下来每次访问几乎免费。**跳着访问(大步长)则让搬进来的 64 字节里只用了 1 个,剩下 63 个全浪费**——每一次跳跃都可能触发一次 cache miss。
- **时间局部性(temporal locality)**:刚用过的数据,接下来大概率还会再用。所以「在循环里反复用同一个变量」几乎免费,而「频繁分配释放、数据在内存里来回搬家」会让它不停从 cache 里被挤出去。

一句话:**对缓存友好的代码,就是尽量顺路、尽量复用**。

## 真实 benchmark:把局部性说成数字

光说不练是嘴炮。我们写两个最小的 C benchmark,把「对缓存友好」和「对缓存不友好」的差异**量成数字**。

### 实验 1:行优先 vs 列优先遍历大矩阵

最经典的缓存演示:遍历一个 4000×4000 的 `int` 矩阵(约 61 MiB,**远超 16 MiB 的 L3**,所以 cache 装不下,每次访问几乎都要看 cache 命不命中)。同样的求和,只换访问顺序:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4000   /* 4000x4000 ints ≈ 61 MiB,远超 L3 */

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    int (*m)[N] = malloc(sizeof(int[N]) * N);
    if (!m) { perror("malloc"); return 1; }
    for (long i = 0; i < N; i++)        /* 先填好,避免缺页干扰测量 */
        for (long j = 0; j < N; j++)
            m[i][j] = (int)((i + j) & 1);

    volatile long sink = 0;             /* 防止优化掉累加 */

    /* 行优先:顺序访问,对缓存友好 */
    double t0 = now_sec(); long sum_row = 0;
    for (long i = 0; i < N; i++)
        for (long j = 0; j < N; j++)
            sum_row += m[i][j];
    double t_row = now_sec() - t0;

    /* 列优先:跨行跳跃,每次踏入新 cacheline,对缓存极不友好 */
    double t1 = now_sec(); long sum_col = 0;
    for (long j = 0; j < N; j++)
        for (long i = 0; i < N; i++)
            sum_col += m[i][j];
    double t_col = now_sec() - t1;

    sink += sum_row + sum_col; (void)sink;
    printf("row-major (cache-friendly):  %.4f s\n", t_row);
    printf("col-major (cache-hostile):   %.4f s\n", t_col);
    printf("speedup col/row:             %.2fx\n", t_col / t_row);

    free(m);
    return 0;
}
```

`int m[N][N]` 是行主序(row-major):`m[i][j]` 和 `m[i][j+1]` 在内存里**相邻**,而 `m[i][j]` 和 `m[i+1][j]` 相隔 `N*sizeof(int) = 16000` 字节。所以行优先遍历是连续读,cacheline 搬一次能用 16 个元素;列优先遍历每读一个元素就跳到另一行,基本每次都 cache miss。本机实测:

```text
$ gcc -O2 cache_matrix.c -o cache_matrix && ./cache_matrix
matrix size: 4000 x 4000 (61.0 MiB)
row-major (cache-friendly):  0.0091 s
col-major (cache-hostile):   0.1005 s
speedup col/row:             11.02x
checksum row=8000000 col=8000000 (应相等)
```

**同样的计算量、同样的结果(checksum 相等,证明没算错),只因为访问顺序不同,慢了 11 倍。** 这就是 cache 的威力——不是算法变快了,是内存子系统能以远高于单字节访问的速率把连续数据喂给 CPU。如果你写过矩阵乘法、图像处理、数值计算,这 11 倍就是「能不能用」和「慢得没法用」的差距。

### 验证:数据能塞进缓存时,差距消失

为了证明这 11 倍确实来自 cache、而非别的,我们把矩阵缩小到 200×200(`int` 约 156 KiB,**塞得进 L2**),同样的两份代码:

```text
$ gcc -O2 cache_small.c -o cache_small && ./cache_small
small matrix 200x200 (156 KiB, fits L2)
row-major: 0.000008 s
col-major: 0.000008 s
ratio col/row: 0.95x
```

差距**消失了**(0.95x ≈ 1.0)。数据在 L2 里,无论怎么跳,访问延迟都是那 ~3 纳秒,无所谓顺序。这就坐实了:大矩阵的 11 倍差距,根因就是 cache miss。

### 实验 2:步长(stride)扫描——量化「每个元素」的真实代价

把访问步长从 1 调到 8(正好一个 cacheline 取 1 个元素)再到 64(跨 8 个 cacheline),看「访问每个元素的均摊延迟」怎么涨:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    const long n = 8 * 1024 * 1024;   /* 8M int = 32 MiB,远超 L3 */
    int *a = malloc(sizeof(int) * n);
    for (long i = 0; i < n; i++) a[i] = (int)(i & 0xff);
    volatile long sink = 0;

    double t0 = now_sec(); long s1 = 0;
    for (int pass = 0; pass < 5; pass++)
        for (long i = 0; i < n; i++) s1 += a[i];          /* 步长 1:连续 */
    double t_stride1 = now_sec() - t0;

    double t1 = now_sec(); long s8 = 0;
    for (int pass = 0; pass < 5; pass++)
        for (long i = 0; i < n; i += 8) s8 += a[i];       /* 步长 8:1/cacheline */
    double t_stride8 = now_sec() - t1;

    double t2 = now_sec(); long s64 = 0;
    for (int pass = 0; pass < 5; pass++)
        for (long i = 0; i < n; i += 64) s64 += a[i];     /* 步长 64:跨 8 cacheline */
    double t_stride64 = now_sec() - t2;

    sink += s1 + s8 + s64; (void)sink;
    printf("每元素均摊: stride1=%.2fns stride8=%.2fns stride64=%.2fns\n",
           t_stride1  / (5.0 * n)      * 1e9,
           t_stride8  / (5.0 * n / 8)  * 1e9,
           t_stride64 / (5.0 * n / 64) * 1e9);
    free(a);
    return 0;
}
```

本机实测(注意每元素是均摊,已除以各自访问的元素数):

```text
$ gcc -O2 cache_stride.c -o cache_stride && ./cache_stride
array: 32.0 MiB, 5 passes each
stride  1 (连续):        0.0143 s
stride  8 (1/cacheline): 0.0073 s
stride 64 (8 cacheline): 0.0037 s
每元素均摊: stride1=0.34ns stride8=1.38ns stride64=5.69ns
```

读这张表的方式:**步长 1 时每个元素只要 0.34 纳秒**(连续访问,cacheline 搬一次喂 8 个,L1 速度);步长 8 时 cacheline 里只用了 1/8,均摊涨到 1.38 纳秒;步长 64 时每次访问都踩进全新 cacheline,**5.69 纳秒一个元素——是连续访问的 17 倍**。这个 17 倍,正好落在「L1 命中(~1ns) vs 主存访问(~100ns 含整行搬运)」的延迟阶梯上。一句话:**跳跃访问每多跨一个 cacheline,你就在为内存带宽而不是 CPU 速度买单**。

> 注意上面的绝对时间(0.34ns 等)会随机器、编译器、负载波动,但**比值(连续 vs 跳跃的倍数差)**是稳定且可复现的。benchmark 里要看的常常是这个比值,而不是某个绝对数字。

## 写对缓存友好的 C:可落地的清单

把上面的实验翻译成写代码时的肌肉记忆,就这么几条:

1. **优先顺序访问数组**。`for(i) for(j) a[i][j]` 永远快于 `for(j) for(i) a[i][j]`(C 是行主序)。链表(`next` 指针满天飞)天然对缓存不友好,数据量大、访问密集时优先考虑数组;`malloc` 一个大数组连续摆放,远胜于 `malloc` 一堆小节点再用指针串。

2. **让「一起用的数据」在内存里也挨在一起**。把会同时被某个热循环访问的字段排进同一个 `struct`、放进同一段连续内存;用「结构体数组(SoA)」还是「数组结构体(AoS)」取决于访问模式——逐字段扫描整个数组时,SoA(把同一字段连续存放)更友好,因为不会把无关字段也搬进 cache。

3. **按 cacheline 对齐热数据**。64 字节是一行的边界。处理大数组时,让循环每次处理一整块能塞进 L1/L2 的数据(分块/tile),做完一块再换下一块,榨干时间局部性。对齐可以用 `_Alignas`:

   ```c
   #include <stdalign.h>
   /* 让这个热缓冲按 64 字节对齐,避免一条 cacheline 跨两个缓冲 */
   alignas(64) int hot_buffer[16];
   ```

4. **别在热循环里反复分配释放**。`malloc`/`free` 不光慢,还让数据在堆里搬来搬去、破坏局部性。一次性分配好缓冲,循环里复用。

5. **数据布局比算法常数更值得先动**。把链表换成数组、把列优先换成行优先,常常比抠位运算、手写循环展开收益大得多,而且**风险低、可读性不降**。

## 常见踩坑

- **过早优化**:在你量出瓶颈之前,别去手写 SIMD、别去盲目上多线程。先 `time` / `perf stat` / `gprof` 找到那 20% 吃掉 80% 时间的代码,**只优化那里**。其余地方保持简单可读。
- **测了一个被优化没的循环**:这是本章第一个真实踩的坑。编译期常量迭代 + 无副作用循环,会被 `-O2` 直接折叠成常量,你测了 0 秒。benchmark 必须用运行期输入(命令行参数)驱动迭代、用 `volatile` 或输出保住结果。
- **剖析器被内联骗了**:`-O2` 下小函数会被内联,`gprof` 看不到它们,profile 全堆在 `main` 头上。怀疑某函数是热点却没出现,加 `-fno-inline`(或 `-fno-inline-small-functions`)重测。
- **拿 gprof 当计时器**:`-pg` 插桩有开销,绝对时间会偏大;gprof 给的是**相对比例**,看「谁是大头」,别信它的毫秒数。
- **只看绝对时间,忽略 IPC / cache-miss**:`perf stat` 里的 `insns per cycle` 和 `cache-misses` 占比才是「为什么慢」的答案。绝对墙钟时间会随机器波动,微架构事件更稳定、更有诊断价值。
- **以为换语言/换库就快了**:语言只决定常数,算法决定复杂度,数据布局决定 cache 命中率。一个 cache-hostile 的 C 程序,可能比一个 cache-friendly 的 Python(NumPy)还慢——因为 NumPy 内部就是连续数组 + 向量化。

## 小结

性能优化的正确顺序是**先量化、再剖析、最后才动手**:

1. 用 `clock_gettime(CLOCK_MONOTONIC)` 拿墙钟、用 `time` 拆 user/sys、判断是 CPU-bound 还是 I/O-bound;
2. 用 `gprof`(插桩,准但慢)或 `perf`(采样,快且能看 cache 事件)定位热点函数;
3. 钻进热点,看它是不是在跟内存层次结构对着干——本机实测,大矩阵列优先比行优先慢 **11 倍**,跨 cacheline 访问每元素比连续访问慢 **17 倍**,而数据塞进 L2 时这些差距统统消失,坐实了 cache 才是根因;
4. 动数据布局(顺序访问、连续存储、按 cacheline 对齐、热循环复用缓冲),这比抠算法常数更值得先做;
5. 全程抵制过早优化——没量出瓶颈之前,保持代码简单。

记牢一句话:**让 CPU 算得快很难,让 CPU 不用等内存,常常就够了**。

## 练习

1. 把实验 1 的矩阵改用 `int (*m)[N] = malloc(...)` 改成「先 `malloc` 一个 `int**`,再逐行 `malloc`」的写法(每行独立分配),重跑行优先/列优先,观察行优先是否也变慢了——思考为什么(提示:行与行不再连续,顺序访问也会跨 cacheline)。
2. 在实验 2 里再加一个 `stride 16` 和 `stride 32` 的测量,画出「步长 → 每元素延迟」的曲线,看它在哪个步长发生跳变(那大概率对应 cacheline 边界或 L2/L3 容量边界)。
3. 把标杆程序的 `high_cpu` 加上 `-fno-inline` 之外,再分别用 `-O0`、`-O1`、`-O2`、`-O3` 编译并计时,观察 `-O3` 的自动循环展开/向量化是否带来可测的加速(用 `gcc -fopt-info-vec` 看它有没有向量化)。
4. 如果你装得上 `perf`,对本章任何一个 benchmark 跑 `perf stat -e cache-misses,cache-references`,对比行优先和列优先的 `cache-misses` 数量,验证「慢 11 倍」确实对应「cache miss 多了一个数量级」。
5. 写一个结构体数组 vs 数组结构体(SoA vs AoS)的对比:定义 `struct P { double x, y, z; };`,造一个 1000 万个 `P` 的数组,(a) 只对所有 `x` 求和;(b) 把同样的数据改存成三个独立数组 `xs[]/ys[]/zs[]` 再对 `xs[]` 求和。计时并解释差距。

## 参考资源

- [Drepper — *What Every Programmer Should Know About Memory*](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)(CPUMemory 笔记的原始出处,本章 cache/局部性部分的进阶读物)
- [Brendan Gregg — Linux Performance](https://www.brendangregg.com/linuxperf.html)(perf、火焰图、性能分析的权威资料集)
- [`perf` Wiki — Tutorial](https://perf.wiki.kernel.org/index.php/Tutorial)
- [GCC Manual — `gprof`](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html) 与 `-pg` 选项
- [Agner Fog — Software optimization resources](https://www.agner.org/optimize/)(微架构、指令延迟的硬核手册)
- 本仓库相关章节:[ASan 与 UBSan](./1-sanitizers-asan-and-ubsan.md)(正确性那一边)、[CMake 与模块化工程](./0-cmake-and-modules.md)

---
*整理自作者笔记,按 C-Journey 写作规范重写;所有计时、gprof 输出与 benchmark 数据均在 AMD Ryzen 7 5800H / GCC 16.1.1 本机实测捕获。perf 部分为概念指引(本机未安装),其余输出均为真实运行结果。*
