---
title: "动态内存入门：malloc / calloc / realloc / free 与堆"
description: "这一章带读者走进堆——前面所有变量都在栈上(自动回收),这里讲运行期手动管理的动态内存(第 9 章说的「动态存储期」)。先讲栈 vs 堆:栈小、自动、函数返回即回收;堆大、手动、malloc 要 free 归还、生命周期不受函数限制——这是为什么需要堆(运行期才知大小、数据要活过函数返回)。真跑 malloc 动态数组:int* a = malloc(n*sizeof(int)) 要 5 个 int(运行期才知 n)、必查 == NULL、像数组一样 a[i]=i*i 用(真跑打印 0 1 4 9 16)、用完 free(a)、free 后置 NULL 防悬垂。真跑 calloc(3,sizeof(int)) 清零(真跑 0 0 0,对照 malloc 不清零、读未初始化是 UB)。真跑 realloc 扩容的正确姿势——用 tmp 接 realloc、查 NULL、成功才赋回 a(失败时原 a 仍有效必须手动 free,直接 a=realloc(a,...) 失败会泄漏 + 丢指针),真跑 3 个扩到 5 个数据保留(10 20 30 40 50)。强调用完必 free 否则内存泄漏(下一章用 ASan 抓 UAF/double-free)。全 gcc16+clang22 真跑。"
chapter: 2
order: 6
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第1章：指针是什么（指针装地址、NULL）"
  - "第 9 章：作用域、存储期与 static（自动存储期 vs 动态存储期）"
  - "第 2 章：整型与 sizeof（sizeof 求字节数）"
  - "第 10 章：数组（数组下标用法）"
related:
  - "阶段2·第7章：动态内存的坑（UAF/double-free/泄漏,ASan 抓）"
  - "阶段2·第12章：内存布局与生命周期（栈 vs 堆的内存地图）"
---

# 动态内存入门：malloc / calloc / realloc / free 与堆

## 引言：栈装不下、也留不住，所以要堆

到本章之前，我们所有的变量——`int x`、`int a[10]`、`char s[]`——都住在**栈**上。栈上的变量是**自动存储期**的（第 9 章）：进函数时分配、出函数时自动回收，你不用管。可栈有两个硬限制：**大小有限**（Linux 默认每进程 8 MB 左右，要个几百万元素的大数组就爆栈了），**长度必须编译期定死**（`int a[n]` 里的 `n` 在 C89 必须是常量；变长数组 VLA 第 10 章提过、又栈分配又有坑）。更麻烦的是「生命周期」——栈变量一出函数就没了，没法把一块「函数里要的内存」交给调用者继续用（返回局部数组地址是悬垂指针，第 7 章会真跑）。

**堆**（heap）就是为绕开这两条限制而存在的。它是程序另一块大得多的内存池，由你**手动**管理：`malloc` 去「要」一块、`free` 把它「还」回去——这块内存的生命周期完全由你决定，可以跨越函数调用、可以运行期才知道要多大。第 9 章把这叫**动态存储期**。代价是你得自己负责「用完归还」——忘了 `free` 就是内存泄漏、`free` 了还在用就是 use-after-free，这些都是第 7 章要用 ASan 抓的重点。这一章先讲四个基本函数（`malloc`/`calloc`/`realloc`/`free`）的正经用法，把「要、用、还」这套流程走通，坑留给下一章。

## malloc：要一块堆内存

