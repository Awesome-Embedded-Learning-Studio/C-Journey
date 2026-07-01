---
title: "二分查找:有序数组、O(log n)、bsearch"
description: "阶段3·第 11 章。第 9/10 章把数组排好了序,这一章拿排序换速度——在有序数组里找一个数,线性扫描要 O(n)、二分查找每次砍一半只要 O(log n),一百万个元素顶多 20 次比较。前提就一条铁律:数组必须先排好序,无序数组里二分毫无意义。本章手写两版:迭代版 while(lo<=hi) + mid=lo+(hi-lo)/2(那个 mid 用 lo+(hi-lo)/2 而不是 (lo+hi)/2,正是阶段1 第3 章有符号溢出 UB 的经典坑——lo+hi 可能溢出)、递归版同一套逻辑递归下去。再用标准库 bsearch(§7.22.5.1,和阶段2 第9 章 qsort 同一套比较函数指针+void* 泛型套路)做对照,在有序 {1,3,4,5,7,8,9} 里查 7 命中(下标 4)、查 6 落空(返回 -1 / NULL),三个程序结果一字不差。全 gcc16+clang22 真跑。"
chapter: 3
order: 11
tags:
  - host
  - data-structures
  - algorithm
difficulty: intermediate
reading_time_minutes: 12
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 10 章:数组(下标)、阶段3·第9/10章:排序(先排好序才能二分)"
  - "阶段2·第9章:函数指针(bsearch 的比较函数,呼应 qsort)、阶段1·第3章:整型溢出(lo+hi 溢出坑)"
related:
  - "阶段3·第8章:哈希表(O(1) 查找对照二分 O(log n))、第12章:大 O(O(log n) 推导)"
---

# 二分查找:有序数组、O(log n)、bsearch

## 引言:排序换速度

第 9 章我们手写了三种 O(n²) 排序、第 10 章用快排和归并把排序压到了 O(n log n)。可排序这东西,排完不是拿来好看的——你排好序,图的往往是后面这一件事:**快速查一个数在不在里面**。如果数组是乱的,查一个数你只能从头扫到尾,运气差就要扫 n 个元素,这就是 O(n) 的线性查找。可一旦数组**已经排好序**,你手上有了一件神器:每次拿中间那个元素比一下,就能直接砍掉一半不可能的范围——这就是**二分查找**(binary search),把 O(n) 的查找压到 **O(log n)**。

O(log n) 是个什么概念?log₂ 1000000 ≈ 20,也就是说**一百万个元素,最多 20 次比较就能给出答案**;线性查找平均得比较五十万次。这就是排序换来的速度红利——你先花 O(n log n) 排一次序,之后无数次查找每次都只要 O(log n),数据量越大越值。这一章我们先把二分查找手写两遍(迭代版 + 递归版),再换成标准库的 `bsearch`,看它们在同一份有序数组上给出的答案是不是一字不差。

但二分查找有个**铁律一样的前提,先别急着写代码**:它只对**有序数组**有意义。无序数组里用二分,结果纯属瞎蒙——中间那个元素比 key 大,不代表左边就全是小的,你「砍掉一半」砍掉的可能正是答案。所以每一次二分之前都得先确认数组是升序(或降序,看你比较函数怎么写)排好的;第 9/10 章的排序,正是二分的前置工序。

## 迭代版:每次砍一半

二分查找的迭代版,思路朴素到一句话:**维护一个 `[lo, hi]` 的搜索区间,每次拿中间元素 `a[mid]` 比——相等就找到、key 更大就扔掉左半、key 更小就扔掉右半,区间每轮砍一半,直到区间空了或命中为止**。先上代码,再拆坑:

