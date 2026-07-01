---
title: "快排与归并:分治、O(n log n)、对照 qsort"
description: "阶段3·第 10 章。第 9 章的冒泡/插入/选择在大数据上慢到 O(n^2),这一章拆掉这道天花板:用「分治」(divide and conquer) 把排序压到 O(n log n)。两条经典路线各管一头——快排 quicksort 原地、平均 O(n log n),选个 pivot 把数组分成「小的左、大的右」再对左右递归,本章用 Lomuto 分区(选 a[high] 当 pivot、一趟扫描把 <=pivot 的往左堆)真跑 {5,2,8,1,9,3} 得 1 2 3 5 8 9;坑是选首/尾元素当 pivot 时,遇上已有序数据会退化到最坏 O(n^2),靠「三数取中」缓解。归并 mergesort 走另一条路:把数组对半切、两边各自递归排好、再合并两个有序段,最坏就是 O(n log n) 而且稳定,代价是合并要借一块 O(n) 的 malloc 临时数组(呼应阶段2 第 6 章,用完即 free、ASan 复核无泄漏)。最后把三种排序(自写快排/自写归并/标准库 qsort,后者阶段2 第 9 章见过)放跑同一数组,结果一字不差。全 gcc16+clang22 真跑。"
chapter: 3
order: 10
tags:
  - host
  - data-structures
  - algorithm
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第9章:排序入门(O(n^2) 对照)"
  - "阶段2·第9章:函数指针(标准库 qsort)、第6章:动态内存(归并的临时数组)"
  - "第 8 章:函数(递归)"
related:
  - "阶段3·第12章:大 O(O(n log n) 推导、快排最坏 O(n^2))"
  - "阶段2·第9章:函数指针(qsort 回调)"
---

# 快排与归并:分治、O(n log n)、对照 qsort

## 引言:O(n^2) 那道天花板

第 9 章我们把冒泡、插入、选择三种 O(n^2) 排序真跑了一遍,亲眼看到数组一大、比较次数就按平方往上窜——一万个元素要比较上亿次,十万个就上万亿。那道「平方」的天花板,在大数据上是真扛不住的。这一章我们拆掉它,把排序压到 **O(n log n)** 这个量级,而拆墙的锤子只有两个字:**分治**(divide and conquer)。

分治的思路朴素到一句废话:**大问题难,就把它切成小问题**。排序一百万个数太重,那就切成两半各排五十万、再把两个有序结果「合并」起来——子问题规模减半,而「合并两个有序数组」是 O(n) 的。每次砍一半,砍 log₂ n 次到底,每层 O(n),乘起来就是 O(n log n)。这正是归并排序(mergesort)的形状。快排(quicksort)换了个角度,但同样是「切一刀再递归」:它不靠合并,而是靠「分区」——挑一个元素当基准(pivot),把比它小的全甩到左边、大的全甩到右边,然后对左右两段各再来一次。两条路殊途同归,都把平方天花板掀了。

这一章我们两个都手写一遍,再用阶段 2 第 9 章见过的标准库 `qsort`(§7.22.5.2,底层用的就是这套分治思路)做对照——三种排序喂同一份 `{5, 2, 8, 1, 9, 3}`,看结果是不是一字不差。

## 快排:选 pivot、分区、左右递归

快排三步走,核心全在第二步「分区」。用 Lomuto 分区法(比 Hoare 法多几次交换、但好写好懂,教学首选),整个算法长这样:

