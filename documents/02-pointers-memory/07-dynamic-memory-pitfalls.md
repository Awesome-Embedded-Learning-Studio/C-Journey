---
title: "动态内存的坑：UAF / double-free / 越界 / 泄漏，ASan 逐个抓"
description: "第 6 章说「每块分配必须 free 一次且仅一次」,这一章把违反它的每种错误真跑出来,用 ASan(AddressSanitizer)当场抓——这是动态内存的主战场。真跑 use-after-free:free(p) 后还读 *p,gcc 编译期 -Wuse-after-free 警告 + ASan 报 heap-use-after-free(READ of size 4)、ABORTING。真跑 double-free:同一块 free 两次,ASan 报 attempting double-free。真跑堆越界:malloc(3*sizeof(int)) 后写 a[3],ASan 报 heap-buffer-overflow(WRITE of size 4)。真跑内存泄漏:malloc 后忘 free,程序结束时 ASan 内置的 LeakSanitizer 报 detected memory leaks(400 byte(s) leaked in 1 allocation(s))。强调这些全是 UB(可能崩、可能静悄悄乱写、可能被 ASan 抓),所以工程里 -fsanitize=address 是动态内存代码的必备护栏;顺带说明「读 malloc 未初始化的内存」ASan 抓不到、要换 MSan。全 gcc16 真跑。"
chapter: 2
order: 7
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第6章：动态内存入门（malloc/free/calloc/realloc 用法、每块 free 一次）"
  - "阶段 0·第10章：Sanitizer 门禁（UBSan/ASan 的 recover/abort 区别）"
  - "阶段2·第1章：指针是什么（NULL、悬垂指针概念）"
related:
  - "阶段2·第6章：动态内存入门（正确用法）、第12章：内存布局（栈 vs 堆、泄漏的内存地图）"
  - "阶段 0·第10章：Sanitizer 门禁（ASan 抓栈越界/UAF/double-free/leak 的全景）"
---

> 🟡 状态:待审核(2026-06-30)

# 动态内存的坑：UAF / double-free / 越界 / 泄漏，ASan 逐个抓

## 引言：第 6 章那条规矩的「违反面」

第 6 章给了一条规矩：**每块 `malloc`/`calloc`/`realloc` 的内存，必须被 `free` 一次、且仅一次**。这一章把违反这条规矩（以及它周边）的每种典型错误，逐个真跑出来——`free` 了还用、`free` 两次、堆上越界、忘了 `free`。它们全是**未定义行为**：可能当场崩、可能「静悄悄」乱写别人内存埋下地雷、也可能什么都不发生。光靠人眼审查很难揪，好在 ASan（AddressSanitizer，阶段 0 第 10 章见过）能把这四种错误**精确分类、当场抓现行**——所以写动态内存的代码，`-fsanitize=address` 是必备护栏，这一章就是它的主场。注意 ASan 报错时那些地址、进程号每次跑都不一样，但「错误类型」(`heap-use-after-free`/`double-free`/`heap-buffer-overflow`/泄漏)是稳定的。

## use-after-free：`free` 了还在用

最常见也最阴险的坑——`free(p)` 把内存还回堆了，可 `p` 这个指针变量还指着那块旧地址（`free` 不清指针），后面要是再 `*p` 就是「用了已经归还的内存」：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* p = malloc(sizeof(int));
    *p = 42;
    free(p);                  /* 内存归还堆 */
    printf("*p = %d\n", *p);  /* UB:free 后还读(use-after-free) */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall uaf.c -o uaf
uaf.c:8:5: warning: pointer 'p' used after 'free' [-Wuse-after-free]
    8 |     printf("*p = %d\n", *p);  /* UB:free 后还读(use-after-free) */
      |     ^~~~~~~~~~~~~~~~~~~~~~~
uaf.c:7:5: note: call to 'free' here
    7 |     free(p);                  /* 内存归还堆 */
      |     ^~~~~~~~~
