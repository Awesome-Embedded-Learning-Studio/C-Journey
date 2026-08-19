---
title: "阶段 3 Lab 实验参考"
description: "阶段 3 Lab（查找进化史）的实验参考：五个步骤加 L5 挑战的逐步解答，每步标注知识点链接，所有输出在 WSL Arch（gcc 16.1.1）真实运行得到，计时数据如实标注波动。"
chapter: 3
order: 3
tags:
  - host
  - data-structures
  - algorithm
difficulty: intermediate
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 3 Lab 题面"
related:
  - "阶段 3 各章"
---

# 阶段 3 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。第 5 步的计时数字每次跑会因机器负载波动，但量级之间的倍数关系稳定——大 O 抓的正是这个不随机器变的趋势。建议卡住时先看「思路」逐步对照。

## 步骤 1（L1）：O(n) 的起点——线性查找数比较次数 {#lab-1}

**思路**：线性查找就是「从头顺着比」——命中比较次数 = 目标下标 + 1，落空必比较 n 次；这就是 O(n) 里的那个 n。

1. 每轮循环先 `(*compares)++` 再比，命中和落空都数得清清楚楚。找 9 比 3 次（下标 2），找 7 比满 5 次。→ 知识点：[第 11 章：二分查找:有序数组、O(log n)、bsearch](/03-data-structures/11-binary-search)「引言:排序换速度」一节的线性查找、[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)「常见量级」一节的 O(n)
2. 最坏情况比较 n 次：要找的数不在表里（或恰好在最后）时，必须把 n 个元素一个不漏地比完——「比较次数随 n 线性增长」就是 O(n) 的含义。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)（O(n) 的定义）

```c
/* 线性查找:从头扫到尾,记录比较次数 */
static int linear_search(const int* a, int n, int key, int* compares) {
    for (int i = 0; i < n; i++) {
        (*compares)++;
        if (a[i] == key) {
            return i;
        }
    }
    return -1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab1.c -o lab1 && ./lab1
找 9: 下标 2,比较 3 次
找 7: 下标 -1,比较 5 次
```

## 步骤 2（L2）：先排序，再二分 {#lab-2}

**思路**：冒泡把数组排成升序，二分才有用武之地——「中间那个元素比 key 大，右边就全大」这个能砍一半的保证，只对有序数组成立。

1. 带 `swapped` 的冒泡排 `{29,10,14,37,13,25}` 得 `10 13 14 25 29 37`；闭区间二分查 25 命中下标 3、查 30 返回 -1。→ 知识点：[第 9 章：排序入门:冒泡、插入、选择(O(n²) 三件套)](/03-data-structures/09-sorting-quadratic)「冒泡排序」一节、[第 11 章](/03-data-structures/11-binary-search)「迭代版:每次砍一半」一节
2. 二分必须先排序：乱序数组里「a[mid] < key 就扔左半」纯属瞎蒙——砍掉的半区里可能正藏着答案。先花一次 O(n²)（小数据）或 O(n log n) 排序，之后每次查找 O(log n)，数据越大越值。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)「排序换速度」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab2.c -o lab2 && ./lab2
排序后: 10 13 14 25 29 37
二分查 25 -> 下标 3
二分查 30 -> -1
```

## 步骤 3（L3）：BST——把「有序」嵌进树形 {#lab-3}

**思路**：BST 用「左小右大」把有序性嵌进树形，查找每比一次甩掉一半子树——前提是树得矮；升序插入会把树拉成一条链，把 O(log n) 拖回 O(n)。

1. 乱序插入 `{50,30,70,20,40,60,80}`：中序 `20 30 40 50 60 70 80` 有序、树高 2（三层近乎填满）；`search(60)` 命中、`search(90)` 落空。→ 知识点：[第 7 章：二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序](/03-data-structures/07-bst)「插入」「查找」两节
2. 升序插入 `1..7`：每个新值都比之前所有值大，一路向右挂，树高 6 = 节点数 - 1——查找最末元素要一路走到底，和链表遍历一个价。→ 知识点：[第 7 章](/03-data-structures/07-bst)「退化:有序插入，BST 退化成链表」一节
3. 树高就是查找的最坏步数：矮树 O(log n)、链树 O(n)。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)（BST 平均 O(log n) vs 退化 O(n)）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab3.c -o lab3 && ./lab3
乱序插入 7 个:
  inorder: 20 30 40 50 60 70 80
  height = 2, search(60)=找到, search(90)=没找到
升序插入 1..7:
  inorder: 1 2 3 4 5 6 7
  height = 6(退化,= 节点数 - 1)
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined lab3.c -o lab3_asan && ./lab3_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 步骤 4（L3）：哈希表——冲突与 rehash {#lab-4}

**思路**：8 个 key 全 `% 7 == 3`，是「哈希函数依赖桶数」的绝佳教材——负载因子一超阈值就得换桶数、全部重新哈希。

1. 8 个全挤 bucket[3]，第 6 个插入后负载因子 0.86 超阈值，rehash 到 17 桶：`3%17=3、10%17=10、17%17=0、24%17=7、31%17=14、38%17=4、45%17=11、52%17=1`，负载因子降到 0.471。→ 知识点：[第 8 章：哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找](/03-data-structures/08-hash-table)「负载因子与扩容」一节
2. 为什么必须重新哈希：`hash(key) = key % n_buckets` 依赖桶数——桶数 7 变 17，同一个 key 算出的桶下标会变；只把桶数组变大、节点不搬，`search` 拿着新桶数去算、永远找不到还挂在旧桶里的节点。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)（rehash 的机制）
3. rehash 循环「先存 next → 新桶数重算 → 头插」全程复用节点、不重新 malloc，ASan 复核 0。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)（复用节点）、[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab4.c -o lab4 && ./lab4
  [rehash] 7 -> 17 桶(load=0.86)
after insert: size=8, n_buckets=17, load=0.471
[ 0]: 17
[ 1]: 52
[ 2]:
[ 3]: 3
[ 4]: 38
[ 5]:
[ 6]:
[ 7]: 24
[ 8]:
[ 9]:
[10]: 10
[11]: 45
[12]:
[13]:
[14]: 31
[15]:
[16]:
search(38) -> found
search(99) -> not found
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined lab4.c -o lab4_asan && ./lab4_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 步骤 5（L4）：大审判——clock() 计时四结构 {#lab-5}

**思路**：同一批数据、同一个 key、同样重复 100 次，四种结构各跑各的量级——计时表就是第 12 章大 O 表的实锤。

1. 线性查找 10 万个元素查最末、重复 100 次，约 6 毫秒；二分每回约 17 次比较、100 回合不到微秒级；BST（中位优先插入造平衡树，树高约 17）和二分同量级；哈希每回 1 次比较，再低一档。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)「真跑对比一、二」两节的同款实验、[第 11 章](/03-data-structures/11-binary-search)（二分）、[第 7 章](/03-data-structures/07-bst)（BST 树高）、[第 8 章](/03-data-structures/08-hash-table)（哈希 O(1)）
2. 二分和 BST 耗时同量级的原因：两者都靠「每次比较砍一半」，一次在数组下标上砍、一次在树形上砍，层数都是 log₂ n；哈希不比较、直接算桶，所以还能再低一档。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)「常见量级」一节
3. 计时数字会随机器负载波动（下表的 ASan 版里线性那行明显变慢是 sanitizer 的开销），但倍数关系稳定——大 O 抓的是趋势不是秒数。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)（「秒数每次跑会略变，趋势稳定」）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab5.c -o lab5 && ./lab5
n=100000,查 99999,重复 100 次:
  线性 O(n)      : 0.006447s
  二分 O(log n)  : 0.000006s (1075x)
  BST  O(log n)  : 0.000005s (1289x)
  哈希 O(1)      : 0.000001s (6447x)
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined lab5.c -o lab5_asan && ./lab5_asan; echo "asan_exit=$?"
n=100000,查 99999,重复 100 次:
  线性 O(n)      : 0.017441s
  二分 O(log n)  : 0.000009s (1938x)
  BST  O(log n)  : 0.000009s (1938x)
  哈希 O(1)      : 0.000001s (17441x)
asan_exit=0
```

