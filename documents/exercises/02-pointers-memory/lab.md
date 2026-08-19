---
title: "阶段 2 Lab：指针解剖台——从 &x 到内存地图"
description: "指针与内存阶段的动手实验：把指针别名、指针算术、指针参数、动态数组扩容、ASan 抓四坑、六大内存区分层串成一条解剖线——六步从 &x 与别名一路解剖到内存地图，最后附一道徒手 memmove 的 L5 挑战（改编自 CSAPP/K&R 风格练习）。每步有目标、步骤与验收标准，实验参考独立成文件。"
chapter: 2
order: 2
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 第 1~12 章"
related:
  - "阶段 2 Homework"
  - "阶段 2 Project"
---

# 阶段 2 Lab：指针解剖台——从 &x 到内存地图

## 实验目标

Homework 是一道道独立的题，这个 Lab 则是一条贯穿的线：我们从「`&x` 和 `*p` 这一对运算符」出发，一路解剖到「整张内存地图」——先看指针怎么隔着地址动变量，再让它走数组、进函数、管堆内存，然后亲手用 ASan 把动态内存的四个坑一个个抓现行，最后站到高处把 `.text`/`.rodata`/`.data`/`.bss`/堆/栈六大区分层画出来。做完这个 Lab，阶段 2 的 12 章就不是 12 个孤立知识点，而是一条你亲手摸过的「地址流水线」。

