---
title: "整型家族与 sizeof：int 不是你想的「固定 4 字节」"
description: "很多人以为 C 的 int 就是 4 字节、long 就是 8 字节——这一章打破这个迷思。C 的整数是一大家子（char/short/int/long/long long，各有 signed/unsigned），而且它们的大小是「实现定义」的：标准只规定了相对关系，不规定绝对字节数。我们用 sizeof 真跑出本机各整型的大小和范围（int=4、long=8，因为这台 64 位 Linux 是 LP64），讲清 sizeof 是个返回 size_t 的编译期运算符、limits.h 给范围、<stdint.h> 的定宽类型（int32_t/int64_t）为什么是跨平台的首选，并点破那个最坑的平台差异——同样是 64 位，Linux 的 long 是 8 字节（LP64），Windows 的 long 却是 4 字节（LLP64）。"
chapter: 1
order: 2
tags:
  - host
  - type
difficulty: beginner
reading_time_minutes: 12
platform: host
c_standard: [11, 99]
prerequisites:
  - "第 1 章：程序结构与编译四阶段"
related:
  - "第 3 章：整型提升、溢出与回绕（这些大小的整型在一起运算会发生什么）"
  - "第 10 章：数组（数组作函数参数退化成指针后 sizeof 的坑）"
---

> 🟡 状态：待审核（2026-06-30）

# 整型家族与 sizeof：int 不是你想的「固定 4 字节」

## 引言：C 的整数是一大家子，而且大小不固定

如果你从 Python 之类的高层语言过来，会习惯「整数就是整数，能装多大就装多大」。C 不是。C 的整数是一整个家族——`char`、`short`、`int`、`long`、`long long`，每个还分 `signed`/`unsigned`，而且**它们各自占几个字节，标准并没有写死**。C 标准只规定了它们的「相对大小关系」（`short` 不比 `int` 长、`int` 不比 `long` 长）和「最小保证」（比如 `int` 至少 16 位、`long` 至少 32 位），至于你机器上 `int` 到底是 4 字节还是别的，是「实现定义」的。所以写 C 代码，搞清「这个类型在这台机器上到底多大」是基本功，而工具就是 `sizeof`。

## 整型家族与 sizeof：真跑一次全看见

我们写一段程序，把常见整型的**大小**（`sizeof`）和**取值范围**（`<limits.h>` 的宏）一次性打出来。`sizeof` 是个运算符（不是函数），它返回一个类型或表达式所占的字节数，类型是 `size_t`（所以要用 `%zu` 打印）：

```c
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("类型          sizeof   范围\n");
    printf("char          %zu       %d..%d\n", sizeof(char), CHAR_MIN, CHAR_MAX);
    printf("short         %zu       %d..%d\n", sizeof(short), SHRT_MIN, SHRT_MAX);
    printf("int           %zu       %d..%d\n", sizeof(int), INT_MIN, INT_MAX);
    printf("long          %zu       %ld..%ld\n", sizeof(long), LONG_MIN, LONG_MAX);
    printf("long long     %zu       %lld..%lld\n", sizeof(long long), LLONG_MIN, LLONG_MAX);
    /* 定宽类型见下文 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall sizes.c -o sizes && ./sizes
类型          sizeof   范围
char          1       -128..127
short         2       -32768..32767
int           4       -2147483648..2147483647
long          8       -9223372036854775808..9223372036854775807
long long     8       -9223372036854775808..9223372036854775807
```

挨个看。**只有 `char` 的大小是标准钉死的——永远是 1 字节**（`sizeof(char) == 1` 是 C 标准的规定，ISO/IEC 9899 §6.5.3.4）；其它都是实现定义。这台机器上 `short` 是 2、`int` 是 4、`long` 是 8、`long long` 也是 8。范围（`<limits.h>` 的宏：`INT_MIN`/`INT_MAX`、`LONG_MIN`/`LONG_MAX` 等，见 §5.2.4.2.1）就由大小和有无符号推出来：4 字节的 `int` 是 −21 亿多到 +21 亿多，8 字节的 `long` 能装到约 ±9.2×10¹⁸。`unsigned` 版本就是把负的那一半挪给正数（`unsigned int` 是 0 到约 42 亿），用法一样、头文件里也有对应的 `UINT_MAX` 之类。

`long` 和 `long long` 都是 8——这台 64 位 Linux 上它俩一样大，这在下一节解释。

## sizeof：编译期就知道的事