```c
#include <stdio.h>

/* Lomuto 分区:选 a[high] 当 pivot,把 <=pivot 的往左堆,
   最后把 pivot 换到分界处,返回 pivot 的最终下标。 */
static int partition(int* a, int low, int high) {
    int pivot = a[high]; /* 取最右元素当 pivot */
    int i = low - 1;     /* i 指向「<=pivot 区」的右边界 */
    for (int j = low; j < high; j++) {
        if (a[j] <= pivot) {
            i++;
            /* 交换 a[i] 和 a[j]:把小元素挪到左边 */
            int tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
    }
    /* 把 pivot 放到分界处:a[i+1] 与 a[high] 交换 */
    int tmp = a[i + 1];
    a[i + 1] = a[high];
    a[high] = tmp;
    return i + 1;
}

static void quicksort(int* a, int low, int high) {
    if (low < high) {
        int p = partition(a, low, high);
        quicksort(a, low, p - 1);  /* 排 pivot 左边 */
        quicksort(a, p + 1, high); /* 排 pivot 右边 */
    }
}

int main(void) {
    int a[] = {5, 2, 8, 1, 9, 3};
    int n = sizeof(a) / sizeof(a[0]);

    quicksort(a, 0, n - 1);

    for (int i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra quicksort.c -o qs && ./qs
1 2 3 5 8 9
```

来拆这段代码。`quicksort(a, low, high)` 是个典型的递归骨架(第 8 章函数那章讲过递归的「基线 + 自调用」):基线条件是 `low >= high`(段里 0 个或 1 个元素,本来就「排好了」直接返回);否则进 `partition` 把段切成两半、拿到 pivot 的落点 `p`,再对 `p` 左右两段各调一次自己。真正干活的是 `partition`,我们一段段看。

Lomuto 分区的妙处在于它用一个游标 `i` 把「已经发现的 <=pivot 区」和「还没扫描的区」划开,这正是数组退化(阶段1 第 10 章 `a[i] ≡ *(a+i)`)之后我们仍在做的那件事——但这里它不是指针,是「边界下标」。初始化 `i = low - 1` 表示「<=pivot 区」此刻是空的;然后 `j` 从 `low` 扫到 `high-1`,每撞见一个 `a[j] <= pivot`,就把 `i` 往右推一格、再让 `a[i]` 和 `a[j]` 交换——这一手交换的意思是「把这个小元素收编进左区」。扫完一圈后,`i+1` 就是左区的紧后方,正是 pivot 该落脚的位置:把 `a[high]`(pivot 本尊)和 `a[i+1]` 一换,pivot 就坐进了它的最终位置——左边全 <=它、右边全 >=它。返回 `i+1` 这个下标给上层,上层据此切出左右两段递归。

拿 `{5, 2, 8, 1, 9, 3}` 走一趟 `partition(a, 0, 5)`:`pivot = a[5] = 3`。`j=0` 看 `5`,大于 3 不收;`j=1` 看 `2` 小于等于 3,`i` 推到 0、`a[0]` 和 `a[1]` 交换得 `{2,5,8,1,9,3}`;`j=2` 看 `8` 不收;`j=3` 看 `1`,`i` 推到 1、`a[1]` 和 `a[3]` 交换得 `{2,1,8,5,9,3}`;`j=4` 看 `9` 不收。循环结束,`i+1 = 2`,把 `a[2]` 和 `a[5]` 交换,pivot 落到下标 2:`{2, 1, 3, 5, 9, 8}`,返回 2。于是 `3` 已经坐稳了,接下来对 `{2,1}` 和 `{5,9,8}` 各自递归。整个过程在原地(只借了几个临时变量),没有额外数组——这是快排「省空间」的关键。

### 最坏的坑:有序数据 + 选首/尾当 pivot

快排平均 O(n log n),但它有一个让人血压拉满的最坏情况:**O(n^2)**。罪魁是 pivot 选得烂。我们的 Lomuto 版固定取 `a[high]` 当 pivot,想象一下输入已经升序好了 `{1,2,3,5,8,9}`——`partition(a,0,5)` 取 `a[5]=9` 当 pivot,扫一遍没有任何元素比 9 大,`i` 一路推到 4,pivot 仍然落回下标 5,**左半是 5 个元素、右半是 0 个**。下一步对这 5 个再分区,pivot 又是最大的那个、左 4 右 0……每一层只把问题规模砍掉 1,要砍 n 次到底,每次 O(n),乘起来就是 O(n^2)。降序数据同理,反过来 pivot 永远最小、左 0 右 n-1。