`malloc`（§7.22.3）向堆申请「指定字节数」的连续内存，返回指向这块内存首字节的指针。它的返回类型是 `void*`（通用指针），赋给 `int*`/`char*` 等具体指针时 C 会自动转换（§6.3.2.3p1，`void*` 可隐式转成任何对象指针）——所以**不用写强转** `(int*)`，这是 C 的地道写法（C++ 才必须强转；在 C 里强转反而会掩盖「忘了 `#include <stdlib.h>` 导致的隐式声明」这种老 bug）。真跑一个动态数组：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int n = 5;
    /* 运行期才知 n:在堆上要 5 个 int;malloc 返回 void*,C 自动转 int* */
    int* a = malloc((size_t)n * sizeof(int));
    if (a == NULL) { /* 必查:分配可能失败(尤其 n 很大时) */
        printf("分配失败\n");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        a[i] = i * i; /* 拿到后像普通数组一样用 */
    }
    printf("动态数组:");
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf("\n");
    free(a); /* 用完归还堆内存,否则泄漏 */
    a = NULL; /* 好习惯:free 后置 NULL,避免后面误用成悬垂指针 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall malloc_array.c -o ma && ./ma
动态数组: 0 1 4 9 16
```

`malloc((size_t)n * sizeof(int))` 算出「5 个 `int` 的字节数」（5×4=20），去向堆要这么大一块，返回的指针赋给 `a`。之后 `a[i] = i*i` 把它**当数组用**——动态数组和普通数组在「下标访问」上没区别（第 10 章的 `p[i]≡*(p+i)` 对 `malloc` 来的指针同样成立）。打印 `0 1 4 9 16`（0、1、4、9、16 是 `i*i`）。

这里有四条纪律，每条都关乎正确性。**第一，必查 `NULL`**：`malloc` 可能失败（比如要的字节数太大、堆耗尽），失败时返回 `NULL`，直接 `a[i]=...` 就是解引用空指针、段错误（第 1 章真跑过），所以拿到指针先 `if (a == NULL)`。**第二，用 `(size_t)` 防溢出**：`n * sizeof(int)` 里 `sizeof(int)` 本就是 `size_t`（无符号），但把 `n` 也 `(size_t)` 转一下更稳，避免 `int` 乘法溢出（第 3 章见过有符号溢出 UB）。**第三，用完 `free(a)`**：堆内存不会自动归还，`free` 把这块内存还回池子、可以被后续 `malloc` 复用；不 `free` 就是泄漏。**第四，`free` 后 `a = NULL;`**：`free` 只归还内存、不改 `a` 的值（`a` 还指着那块已归还的地址），如果后面误写 `a[0]` 就是 use-after-free（第 7 章抓）；把 `a` 置成 `NULL` 后，万一误用至少是段错误（解引用 NULL）而不是悄悄乱写——更容易被发现。开 ASan 复核这个程序，没有任何报错、退出码 0，说明 `malloc` 和 `free` 配对正确、无泄漏：

```text
$ gcc -std=c11 -Wall -fsanitize=address malloc_array.c -o ma_asan && ./ma_asan
动态数组: 0 1 4 9 16
$ echo $?
0
```

## calloc：要一块、并清零

`malloc` 给你的内存**内容是垃圾**（之前用过的残留），直接读它是**未定义行为**（读未初始化）。如果你本来就打算把每个元素都先设成 0（比如一个计数表、一个标志位数组），用 `calloc` 更省事——它分配的同时把每一位置 0：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* calloc 把分配的内存清零;malloc 不清零(内容是垃圾、读它是 UB) */
    int* b = calloc(3, sizeof(int));
    if (b == NULL) {
        return 1;
    }
    printf("calloc 后(已清零):");
    for (int i = 0; i < 3; i++) {
        printf(" %d", b[i]);
    }
    printf("\n");
    free(b);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall calloc_demo.c -o ca && ./ca
calloc 后(已清零): 0 0 0
```

`calloc(3, sizeof(int))` 的参数是「3 个元素、每个 `sizeof(int)` 字节」——和 `malloc(3 * sizeof(int))` 要的总字节数一样，但 `calloc` **额外把这块内存全置 0**，所以打印出来是 `0 0 0`、不是垃圾值。一句话区分：`malloc` 要了不清零（快、但你得自己初始化）、`calloc` 要了顺带清零（省一次循环、但稍慢一点点）。需要清零就 `calloc`、要自己填值就 `malloc`，两者都别忘了 `free`。

## realloc：调整一块已有内存的大小

运行期数据量常常会变——数组先要 3 个、后来发现要 5 个。`realloc` 用来**调整一块已分配内存的大小**：可能原地扩、也可能另找一块大的、把原数据拷过去、再释放旧的。它有个容易栽的坑，所以正确的姿势得用「临时指针」接：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* a = malloc(3 * sizeof(int)); /* 先要 3 个 */
    if (a == NULL) {
        return 1;
    }
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;

    /* 想扩到 5 个:用 tmp 接 realloc,成功再赋回 a */
    int* tmp = realloc(a, 5 * sizeof(int));
    if (tmp == NULL) { /* 失败时原 a 仍有效、必须手动 free */
        free(a);
        return 1;
    }
    a = tmp;       /* 成功:a 指向新的 5 元素块,原 3 个数据被保留 */
    a[3] = 40;
    a[4] = 50;

    printf("扩容后:");
    for (int i = 0; i < 5; i++) {
        printf(" %d", a[i]);
    }
    printf("\n");
    free(a);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall realloc_basic.c -o rb && ./rb
扩容后: 10 20 30 40 50
```

