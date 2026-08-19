---
title: "阶段 3 课后练习（Homework）"
description: "数据结构与算法阶段的课后练习：12 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战（LRU 缓存，改编自 LeetCode 146 与 CS61B）。难度覆盖 L1~L5 五档，题目都做了变式处理（换场景、换方向），照抄教材例题抄不出答案；参考答案独立成文件，逐步解答附知识点链接。"
chapter: 3
order: 0
tags:
  - host
  - data-structures
  - algorithm
difficulty: intermediate
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 3 全部章节（第 1~12 章）"
related:
  - "阶段 3 Lab：查找进化史"
  - "阶段 3 Project：电话簿双引擎"
---

# 阶段 3 课后练习（Homework）

## 引言

这里的题按章组织，每章两道（基础 + 进阶），最后是两道跨章综合和一道 L5 挑战。每题标注难度档位（L1~L5，见[练习总览](/exercises/)）和涉及章节。题目都是「变式」——换场景、换数据、换推理方向，照抄教材例题是抄不出答案的；本阶段偏难，L1 仅在开篇出现一次，L3/L4 是主战场。

答案在独立的[参考答案](/exercises/03-data-structures/homework-solutions)文件里，按题号对应，每步解答带知识点链接。建议一章做完再看答案。所有代码用 `-std=c11 -Wall -Wextra` 起步（个别题目要求 sanitizer，题面会写明）；凡是 `malloc` 了的程序，交卷前都用 `-fsanitize=address,undefined` 跑一遍，这是本阶段的硬习惯。

## 3.1 单链表

### 3.1-A {#hw-3-1-a}

难度 **L1** · 涉及[第 1 章：单链表:节点、指针、把内存串成一条链](/03-data-structures/01-singly-linked-list)

写一个单链表（节点存 `int`），实现 `new_node`、`push_back`、`push_front`、`print_list`。场景：室内温度记录——先按时间顺序尾插 12:00、14:00、16:00 三个读数（22、25、24）并打印；早上 8:00 的读数（21）后来才补录，用头插放到最前，再打印。回答两问：①`push_front` 为什么必须返回新头指针（不返回会怎样）？②头插是 O(1)、尾插是 O(n)，这个差别是从哪一步操作来的（可以留到第 12 章再正式回答）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-1-a)

### 3.1-B {#hw-3-1-b}

难度 **L3** · 涉及[第 1 章：单链表:节点、指针、把内存串成一条链](/03-data-structures/01-singly-linked-list)、[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

教材的 `remove_value` 只删第一个匹配的节点。现在写 `remove_all`：**删除链表中所有**等于 `val` 的节点，把删除个数写进 `int* removed` 带出来。场景：传感器读数清洗——链表中混进了异常值 3（数据 `{3, 7, 3, 2, 3, 5}`，注意 3 出现在头部、中间、尾部），要求全部剔除。删头节点（`prev == NULL`）和删中间节点两条路径都要覆盖，被删节点逐个 `free`。普通构建跑一遍，再用 `-fsanitize=address,undefined` 复核无泄漏。回答：为什么「删头」和「删中间」的指针接法不一样，漏写删头分支的后果是什么？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-1-b)

## 3.2 双向链表

### 3.2-A {#hw-3-2-a}

难度 **L2** · 涉及[第 2 章：双向链表:prev+next,O(1) 删除与前驱遍历](/03-data-structures/02-doubly-linked-list)

写一个带头哨兵的双向链表（`List { Node* dummy; }`），场景：浏览器标签页编号列表。除教材的 `push_back`/`push_front`/正反向打印外，再补两个函数：`count` 统计真节点个数、`is_empty` 判空（`dummy->next == dummy`）。程序按这个顺序走：打印空表的 count 和 is_empty；尾插 1、2、3 后打印 count、正向、反向；头插 0 后打印 count 和正向。回答：哨兵是怎么让「往空表插第一个节点」和「往有数据的表插」走完全相同的四行代码的？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-2-a)

### 3.2-B {#hw-3-2-b}

难度 **L3** · 涉及[第 2 章：双向链表:prev+next,O(1) 删除与前驱遍历](/03-data-structures/02-doubly-linked-list)

