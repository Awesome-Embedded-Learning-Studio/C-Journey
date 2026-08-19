---
title: "阶段 2 Lab 实验参考"
description: "阶段 2 Lab（指针解剖台）的实验参考：六个步骤加 L5 挑战的逐步解答，每步标注知识点链接，所有输出在 WSL Arch（gcc 16.1.1 / clang 22.1.8）真实运行得到，ASan 报告照实贴出。"
chapter: 2
order: 3
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 Lab 题面"
related:
  - "阶段 2 各章"
---

# 阶段 2 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1，L5 另跑 clang 22.1.8）真实运行得到。建议卡住时先看「思路」逐步对照。

## 步骤 1：指针的基本功 {#lab-1}

**难度 L1** · 步骤见 [lab](lab#lab-1)

**思路**：`p` 里装的是 `x` 的地址，所以 `&x` 和 `p` 相等；`&p` 是 `p` 这个指针变量自己占的内存，是另一个地址；`*p = 100` 顺着地址改的是 `x` 本体。

1. `&x == p` 实锤「指针装地址」；`&p` 与 `p` 不同——`p` 自己也是变量、也有地址（第 1 章多级指针的伏笔）。→ 知识点：[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)「指针自己也是个变量，有自己的地址」一节
2. `*p = 100` 之后 `x` 变 100：`*p` 是 `x` 的别名。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)「`&` 取地址，`*` 解引用」一节
3. 四个 `sizeof` 全 8：指针大小由地址空间宽度决定，与指向类型无关。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)「指针的类型与大小」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab1.c -o lab1 && ./lab1
&x = 0x...
p  = 0x...                     ← 和 &x 相等
&p = 0x...                     ← 另一个地址:p 自己的
改 *p 后 x = 100
sizeof(int*) = 8, sizeof(char*) = 8, sizeof(double*) = 8, sizeof(void*) = 8
```

## 步骤 2：指针算术与 past-the-last 哨兵 {#lab-2}

**难度 L2** · 步骤见 [lab](lab#lab-2)

**思路**：`p - a` 是「元素个数差」，自动除掉 `sizeof(int)`；`char s[] = "lab"` 是 `char[4]`，`s + 3` 指向**终止符 `\0` 那个真实元素**（合法、读出来就是 0），这个数组真正的 past-the-last 是 `s + 4`。

1. `sum = 66`（4+8+15+16+23），循环结束 `p` 停在 past-the-last，`p - a = 5`。→ 知识点：[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)「两个指针相减」「用指针遍历数组」两节
2. `char* end = s + 3;` 是「3 个内容字符之后的那个 `\0`」——它**合法可读**（读出来就是终止符）；`q < end` 当循环条件，`q` 永远到不了 `end`，所以循环只打印 `l a b` 三个内容字符。真正「可指向、绝不能解引用」的是 `s + 4`（这个 4 元素数组的 past-the-last）——概念别混。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)「越界与 past-the-last」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab2.c -o lab2 && ./lab2
sum = 66, 走过元素数 = 5
逐字符: l a b
```

## 步骤 3：指针参数三件套 {#lab-3}