```c
#include <stdio.h>

/* 在已升序排好的数组 a[0..n-1] 里找 key。
   找到返回下标,找不到返回 -1。 */
static int bsearch_iter(const int* a, int n, int key) {
    int lo = 0;
    int hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2; /* 防 lo+hi 整数溢出 */
        if (a[mid] == key) {
            return mid;
        }
        if (a[mid] < key) {
            lo = mid + 1; /* key 在右半,扔掉左半(含 mid) */
        } else {
            hi = mid - 1; /* key 在左半,扔掉右半(含 mid) */
        }
    }
    return -1;
}

int main(void) {
    int a[] = {1, 3, 4, 5, 7, 8, 9};
    int n = (int) (sizeof(a) / sizeof(a[0]));

    int idx7 = bsearch_iter(a, n, 7); /* 命中 */
    int idx6 = bsearch_iter(a, n, 6); /* 落空 */

    printf("数组: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    printf("查 7 -> 下标 %d\n", idx7);
    printf("查 6 -> %d\n", idx6);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra bsearch_iter.c -o bi && ./bi
数组: 1 3 4 5 7 8 9 
查 7 -> 下标 4
查 6 -> -1
```

数组 `{1, 3, 4, 5, 7, 8, 9}` 是升序的(这条前提是后面一切成立的基础)。查 7 的过程:区间 `[0, 6]`、`mid = 3`、`a[3] = 5 < 7` → 扔左半(含 5)、`lo = 4`;新区间 `[4, 6]`、`mid = 5`、`a[5] = 8 > 7` → 扔右半(含 8)、`hi = 4`;新区间 `[4, 4]`、`mid = 4`、`a[4] = 7 == 7` → 命中,返回下标 4。整个过程只比较了 3 次,线性扫到第 5 个才找到 7 要比较 5 次——量级一旦上去,差距是天壤之别。查 6 时区间一路砍到 `[5, 4]`(`lo > hi`),循环退出,返回 -1 表示「不在表里」。clang 22 跑出同样结果,逐字一致。

### 那个 mid 为什么不写 (lo+hi)/2

这一行 `int mid = lo + (hi - lo) / 2;` 是二分查找最经典的坑,值得单独拎出来讲。直觉上「中间下标」就该是 `(lo + hi) / 2`,数学上完全对。但在 C 里,**`lo + hi` 是 `int` 加 `int`,当两个下标都很大时可能溢出**——比如 `lo = 1500000000`、`hi = 2000000000`,`lo + hi = 3500000000` 已经超过 `INT_MAX`(2147483647),有符号整数溢出是 UB(ISO/IEC 9899:2011 §6.5 第 5 段,阶段 1 第 3 章真跑过 UBSan 当场抓),UBSan 会在这一行报 `signed integer overflow`。溢出之后那个负的下标再拿去访问 `a[mid]`,轻则读到数组外的垃圾、重则直接段错误。这正好是阶段 1 第 3 章「整型溢出」埋的伏笔在真实算法里的回响——**有符号溢出 UB 不是教科书吓唬人的,二分查找这道经典题就栽在这儿**。

`lo + (hi - lo) / 2` 在数学上和 `(lo + hi) / 2` 完全等价,但 `hi - lo` 永远是非负数(只要循环里维持 `lo <= hi`)、且不超过数组长度,绝不会溢出,于是整个表达式稳如老狗。这就是「换一种写法避开 UB」的标准操作——你不需要去判断「这里会不会溢出、加个 if 检查」,直接写一个不会溢出的等价形式就一劳永逸。这个坑 Google 的 Joshua Bloch 在 2006 年专门写过一篇博客回忆——他写的 JDK `Arrays.binarySearch` 里 `(low + high) / 2` 在大数组上就溢出了,正是同一个问题。

### 边界:lo <= hi 还是 lo < hi

另一处新手翻车点是循环条件写 `lo <= hi` 还是 `lo < hi`,以及对应 `mid` 命中后怎么动边界。我们这版用「闭区间 `[lo, hi]`」写法:循环条件 `lo <= hi`(区间里至少还有元素就继续),`mid` 比完之后无论走哪条分支都把 `mid` 自己也扔掉——`lo = mid + 1` 或 `hi = mid - 1`,因为 `a[mid]` 既然不等于 key,它就不可能是答案,没必要留在下一轮的区间里。这一对选择是配套的:**闭区间 + `lo <= hi` + `mid ± 1`**,逻辑自洽,绝不会死循环(区间每轮严格缩小),也绝不会漏查 `mid` 元素。

