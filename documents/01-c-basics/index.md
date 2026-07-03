---
title: "阶段 1 · C 语言基底:13 章导读"
description: "阶段 1 的 13 章怎么读——从程序结构、整型与算术、运算符与控制流,到函数、作用域、数组、字符串、IO 与复合类型,按主题分组给一条循序渐进的路线。"
chapter: 1
order: 0
tags:
  - host
  - syntax
difficulty: beginner
reading_time_minutes: 5
platform: host
c_standard: [99, 11]
prerequisites: []
related:
  - "阶段 0:编译流程、GDB 与库"
  - "阶段 2:指针、内存布局与位运算"
---

# 阶段 1 · C 语言基底:13 章导读

这一阶段是整个 C-Journey 的**地基**——从「一行 `gcc` 到底干了什么」开始,把 C 的类型、运算、控制流、函数、作用域、数组、字符串、IO、复合类型一座座搭起来,目标是让你**写得出、看得懂、调得动**基础的 C 程序。13 章按下面的顺序读最顺,每组学完都建议去 [examples/](../../examples/) 找对应小练习跑一遍。

> 全部代码 gcc 16 + clang 22 真编真跑(`-std=c11 -Wall -Wextra`),贴真实输出、引 ISO/IEC 9899 条款。

## 路线图

### 🟢 程序与类型基础:先把「数」搞清楚

- [01 程序结构与编译四阶段](./01-program-structure-and-compilation.md) — 翻译单元、声明 vs 定义、链接,顺带把编译四阶段再过一遍
- [02 整型家族与 sizeof](./02-integer-types-and-sizeof.md) — int 不是固定 4 字节、size_t、limits.h、LP64 vs LLP64
- [03 整型提升、溢出与回绕](./03-integer-promotion-overflow.md) — C 算术三座大山,有符号溢出 UB、无符号回绕
- [04 浮点、字符、常量与隐式转换](./04-float-char-const-cast.md) — 0.1+0.2≠0.3、char 是小整数、隐式转换的坑

### 🔵 运算符与控制流:让数据动起来、让程序会选会循环

- [05 运算符基础](./05-operators-basics.md) — 优先级、自增自减、短路求值、`a[i]=i++` 这个 UB
- [06 位运算与移位](./06-bitwise-and-shift.md) — `&|^~`、移位 UB、标志位三件套
- [07 控制流](./07-control-flow.md) — `if/for/while/switch` 与那个 fall-through 坑

### 🟣 函数与作用域:把代码组织起来

- [08 函数](./08-functions.md) — 值传递、递归、`static` 局部
- [09 作用域、存储期与 static](./09-scope-storage-static.md) — 名字看得见、变量活得久,`static` 的三重含义

### 🟠 数据组织:数组、字符串与复合类型

- [10 数组](./10-arrays.md) — 一排格子,以及它怎么悄悄退化成指针(decay)
- [11 C 字符串与不安全 libc](./11-c-strings-and-libc.md) — `\0` 结尾、strcpy/snprintf 的越界与截断
- [13 结构体、联合、枚举与内存对齐](./13-struct-union-enum.md) — padding、`offsetof`、union 的位模式玩法、FAM

### 🟡 输入输出:跟外面打交道

- [12 基础 IO](./12-basic-io.md) — printf/scanf 的格式化江湖、`%lf`、格式串漏洞

## 学完之后

阶段 1 走完,你已经能写、能读、能调基础 C。接下来:

- **阶段 2**([指针、内存布局](../02-pointers-memory/)):把指针和内存模型往深里砸,四区布局、`void*`、函数指针全在这里;
- **阶段 3**([数据结构](../03-data-structures/)):用阶段 1 学的结构体与动态内存,手搓链表、动态数组、BST、哈希表;
- **阶段 4**([工程化](../04-engineering/)):CMake、库、ASan/UBSan、测试——把零散的 `.c` 变成可维护的工程。

> 想跳读也行:有 Python/Java 基础的,可以从 **10 数组** 或直接进阶段 2 的指针切入;只想速通语法的,01→07→08→10 主线即可,其余按需查。