实现 `move_to_front(List* L, Node* n)`：把表里已存在的一个节点 `n` 移到表头——先两行重连把它摘下来（教材 `delete_node` 的摘除部分），再四行头插（教材 `push_front`）。场景：最近使用文件列表。建表 `1 2 3`，依次执行「用 3 → 打印」「再用 2 → 打印」「2 已经在最前，再移一次 → 打印」，每一步都验证结构没坏。用 ASan 复核。回答：为什么单链表做不到「给定节点指针 O(1) 移位置」——单链表缺的是什么？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-2-b)

## 3.3 栈

### 3.3-A {#hw-3-3-a}

难度 **L2** · 涉及[第 3 章：栈:LIFO、数组与链表两种实现、括号匹配实战](/03-data-structures/03-stack)

写一个数组栈（`STACK_SIZE = 5`，`top = -1` 表示空栈的约定），实现 `push`/`pop`/`is_empty`/`is_full`。场景：一段录音要倒着放——把音符编号 `{1, 2, 3, 4, 5}` 按顺序压栈，验证栈满；再逐个弹出打印（应得什么顺序？）；最后再压 5 个数、尝试压第 6 个，验证第 6 个被 `is_full` 拦住而不是写越界。回答：`push` 为什么是 `data[++top]`（前置 `++`）而 `pop` 是 `data[top--]`（后置 `--`）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-3-a)

### 3.3-B {#hw-3-3-b}

难度 **L3** · 涉及[第 3 章：栈:LIFO、数组与链表两种实现、括号匹配实战](/03-data-structures/03-stack)

教材正文留了一道练习：括号匹配从「只支持圆括号」升级到**三种括号 + 类型匹配**。写 `check_brackets`：遇 `(`/`[`/`{` 入栈，遇 `)`/`]`/`}` 弹栈，弹出的开括号类型必须和闭括号对得上，否则判 `MISMATCH`；枚举四个结果 `OK / EXTRA_RIGHT / EXTRA_LEFT / MISMATCH`。测试这 6 个用例：`{[()]}`、`([)]`、`{{}`、`[]))`、`{a+(b*[c-d])}`（混入字母和运算符，应跳过）、`""`（空串）。场景：代码编辑器的括号配对检查。回答：如果只用计数器（记左括号个数），哪种情况判断不出来、为什么必须用栈？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-3-b)

## 3.4 队列

### 3.4-A {#hw-3-4-a}

难度 **L2** · 涉及[第 4 章：队列:FIFO、环形缓冲与链表实现](/03-data-structures/04-queue)

写一个环形缓冲队列（`N = 5`，「留一格不填」方案，最多存 4 个），带 `debug` 打印每步的 `head/tail/empty/full`。场景：打印作业缓冲——按这个操作序列走：push 10 20 30 40（判满）；push 50（应被拦，返回 -1）；pop 两个（应得 10、20）；再 push 50、60（观察 tail 回绕到 0）；把剩下的全部按序出队。贴出每一步的 head/tail 状态。回答：`(tail+1) % N == head` 为什么是「满」、`head == tail` 为什么是「空」，两个条件为什么不会打架？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-4-a)

### 3.4-B {#hw-3-4-b}

难度 **L3** · 涉及[第 4 章：队列:FIFO、环形缓冲与链表实现](/03-data-structures/04-queue)

经典约瑟夫问题（**教材外补充**：问题场景出自经典数学问题「约瑟夫环」，解法只用到本章链表队列的 enqueue/dequeue，不引入任何新结构）：n=7 个人围成一圈，从 1 号开始报数，报到 3 的人出列，然后从下一个人重新报数，问出列顺序。用第 4 章的链表队列（head/tail 双指针）模拟：报 1、2 的人不出列——从队头弹出、立即入队到队尾（围成一圈）；报 3 的人弹出出列。打印完整出列序列，用 ASan 复核。回答：这一题里队列的「队头」和「队尾」各自扮演什么角色？为什么这题不用数组、用链表队列更自然？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-4-b)

## 3.5 动态数组

### 3.5-A {#hw-3-5-a}

难度 **L2** · 涉及[第 5 章：动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)](/03-data-structures/05-dynamic-array)