你也会看到另一种「半开区间 `[lo, hi)`」写法:循环条件 `lo < hi`、`hi = mid`(不 -1),Koenig 的《C Traps and Pitfalls》第 8 章给的二分答案就是这套(他用 `lo < hi`、区间 `[0, n)`)。两种都正确,关键是**条件、区间约定、边界动法三者要配套**,不能混着写——比如你写 `lo <= hi` 却用 `hi = mid`(不 -1),当 `lo == hi == mid` 且 `a[mid] > key` 时,`hi = mid` 会让区间原地不动,直接死循环。所以新手别两边抄——选定一套区间约定,从条件到边界动法都按那套来。本章全程走闭区间 `[lo, hi]` 这套,和大多数教材一致、好讲。

找不到时返回 -1 是约定俗成的「哨兵值」——用 -1 表示「这不是一个合法下标」(下标从 0 起,合法下标都是非负的)。调用方拿到返回值必须先判 `if (idx >= 0)` 再用 `a[idx]`,否则下标 -1 就是越界访问 UB(呼应第 10 章数组越界)。

## 递归版:同一套逻辑换种壳

把上面那个 `while` 循环换成「函数自己调自己」,就是二分查找的递归版——逻辑一字不差,只是区间 `[lo, hi]` 不再用循环变量维护、而是通过函数参数往下传:

```c
#include <stdio.h>

/* 递归二分:在升序数组 a[lo..hi] 里找 key,返回下标或 -1。 */
static int bsearch_recur(const int* a, int lo, int hi, int key) {
    if (lo > hi) {
        return -1; /* 区间空,找不到 */
    }
    int mid = lo + (hi - lo) / 2;
    if (a[mid] == key) {
        return mid;
    }
    if (a[mid] < key) {
        return bsearch_recur(a, mid + 1, hi, key); /* 右半 */
    }
    return bsearch_recur(a, lo, mid - 1, key); /* 左半 */
}

int main(void) {
    int a[] = {1, 3, 4, 5, 7, 8, 9};
    int n = (int) (sizeof(a) / sizeof(a[0]));

    int idx7 = bsearch_recur(a, 0, n - 1, 7);
    int idx6 = bsearch_recur(a, 0, n - 1, 6);

    printf("数组: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    printf("查 7 -> 下标 %d\n", idx7);
    printf("查 6 -> %d\n", idx6);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra bsearch_recur.c -o br && ./br
数组: 1 3 4 5 7 8 9 
查 7 -> 下标 4
查 6 -> -1
```

`lo > hi` 是递归的**基线**(base case)——区间空了说明 key 不在表里,返回 -1 终止递归。命中时返回 `mid`、不命中时根据 key 在左半还是右半,把缩小的区间递归下去。每个 `return bsearch_recur(...)` 都是「尾递归」(tail recursion)——递归调用是函数最后一步,所以这版递归的语义和迭代版几乎可以一一对应:你可以把迭代版每一次 `while` 循环体,理解成递归版的一次函数调用。

但递归版有个迭代版没有的小代价:**每递归一层就压一层栈帧**(lo、hi、key、mid 这些局部变量各占一份栈空间),最深 log₂ n 层。查一百万个元素大约 20 层栈、对现代栈空间(默认 8 MB)毫无压力,但极端情况(数据量巨大或栈空间受限的嵌入式环境)要注意——这也是为什么大多数库实现走迭代版。函数式风格喜欢递归好读,工程实战里迭代版更主流,两者你都得看得懂。

## 标准库 bsearch:比较函数 + void*