这就是为什么教科书版快排(以及我们这版)不能直接拿去处理「可能已经有序」的真实数据——而真实世界里「几乎有序」的输入比比皆是(增量日志、按时间戳的流、默认排好序的数据库读出)。缓解办法叫 **三数取中**(median-of-three):不固定取首或尾,而是看 `a[low]`、`a[mid]`、`a[high]` 三个,取它们的中位数当 pivot。中位数天然把数组切得比较均衡(左半至少有 1/4、右半也至少有 1/4),于是最坏情况几乎触发不了,期望复杂度稳稳落在 O(n log n)。生产级实现(glibc 的 `qsort` 内部)还会再叠一层:子段小到一定程度(比如十几个元素)就改用插入排序(第 9 章),因为小数组上插入排序的常数因子更小、还省递归调用。这些优化我们这章不动手,知道「pivot 怎么选」是快排性能命门就够了。

顺带说一句,标准库的 `qsort`(§7.22.5.2)名字里那个 `q` 是 quicksort 的缩写,但**标准并不要求**它内部一定用快排——只要结果是升序、并且是「稳定」之外的任意有效排序就行(§7.22.5 对 `qsort` 明确不保证稳定性)。glibc 的实现确实是快排族(配三数取中和插入排序兜底),但你不该依赖这个细节。阶段 2 第 9 章我们用 `qsort` 排过 `{5,2,8,1,9,3}`,得 `1 2 3 5 8 9`,本章末尾我们会再拿它当对照基线。

## 归并:对半切、各自排好、合并

归并走的是和快排完全对称的另一条路。快排是「先分区(干完活才知道 pivot 在哪)、再递归」;归并是「先递归(直接对半切,不挑)、合并时才干活」。代价是合并那一步要借一块和数组等大的临时内存——O(n) 的额外空间,这正是它和快排(原地、O(1) 额外空间)最实在的差距。

```c
#include <stdio.h>
#include <stdlib.h>

/* 把两个有序段 a[lo..mid] 和 a[mid+1..hi] 合并成一段。
   借一块临时数组装左半边,再回写进 a。临时数组用完即 free。 */
static void merge(int* a, int lo, int mid, int hi) {
    int n_left = mid - lo + 1;
    int* left = malloc((size_t) n_left * sizeof(int));
    if (left == NULL) {
        return; /* 分配失败,放弃合并(教学版简化处理) */
    }
    for (int i = 0; i < n_left; i++) {
        left[i] = a[lo + i];
    }

    int i = 0;       /* 走 left */
    int j = mid + 1; /* 走右半(右半就地,从 mid+1 起) */
    int k = lo;      /* 回写进 a */
    while (i < n_left && j <= hi) {
        if (left[i] <= a[j]) {
            a[k++] = left[i++];
        } else {
            a[k++] = a[j++];
        }
    }
    while (i < n_left) {
        a[k++] = left[i++];
    }
    /* 右半若还有剩,本来就在 a 里原位,不用动 */
    free(left);
}

static void mergesort(int* a, int lo, int hi) {
    if (lo < hi) {
        int mid = lo + (hi - lo) / 2; /* 防 (lo+hi) 溢出 */
        mergesort(a, lo, mid);
        mergesort(a, mid + 1, hi);
        merge(a, lo, mid, hi);
    }
}

int main(void) {
    int a[] = {5, 2, 8, 1, 9, 3};
    int n = sizeof(a) / sizeof(a[0]);

    mergesort(a, 0, n - 1);

    for (int i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra mergesort.c -o ms && ./ms
1 2 3 5 8 9
```

`mergesort` 的骨架和 `quicksort` 是镜像:基线条件 `lo < hi` 不成立就返回(0 或 1 个元素天然有序);否则取中点 `mid`、对 `[lo,mid]` 和 `[mid+1,hi]` 各递归一次,排完之后调 `merge` 把两段有序的东西缝成一段。注意那个 `mid = lo + (hi-lo)/2` 而不是 `(lo+hi)/2`——这不是炫技,是真坑:`lo+hi` 在两个 `int` 都接近 `INT_MAX` 时会先溢出(有符号溢出是 UB,阶段1 第 3 章见过的),`lo + (hi-lo)/2` 保证中间不溢。规模小看不出区别,数据量大、`lo`/`hi` 是大下标时就救命。