写动态数组四件套 `vec_init`/`vec_push`/`vec_get`/`vec_free`（扩容用 realloc 的 tmp 模式）。场景：期末成绩陆续录入、事先不知道几份——初始容量给 **2**，连续 push 7 个成绩（10、20、…、70），每步打印 `size` 和 `capacity`；打印全部内容；用 `vec_get` 访问下标 7（越界，应被拦）和下标 6（应得 70）。用 ASan 复核无泄漏。回答：`size` 和 `capacity` 为什么要分成两个变量存？如果只存一个会出什么问题？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-5-a)

### 3.5-B {#hw-3-5-b}

难度 **L3** · 涉及[第 5 章：动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)](/03-data-structures/05-dynamic-array)、[第 3 章：栈:LIFO、数组与链表两种实现、括号匹配实战](/03-data-structures/03-stack)

教材第 3 章预告过：「第 5 章我们会用 realloc 做动态数组栈，栈满了就自动扩容，从根本上解决这事」。现在把它实现出来：`VecStack { int* data; size_t size; size_t capacity; }`，push 满则**翻倍** realloc（tmp 模式）、pop 只减 `size` 不缩容、peek 看栈顶、空栈 pop 返回失败。初始容量 4，连续 push 1..10 打印扩容轨迹，peek、逐个 pop 到空，再验证空栈 pop 被拦，`stack_destroy` 释放。用 ASan 复核。回答：为什么 pop 不用缩容（结合第 12 章的均摊思想回答）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-5-b)

## 3.6 二叉树

### 3.6-A {#hw-3-6-a}

难度 **L2** · 涉及[第 6 章：二叉树基础:节点、前/中/后序遍历、递归与释放](/03-data-structures/06-binary-tree)

手动建一棵和教材不同形状的树（公司组织架构编号）：根 8，左子 3、右子 10；3 的左子 1、右子 6；10 的右子 14。打印前序、中序、后序三种遍历；再写两个递归函数 `count_nodes`（节点数）和 `count_leaves`（叶子数），打印结果。用 ASan 复核后序释放无泄漏。回答：三种遍历的代码结构完全一样、差别只在 `printf` 那行的位置——中序遍历的一个特殊性质（第 7 章 BST 会用到）是什么？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-6-a)

### 3.6-B {#hw-3-6-b}

难度 **L3** · 涉及[第 6 章：二叉树基础:节点、前/中/后序遍历、递归与释放](/03-data-structures/06-binary-tree)、[第 4 章：队列:FIFO、环形缓冲与链表实现](/03-data-structures/04-queue)

层序遍历（408 真题风格，**教材外补充**：层序 = 广度优先 BFS 的思想，教材只讲了深度优先的三种遍历；这里用第 4 章队列承载它）。写一个数组队列（size 计数版，装的元素是 `Node*`），实现 `level_order`：根入队；出队一个节点就访问它，再把它的左右孩子（非 NULL）入队；队空结束。对 3.6-A 那棵树打印层序结果。用 ASan 复核。回答：为什么层序不能像前/中/后序那样纯递归写——队列在这题里替递归承担了什么？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-6-b)

## 3.7 二叉搜索树 BST

### 3.7-A {#hw-3-7-a}

难度 **L2** · 涉及[第 7 章：二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序](/03-data-structures/07-bst)

写 BST 的 `insert`/`search`/`find_min`/中序遍历/后序释放。场景：座位号乱序入场——按 `{4, 2, 6, 1, 3, 5, 7}` 依次插入（和教材的 `{5,3,8,1,4,7,9}` 不同的一组数），中序打印、`find_min` 打印最小值、`search(5)` 和 `search(8)` 各查一次。用 ASan 复核。回答：同一组数按不同顺序插入，长出来的树形状可以完全不同——这为第 7 章末尾的「退化」埋了什么伏笔？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-7-a)

### 3.7-B {#hw-3-7-b}

难度 **L4** · 涉及[第 7 章：二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序](/03-data-structures/07-bst)