## 附加挑战（L5）：双引擎随机交叉验证 {#lab-l5}

**思路**：BST 和哈希集合是两个完全独立的实现，喂同一串随机操作、每一步互查——任何一方出 bug，对方立刻喊停。

1. 两个引擎各管一个集合语义：`set_insert` 已存在返回 0（两边据此同步插入）、`set_delete` 不在返回 0（两边据此同步删除）；查询时 `哈希查找结果 == (BST 查找 != NULL)`，不等立即报错。→ 知识点：[第 7 章](/03-data-structures/07-bst)（插入/查找/删除三情况）、[第 8 章](/03-data-structures/08-hash-table)（链地址查找）、[第 1 章：单链表:节点、指针、把内存串成一条链](/03-data-structures/01-singly-linked-list)（桶链表删除的记前驱）
2. 每 100 次操作自检：BST 中序遍历严格升序（用指针带出上一个值）+ 两引擎节点数相等。随机种子固定为 42，结果可复现。→ 知识点：[第 7 章](/03-data-structures/07-bst)「中序得有序」一节
3. 为什么交叉验证比单测强：单测你只验证「这一个实现对自己记录的正确答案」；交叉验证是让两个实现互相出题、互相对答案——它们内部任何一处（删除三情况的化归、rehash 的搬运、桶链的前驱）写错，另一台引擎都会当场指出。2000 次操作、20 次自检全过、ASan 退出码 0，才算数。→ 知识点：[第 7 章](/03-data-structures/07-bst)、[第 8 章](/03-data-structures/08-hash-table)（两个结构互为考官）

**验证输出**（截取首尾，中间 18 次自检同理）：

```text
$ gcc -std=c11 -Wall -Wextra lab_l5.c -o lab_l5 && ./lab_l5
第  100 次操作后自检通过(中序有序,双引擎节点数 20 一致)
第  200 次操作后自检通过(中序有序,双引擎节点数 34 一致)
...（中间 16 次自检同理）
第 1900 次操作后自检通过(中序有序,双引擎节点数 44 一致)
第 2000 次操作后自检通过(中序有序,双引擎节点数 46 一致)
2000 次随机操作完成,20 次自检全部通过
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined lab_l5.c -o lab_l5_asan && ./lab_l5_asan; echo "asan_exit=$?"
...（输出同普通构建）
2000 次随机操作完成,20 次自检全部通过
asan_exit=0
```

节点数在 20 到 53 之间随机游走——插入和删除大致平衡，两引擎全程咬合。写这道题时踩过一个小坑顺带记下：函数名别叫 `bsearch`，`<stdlib.h>` 里的标准库 `bsearch` 会和你撞名、当场编译冲突——改叫 `bst_search` 就清静了。
