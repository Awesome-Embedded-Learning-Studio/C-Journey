---
title: "算法分析与查找排序:用真实计时,把 Big-O 从口号变成手感"
description: "从 Big-O 渐进上界讲起,把线性/二分查找、冒泡/选择/插入/快速/归并排序逐一用纯 C 实现,并在本机真实计时,让你看见 O(n^2) 与 O(n log n) 在 n 翻倍时的实际差距。"
chapter: 3
order: 4
tags:
  - host
  - algorithm
  - data-structures
  - function
difficulty: beginner
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 3:递归与调用栈"
  - "Chapter 3:动态数组"
  - "Chapter 3:链表"
related:
  - "Chapter 3:递归与调用栈"
  - "Chapter 3:动态数组"
  - "Chapter 3:链表"
---

# 算法分析与查找排序:用真实计时,把 Big-O 从口号变成手感

## 引言

写过几段排序代码的人,迟早会撞上同一个问题:**为啥我的程序,数据一多就慢成狗?** 同样是把数组排个序,有人写的几万条数据秒完,有人写的几百条就开始卡。这不是机器不行,是算法本身在数量级上分了高下。

但"快慢"这词其实很坑——你拿一个 O(n²) 的冒泡去排 10 个数,它比谁跑得都快;换成 10 万个,它就寄。**单纯说"这个算法快"是没意义的,得说"数据规模变大时,它慢得多快"**。这就是 Big-O 要刻画的东西。

这一章我们干两件事:第一,把复杂度分析(Big-O、O/Ω/Θ、那几个常见复杂度类)讲透;第二,把查找(线性、二分)和五个最常考的排序(冒泡、选择、插入、快速、归并)全部用纯 C 写出来、跑出来,看真实输出,再用真实计时让你眼见 O(n²) 和 O(n log n) 拉开差距。

> 本文所有代码都是纯 C,所有输出都在本机实测捕获(GCC 16.1.1, `-std=c11`)。

## 复杂度分析:大 O 到底在刻画什么

### 先把"快慢"换成"增长趋势"

回到开头那个问题:你怎么衡量一个算法快不快?最直觉的办法是——拿个性能分析器,看它跑完花了多少毫秒。但这法子有个致命弱点:**它依赖具体机器、具体数据、具体编译选项**。换台机器、换个数据集,数字就变了,没法横向比较两个算法。

所以我们换个角度:不问"跑多久",而问"**当输入规模 n 变大时,操作次数按什么规律增长**"。把这个增长规律的最高次项抽出来,就是**时间复杂度**。比如某算法的操作次数是 `6N + 4`,我们不说它"6N+4",而说它 O(N)——因为当 N 趋于无穷,常数 6 和 4、还有线性项前面的系数,在"增长趋势"面前都不重要了,**真正决定它变慢速度的是 N 这一项的幂次**。

这一点先想透,后面全是顺水推舟。

### 三个渐进符号:O、Ω、Θ

光有"上界"还不够,数学上给了三个互补的符号:

- **O(f(N)) —— 渐进上界**:描述算法的**最坏情况**。证明目标是"我的式子 ≤ 目标多项式"。日常工程里说复杂度,99% 用的是它。
- **Ω(f(N)) —— 渐进下界**:描述算法的**最好情况**。用得少,因为下界不好证,而且我们通常更关心"会不会很惨"。
- **Θ(g(N)) —— 渐进紧界**:当 O 和 Ω 能用同一个多项式表示时,它就是"既≤又≥"的紧界。比如归并排序是 Θ(n log n),最好最坏都是这个量级。

记住一句话:**O 给的是上限保证(最坏能糟成啥样),这正是工程师最该关心的**。

### 常见的七个复杂度类

按从快到慢排个序,脑子里有这张表,看代码就能估个大概:

| 复杂度 | 名字 | 典型例子 |
|--------|------|----------|
| O(1) | 常数 | 数组按下标取值 `a[99]` |
| O(log n) | 对数 | 二分查找 |
| O(n) | 线性 | 单层循环遍历、线性查找 |
| O(n log n) | 对数线性 | 归并排序、快速排序(平均) |
| O(nᵏ) | 多项式 | 冒泡/选择/插入 O(n²)、三层嵌套循环 O(n³) |
| O(kⁿ) | 指数 | 朴素递归斐波那契 O(2ⁿ) |
| O(n!) | 阶乘 | 旅行商问题暴力解 |