写一个「BST 体检仪」`is_bst`（CS61B 风格练习；**教材外补充**：范围检查法是教材第 7 章体检手段「中序遍历看有序」之外的另一种判法）：用**范围检查法**递归验证——每个节点值必须落在开区间 `(min, max)` 内，向左递归时上界收紧为根值、向右递归时下界收紧为根值。程序按这个顺序走：插入 `{5, 3, 8, 1, 4, 7, 9}` 验证为真；**人为破坏**——把节点 1 改成 10（1 是 3 的左孩子，改完左子树里出现比 3 大的值），验证为假；改回来，删除 8（双子节点、后继 9 顶上），打印中序并再验一次为真。用 ASan 复核。回答：为什么「每个节点只和它的左右孩子比大小」不够，必须带着 `(min, max)` 区间一路收紧？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-7-b)

## 3.8 哈希表

### 3.8-A {#hw-3-8-a}

难度 **L2** · 涉及[第 8 章：哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找](/03-data-structures/08-hash-table)

写一个链地址哈希表（桶数 **5**，`hash(key) = key % 5`，头插、查找、打印桶分布、逐个 free）。场景：车牌号散列——插入 `{3, 8, 13, 18, 23}`（五个都 `% 5 == 3`，全挤进 bucket[3]），再插 `{1, 6}`（都落 bucket[1]）。打印每个桶的链表、负载因子，`search(13)` 和 `search(99)` 各查一次。用 ASan 复核。回答：bucket[3] 挂出 5 个节点意味着什么？如果在这个桶里找最先进来的那个 key，要比较几次？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-8-a)

### 3.8-B {#hw-3-8-b}

难度 **L4** · 涉及[第 8 章：哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找](/03-data-structures/08-hash-table)

教材在 rehash 一节的「教学版诚实交代」里说：真实库的桶数组是 malloc 出来的、扩容时换掉整个数组。现在把这件事做完——实现**动态桶数组哈希表**：`Node** buckets` 用 `calloc` 分配；负载因子 `size / n_buckets > 0.75` 时，realloc 出一块更大的新桶数组（容量沿质数表 `{5, 11, 23, 47, 97}` 往上跳），把所有节点**复用**（摘下 → 新桶数重新哈希 → 头插，不重新 malloc）；`ht_destroy` 逐个 free 节点再 free 桶数组。测试：从 5 桶起步，插 10 个都 `≡ 2 (mod 5)` 的 key `{2,7,12,17,22,27,32,37,42,47}`，逼出两次 rehash，打印每次 rehash 的桶数变化与最终负载因子，`search(27)`/`search(99)` 验证，ASan 复核。回答：为什么扩容必须把每个 key 重新哈希一遍，而不是只把桶数组变大？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-8-b)

## 3.9 排序入门

### 3.9-A {#hw-3-9-a}

难度 **L2** · 涉及[第 9 章：排序入门:冒泡、插入、选择(O(n²) 三件套)](/03-data-structures/09-sorting-quadratic)

写冒泡排序（带 swapped 提前结束）和插入排序（相邻交换口径，和教材一致），各自插计数器统计**比较次数**和**交换次数**，跑同一份成绩单 `{7, 3, 9, 1, 4}`。动笔之前先手算这组数据的逆序对个数，再真跑对答案。回答：①为什么两个算法的交换次数必然相同？②比较次数为什么不同、差在哪一步？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-9-a)

### 3.9-B {#hw-3-9-b}

难度 **L3** · 涉及[第 9 章：排序入门:冒泡、插入、选择(O(n²) 三件套)](/03-data-structures/09-sorting-quadratic)

教材引言讲了稳定性的实战价值：「先按部门排、再按年龄排」，靠稳定性叠出多关键字排序。现在把这句话变成实验——员工表 `{部门, 工资}`，部门（A/B）上一轮已经排好，这一轮按工资**稳定**排序。数据：`{{'A',5000}, {'B',5000}, {'A',8000}, {'B',3000}}`——两个工资 5000 的员工分属 A、B 部门。用插入排序排一遍，再用选择排序排一遍，打印两张结果表，重点盯两个 5000 的相对先后。回答：①插入排序为什么稳定（比较符号写什么才稳）？②选择排序为什么天然不稳定（跨距交换如何翻转了相等的元素）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-9-b)

## 3.10 快排与归并

### 3.10-A {#hw-3-10-a}

难度 **L2** · 涉及[第 10 章：快排与归并:分治、O(n log n)、对照 qsort](/03-data-structures/10-quicksort-mergesort)

