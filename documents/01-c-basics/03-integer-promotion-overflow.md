---
title: "整型提升、溢出与回绕：C 的算术三座大山"
description: "上一章我们认识了整型家族——这一章看它们一起运算时会发生什么，是 C 里最经典的一片雷区。三件事真跑给你看：第一，char + char 的结果不是 char、是 int（这叫整型提升，小类型一进算术表达式就被「升级」，sizeof 一测就露馅）；第二，有符号整数溢出（INT_MAX + 1）是未定义行为 UB——编译器其实能给你一个回绕值，但它不保证，UBSan 还会当场把它揪出来（呼应阶段 0 第 10 章）；第三，无符号整数溢出却完全合法、按模 2^N 确定地回绕（UINT_MAX + 1 一定是 0）。把这三座大山翻过去，你就懂了为什么 C 代码里到处是「int 而不是 char 做运算」「计数器爱用 unsigned」这些看似随意的习惯。"
chapter: 1
order: 3
tags:
  - host
  - type
  - operator
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [11, 99]
prerequisites:
  - "第 2 章：整型家族与 sizeof（各整型的大小，这一章的舞台）"
related:
  - "阶段 0 · 第 10 章：Sanitizer 门禁（UBSan 当场抓有符号溢出）"
  - "第 6 章：位运算与移位（移位里的提升和溢出坑）"
---

# 整型提升、溢出与回绕：C 的算术三座大山

## 引言：C 的算术，比你以为的坑多了去了

上一章我们认清了 C 的整型家族——`char`/`short`/`int`/`long`，大小是「实现定义」的。但你可能以为「知道了大小就够了，运算不就是加减乘除嘛」。没那么简单。在 C 里，**`char + char` 的结果不是 `char`、是 `int`**；**有符号整数溢出是未定义行为（UB）**，编译器给你一个值纯属客气、它一个字都没保证；而无符号整数溢出反而**确定地回绕**、是合法操作。这三件事——**整型提升、有符号溢出（UB）、无符号回绕**——是 C 算术的三座大山，翻不过去你写的算术代码就随时可能「在我机器上是对的、换个编译选项就翻车」（阶段 0 第 9 章我们已经见过 `gcc` 连 `-O0` 都假定不溢出的活例子）。这一章我们一座一座翻，全程真跑。

## 第一座山：整型提升——小类型一运算就「升级」

先来个会让你愣一下的真跑。两个 `char` 相加，结果是什么类型？直觉是 `char` 吧？测一下 `sizeof`：

```c
#include <stdio.h>

int main(void) {
    char c = 100;
    short s = 100;
    printf("sizeof(char)   = %zu\n", sizeof(c));
    printf("sizeof(short)  = %zu\n", sizeof(s));
    printf("sizeof(c + c)  = %zu   ← char+char 居然是 int!\n", sizeof(c + c));
    printf("sizeof(s + s)  = %zu   ← short+short 也是 int\n", sizeof(s + s));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall promotion.c -o promo && ./promo
sizeof(char)   = 1
sizeof(short)  = 2
sizeof(c + c)  = 4   ← char+char 居然是 int!
sizeof(s + s)  = 4   ← short+short 也是 int
```

`sizeof(c) = 1`、`sizeof(c + c) = 4`——**两个 `char` 一相加，结果就跑到了 `int`（4 字节）里**，不再是 `char`。`short` 也一样，`short + short` 出来是 `int`。这个机制叫**整型提升**（integer promotion，ISO/IEC 9899 §6.3.1.1）：凡是 `char`、`short`（以及位域、`_Bool`）这些「比 `int` 窄」的类型，一参与算术运算（加减、位运算、逻辑运算……），就会被**自动提升成 `int`**（如果 `int` 装得下；装不下才提升成 `unsigned int`）再算。

为什么这么设计？说白了是硬件和历史的原因——CPU 的运算单元（ALU）天生是按 `int` 那么宽的寄存器来跑的，把一个 `char` 塞进去算，它也会被扩展成 `int` 再算，C 标准索性把这个事实写进了语言（早期机器这么做最省事，也就这么沿袭下来了）。所以你写 `c + c`，编译器其实是「先提升成两个 `int`，再加」。