真正花钱的是 `merge`。它要合并两个「各自已经有序」的段 `a[lo..mid]` 和 `a[mid+1..hi]`。常规做法是借一块和总长等大的临时数组,把整段拷过去,再两个指针分别走、挑小的回写进 `a`。我们这里做了个常见的小优化:**只拷左半边**进 `left`,右半边留在 `a` 里就地。为什么能这么省?因为 `k` 是从 `lo` 开始往右回写的,只要「左边先耗尽」,右半边剩下的元素**本来就在它们该在的位置**(原本就排在 `a` 的右段、回写指针也还没走到那),一个都不用动。这把临时数组从 O(n) 砍到 O(n/2)——量级还是 O(n),但常数小一半,工程上划算。

`malloc` 那两行要瞪大眼睛看(阶段 2 第 6 章的规矩):`left` 是从堆上借的内存,`malloc` 可能失败返回 `NULL`,所以紧跟一句 `if (left == NULL) return;`——教学版这里直接放弃合并(数组会留在半排好状态、不正确),生产代码里更严谨的做法是提前一次性分配一个全局临时缓冲区、复用之,免得每次 `merge` 都 `malloc`/`free` 一回。但不管哪种写法,**借了就得还**:`merge` 末尾的 `free(left)` 一个字都不能漏。漏一次,排序跑一遍就漏一块内存,数据量大点就是实打实的泄漏(阶段 2 第 7 章用 ASan 抓过一模一样的漏)。我们拿 ASan 复核一下,确认这版 `mergesort` 干净:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined mergesort.c -o ms_asan && ./ms_asan
1 2 3 5 8 9
$ echo $?
0
```

退出码 `0`、没有任何 `ERROR:` 行——意味着既没堆泄漏(`LeakSanitizer` 也没吭声),也没触发 UB。`malloc` 和 `free` 一进一出严格配平,这就是归并排序要付出的 O(n) 空间代价。

## 性能对照:平均都 O(n log n),差别在常数、最坏、稳定性

讲到这里你可能会问:既然平均都是 O(n log n),那我到底用哪个?差别藏在三个维度上,我们一条条摊开。

**最坏情况**是最关键的一条。归并排序**最坏**就是 O(n log n)——不管输入长什么样,它都老老实实对半切、每层 O(n) 合并、共 log n 层,稳如老狗。快排**平均** O(n log n)、**最坏**却会跌到 O(n^2),原因就是前面讲的 pivot 选烂了。换句话说:你要的如果是「不管数据多刁钻都保证 O(n log n)」这种承诺,归并给你;快排不保证,但配三数取中后实际触发最坏的概率极低。

**额外空间**是第二条。快排原地,递归调用栈之外只借 O(1) 的临时变量,空间复杂度 O(log n)(栈深度);归并每次 `merge` 都要 O(n) 临时数组,空间复杂度 O(n)。这就是为什么内存在意得紧的场景(嵌入式、超大数组)往往偏好快排——它不额外吃一块和原数组等大的堆。

**稳定性**(stable sort)是第三条,而且常被新手忽略。一个排序「稳定」指的是:对于值相等的两个元素,排完序后它们之间的**相对顺序**和排序前一样。归并是稳定的(我们的 `merge` 里那行 `if (left[i] <= a[j])` 用了 `<=`,相等时先取左半边,所以左边那个本来在前、回写后仍在前);快排不稳定(Lomuto 分区在交换时根本不在乎相等元素的先后,一通乱甩)。稳定性什么时候要紧?当你按多个键排序时——先按次要键排一遍、再按主要键稳定排一遍,主要键相同的那些元素就会保留住次要键的顺序。标准库 `qsort` 也**不保证**稳定(§7.22.5),所以需要稳定排序的场合你不能指望它。

最后是**常数因子**。快排原地、分区时内存访问是连续的(对 `i`、`j` 线性扫描),CPU 缓存命中率高、分支预测友好;归并的 `merge` 要在 `left` 和 `a` 两块之间来回跳,缓存表现差一些。所以同样是 O(n log n),快排在实际机器上通常跑得更快——这也是为什么 `qsort` 的实现主流都选快排族。一句话收口:**要快、内存够、不在乎稳定——快排;要最坏保证、要稳定、能付 O(n) 空间——归并**。这不是哪个更好的问题,是各自守住哪条线的取舍。

## 三种排序同台:自写快排、自写归并、标准库 qsort

说了这么多,该把三者拉到同一份数据上跑跑了。我们准备三份 `{5,2,8,1,9,3}` 的拷贝,分别喂给自写的 `quicksort`、自写的 `mergesort` 和标准库 `qsort`(阶段 2 第 9 章见过它的回调机制),看结果是不是一字不差:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int partition(int* a, int low, int high) {
    int pivot = a[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (a[j] <= pivot) {
            i++;
            int tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
    }
    int tmp = a[i + 1];
    a[i + 1] = a[high];
    a[high] = tmp;
    return i + 1;
}

static void quicksort(int* a, int low, int high) {
    if (low < high) {
        int p = partition(a, low, high);
        quicksort(a, low, p - 1);
        quicksort(a, p + 1, high);
    }
}

static void merge(int* a, int lo, int mid, int hi) {
    int n_left = mid - lo + 1;
    int* left = malloc((size_t) n_left * sizeof(int));
    if (left == NULL) {
        return;
    }
    for (int i = 0; i < n_left; i++) {
        left[i] = a[lo + i];
    }
    int i = 0, j = mid + 1, k = lo;
    while (i < n_left && j <= hi) {
        if (left[i] <= a[j]) {
            a[k++] = left[i++];
        } else {
            a[k++] = a[j++];
        }
    }
    while (i < n_left) {
        a[k++] = left[i++];
    }
    free(left);
}

static void mergesort(int* a, int lo, int hi) {
    if (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        mergesort(a, lo, mid);
        mergesort(a, mid + 1, hi);
        merge(a, lo, mid, hi);
    }
}

/* qsort 的比较回调:a-b 升序 */
static int cmp_int(const void* x, const void* y) {
    int a = *(const int*) x;
    int b = *(const int*) y;
    return (a > b) - (a < b);
}

static void print_array(const char* label, const int* a, int n) {
    printf("%s", label);
    for (int i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
}

int main(void) {
    int orig[] = {5, 2, 8, 1, 9, 3};
    int n = sizeof(orig) / sizeof(orig[0]);

    int a1[6], a2[6], a3[6];
    memcpy(a1, orig, sizeof(orig));
    memcpy(a2, orig, sizeof(orig));
    memcpy(a3, orig, sizeof(orig));

    quicksort(a1, 0, n - 1);
    mergesort(a2, 0, n - 1);
    qsort(a3, (size_t) n, sizeof(int), cmp_int);

    print_array("quicksort: ", a1, n);
    print_array("mergesort: ", a2, n);
    print_array("qsort    : ", a3, n);

    /* 三者结果应完全一致 */
    int ok = (memcmp(a1, a2, sizeof(a1)) == 0) && (memcmp(a1, a3, sizeof(a1)) == 0);
    printf("结果一致: %s\n", ok ? "是" : "否");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra compare.c -o cmp && ./cmp
quicksort: 1 2 3 5 8 9
mergesort: 1 2 3 5 8 9
qsort    : 1 2 3 5 8 9
结果一致: 是
```