手写归并排序（merge 只拷左半边进临时数组），排库存编号 `{9, 4, 7, 1, 3}`。再用带卫星数据的 `Item {value, tag}` 排 `{{5,'A'}, {2,'X'}, {5,'B'}, {1,'Y'}}`，验证排完后 `5A` 仍在 `5B` 前面（稳定）。用 ASan 复核（重点盯每次 merge 的 malloc 都有对应 free）。回答：merge 里那句 `left[i] <= a[j]` 的 `<=` 换成 `<` 会怎样？为什么这个符号决定稳定性？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-10-a)

### 3.10-B {#hw-3-10-b}

难度 **L4** · 涉及[第 10 章：快排与归并:分治、O(n log n)、对照 qsort](/03-data-structures/10-quicksort-mergesort)

教材说「三数取中」能缓解快排最坏情况，但只讲不写。现在写出来，并做对照实验：实现两个版本的分区——①固定取 `a[high]` 当 pivot（教材 Lomuto 版）；②三数取中——把 `a[lo]`/`a[mid]`/`a[hi]` 排成升序后，**中位数换到 pivot 位** `a[hi]` 再走 Lomuto。两个版本都插比较计数器，输入都是升序 `1..20`（这是固定版的最坏输入），各自排序并打印比较次数、验证结果有序。回答：①为什么升序数据是固定取尾 pivot 版的最坏情况？②三数取中为什么能切开这个最坏情况？③如果排完三数后忘了把中位数挪到 pivot 位（中位数还停在 `a[mid]` 上），会发生什么——这一问的答案文件里有真实翻车现场。

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-10-b)

## 3.11 二分查找

### 3.11-A {#hw-3-11-a}

难度 **L2** · 涉及[第 11 章：二分查找:有序数组、O(log n)、bsearch](/03-data-structures/11-binary-search)

写迭代版二分查找（闭区间 `[lo, hi]` 配套写法）。场景：货架上的商品编号升序摆放 `{2, 5, 8, 12, 16, 23, 38, 56, 72, 91}`，查 23 和 30，打印下标（找不到 -1）。回答：`mid` 为什么必须写 $lo + \frac{hi - lo}{2}$ 而不是 $\frac{lo + hi}{2}$？什么规模的数据会引爆 $\frac{lo + hi}{2}$？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-11-a)

### 3.11-B {#hw-3-11-b}

难度 **L3** · 涉及[第 11 章：二分查找:有序数组、O(log n)、bsearch](/03-data-structures/11-binary-search)

二分查找的经典边界变式（408 真题风格）：写 `lower_bound(const int* a, int n, int key)`——返回**第一个 `a[i] >= key` 的下标**，全小于 key 时返回 n。这版改用半开区间 `[lo, hi)` 配套写法（循环 `lo < hi`、命中候选时 `hi = mid` 而不是 `mid - 1`）。场景：版本号列表 `{1, 3, 3, 3, 5, 7, 9}`（含重复），测 `lower_bound(3)`、`lower_bound(4)`（4 不在表里）、`lower_bound(1)`、`lower_bound(10)`。回答：①为什么这题用半开区间 + `hi = mid` 比闭区间顺手（提示：答案候选区间和「扔掉」区间的写法）？②如果把闭区间的 `hi = mid - 1` 直接搬到半开区间会出什么 bug？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-11-b)

## 3.12 算法复杂度与大 O

### 3.12-A {#hw-3-12-a}

难度 **L2** · 涉及[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)

两部分。①**判断**：给下面 10 个操作/算法标复杂度（从 O(1)/O(log n)/O(n)/O(n log n)/O(n²) 里选）：链表头插、链表按值查找、栈 push、环形缓冲入队、动态数组 push（均摊）、BST 平衡插入、哈希表平均查找、冒泡排序、快排平均、二分查找。②**真跑**：给选择排序插比较计数器，n=6（数据 `{5,2,8,1,9,3}`）和 n=10 各跑一次，打印比较次数，验证它恒等于 $\frac{n(n-1)}{2}$。回答：为什么选择排序的比较次数和输入长什么样**无关**——这是它相对冒泡/插入的一个「个性」。

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-12-a)

### 3.12-B {#hw-3-12-b}

难度 **L4** · 涉及[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)、[第 5 章：动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)](/03-data-structures/05-dynamic-array)