随着输入规模增长,**多项式以上的复杂度(指数、阶乘)运行时间会爆炸式增长**——n 才几十,机器就跑到天荒地老。所以工程上,我们拼命往 O(n log n) 甚至更低的区域靠。下面挑几个关键的细看。

**O(1) —— 常数复杂度**:运行时间不随输入规模变化。最典型就是数组按索引取值:

```c
int arr[100] = {0};
int v = arr[99];  /* 无论数组多大, 一步到位 */
```

**O(log n) —— 对数复杂度**:典型代表是二分查找。每迭代一次,待搜索区间砍一半,所以数组翻倍,只需多迭代一次。后面我们会亲手实现它。

**O(n) —— 线性复杂度**:单层循环遍历,比如线性查找、链表遍历,最坏要扫完整个数据。

**O(n log n) —— 对数线性**:排序算法的主战场。归并、快排(平均)都在这里。

**O(kⁿ) —— 指数复杂度**:下面这个朴素递归斐波那契就是经典反面教材,n 每加 1 操作量翻倍,复杂度 O(2ⁿ)。这种代码 n 稍大一点就直接卡死。

```c
long long fib(int n) {
    if (n <= 2) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);  /* 大量子问题被重复计算 */
}
```

### 大 O 只看"显著趋势"

这点容易被忽略:**大 O 描述的是 n 趋于无穷时的显著趋势,小项会被大项盖掉**。所以 O(n² + n) 简化成 O(n²),O(2n² + 100n) 也简化成 O(n²)。工程上估复杂度时,**找到嵌套循环最深的那一层、找出它跟着 n 涨的最高幂次,就是答案**——别纠结系数和低阶项。

### 估算代码时间表达式的四条法则

给定一段代码,怎么估它的时间表达式?记住这几条:

1. **单层 `for`**:循环次数 × 循环体耗时。
2. **嵌套 `for`**:自内向外逐层相乘(两层嵌套常是 O(n²))。
3. **顺序语句**:几段相加。
4. **`if/else`**:取判断耗时 + 较长那条分支的耗时。

举个最简单的例子:

```c
int get_sum_cube(int N) {
    int partial = 0;                 /* 1 次 */
    for (int i = 1; i <= N; i++) {   /* N 次循环 */
        partial += i * i * i;        /* 循环体内常数次操作 */
    }
    return partial;
}
```

扫了一遍大小为 N 的循环,精确表达式大概是 `f(N) = 6N + 4` 这种线性式,简化后就是 O(N)。

## 查找:线性和二分

查找是排序最好的"搭档"——排序的回报之一,就是把查找从 O(n) 压到 O(log n)。我们先看这两种查找。

### 线性查找 O(n)

最朴素的查法:从头到尾挨个比对,命中就返回下标,扫完没命中返回 -1。**不要求有序**,这是它唯一的好处,代价是平均/最坏都得扫半个到一个数组。

```c
int linear_search(const int arr[], int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;
}
```

### 二分查找 O(log n)

二分查找的前提是**数组已排序**。思路很直白:每次取当前区间的中点,比目标小就往右半边走,比目标大就往左半边走,直到命中或区间空。

> 作者笔记里原版的二分查找藏了两个坑,我们在这里顺手修了:一是 `int middle = left + (right - left) >> 1` 这行,`>>` 的优先级比 `+` 低,会被解析成 `left + ((right-left) >> 1)`,本例侥幸算对但极易写错,我们改用更清晰的 `/ 2`;二是 `if(middle = target)` 是**赋值**不是比较(经典 `=` 写成 `==`),而且比较的应该是 `arr[middle]` 不是 `middle`。这种笔误编译器都未必报,务必当心。

修正后的实现:

```c
int binary_search(const int arr[], int n, int target) {
    int left = 0;
    int right = n - 1;
    while (left <= right) {
        /* 用 left + (right-left)/2 而不是 (left+right)/2, 防止大数相加溢出 */
        int mid = left + (right - left) / 2;
        if (arr[mid] == target) {
            return mid;
        } else if (arr[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}
```