写到这儿你会发现,手写二分每次都要自己维护 lo/hi/mid、自己处理 -1,挺啰嗦的。C 标准库其实早就给你备好了 `<stdlib.h>` 里的 `bsearch`(ISO/IEC 9899:2011 §7.22.5.1),它的签名长这样:

```c
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*));
```

这一串看着吓人,其实拆开很面熟——它和阶段 2 第 9 章的 `qsort` 是**孪生兄弟**,同一套「void* 泛型 + 比较函数指针」的套路:`key` 指向要找的那个值、`base` 是数组首地址、`nmemb` 是元素个数、`size` 是每个元素的字节数(这两项用 `size_t` 是因为它们表示大小、非负,呼应第 2 章)、`compar` 是你提供的比较函数指针。比较函数的约定也和 `qsort` 一模一样:**传给它两个 `const void*`(第一个是 key、第二个是数组元素),它返回负/零/正**表示 key 小于/等于/大于元素。King《C Programming: A Modern Approach》第 17 章和第 26 章对这套接口有完整讲解——`bsearch` 虽然标准没强制,但几乎都就是用二分查找实现的(查找 1000 个元素最多 10 次比较、100 万个最多 20 次)。

来一份真跑版,在同一个数组里查 7 和 6,和手写版对照:

```c
#include <stdio.h>
#include <stdlib.h>

/* bsearch 要的比较函数:key 与元素比,返回负/零/正。
   和阶段2 第9章 qsort 的比较函数同一套套路。 */
static int cmp_int(const void* p, const void* q) {
    int a = *(const int*) p;
    int b = *(const int*) q;
    return (a > b) - (a < b); /* 防 a-b 异号大数溢出 */
}

int main(void) {
    int a[] = {1, 3, 4, 5, 7, 8, 9};
    size_t n = sizeof(a) / sizeof(a[0]);

    int key7 = 7;
    int key6 = 6;
    /* bsearch 返回的是「匹配元素的指针」或 NULL,不是下标 */
    int* found7 = (int*) bsearch(&key7, a, n, sizeof(a[0]), cmp_int);
    int* found6 = (int*) bsearch(&key6, a, n, sizeof(a[0]), cmp_int);

    printf("数组: ");
    for (size_t i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    /* 拿指针减数组首地址,把指针还原成下标 */
    printf("查 7 -> 指针 %p, 下标 %ld\n", (void*) found7, found7 ? (long) (found7 - a) : -1L);
    printf("查 6 -> 指针 %p\n", (void*) found6);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra bsearch_stdlib.c -o bs && ./bs
数组: 1 3 4 5 7 8 9 
查 7 -> 指针 0x7fff99efebb0, 下标 4
查 6 -> 指针 (nil)
```

查 7 得到指针、还原成下标 4(和手写版一字不差);查 6 落空、返回 NULL(在 `%p` 里打成 `(nil)`)。下标 4 这一项三个程序全对得上——`bsearch` 确实是用二分实现的。注意两点:`bsearch` 返回的是**指向匹配元素的指针**、不是下标,要下标就 `指针 - 数组首地址`(指针减法得 `ptrdiff_t`,呼应阶段 2 第 2 章);找不到返回的是 **NULL**(不是 -1,因为返回类型是 `void*`,没法塞 -1),所以判空用 `if (found != NULL)` 而不是 `if (found >= 0)`。

那个比较函数 `cmp_int` 里有第三个值得说的细节:`return (a > b) - (a < b);`。为什么不直接 `return a - b;`?因为 `a - b` 在「a 很大正、b 很大负」时同样会溢出(`a - b` 远超 `INT_MAX`),又是 UB、又可能在比较函数里给出错误符号——这正是第 10 章 `quicksort` 用过的同一个坑、同一个写法。`(a > b) - (a < b)` 是纯比较、不参与算术,不会溢出,稳定返回 -1/0/1。这种「跨章节复用的同一道坑、同一个修法」,正是 C 里值得反复打磨的手感。