教材断言「扩容每次只 +1 会退化成 O(n²)」——把这句话变成实测。写两个动态数组：`grow_double`（翻倍扩容）和 `grow_plus1`（+1 扩容），各自在结构体里加一个 `copies` 计数器，每次扩容把「搬家拷贝了多少个旧元素」累加进去。n=10000，分别 push 1..10000，打印各自的拷贝总数、平均每次 push 的写入次数、两者比值。回答：①为什么 +1 扩容的拷贝总量是 O(n²)、翻倍是 O(n)？②把「偶尔 O(n) 的扩容」摊到 n 次 push 上，为什么翻倍方案每次 push 均摊还是 O(1)？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-12-b)

## 3.C 跨章综合与挑战

### 3.C-1 {#hw-3-c-1}

难度 **L3** · 涉及[第 5 章：动态数组](/03-data-structures/05-dynamic-array)、[第 10 章：快排与归并](/03-data-structures/10-quicksort-mergesort)、[第 11 章：二分查找](/03-data-structures/11-binary-search)

综合题：做一个「成绩查询系统」的骨架，把三章的零件拧成一条流水线。①动态数组（初始容量 2）装进 8 个期末成绩 `{72, 95, 58, 88, 67, 91, 74, 83}`（模拟「录入数量未知」）；②归并排序把 `v->data` 排成升序；③二分查找查 74（应在表里）和 60（应不在）。打印排序后的完整数组和两次查找结果，用 `-fsanitize=address,undefined` 复核。回答：这条流水线里三章各贡献了什么、为什么顺序不能乱（为什么不能先二分再排序）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-c-1)

### 3.C-2 {#hw-3-c-2}

难度 **L4** · 涉及[第 8 章：哈希表](/03-data-structures/08-hash-table)、[第 1 章：单链表](/03-data-structures/01-singly-linked-list)

综合题：访问日志**保序去重**。日志里的页面编号序列是 `{3, 1, 3, 2, 1, 4, 3}`——要求去掉重复、但保留每个编号**首次出现**的顺序，输出 `3 1 2 4`。分工：哈希集合（链地址，桶数 7）只负责「这个编号见没见过」（`set_add` 返回 1 表示新插入），单链表只负责按出现顺序把首次见到的编号追加进去。统计拦下重复的次数，用 ASan 复核。回答：①这个分工里哈希表把整体复杂度从朴素双重循环的 O(n²) 压到了多少？②为什么不能只用哈希表（它保不了序）、为什么不能只用链表（它判重是 O(n)）？

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-c-2)

### 3.C-3 {#hw-3-c-3}

难度 **L5** · 涉及[第 2 章：双向链表](/03-data-structures/02-doubly-linked-list)、[第 8 章：哈希表](/03-data-structures/08-hash-table)、[第 1 章：单链表](/03-data-structures/01-singly-linked-list)

挑战题（改编自 LeetCode 146「LRU 缓存」，CS61B 数据结构课程的同款练习，如实标注）：实现一个 LRU（Least Recently Used，最近最少使用）缓存，`get(key)` 返回缓存里 key 对应的值、没有则返回 -1；`put(key, value)` 写入/更新，容量满时**淘汰最久未使用**的那条；get 和 put 都要求平均 O(1)。用阶段 3 的家当搭它：**哨兵双向链表**（第 2 章）按「最近使用在前、最久未用在尾」排序——`dummy->next` 是最近使用、`dummy->prev` 是最久未用；**链地址哈希表**（第 8 章）从 key 直接定位节点；淘汰和移动全靠第 2 章的摘除/头插、第 1 章的「记前驱」删除。

容量 2，按 LeetCode 官方示例的操作序列真跑：`put(1,1)`、`put(2,2)`、`get(1)`、`put(3,3)`、`get(2)`、`put(4,4)`、`get(1)`、`get(3)`、`get(4)`，打印每次 get 的返回值。用 `-fsanitize=address,undefined` 复核无泄漏。动笔前先想清楚一道送命题：**双向链表的 next 指针和哈希桶链的链接指针能不能共用同一个字段**？如果共用，`push_front` 改 next 时会把桶链改成什么样——这题的答案文件里有真实的死循环翻车现场和修复方法。

[参考答案 →](/exercises/03-data-structures/homework-solutions#hw-3-c-3)