`sizeof` 有几个要点。第一，它求值在**编译期**（除了变长数组 VLA 这种极少数运行期情形）——也就是说 `sizeof(int)` 在编译时就换成 `4`，程序运行时根本没有「算 sizeof」这个动作。第二，它的返回类型是 `size_t`（定义在 `<stddef.h>`，`<stdio.h>` 也带进来），这是个**无符号**的、能装下「任何对象大小」的整型（这台机器上是 8 字节，所以是 64 位无符号）；打印它要用 `%zu`（旧代码里常见的 `%u`/`%lu` 在 `size_t` 不是 `unsigned`/`unsigned long` 的平台上会出错，第 16 章 CI 里的 `-Wformat` 就会抓这种不匹配）。第三，对「表达式」求 `sizeof` 不实际计算那个表达式——`sizeof(a = 5)` 里的 `a = 5` 不会真的执行（这是「不求值上下文」），这点偶尔会让人困惑。

最容易绊倒新手的一个 sizeof 陷阱，是**数组作为函数参数时会退化成指针**，于是 `sizeof` 得到的是指针大小、不是数组大小。我们会在第 10 章讲数组时专门演示它（gcc 会用 `-Wsizeof-array-argument` 提醒你），这里先记住：想在函数里知道「传进来的数组有几个元素」是办不到的，数组长度得作为额外参数传进去。

## `<stdint.h>`：要可移植，就用定宽类型

既然 `int`、`long` 的大小随平台变，那「我就是要一个确切的 32 位整数」怎么办？C99 引入了 `<stdint.h>`，给你一批**定宽类型**（§7.20）：

```text
定宽类型(<stdint.h>):
int32_t       4
int64_t       8
size_t        8  (sizeof 的返回类型)
```

`int32_t` 恰好 32 位、`int64_t` 恰好 64 位——不管在哪个平台、不管 `int`/`long` 是几字节，`int32_t` 永远是 32 位（前提是平台提供了这样的类型，几乎所有现代平台都有）。需要写协议、操作二进制格式、存档文件结构时，**永远用定宽类型**（`uint32_t`/`int64_t` 等），别用 `int`/`long`，否则「在我的机器上 `int` 是 4 字节、换台机器成 2 字节」会把你的数据布局整个搞乱。`<stdint.h>` 还提供「最快」类型（`int_fast32_t`：至少 32 位里运算最快的）、「至少」类型（`int_least8_t`：至少 8 位里最小的）——日常用 `int32_t`/`int64_t`/`uint8_t` 这些定宽的最多。

## 平台差异：`long` 的 LP64 vs LLP64

回到那个「`long` 为什么是 8」的问题，这里有个跨平台的大坑。同样都是 64 位系统、同样指针都是 8 字节，但**64 位 Linux/macOS 用 LP64 模型**（`long` 和指针都是 8 字节），**64 位 Windows 用 LLP64 模型**（`long` 仍是 4 字节、只有指针是 8 字节）。也就是说，`long` 这一个类型，在 Linux 上是 8 字节、换到 Windows 上是 4 字节——同一段用 `long` 的代码、同样的数据，两个平台上表现不同。这就是为什么可移植代码要用 `int64_t`（在两个平台上都是 8 字节）而不是 `long`（大小会变）。本课程示例都在这台 64 位 Linux（LP64）上跑，所以你看到的 `long` 是 8——但你心里要清楚它换个平台可能不是。

## 小结

C 的整数是一大家子（`char`/`short`/`int`/`long`/`long long`，各有 `signed`/`unsigned`），大小是**实现定义**的——标准只规定相对关系和最小位宽，不规定绝对字节数；唯一钉死的是 `sizeof(char) == 1`。我们用 `sizeof`（一个返回 `size_t` 的编译期运算符，`size_t` 是无符号、打印用 `%zu`）看大小，用 `<limits.h>` 的 `INT_MAX` 之类宏看范围（§5.2.4.2.1）。这台 64 位 Linux（LP64）上 `int`=4、`long`=8、`long long`=8。要写跨平台代码，就用 `<stdint.h>` 的**定宽类型**（`int32_t`/`int64_t`/`uint8_t`，§7.20）——尤其要记住 `long` 的大小会变（Linux LP64 是 8、Windows LLP64 是 4），涉及二进制布局、协议、存档一律用定宽类型、别用 `long`。下一章我们看这些不同大小的整型放在一起运算时会发生什么——整型提升、有符号溢出（UB）和回绕，那是 C 里另一片经典雷区。

## 参考资源

- ISO/IEC 9899:2011 §6.2.5（类型，整型的相对大小关系）、§5.2.4.2.1（`<limits.h>` 的范围宏）、§6.5.3.4（`sizeof` 运算符，`sizeof(char)==1`）、§7.20（`<stdint.h>` 定宽类型）、§7.19（`<stddef.h>` 的 `size_t`）
- 第 3 章：整型提升、溢出与回绕（不同整型一起运算的规则与陷阱）
- 第 10 章：数组（数组退化成指针后 `sizeof` 的坑，`-Wsizeof-array-argument`）