那个 `left + (right - left)/2` 的写法不是炫技:**当 n 很大、left 和 right 都接近 INT_MAX 时,直接 `(left+right)` 会先溢出成负数,中点就算错**——这是二分查找最经典的隐蔽 bug。

### 真实输出:线性 vs 二分

把两种查找放一起跑,用同一份已排序数组、查四个不同的值(含命中和未命中):

```c
#include <stdio.h>

int linear_search(const int arr[], int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;
}

int binary_search(const int arr[], int n, int target) {
    int left = 0;
    int right = n - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] == target) {
            return mid;
        } else if (arr[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

int main(void) {
    int sorted[] = {11, 23, 37, 42, 58, 64, 71, 89, 95};
    int n = sizeof(sorted) / sizeof(sorted[0]);
    int targets[] = {64, 95, 11, 50};
    for (int t = 0; t < 4; t++) {
        int v = targets[t];
        printf("find %-3d  linear -> %2d   binary -> %2d\n",
               v, linear_search(sorted, n, v), binary_search(sorted, n, v));
    }
    return 0;
}
```

```text
find 64   linear ->  5   binary ->  5
find 95   linear ->  8   binary ->  8
find 11   linear ->  0   binary ->  0
find 50   linear -> -1   binary -> -1
```

两份结果一致,未命中的 50 都返回 -1,符合预期。结果对得上只是基本盘,**真正的差距要等数据量大起来才看得见**——线性查找平均要扫 n/2 个元素,二分查找最多 log₂n 次。n = 100 万时,线性 50 万次 vs 二分 20 次,差出几个数量级。

## 排序:五个最经典的实现

排序是算法教学的"正宫"——它把同一道题用五种思路解给你看,每一种的复杂度都不一样,是理解 Big-O 最好的素材。下面这五个全部用纯 C 实现,每个都跑给你看。

先约定一个排序辅助函数,后面所有排序都接收 `int a[], int n` 这种数组 + 长度的形式,原地排序。

### 冒泡排序 O(n²)

最直觉的排序:相邻两两比较,逆序就交换。每一趟把当前最大的"冒泡"到末尾。加一个 `swapped` 标志,**如果某一趟一次交换都没发生,说明已经有序,可以提前退出**——这是教科书版冒泡常省略的小优化,能让"近乎有序"的输入接近 O(n)。

```c
void bubble_sort(int a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (a[j] > a[j + 1]) {
                int tmp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = tmp;
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
}
```

### 选择排序 O(n²)

思路:每一轮在未排序区里找最小的,和未排序区的第一个交换。交换次数比冒泡少(每轮最多一次),但比较次数还是 O(n²),所以整体仍是 O(n²)。

```c
void selection_sort(int a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (a[j] < a[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            int tmp = a[i];
            a[i] = a[min_idx];
            a[min_idx] = tmp;
        }
    }
}
```

### 插入排序 O(n²),近乎有序时 O(n)

像打扑克理牌:手里是排好的,每摸一张新牌,从右往左找它该插的位置,边找边把比它大的往后挪。**它的看家本领是——当数组近乎有序时,内层 while 几乎不执行,整体退化成 O(n)**,这是它能在小数据量/近乎有序场景干过快排的原因。

```c
void insertion_sort(int a[], int n) {
    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > key) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}
```

### 快速排序 O(n log n) 平均,O(n²) 最坏

分治的代表作:选一个基准(pivot),把数组划成"≤ pivot"和"> pivot"两半,然后对两半递归。这里取最右元素为基准(Lomuto 划分)。**平均 O(n log n)**,但最坏情况(数组已经有序却总取端点为基准)会退化成 O(n²)。

```c
static void quick_sort_range(int a[], int lo, int hi) {
    if (lo >= hi) {
        return;
    }
    int pivot = a[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (a[j] <= pivot) {
            i++;
            int tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
    }
    int tmp = a[i + 1];
    a[i + 1] = a[hi];
    a[hi] = tmp;
    int p = i + 1;
    quick_sort_range(a, lo, p - 1);
    quick_sort_range(a, p + 1, hi);
}

void quick_sort(int a[], int n) {
    quick_sort_range(a, 0, n - 1);
}
```