$ gcc -std=c11 -Wall -fsanitize=address uaf.c -o uaf_asan && ./uaf_asan
==81558==ERROR: AddressSanitizer: heap-use-after-free on address 0x74e3bfbe0010
READ of size 4 at 0x74e3bfbe0010 thread T0
SUMMARY: AddressSanitizer: heap-use-after-free (.../uaf_asan+0x1252) in main
==81558==ABORTING
```

两个护栏都响：**gcc 的 `-Wuse-after-free` 在编译期就警告**（当 `free` 和后续 `*p` 在同一函数里、编译器看得见时，它就能抓），告诉你「`p` 在第 7 行 `free` 之后、第 8 行又被用了」；开了 ASan 则在运行期精确报 `heap-use-after-free`、`READ of size 4`（读了 4 字节，一个 `int`），进程 abort。UAF 为什么阴险？因为那块内存归还堆后**可能已经被别的 `malloc` 复用了**，你 `*p` 读到的是别人新写进去的数据、毫无意义；更糟的是你 `*p = ...` 写进去、把别人的数据搅乱了，bug 隔很远才发作。第 6 章教的 `free(p); p = NULL;`（置空）就是防这个——置空后万一误用，至少是解引用 NULL 段错误、立刻暴露，而不是悄悄乱写。

## double-free：同一块 `free` 两次

和 UAF 是一对：`free` 过的指针又被 `free` 一次。堆管理器看到「同一块被归还两次」会把它内部的数据结构搞乱（严重的能导致后续 `malloc` 崩、甚至被利用来执行任意代码），是 UB：

```c
#include <stdlib.h>