这看着无害，但它能挖坑。一个典型场景：你想把两个 `unsigned char` 加起来、赋回 `unsigned char`，结果在中间这一步是 `int`，溢出检测和符号都会按 `int` 来。再比如有人写 `if ((c - d) < 0)`，本意是「`c` 小于 `d` 时为负」，但因为 `c - d` 提升成了 `int`、确实能表示负数，这个比较勉强能用；可一旦换成 `unsigned char`，`c - d` 仍是提升后的 `int`（不是 `unsigned`），行为又不一样了——这种「提升后的类型到底是有符号还是无符号」的纠结，是 C 里一整类隐蔽 bug 的来源，我们用到具体例子时再细抠。现在先记住一条铁律：**算术运算不会在 `char`/`short` 这种窄类型里发生，它们永远先提升到 `int` 再算**。

## 第二座山：有符号溢出是 UB，别赌它回绕

现在说第二件、也是最要命的一件：**有符号整数溢出是未定义行为**。意思是 C 标准对「`INT_MAX + 1` 等于几」**不置一词**——它不保证回绕、不保证报错、不保证任何特定结果，程序一旦溢出，整个行为就脱出了标准的保护伞（ISO/IEC 9899 §6.5 第 5 段）。我们先看看这台 gcc 实际跑出来是啥：

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int si = INT_MAX;
    unsigned ui = UINT_MAX;
    printf("INT_MAX + 1  = %d   ← 有符号溢出:UB!这个值不保证\n", si + 1);
    printf("UINT_MAX + 1 = %u   ← 无符号回绕:well-defined,确定是 0\n", ui + 1);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall overflow.c -o ov && ./ov
INT_MAX + 1  = -2147483648   ← 有符号溢出:UB!这个值不保证
UINT_MAX + 1 = 0   ← 无符号回绕:well-defined,确定是 0
```

`INT_MAX + 1` 跑出来是 `-2147483648`（看起来像「溢出后回绕成了最小负数」）。**很多人就据此以为「C 的有符号数也会回绕」——这是个危险的误会。** 这只是这台 gcc 在这台 x86 机器上、碰巧给你的结果（x86 的加法指令天生是二补码回绕的，gcc 就顺手把这个硬件结果递给了你）。但 C 标准**没承诺**这个值：换个编译器、开个优化（阶段 0 第 9 章见过，gcc 一上 `-O2` 甚至 `-O0` 就可能基于「有符号不会溢出」把你那段检测溢出的 `if` 整段删掉）、移植到别的架构，它可能给你完全不同的结果，甚至直接崩。**UB 的本质是「标准撒手不管」，不是「保证回绕」。**

那要怎么发现自己写了会溢出的代码？靠第 10 章的 UBSan——同样的程序，加上 `-fsanitize=undefined` 再跑一遍，它当场就抓：

```text
$ gcc -std=c11 -Wall -fsanitize=undefined overflow.c -o ovu && ./ovu
overflow.c:9:5: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

`runtime error: signed integer overflow`——UBSan 在那条 `si + 1` 真溢出的那一刻报了警，精确到行列。所以涉及到「可能溢出的有符号算术」，**别赌回绕、上 UBSan 测、或者干脆换成下面这种确定回绕的写法**。

> 上面是书里的真跑。想自己改数字、当场看 UBSan 怎么抓溢出?试试这个(运行默认带 `-fsanitize=undefined`,点「运行」就行;把 `INT_MAX` 改成别的数、或把 `+ 1` 改成 `* 2`,看 UBSan 什么时候报警、什么时候静默):

<OnlineCompilerDemo
  title="亲手玩:有符号溢出,UBSan 当场抓"
  description="si + 1 在 si = INT_MAX 时是有符号溢出(UB)。运行默认带 -fsanitize=undefined,UBSan 会精确到行列地报 signed integer overflow。改改 si 的值或运算符,看 UBSan 什么时候报、什么时候不报。"
  allow-run="true"
  run-options="-std=c11 -Wall -Wextra -O0 -fsanitize=undefined"
  sourcePath="/demos/int_overflow.c"
/>

## 第三座山：无符号溢出确定回绕（合法！）

和无符号溢出形成鲜明对比：**无符号整数的溢出不是 UB，而是「确定地按模 2^N 回绕」**（ISO/IEC 9899 §6.2.5 第 9 段：无符号整数的算术是模 2^N 的）。所以 `UINT_MAX + 1` 一定是 `0`、`0 - 1`（`unsigned`）一定是 `UINT_MAX`，这在所有平台、所有编译器上都一样，是**你可以放心依赖**的行为——上面真跑出来 `UINT_MAX + 1 = 0` 就是铁证。