三行排序输出完全相同,最后那句 `结果一致: 是` 是用 `memcmp`(§7.24.4.1,逐字节比)三两两对照得出来的实锤。这套对照之所以有意义,在于它把「同一份输入、三种实现路径」放在一张表上:自写的 `quicksort` 走 Lomuto 分区、自写的 `mergesort` 走对半切 + malloc 合并、标准库 `qsort` 走它自己那套优化过的分治——三条互不相干的代码路径,得出**字节相同**的结果。这正是一个算法「正确」最朴素的定义:不管你怎么排,排完都是同一个有序序列。

留意 `cmp_int` 的写法:`return (a > b) - (a < b);`。阶段 2 第 9 章我们见过 `qsort` 的回调要求返回「负/零/正」三态,新手最常犯的错是直接 `return a - b;`——这在两个 `int` 异号且差值大到溢出时会爆(比如 `a` 是大正数、`b` 是大负数,`a - b` 溢出 UB)。`(a > b) - (a < b)` 这种写法用两次比较、永远在 `{-1, 0, 1}` 里,什么输入都安全。工程里这是个值得养成肌肉记忆的小习惯。

## 小结

第 9 章的 O(n^2) 排序在大数据上撞墙,这一章用**分治**把排序压到 O(n log n):把大问题切成小问题递归解,log n 层、每层 O(n),乘起来就是 O(n log n)。两条经典路线殊途同归——**快排**(quicksort)靠「分区」:挑个 pivot、把小的甩左大的甩右、左右各自递归,我们用 Lomuto 分区(选 `a[high]` 当 pivot、游标 `i` 把 <=pivot 的往左堆、最后 pivot 落到 `i+1`)真跑 `{5,2,8,1,9,3}` 得 `1 2 3 5 8 9`,全程原地、只借 O(1) 临时变量;**归并**(mergesort)靠「合并」:对半切、两边各自排好、再 `merge` 两个有序段,最坏就是 O(n log n) 而且稳定,代价是 `merge` 要借一块 O(n) 的 `malloc` 临时数组(呼应阶段 2 第 6 章),用完即 `free`、ASan 复核退出码 0 无泄漏。两者的真正分野藏在三条线上:**最坏情况**(归并稳如老狗都是 O(n log n),快排会因 pivot 选烂跌到 O(n^2),靠三数取中缓解);**额外空间**(快排原地 O(log n) 栈、归并 O(n) 临时数组);**稳定性**(归并稳定——`<=` 保证相等元素相对顺序不变,快排不稳定、标准库 `qsort` 也不保证稳定)。要快、内存够、不在乎稳定选快排;要最坏保证、要稳定、能付 O(n) 空间选归并——这是各自守住哪条线的取舍,不是谁更好的问题。三种排序(自写快排 / 自写归并 / 标准库 `qsort`,后者阶段 2 第 9 章见过它的函数指针回调机制)放跑同一数组,`memcmp` 三两两对照、结果字节相同,这是「算法正确」最朴素的实锤。工程上几个值得记的小细节:`mid = lo + (hi-lo)/2` 防 `lo+hi` 溢出 UB;比较回调用 `(a>b) - (a<b)` 而不是 `a-b` 防异号大数溢出;`malloc` 必查 `NULL`、`free` 一字不漏。下一章第 11 章二分查找会再次用到「有序数组 + O(log n)」,这章打下的分治直觉在那里直接接上。

## 参考资源

- ISO/IEC 9899:2011 §6.5.2.1(数组下标)、§6.5.2.2(函数调用)、§7.22.3(`malloc`/`free` 等内存管理函数)、§7.22.5.2(`qsort`)
- K. N. King《C Programming: A Modern Approach》第 9 章·The Quicksort Algorithm(分治思想、Lomuto 风格的分区、三数取中改进方向)、附录 B 标准库 `qsort`
- Robert Sedgewick《Algorithms in C, Parts 1–4》第 7 章·Quicksort(Lomuto 与 Hoare 分区对比、三数取中、小数组改插入排序的工程优化)、第 8 章·Mergesort(自顶向下、自底向上、稳定性证明)
- 第 8 章:函数(递归的基线条件与自调用)、阶段 2 第 9 章:函数指针(`qsort` 的回调机制)、阶段 2 第 6 章:动态内存入门(`malloc`/`free` 配对、归并的临时数组)、第 9 章:排序入门(O(n^2) 对照、插入排序当小数组兜底)
- 阶段3·第 11 章:二分查找(有序数组 + O(log n))、第 12 章:算法复杂度与大 O(O(n log n) 推导、快排最坏 O(n^2) 分析收口)