所有实验都在 `/tmp/cj-ex2-lab/` 下做（这个目录每个程序各自一个 `.c`，别混在一个文件里）。每步有验收标准，输出对得上才算过；卡住先回[题面标注的章节链接](#lab-1)读教材，再不行就看[实验参考](lab-solutions)。

## 步骤 1：指针的基本功 {#lab-1}

难度 **L1** · 涉及[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)

**目标**：把「指针 = 装着地址的变量」这件事钉死——`&x` 与 `p` 相等、`&p` 是另一回事、`*p` 是别名、指针大小与类型无关。

1. 写 `lab1.c`：`int x = 42; int* p = &x;`，打印 `&x`、`p`、`&p` 三个地址。
2. 通过 `*p` 把 `x` 改成 100，再打印 `x`。
3. 打印 `sizeof(int*)`、`sizeof(char*)`、`sizeof(double*)`、`sizeof(void*)`。

**验收标准**：贴出输出；说清三件事——①`&x` 和 `p` 为什么相等、②`&p` 和 `p` 为什么不同、③四个 `sizeof` 全一样说明了什么（对应[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)）。

[实验参考 →](lab-solutions#lab-1)

## 步骤 2：指针算术与 past-the-last 哨兵 {#lab-2}

难度 **L2** · 涉及[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)

**目标**：亲手验证「步长 = sizeof(指向类型)」与「数组末尾之后那位（past-the-last）可以指向、不能解引用」。

1. 写 `lab2.c`：对 `int a[5] = {4, 8, 15, 16, 23};` 用指针遍历（不许用下标）求 `sum`，循环结束后打印 `p - a`。
2. 对 `char s[] = "lab";`（`char[4]`，含 `\0`）用 `char* end = s + 3;`（正好指向**终止符 `\0` 那个真实元素**）加指针比较遍历，逐个打印字符。

**验收标准**：贴出输出；说清三件事：`p - a` 为什么是 5（不是 20 字节）；`s + 3` 指向的元素为什么合法可读（读出来是 `\0`）；这个数组真正的 past-the-last 是 `s + 4`——**那**一位才是「可指向、绝不能解引用」的对象（对应[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)）。

[实验参考 →](lab-solutions#lab-2)

## 步骤 3：指针参数三件套 {#lab-3}

难度 **L2** · 涉及[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)

**目标**：对照值传递与传指针，并跑一遍「多返回值」套路。

1. 写 `lab3.c`：`swap_bad(int a, int b)`（值传递）与 `swap(int* a, int* b)`（指针版），各调用一次，对照输出。
2. 写 `inc_ptr(int* p)` 通过指针加一，调用后打印原变量。
3. 写 `divmod(int a, int b, int* rem)`：商 `return`、余数用指针带出，调用 `divmod(20, 6, &rem)`。

**验收标准**：贴出输出；说清 `swap_bad` 为什么「换了等于没换」、`divmod` 的第三个参数为什么必须传 `&rem`（对应[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)）。

[实验参考 →](lab-solutions#lab-3)

## 步骤 4：动态数组与 realloc 的 tmp 模式 {#lab-4}

难度 **L3** · 涉及[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)、[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)

**目标**：把 10 个输入元素塞进「容量从 4 起步」的动态数组，亲手写对 `realloc` 的 tmp 模式，再用 ASan 确认全程干净。

1. 写 `lab4.c`：`int input[] = {7, 3, 9, 1, 5, 2, 8, 4, 6, 0};`，容量 `cap = 4` 起步，放满就 tmp 模式翻倍扩容，每次扩容打印新容量，最后打印全部元素与最终容量。
2. 用 `-O0 -g -fsanitize=address` 重编译运行一遍，确认零报告。

**验收标准**：贴出两次扩容的容量（4→8→16）、最终输出与 ASan 零报告；说清 `int* tmp = realloc(a, ...)` 为什么不能简化成 `a = realloc(a, ...)`（对应[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)）。

[实验参考 →](lab-solutions#lab-4)

## 步骤 5：ASan 四坑一条龙 {#lab-5}

难度 **L4** · 涉及[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)

**目标**：把 UAF、double-free、堆越界、泄漏四个坑亲手埋一遍、亲手抓一遍，各写一句修复。

1. 写四个小文件：`lab5uaf.c`（`free` 后还 `printf("%d", *p)`）、`lab5df.c`（同一块 `free` 两次）、`lab5ovf.c`（`malloc(2 * sizeof(int))` 后写 `a[2]`）、`lab5leak.c`（`malloc(50 * sizeof(int))` 后忘了 `free`）。
2. 全部用 `-O0 -g -fsanitize=address` 编译运行，收集四份报告。
3. 给每个坑写一句修复（注意：堆越界那一句要落到「边界写成 `< len`」，泄漏那一句要落到「谁分配谁释放」）。

**验收标准**：贴出四份报告的关键行（错误类型、`READ`/`WRITE of size`、`SUMMARY`、泄漏字节数）；注意 ASan 构建**不要加 `-O1`**——做完想一想为什么（本机真跑过：`-O1` 会把 double-free 和越界写直接优化折叠、ASan 抓个空，对应[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)）。

[实验参考 →](lab-solutions#lab-5)

## 步骤 6：内存地图 {#lab-6}

难度 **L4** · 涉及[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)

**目标**：把阶段 2 的终点站画出来——六大区地址分层 + `nm` 符号落段 + 栈向低地址。

1. 写 `lab6.c`：打印栈局部、堆（`malloc`）、`.data` 全局、`.data` static 局部、`.bss` 全局、`.rodata` 字面量六类地址，再打印未初始化全局的值。
2. 加一个递归 `walk(int depth)` 打印每层局部变量地址（递归到 depth 2）。
3. 对可执行文件跑 `nm`，把两个全局符号与 `main` 的类型字母贴出来。

**验收标准**：贴出输出；按地址从高到低说出六类各落在哪个段、`nm` 的 `D`/`B`/`T` 各代表什么、递归地址一路变小说明什么（对应[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)）。

[实验参考 →](lab-solutions#lab-6)

## 附加挑战（L5）：徒手 memmove {#lab-l5}

**目标**：徒手实现重叠安全的 `memmove`，并与标准库对拍。改编自 CSAPP/K&R 的经典练习（**如实标注来源**）：拷贝方向由「目标相对源的位置」决定。

1. 写 `void* my_memmove(void* dst, const void* src, size_t n)`：只用 `unsigned char*` 字节指针；`n == 0` 或 `dst == src` 直接返回；`dst < src` 时从头往尾拷，否则从尾往头拷（想清楚为什么这个分支保证重叠也安全——源区间是 `[src, src+n)`）。
2. 验证：`int overlap[8] = {1..8}`，`my_memmove(overlap + 2, overlap, 4 * sizeof(int))` 后打印数组（应为 `1 2 1 2 3 4 7 8`）。
3. 对拍：五组不同 `dst` 偏移 × 不同长度的用例，`my_memmove` 与标准库 `memmove` 的 `memcmp` 必须全等。

**验收标准**：贴出重叠搬移与普通搬移的输出、对拍结果（0 组不一致）；一句话说清「从尾往头拷」为什么在 `dst > src` 时安全、在 `dst < src` 时却会翻车。

[实验参考 →](lab-solutions#lab-l5)

## 提交物清单

一个目录装下全部源码、每步终端记录（`stepN.log`）、以及 200 字以内的小结——用你自己的话说清「指针隔着一层地址动内存」这件事你在哪一步看得最真切。