为什么无符号能这样保证、有符号就不行？因为无符号数的表示是唯一的（没有「符号位」那些歧义），标准可以直接规定它「按模回绕」；而有符号数有二补码、反码、原码多种可能表示，标准当年没拍板（「实现定义」），于是干脆把溢出划成 UB，把锅甩给编译器——编译器又借着「反正 UB 我可以假定它不发生」去做优化，就有了上面那些「`-O2` 把你的溢出检测删光」的事（所以啊，这事儿一半是历史遗留、一半是编译器拿 UB 当优化借口，骂谁都有理）。

这个差异在工程里很有用。比如你想写一个「只会增长、永远不会负」的计数器，或者一个「超时回退」的计时逻辑，用 `unsigned`（或定宽的 `uint32_t`）就敢放心地让它溢出回绕——`uint32_t` 的 `0 - 1` 一定是 `4294967295`，写环形缓冲、哈希、校验和这类代码时这个「确定回绕」是基础工具。但记住它也有反面：**两个 `unsigned` 相减，结果还是 `unsigned`、永远不会「小于 0」**——如果你写 `for (unsigned i = n; i >= 0; i--)`，因为 `i` 永远 `>= 0`，这就是个死循环（`i--` 到 `0` 再减又回绕成 `UINT_MAX`）。这种「无符号永不为负」的坑，不知道多少人踩过。

## 三座山的交汇:一个混合符号的陷阱

把这三座山放一起，最常见的坑是**有符号和无符号混合运算时的隐式转换**。当 `int` 和 `unsigned` 一起运算时（§6.3.1.8 usual arithmetic conversions），如果 `int` 能装下 `unsigned` 的所有值就转 `int`、装不下（比如 `int` 和 `unsigned` 都是 32 位）就**两个都转成 `unsigned`**——于是那个负的 `int` 会悄悄变成一个巨大的正数。比如 `-1 < sizeof(x)` 这种比较，`-1` 被转成 `unsigned` 后变成 `SIZE_MAX`（一个超大正数），比较结果是**假**——`-1` 居然不小于一个正数！这种「负数在和 `unsigned` 比较时变正」的坑非常隐蔽，靠 `-Wsign-compare` 警告能在编译期挡掉一部分，但更彻底的办法是：**不要让有符号和无符号直接同框运算/比较**，要比较就先显式转换到一个明确的类型。第 6 章讲位运算、后面讲循环和边界时，这些坑还会反复冒头。

## 小结

C 的算术有三座大山要翻。第一，**整型提升**（§6.3.1.1）：`char`、`short` 这些窄类型一进算术表达式就先提升成 `int` 再算，所以 `char + char` 的结果是 `int`（真跑 `sizeof(c+c) == 4`）——别以为运算会在窄类型里发生。第二，**有符号溢出是 UB**（§6.5 第 5 段）：`INT_MAX + 1` gcc 给你个 `-2147483648` 是它客气、不是标准保证（换编译器/上 `-O2` 就可能不一样、甚至把你检测溢出的代码删掉），所以别赌回绕，要查就上 UBSan（真跑当场抓 `signed integer overflow`）。第三，**无符号溢出确定回绕**（§6.2.5 第 9 段）：`unsigned` 是模 2^N 算术，`UINT_MAX + 1` 一定是 `0`、`0 - 1` 一定是 `UINT_MAX`，是能放心依赖的——做环形缓冲、哈希、计数器都用得上，但代价是 `unsigned` 永远不为负（`for (unsigned i=n; i>=0; i--)` 是死循环）。最后那个混合坑：有符号和无符号一起运算/比较时，负数会被悄悄转成巨大的正数（`-1 < sizeof(x)` 居然为假），用 `-Wsign-compare` 挡或显式转换规避。这三座山是 C 算术的核心，后面所有涉及计算的章节都会反复用到它们。

## 参考资源

- ISO/IEC 9899:2011 §6.3.1.1（布尔、字符、整数转换 / 整型提升）、§6.3.1.8（usual arithmetic conversions，混合类型运算的转换）、§6.5 第 5 段（表达式求值 UB，有符号溢出）、§6.2.5 第 9 段（无符号整数的模 2^N 回绕）
- 阶段 0 · 第 10 章：Sanitizer 门禁（UBSan 抓有符号溢出）、第 9 章：标准与优化（`-O` 让有符号溢出 UB 现形）
- 第 6 章：位运算与移位（移位操作里的整型提升与移位 UB）