ASan + UBSan 把三个程序都过了一遍(`-fsanitize=address,undefined`),退出码全 0、无越界无泄漏——这是「代码块即真跑版、而且干净」的硬证据,不是嘴上说说。

## 小结

二分查找是把有序数组上的线性查找 O(n) 砍到 O(log n) 的神器(一百万个元素最多 20 次比较),代价就一条铁律——**数组必须先排好序**(第 9/10 章的排序是二分的前置工序,无序数组里二分纯属瞎蒙)。手写两版逻辑同一套:维护搜索区间、每次拿 `a[mid]` 比一下、扔掉不可能的一半,迭代版 `while (lo <= hi)` + `mid = lo + (hi - lo) / 2` + `lo = mid + 1` / `hi = mid - 1`(闭区间配套),递归版把区间当参数往下传、`lo > hi` 当基线。两个坑就地记牢:第一,**mid 必须写 `lo + (hi - lo) / 2` 而不是 `(lo + hi) / 2`**,后者在大下标上 `lo + hi` 溢出是 UB(§6.5p5,呼应阶段 1 第 3 章,JDK 的 `Arrays.binarySearch` 都栽过这个坑);第二,**循环条件、区间约定、边界动法三者必须配套**(闭区间 `[lo, hi]` 就配 `lo <= hi` + `mid ± 1`,半开区间 `[lo, hi)` 就配 `lo < hi` + `hi = mid`,不能混着抄)。找不到时手写版返回 -1(下标非负的哨兵值)、标准库 `bsearch` 返回 NULL(`void*` 没法塞 -1),调用方都得先判再 `a[idx]` / `*found`,否则越界或解引用 NULL 都是 UB。标准库 `bsearch`(§7.22.5.1)和阶段 2 第 9 章的 `qsort` 是孪生兄弟,同一套 `void*` + 比较函数指针的泛型套路,返回指向匹配元素的指针、要下标用指针减法还原;比较函数别忘了用 `(a>b) - (a<b)` 防 `a-b` 异号大数溢出(第 10 章同款坑)。下一章我们把 O(1)/O(log n)/O(n)/O(n log n)/O(n²) 这些复杂度正式收个口,把这几章散落的「快」和「慢」用大 O 给讲透。

## 参考资源

- ISO/IEC 9899:2011 §7.22.5.1(`bsearch` 函数:参数约定、返回匹配元素的指针或 NULL、比较函数返回负/零/正)、§7.22.5(搜索与排序工具大节,含 `qsort`/`bsearch` 一对)、§6.5p5(表达式溢出属未定义行为,`lo+hi` 坑的法理依据)、§6.5.2.1(数组下标,`a[mid]` 的定义)
- K. N. King《C Programming: A Modern Approach》第 17 章(17.7 `qsort` 与排序的比较函数)、第 26 章(`bsearch` 用法与「查找 1000 元素最多 10 次比较、100 万最多 20 次」的复杂度说明)
- Andrew Koenig《C Traps and Pitfalls》第 8 章·Answer 3-3(二分查找两种区间约定的写法对照:半开 `[lo, hi)` + `lo < hi`、闭 `[lo, hi]` 的边界处理)
- Joshua Bloch, 2006, "Extra, Extra - Read All About It: Nearly All Binary Searches and Mergesorts Are Broken"(JDK `Arrays.binarySearch` 里 `(low + high) / 2` 溢出的真实工程案例)
- 第 10 章:数组(下标、`a[i]`、越界 UB)、阶段 1·第 3 章:整型溢出(`lo+hi` 溢出 UB 的根)、阶段 2·第 2 章:指针算术(指针减法还原下标)、阶段 2·第 9 章:函数指针(`bsearch`/`qsort` 的比较函数套路)、阶段 3·第 9/10 章:排序(二分的前置工序)、阶段 3·第 8 章:哈希表(O(1) 查找对照二分 O(log n))、第 12 章:大 O(O(log n) 推导收口)