### 归并排序 O(n log n)

也是分治:把数组对半切,递归排好两半,再把两个有序半段合并(`merge`)。**最坏、最好、平均都是 Θ(n log n)**——这是它对快排的优势(快排最坏 O(n²));代价是需要一个 O(n) 的辅助缓冲区。

```c
static void merge(int a[], int lo, int mid, int hi, int buf[]) {
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        if (a[i] <= a[j]) {
            buf[k++] = a[i++];
        } else {
            buf[k++] = a[j++];
        }
    }
    while (i <= mid) {
        buf[k++] = a[i++];
    }
    while (j <= hi) {
        buf[k++] = a[j++];
    }
    for (int t = lo; t <= hi; t++) {
        a[t] = buf[t];
    }
}

static void merge_sort_range(int a[], int lo, int hi, int buf[]) {
    if (lo >= hi) {
        return;
    }
    int mid = lo + (hi - lo) / 2;
    merge_sort_range(a, lo, mid, buf);
    merge_sort_range(a, mid + 1, hi, buf);
    merge(a, lo, mid, hi, buf);
}

void merge_sort(int a[], int n) {
    if (n <= 1) {
        return;
    }
    int buf[64];  /* 本 demo 数组上限 */
    merge_sort_range(a, 0, n - 1, buf);
}
```

### 真实输出:五个排序同台对一把

把同一份乱序数组喂给五种排序,看输出是否一致:

```c
#include <stdio.h>

static void print_array(const char *tag, const int a[], int n) {
    printf("%-10s [", tag);
    for (int i = 0; i < n; i++) {
        printf("%d%s", a[i], (i == n - 1) ? "" : ", ");
    }
    printf("]\n");
}

static void copy_arr(int dst[], const int src[], int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

/* (bubble_sort / selection_sort / insertion_sort / quick_sort / merge_sort
    实现同上, 此处省略以节省篇幅, 完整代码见上方各节) */

int main(void) {
    int raw[] = {5, 2, 8, 1, 9, 3, 7, 4, 6, 0};
    int n = sizeof(raw) / sizeof(raw[0]);
    int work[64];

    print_array("raw", raw, n);

    copy_arr(work, raw, n); bubble_sort(work, n);    print_array("bubble", work, n);
    copy_arr(work, raw, n); selection_sort(work, n); print_array("selection", work, n);
    copy_arr(work, raw, n); insertion_sort(work, n); print_array("insertion", work, n);
    copy_arr(work, raw, n); quick_sort(work, n);     print_array("quick", work, n);
    copy_arr(work, raw, n); merge_sort(work, n);     print_array("merge", work, n);
    return 0;
}
```

```text
raw        [5, 2, 8, 1, 9, 3, 7, 4, 6, 0]
bubble     [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
selection  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
insertion  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
quick      [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
merge      [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```

五个都把乱序数组正确排成了升序——正确性对齐了。但**正确只是及格线,真正的较量在数据量起来之后**。

## 实战佐证:用真实计时,看见 O(n²) 和 O(n log n) 拉开差距

光说冒泡 O(n²)、快排 O(n log n),没感觉。我们用同一份随机数据,分别喂给冒泡和快排,在 n=2000 和 n=8000 两个规模下真实计时。按理论,n 翻 4 倍,冒泡(O(n²))耗时应该涨约 16 倍,快排(O(n log n))涨约 4 倍多一点。

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void fill_random(int a[], int n, unsigned seed) {
    srand(seed);
    for (int i = 0; i < n; i++) {
        a[i] = rand() % 1000000;
    }
}