**难度 L2** · 步骤见 [lab](lab#lab-3)

**思路**：值传递改副本、传指针改本体；`divmod` 一个 `return` + 一个指针参数凑出「两个返回值」。

1. `swap_bad(3, 7)` 后 `x=3 y=7` 纹丝不动；`swap(&x, &y)` 后 `x=7 y=3`——同一个函数体，差别全在参数是不是指针。→ 知识点：[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)「经典：swap 两个变量」一节
2. `inc_ptr(&n)` 让 `n` 10→11：`*p` 是别名，`*p = *p + 1` 就是 `n = n + 1`。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)「传指针」一节
3. `divmod(20, 6, &rem)` 得 `3 余 2`——`rem` 是输出参数，调用者传 `&rem` 等函数写回。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)「用指针模拟『多返回值』」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab3.c -o lab3 && ./lab3
swap_bad 后: x=3 y=7 (没换)
swap 后: x=7 y=3
inc_ptr 后: n=11
20/6 = 3 余 2
```

## 步骤 4：动态数组与 realloc 的 tmp 模式 {#lab-4}

**难度 L3** · 步骤见 [lab](lab#lab-4)

**思路**：`len == cap` 才扩容，`tmp` 接 `realloc` 的结果、判 `NULL`、成功才赋回 `a`——失败时原 `a` 仍有效，直接覆盖就泄漏加丢数据。

1. 10 个元素从容量 4 起步，扩容两次：4→8→16，最终「容量 16、元素 10」。→ 知识点：[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)「realloc：调整一块已有内存的大小」一节
2. ASan 构建零报告、退出码 0：`malloc`/`realloc`/`free` 全部配对正确。→ 知识点：[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)（ASan 是动态内存代码的护栏）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab4.c -o lab4 && ./lab4
容量翻倍到 8
容量翻倍到 16
最终: 容量=16 元素=10
7 3 9 1 5 2 8 4 6 0
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address lab4.c -o lab4a && ./lab4a
容量翻倍到 8
容量翻倍到 16
最终: 容量=16 元素=10
7 3 9 1 5 2 8 4 6 0                ← 零报告,退出码 0
```

## 步骤 5：ASan 四坑一条龙 {#lab-5}

