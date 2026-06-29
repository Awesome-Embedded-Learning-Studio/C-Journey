---
title: "阶段 1 · C 语言基础:20 章导读"
description: "阶段 1 的 20 章怎么读——从程序结构、数据类型、运算符、控制流,到函数、指针、复合类型、动态内存与多文件工程,按主题分组给一条循序渐进的路线。"
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

# 阶段 1 · C 语言基础:20 章导读

这一阶段是整个 C-Journey 的**地基**——从「一行 `gcc` 到底干了什么」开始，把 C 的语法、类型、运算、控制流、函数、指针、复合类型、内存模型一座座搭起来，目标是让你**写得出、看得懂、调得动**基础的 C 程序。20 章按下面的顺序读最顺，每组学完都建议去 [examples/](../../examples/) 找对应小练习跑一遍。

> 这 20 章源自作者自己的《C 语言系统教程》(c_tutorials) 基础篇，按 C-Journey 写作规范适配;所有代码示例都过 `-std=c11 -Wall -Wextra` 实测。

## 路线图

### 🟢 起步:程序怎么跑起来的

- [01 程序结构与编译基础](./01-program-structure-and-compilation.md) — 预处理/编译/汇编/链接四阶段、头文件、第一个程序
- [02A 数据类型基础:整数与内存](./02A-data-types-basics.md) — 整型家族、有符号/无符号、固定宽度、sizeof
- [02B 浮点、字符、const 与类型转换](./02B-float-char-const-cast.md) — 浮点精度、字符编码、隐式转换陷阱

### 🔵 运算与控制流:让数据动起来、让程序会选会循环

- [03A 运算符基础](./03A-operators-basics.md) — 算术/关系/逻辑、短路求值
- [03B 位运算与求值顺序](./03B-bitwise-and-evaluation.md) — 位运算、移位陷阱、序列点
- [04 控制流:选择与重复](./04-control-flow.md) — 分支、循环、switch 穿透、状态机雏形

### 🟣 函数与作用域:把代码组织起来

- [05 函数基础与参数传递](./05-function-basics.md) — 声明/定义/调用、值传递、指针参数、递归
- [06 作用域与存储类别](./06-scope-and-storage.md) — 作用域、`static` 三种用法、链接性

### 🟠 指针:C 的灵魂(也是阶段 2 的前哨)

- [07A 指针入门:地址的世界](./07A-pointer-essentials.md) — 取地址/解引用、指针运算
- [07B 指针与数组、const 和空指针](./07B-pointers-arrays-const.md) — 数组退化、const 组合、NULL/野指针
- [08A 多级指针与声明读法](./08A-multi-level-pointers.md) — 多级指针、指针数组 vs 数组指针、cdecl
- [08B restrict、不完整类型与结构体指针](./08B-restrict-incomplete-types.md) — restrict 优化、前向声明、opaque pointer

### 🟡 函数指针、数组与字符串

- [09 函数指针与回调模式](./09-function-pointers-and-callbacks.md) — 函数指针、回调、qsort
- [10 数组深入](./10-arrays-deep-dive.md) — 多维、VLA、数组与指针的区别
- [11 C 字符串与缓冲区安全](./11-c-strings-and-buffer-safety.md) — `strncpy`/`snprintf`、越界防御

### 🔴 复合类型:把数据组织起来

- [12 结构体与内存对齐](./12-struct-and-memory-alignment.md) — struct、padding、`offsetof`、位域
- [13 联合体、枚举、位域与 typedef](./13-union-enum-bitfield-typedef.md) — union/enum/typedef 的取舍

### ⚙️ 工程化收尾:走向真实项目

- [14 动态内存管理](./14-dynamic-memory.md) — malloc/calloc/realloc/free、泄漏、Valgrind
- [15 预处理器与多文件工程](./15-preprocessor-and-multifile.md) — 宏、条件编译、头文件、多文件组织
- [16 文件 I/O 与标准库概览](./16-file-io-and-stdlib.md) — fopen/fread/fwrite、stdio 入门（深化见阶段 5）

## 学完之后

阶段 1 走完，你已经能写、能读、能调基础 C。接下来：
- **阶段 2**([指针、内存布局](../02-pointers-memory/)):把指针和内存模型再往深里砸,补上 [C 陷阱与坑](../02-pointers-memory/1-c-pitfalls-and-traps.md);
- **阶段 3**([数据结构](../03-data-structures/)):用阶段 1 学的指针/结构体/动态内存,手搓链表、动态数组;
- **阶段 4**([工程化](../04-engineering/)):CMake、库、ASan/UBSan——把零散的 `.c` 变成可维护的工程。

> 想跳读也行:有 Python/Java 基础的,可以从 **07 指针** 直接切入(那是 C 和其他语言分水岭);只想速通语法的,01→04→05→07 主线即可,其余按需查。