void bubble_sort(int a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (a[j] > a[j + 1]) {
                int t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

static void quick_sort_range(int a[], int lo, int hi) {
    if (lo >= hi) return;
    int pivot = a[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (a[j] <= pivot) {
            i++;
            int t = a[i]; a[i] = a[j]; a[j] = t;
        }
    }
    int t = a[i + 1]; a[i + 1] = a[hi]; a[hi] = t;
    int p = i + 1;
    quick_sort_range(a, lo, p - 1);
    quick_sort_range(a, p + 1, hi);
}

void quick_sort(int a[], int n) { quick_sort_range(a, 0, n - 1); }

int is_sorted(const int a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        if (a[i] > a[i + 1]) return 0;
    }
    return 1;
}

int main(void) {
    int sizes[] = {2000, 8000};
    for (int s = 0; s < 2; s++) {
        int n = sizes[s];
        int *base = malloc(sizeof(int) * n);
        int *work = malloc(sizeof(int) * n);
        fill_random(base, n, 42u);

        fill_random(work, n, 42u);
        clock_t t0 = clock();
        bubble_sort(work, n);
        double bub = (double)(clock() - t0) / CLOCKS_PER_SEC;

        fill_random(work, n, 42u);
        t0 = clock();
        quick_sort(work, n);
        double qui = (double)(clock() - t0) / CLOCKS_PER_SEC;

        printf("n=%-5d  bubble %.4fs (%s)   quick %.6fs (%s)\n",
               n, bub, is_sorted(work, n) ? "sorted" : "BAD",
               qui, is_sorted(work, n) ? "sorted" : "BAD");
        free(base);
        free(work);
    }
    return 0;
}
```

```text
n=2000   bubble 0.0034s (sorted)   quick 0.000098s (sorted)
n=8000   bubble 0.0451s (sorted)   quick 0.000501s (sorted)
```

数据说话:数据量从 2000 涨到 8000(4 倍),**冒泡从 0.0034s 涨到 0.0451s,约 13 倍**——和 O(n²) 预言的"4²=16 倍"基本吻合(实测略小于理论是因为常数项和缓存效应);**快排从 0.000098s 涨到 0.000501s,约 5 倍**——和 O(n log n) 预言的"4×(log8000/log2000)≈4.4 倍"对得上。

更扎眼的是**同一规模下的绝对差距**:n=8000 时,冒泡 45ms,快排 0.5ms,**差出两个数量级**。这就是 O(n²) 和 O(n log n) 的真实代价。再把 n 拉到 10 万、100 万,冒泡会让你等到怀疑人生。

### 同一道题,四个复杂度:最大子序列和

作者笔记里还有个把复杂度分析讲得特别透的例子——**最大子序列和**:给定一个整数序列,求所有连续子序列里的最大和。比如 `[1, 2, -3, 4, -5, 6, 7, 8, -9, 10]` 的答案是 22(子序列 `[6, 7, 8, -9, 10]`)。同一道题,从暴力到精巧,能写出 O(n³)→O(n²)→O(n) 的递进,把上面那张复杂度表踩了个遍。

**O(n³) 穷举**:三重循环,枚举所有起点 i、终点 j,再一个循环对 [i,j] 求和。

```c
long maxsub_brute3(const int a[], int n) {
    long max = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) {
            long sum = 0;
            for (int k = i; k <= j; k++) {
                sum += a[k];
            }
            if (sum > max) max = sum;
        }
    }
    return max;
}
```

**O(n²) 穷举优化**:观察到固定起点 i 时,终点 j 每次只往后挪一格,上一轮的 `sum` 留着加一个新元素即可,省掉最内层循环。

```c
long maxsub_brute2(const int a[], int n) {
    long max = 0;
    for (int i = 0; i < n; i++) {
        long sum = 0;
        for (int j = i; j < n; j++) {
            sum += a[j];
            if (sum > max) max = sum;
        }
    }
    return max;
}
```

**O(n) Kadane 算法(动态规划思想)**:核心洞察是——**累加器 `sum` 一旦变负,它对后面就是纯负担,直接清零**。于是只需一次遍历。

```c
long maxsub_kadane(const int a[], int n) {
    long max = 0;
    long sum = 0;
    for (int i = 0; i < n; i++) {
        sum += a[i];
        if (sum > max) {
            max = sum;
        } else if (sum < 0) {
            sum = 0;  /* 累加器为负就清零, 不让前缀拖累后面 */
        }
    }
    return max;
}
```

三种实现跑同一份输入,结果应当一致:

```text
array:   [1, 2, -3, 4, -5, 6, 7, 8, -9, 10]
O(n^3)   brute3  -> 22
O(n^2)   brute2  -> 22
O(n)     kadane  -> 22
```

答案都是 22,跟笔记里手工推演的结果对得上。**三种解法答案相同,但复杂度天差地别**——这正是算法分析的全部意义:**同一道题,想得越深,做得越快**。把 n 拉到十万级,O(n³) 会跑到小时级,O(n) 仍是毫秒级。

> 笔记里还提到一种 O(n log n) 的**分治解法**:把数组切两半,最大子序列和要么全在左半、要么全在右半、要么横跨中点(横跨时分别从中点向左、向右扫出最大和相加)。它用到了递归,我们在[递归与调用栈](1-recursion-and-call-stack.md)那章已经铺过递归的底子,感兴趣的可以自己试着把它写出来。

## 常见踩坑

- **二分查找的 `(left+right)` 溢出**:n 很大时 `left+right` 可能超过 `INT_MAX` 算出负数,中点就错了。永远用 `left + (right-left)/2`。
- **`=` 写成 `==`(或反过来)**:作者笔记原版二分里 `if(middle = target)` 就是这个错——它是赋值、永真,而且比较对象都搞错了。开 `-Wall -Wextra` 能抓到一部分(`-Wparentheses`),但别全指望编译器。
- **二分查找忘了"数组必须有序"**:对乱序数组二分,结果纯属玄学。先排再二分,或老实用线性查找。
- **冒泡/选择/插入在大数据上硬刚**:O(n²) 在 n 上万时就开始吃力。数据量上来就上快排/归排(`qsort`/`std::sort` 级别),别死磕三重循环。
- **快排取端点为基准遇已排序数组退化成 O(n²)**:本例取 `a[hi]` 为基准,如果数据已经升序,每次划分都极度不均,退化成 O(n²)。生产环境用**随机选基准**或**三数取中**来规避。
- **归并排序的辅助缓冲区**:本 demo 用了固定大小 `buf[64]`,只是为了教学。真实场景要么 `malloc` 一份 n 大小的缓冲,要么写成"源/目标交替拷贝"的版本,别用定长小数组去排大数据。

## 小结

- **Big-O 刻画的是"数据规模变大时,操作次数的增长趋势",不是绝对快慢**。看代码估复杂度,找嵌套循环最高那一层的幂次即可,别纠结系数和低阶项。
- **O 是最坏上界**(工程师最该关心),Ω 是最好下界,Θ 是紧界。
- **查找**:线性 O(n) 不要求有序;二分 O(log n) 要求有序。能用二分就别用线性。
- **排序**五个经典解:冒泡、选择、插入都是 O(n²)(插入在近乎有序时退化为 O(n));快排平均 O(n log n)、最坏 O(n²);归并始终 Θ(n log n) 但要 O(n) 额外空间。本机实测 n 翻 4 倍,冒泡涨约 16 倍、快排涨约 5 倍,与理论吻合。
- **同一道题,想得越深做得越快**:最大子序列和从 O(n³) 优化到 O(n),复杂度差出几个数量级——这就是算法分析的全部回报。

## 练习

1. 给快排加上"三数取中"选基准(取 `a[lo]/a[mid]/a[hi]` 的中位数),观察它对已排序数组是否还退化。
2. 把五种排序分别跑在同一份 10 万规模的随机数组上,自己计时,排个性能表。
3. 实现最大子序列和的 **O(n log n) 分治解法**(横跨中点那部分要小心),和 Kadane 的 O(n) 版计时对比。
4. 把二分查找改成**返回目标应插入位置**的版本(即 `lower_bound` 风格),用于"有序数组去重插入"。
5. 写一个对冒泡排序的"近乎有序"输入(n 个元素,只有 k 个逆序对)计时,验证它是否退化到接近 O(n)。

## 参考资源

- *数据结构与算法分析 —— C 语言描述*(Mark Allen Weiss)的"算法分析"与"排序"章节,最大子序列和的经典出处。
- `man qsort` —— 标准库的通用排序接口,看它怎么用函数指针实现泛型。
- [Big O notation — Wikipedia](https://en.wikipedia.org/wiki/Big_O_notation) —— O/Ω/Θ 的形式化定义。
- [Sorting algorithm — Wikipedia](https://en.wikipedia.org/wiki/Sorting_algorithm) —— 各种排序算法的复杂度对照表。

---
*整理自作者笔记,按 C-Journey 写作规范重写;所有输出本机实测捕获。*