先 `malloc` 3 个 `int` 填了 `10 20 30`，然后 `realloc(a, 5 * sizeof(int))` 想扩到 5 个——扩容成功的话，**原来那 3 个数据原封不动保留**，新多出来的位置可以接着填（`a[3]=40`、`a[4]=50`），打印 `10 20 30 40 50`。

关键是为什么**不能**直接写 `a = realloc(a, ...)`：因为 `realloc` **失败时返回 `NULL`、且原指针 `a` 指向的内存仍有效**（没被释放）。如果直接 `a = realloc(a, ...)`，一旦失败 `a` 就被 `NULL` 覆盖、而原来那块内存的地址丢了——既没法再访问、也没法 `free`，**内存泄漏 + 数据丢失**。所以标准姿势是 `int* tmp = realloc(a, ...); if (tmp == NULL) { free(a); /* 失败处理 */ } else { a = tmp; }`：用 `tmp` 接住、判成功、成功才覆盖 `a`，失败时原 `a` 还在、可以继续用或 `free` 掉。这个 `tmp` 模式是写动态数组（`std::vector` 那种）的标配，务必记牢。

## free 与内存泄漏

堆内存的规矩一句话：**每一块 `malloc`/`calloc`/`realloc`（指 `realloc` 新分配的那块）对应的内存，都必须被 `free` 一次、且仅一次**。违反这条规矩的两个常见错误，是第 7 章的主角——**忘了 `free`（内存泄漏）**：那块内存一直占着、程序运行越久堆越少；**`free` 了还在用（use-after-free）或 `free` 两次（double-free）**：行为未定义、多半崩溃或被 ASan 抓。本章只要记住「用完 `free`、`free` 后置 `NULL`、`realloc` 用 `tmp` 模式」这三条，就能把动态内存的基本流程走对。下一章我们专门把这些坑逐个真跑出来、用 ASan 当场抓——那是动态内存真正容易出事的地方。

## 小结

栈变量小且自动回收、长度编译期定死；**堆**（§7.22.3）给你大块的、运行期才知大小的、生命周期不受函数限制的内存，代价是**手动管理**（动态存储期，第 9 章）。`malloc(n*sizeof(T))` 向堆要 `n` 个 `T` 的连续内存、返回 `void*`（C 自动转成具体指针、不需强转，§6.3.2.3p1），**必查 `NULL`**（失败返回 NULL、不查就解引用空指针段错误），拿到后**像数组一样 `a[i]` 用**（真跑动态数组 `0 1 4 9 16`）。`calloc(n, sizeof(T))` 是 `malloc` + **清零**版（真跑 `0 0 0`，对照 `malloc` 不清零、读未初始化是 UB）。`realloc(ptr, newSize)` 调整已有块大小、原数据保留（真跑 3 扩到 5 得 `10 20 30 40 50`），但**必须用 `tmp` 模式**——`int* tmp = realloc(a,...); if(tmp){a=tmp;}`，因为失败时 `realloc` 返回 `NULL` 而原 `a` 仍有效，直接 `a=realloc(a,...)` 失败会泄漏 + 丢指针。规矩:每块分配必须 `free` 一次且仅一次,**用完 `free`、`free` 后置 `NULL` 防悬垂**（ASan 复核无泄漏退出码 0）。忘了 `free` 是泄漏、`free` 后还用是 UAF、`free` 两次是 double-free——这三个坑下一章用 ASan 逐个抓。栈 vs 堆的内存地图、和它们各自的生命周期,留到第 12 章用一张全景图收口。

## 参考资源

- ISO/IEC 9899:2011 §7.22.3（内存管理函数:`malloc`/`calloc`/`realloc`/`free`）、§6.3.2.3p1（`void*` 与其他对象指针的互转）、§7.22.3 几条（分配失败返回空指针、`realloc` 失败时原指针仍有效）
- K. N. King《C Programming: A Modern Approach》第 17 章（动态内存分配:`malloc`/`free`、动态数组、`realloc`）
- Robert C. Seacord《Effective C》第 6 章（`malloc`/`free` 约定、检查 `NULL`、`realloc` 的正确用法）
- 第 9 章：作用域、存储期与 `static`（自动存储期 vs 动态存储期）、第 2 章：`sizeof`、第 10 章：数组（`p[i]≡*(p+i)`）、阶段2·第1章：指针（`NULL`、`void*`）
- 阶段2·第7章：动态内存的坑（UAF/double-free/泄漏,ASan 抓）、第12章：内存布局（栈 vs 堆全景图）、阶段 0·第10章：Sanitizer 门禁（ASan/LeakSanitizer）