int main(void) {
    int* p = malloc(sizeof(int));
    free(p);
    free(p); /* UB:同一块 free 两次(double-free) */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -fsanitize=address double_free.c -o df && ./df
==81567==ERROR: AddressSanitizer: attempting double-free on 0x6d96c79e0010 in thread T0:
freed by thread T0 here:
SUMMARY: AddressSanitizer: double-free (.../df+0x1196) in main
==81567==ABORTING
```

ASan 报 `attempting double-free`，还贴心地告诉你这块内存**之前在哪 `free` 过**（`freed by thread T0 here:`）——这对定位「第二次 `free` 是手滑、第一次在哪」特别有用。`free(p); p = NULL;` 同样能防这个坑：置空后，即使再 `free(p)`，`free(NULL)` 是**合法的空操作**（标准规定 `free(NULL)` 什么都不做），不会出错。所以「`free` 后置 `NULL`」一举防住 UAF 和 double-free 两个坑，是动态内存代码的基本卫生习惯。

## 堆越界：`malloc` 来的内存也越界

第 10 章我们在栈数组上看过越界（`a[5]` 对 5 元素数组），堆内存同样会越界——`malloc(3*sizeof(int))` 只给了 3 个 `int`，写 `a[3]` 就溢出了：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* a = malloc(3 * sizeof(int)); /* 只要了 3 个 int */
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[3] = 99; /* 越界写第 4 个位置,UB(heap-buffer-overflow) */
    printf("a[3] = %d\n", a[3]);
    free(a);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -fsanitize=address heap_overflow.c -o ho && ./ho
==81576==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6e3b0dde001c
WRITE of size 4 at 0x6e3b0dde001c thread T0
SUMMARY: AddressSanitizer: heap-buffer-overflow (.../ho+0x12d8) in main
==81576==ABORTING
```

ASan 报 `heap-buffer-overflow`、`WRITE of size 4`（写了 4 字节）——和栈越界的 `stack-buffer-overflow`（第 10 章真跑过）是同源不同位置的报告，ASan 在每块 `malloc` 内存周围也埋了「红区」，越界一写就踩到、当场被抓住。堆越界的可怕之处和 UAF 类似：那块被踩的内存可能是**别的 `malloc` 来的数据**，你写坏的是不相干的别人数据，bug 表现得极其诡异、和出问题的代码隔着十万八千里。这就是为什么 `for (i=0; i<n; i++)` 里的边界必须严格 `< n`、为什么动态数组的「容量 vs 长度」要分清——第 6 章那种 `realloc` 扩容时算错新大小，就是堆越界的常见源头。

## 内存泄漏：忘了 `free`

最后一种不算「崩」，但和「内存正确性」同样要命——**忘了 `free`**：申请了内存、用完了不归还，那块内存就一直被占着。短命程序无所谓（进程结束操作系统统一回收），但长期运行的服务（Web 服务器、数据库、守护进程）一旦泄漏，内存占用会一路涨、直到耗尽被操作系统杀掉。ASan 内置的 **LeakSanitizer**（LSan）能在程序结束时扫描「还有没有指针指向堆里未释放的块」，报告泄漏：

```c
#include <stdlib.h>

int main(void) {
    int* p = malloc(100 * sizeof(int)); /* 忘了 free */
    p[0] = 1;
    (void)p;
    return 0; /* 程序结束时,ASan 内置的 LeakSanitizer 会报告泄漏 */
}
```

```text
$ gcc -std=c11 -Wall -fsanitize=address leak.c -o lk && ./lk
==81589==ERROR: LeakSanitizer: detected memory leaks
SUMMARY: AddressSanitizer: 400 byte(s) leaked in 1 allocation(s).
```

LSan 报 `detected memory leaks`、`400 byte(s) leaked in 1 allocation(s)`——`400` 正好是 `100 × sizeof(int)` 那一次 `malloc` 忘 `free` 的。LSan 是在程序正常退出时才检查的（所以即使泄漏，程序本身的逻辑不会崩），它给出的「泄漏字节数 + 分配次数」能帮你快速判断有没有漏、漏多少。防泄漏的根本是「谁 `malloc` 谁 `free`」配对清晰——尤其函数里分配、返回指针给调用者的场合，要把「谁负责 `free`」写进注释/文档（比如 `strdup` 返回的字符串要调用者 `free`），否则很容易两边都以为对方会 `free`、结果谁都没 `free`。

## ASan 抓不到的：读未初始化

最后说一个 ASan **管不到**的坑，免得你误以为开了 ASan 就万事大吉——**读 `malloc` 来的未初始化内存**（第 6 章提过 `malloc` 不清零）。读未初始化的值是 UB（读到的是垃圾），但 ASan 只管「越界/UAF/double-free/泄漏」这些**地址类**错误、不管「值未初始化」——它看 `*p` 那个地址是合法分配给你的，就放行了。抓这种要换 **MSan**（MemorySanitizer，`-fsanitize=memory`），它专门追踪「每个字节有没有被写过」。但 MSan 和 ASan 不能同时开（会冲突），所以工程实践是：日常调试开 ASan（覆盖面广）、对「怀疑读未初始化」的场景单独换 MSan 跑一次。最省事的还是从源头避免——要清零就用 `calloc`、要填值就老老实实循环赋值，别留任何「`malloc` 完直接读」的口子。

## 小结

第 6 章的「每块分配 `free` 一次且仅一次」规矩，违反面就是这一章的四大坑——全是 UB（可能崩、可能静悄悄乱写、可能被 ASan 抓），所以写动态内存代码必备 `-fsanitize=address`。**use-after-free**（`free` 后还用 `*p`）：gcc 编译期 `-Wuse-after-free` 警告 + ASan `heap-use-after-free`/`READ of size 4`，阴险在「内存已还堆、可能被别人复用、读到/写坏别人的数据」；**double-free**（`free` 两次）：ASan `attempting double-free` 还会指出之前在哪 `free` 过；**堆越界**（`malloc` 的内存写超界）：ASan `heap-buffer-overflow`（和第 10 章栈越界 `stack-buffer-overflow` 同源不同位置，`malloc` 周围也埋了红区）。这三类 ASan 都让进程 ABORTING（非 0 退出）。`free(p); p = NULL;`（置空）一举防住 UAF 和 double-free——因为 `free(NULL)` 是合法空操作。**内存泄漏**（忘 `free`）由 ASan 内置的 LeakSanitizer 在程序退出时报 `detected memory leaks` + `N byte(s) leaked`（真跑 400 byte 泄漏），短命程序无害、长期服务致命，防法是「谁分配谁释放」配对清晰。最后记住 ASan **抓不到「读未初始化」**（只管地址类错误），那种要换 MSan（`-fsanitize=memory`，与 ASan 互斥），最省事是从源头用 `calloc` 或主动填值。这一章到第 6 章，动态内存的「正反两面」就齐了；栈 vs 堆的整张内存地图、这些坑在内存里的相对位置，留到第 12 章用全景图收口。

## 参考资源

- ISO/IEC 9899:2011 §7.22.3（内存管理:`free` 后指针值不确定、使用它 UB；`free(NULL)` 合法空操作；`free` 同一块两次 UB）、§6.5.6（越界 UB）
- K. N. King《C Programming: A Modern Approach》第 17 章（动态内存的悬垂指针、内存泄漏）、第 18 章（UAF/double-free 惯例）
- Robert C. Seacord《Effective C》第 6 章（动态内存常见错误:UAF/double-free/泄漏、ASan 与 MSan 的分工）
- LLVM AddressSanitizer 文档（heap-use-after-free / double-free / heap-buffer-overflow / LeakSanitizer 各类报告的含义）
- 阶段2·第6章：动态内存入门（正确用法）、阶段 0·第10章：Sanitizer 门禁（ASan/UBSan/MSan 全景、recover/abort）、第10章：数组（栈越界 stack-buffer-overflow 对照）
- 阶段2·第12章：内存布局与生命周期（栈 vs 堆地图、泄漏的长期危害）