**难度 L4** · 步骤见 [lab](lab#lab-5)

**思路**：四个坑四种报法，全是第 7 章的原话兑现——UAF 报 `heap-use-after-free`、double-free 报 `attempting double-free`、越界报 `heap-buffer-overflow`、泄漏由内置 LeakSanitizer 在退出时报。修复句：UAF/double-free → `free(p); p = NULL;`（`free(NULL)` 合法）；越界 → 循环边界 `< len`；泄漏 → 谁分配谁释放。

1. UAF：gcc 编译期 `-Wuse-after-free` 先响，ASan 报 `heap-use-after-free`、`READ of size 4`，还列出「在哪 free、在哪 malloc」。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「use-after-free」一节
2. double-free：ASan 报 `attempting double-free` 并指出第一次 `free` 的位置。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「double-free」一节
3. 堆越界：ASan 报 `heap-buffer-overflow`、`WRITE of size 4`，报告写明 `0 bytes after 8-byte region`——`a[2]` 正好踩在第 9 字节。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「堆越界」一节
4. 泄漏：LeakSanitizer 报 `200 byte(s) leaked in 1 allocation(s)`（50 × sizeof(int) 正好 200）。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「内存泄漏」一节

**验证输出**（关键行）：

```text
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address lab5uaf.c -o lab5uaf && ./lab5uaf
lab5uaf.c:8:5: warning: pointer 'p' used after 'free' [-Wuse-after-free=]
==666==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex2-lab/lab5uaf.c:8
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj-ex2-lab/lab5uaf.c:8 in main
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address lab5df.c -o lab5df && ./lab5df
==672==ERROR: AddressSanitizer: attempting double-free on 0x... in thread T0:
    #1 0x... in main /tmp/cj-ex2-lab/lab5df.c:6
SUMMARY: AddressSanitizer: double-free /tmp/cj-ex2-lab/lab5df.c:6 in main
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address lab5ovf.c -o lab5ovf && ./lab5ovf
==678==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
WRITE of size 4 at 0x... thread T0
0x... is located 0 bytes after 8-byte region
    #0 0x... in main /tmp/cj-ex2-lab/lab5ovf.c:8
SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/cj-ex2-lab/lab5ovf.c:8 in main
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address lab5leak.c -o lab5leak && ./lab5leak
==684==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 200 byte(s) in 1 object(s) allocated from:
    #1 0x... in main /tmp/cj-ex2-lab/lab5leak.c:4
SUMMARY: AddressSanitizer: 200 byte(s) leaked in 1 allocation(s).
```

关于「为什么别加 `-O1`」：本机真跑过，`-O1` 下 gcc 会把 double-free 的第二次 `free` 与越界写 `a[2] = 30` 直接优化折叠（UB 让编译器有权这么干），ASan 什么都抓不到、退出码 0。所以抓坑请用默认 `-O0`，别让优化先一步「消灭证据」。

## 步骤 6：内存地图 {#lab-6}

**难度 L4** · 步骤见 [lab](lab#lab-6)

**思路**：六类地址分三组——栈最高、堆居中、程序映像（`.rodata`/`.data`/`.bss`）在低地址块；`nm` 的 `D`/`B`/`T` 与段一一对应；递归的 `&inner` 一路变小证明栈向低地址增长。

1. 栈 `0x7ffc...` > 堆 `0x5b38...` > `.bss`/`.data`（`0x5b37...`，`.bss` 紧挨 `.data`）> `.rodata`（最低）；`global_uninit = 0` 是 `.bss` 启动清 0。→ 知识点：[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)「程序的六大内存区」一节
2. `walk` 三层地址递减（`0x...3234` → `0x...3204` → `0x...31d4`）：越深调用、帧地址越小。→ 知识点：[第 12 章](/02-pointers-memory/12-memory-layout)「栈向低地址增长」一节
3. `nm`：`D global_init`、`B global_uninit`、`T main`。→ 知识点：[第 12 章](/02-pointers-memory/12-memory-layout)「用 nm 看符号落哪个段」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra lab6.c -o lab6 && ./lab6
栈      &stack_local    0x7ffc...      ← 最高
堆      heap            0x5b38...
.data   &global_init    0x5b37...
.data   &static_local   0x5b37...
.bss    &global_uninit  0x5b37...      ← 紧挨 .data
.rodata literal         0x5b37...      ← 程序映像里最低
global_uninit = 0 (.bss 启动清 0)
depth 0: &inner = 0x7ffc...
depth 1: &inner = 0x7ffc...
depth 2: &inner = 0x7ffc...            ← 一路变小
$ nm lab6 | grep -E 'global_(init|uninit)| main$'
0000000000004030 D global_init
000000000000403c B global_uninit
00000000000011d0 T main
```

## 附加挑战（L5）：徒手 memmove {#lab-l5}

**难度 L5** · 步骤见 [lab](lab#lab-l5)

**思路**：重叠安全的密钥是「拷贝方向」——源区间是 `[src, src+n)`；当 `dst` 落在源区间内或其右边时，从头往尾拷会先把「还没读的源字节」覆盖掉，所以必须从尾往头；`dst` 在源左边时则相反。标准库 `memmove` 干的正是这个分支（教材第 11 章的原型旁带过一句「重叠请换 memmove」，这里把它实现出来，改编自 CSAPP/K&R 经典练习）。

1. `n == 0 || d == s` 提前返回；`d < s` 从头往尾、否则从尾往头——两个方向合起来保证任何重叠都安全。→ 知识点：[第 11 章：void* 与字节操作](/02-pointers-memory/11-void-ptr-and-byte-ops)「memcpy 与 memset」一节（重叠的 UB、memmove 的存在理由）、[第 2 章](/02-pointers-memory/02-pointer-arithmetic)（`d < s` 是指针比较，同一块内存内才有定义）
2. 重叠搬移 `1 2 1 2 3 4 7 8`（前四个元素右移两位）、普通搬移 `9 9 8 7`（右移一位）。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（unsigned char* 字节指针）
3. 五组（dst 偏移 0~4 × 长度 2~4 元素）与标准库 `memmove` 的 `memcmp` 全等，gcc 与 clang 双跑结果一致。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（memcmp 对拍）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra labl5.c -o labl5 && ./labl5
重叠搬移: 1 2 1 2 3 4 7 8
普通搬移: 9 9 8 7
对拍 5 组重叠用例,不一致 0 组
$ clang -std=c11 -Wall -Wextra labl5.c -o labl5c && ./labl5c
重叠搬移: 1 2 1 2 3 4 7 8
普通搬移: 9 9 8 7
对拍 5 组重叠用例,不一致 0 组
```
