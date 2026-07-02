---
title: "头文件契约：include guard、ODR 与「一个符号只定义一次」"
description: "这是工程化阶段的第一章,也是声音锚点——它定下整阶段「折腾工程师 + 第一手编译验证 + 当场说实话」的基调。前面几阶段我们写的大多是单文件程序,一个 .c 从头跑到尾。可真实的 C 工程是几十上百个 .c + .h 协作,而协作的炸点几乎全在『头文件 + 链接』这一层。这一章用三个亲手复现的编译器/链接器报错,把工程化最该先记住的三条契约钉死:其一,头文件要包 include guard(#ifndef/#define/#endif 或 #pragma once),否则被重复包含时 struct/类会重定义——真跑一个无 guard 的头被 #include 两次,gcc 当场报 redefinition of 'struct Point'。其二,ODR(One Definition Rule):一个非 inline 的全局变量/函数,整个程序里只能有一处定义,声明可以到处放、定义只能一份——真跑两个 .c 都定义 int counter,链接器报 multiple definition of 'counter';修法是只在一处定义、别处用 extern 声明。其三,C99 裸 inline 的历史坑:头文件里写 inline(不是 static inline、没配 extern inline),调用方会拿到 undefined reference to——真跑 inline int dbl 在 func.h、main.c 调用,ld 报 undefined reference to 'dbl';修法是 static inline(每翻译单元一份)或配 extern inline。这三条都是『链接器在执法』——它不跟你讲道理,违反了就当场拒绝链接。全 gcc16 真跑,贴真实报错。"
chapter: 4
order: 1
tags:
  - host
  - engineering
  - toolchain
  - linker
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段 0·第 5 章:目标文件与符号（翻译单元、nm 看 T/U 符号）"
  - "阶段 0·第 6 章:链接与静态库（undefined reference/multiple definition 入门）"
  - "阶段 0·第 2 章:编译四阶段（预处理的 #include 文本插入）"
related:
  - "第 2 章:API 设计与不透明类型（前向声明把 struct 藏起来,本章埋的伏笔）"
  - "阶段 0·第 6 章:链接与静态库（库顺序、extern,本章是它的单文件深化）"
---

> 🟡 状态:待审核(2026-07-02)

# 头文件契约：include guard、ODR 与「一个符号只定义一次」

## 引言：从一个 .c 到一群协作的 .c

前面几个阶段,我们手里的程序大多是「一个 `.c` 从头跑到尾」——`main` 在里头、所有函数也在里头,一把 `gcc hello.c` 就完事。可真实的 C 工程不是这样的:`projects/clib-utilities` 那个项目就有几十个 `.c` 和 `.h`,分在好几个目录里,靠头文件互相打招呼、靠链接器最后拼成一个程序。一旦文件多了、要互相调用,**炸点几乎全集中在「头文件 + 链接」这一层**——结构体重定义、符号多重定义、明明写了却 `undefined reference`……这些都是「工程化」要治的第一波病。

这一章是工程化阶段的**声音锚点**:我用三个亲手复现的、链接器当场甩给你的报错,把跨文件协作最该先记住的三条契约钉死——**include guard 防重复包含、ODR 一个符号只定义一次、C99 裸 inline 是个坑**。它们都是「链接器在执法」:你遵守,程序拼得起来;你违反,它一句话不说直接拒绝链接。我们一条一条来,每条都真跑给你看错在哪、怎么修。

## 头文件是什么:预处理的文本插入

先对齐一个基础事实(阶段 0 第 2 章讲过预处理,这里复习一眼)。`#include "foo.h"` 在编译的**预处理阶段**干的事,就是**把 `foo.h` 的全部文本原封不动地、粘贴到 `#include` 这一行**——它是纯文本替换,没有什么「模块」「导入」的智能。所以一个 `.c` 加上它 `#include` 的所有 `.h`,经过预处理后展开成的那个大文件,叫一个**翻译单元**(translation unit);编译器一次编译一个翻译单元,链接器再把多个翻译单元的 `.o` 拼起来。

头文件扮演的角色是**契约**:它声明「我这个模块提供哪些函数、哪些类型、哪些宏」,让别的 `.c` `#include` 它之后就能用。契约写错了、或者被重复履行,链接器就会找你麻烦。下面三条契约,就是头文件最容易写错、最容易让链接器发火的地方。

## 契约一:include guard,防重复包含

一个头文件常常会被同一个翻译单元**间接包含多次**——比如 `main.c` 同时 `#include "a.h"` 和 `#include "b.h"`,而 `a.h` 和 `b.h` 各自又都 `#include "common.h"`,于是 `common.h` 的内容被插进了两次。如果 `common.h` 里有 `struct` 定义,那就等于在同一个翻译单元里**定义了两次同一个 `struct`**,编译器直接报错。真跑给你看,我写一个**故意不写 guard** 的头,然后 `#include` 它两次:

```c
/* no_guard.h:故意不写 include guard */
struct Point {
    int x;
    int y;
};
```

```c
#include "no_guard.h"
#include "no_guard.h" /* 同一个头第二次包含 → struct Point 重定义 */
#include <stdio.h>

int main(void) {
    struct Point p = {3, 4};
    printf("%d %d\n", p.x, p.y);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -I. main.c -o p1
In file included from main.c:2:
no_guard.h:2:8: error: redefinition of 'struct Point'
    2 | struct Point {
      |        ^~~~~
In file included from main.c:1:
no_guard.h:2:8: note: originally defined here
```

第二次包含时 `struct Point` 又被定义了一遍,gcc 直接甩 `error: redefinition of 'struct Point'`,还贴心地指了「它最初是在第一次包含那里定义的」。修法就是给头文件包一层 **include guard**(也叫「包含卫士」)——用 `#ifndef` 检查一个宏有没有定义过,没定义就 `#define` 它、再放头文件正文;这样第二次包含时那个宏已经定义了、整段被预处理器跳过:

```c
#ifndef POINT_H /* include guard:第二次包含时 POINT_H 已定义,整段被跳过 */
#define POINT_H
struct Point {
    int x;
    int y;
};
#endif
```

加了这三行(`#ifndef` / `#define` / `#endif`),同一个头被包含再多次,正文只生效一次——这回编过了,打印 `3 4`。guard 宏名(这里的 `POINT_H`)要**全工程唯一**,撞名了又是一个坑(两个不同头用了同一个 guard 名,后包含的整段被跳过、该拿的定义拿不到),所以起名时带上项目/模块前缀(`MYLIB_POINT_H` 之类)更稳。除了这套 `#ifndef` 写法,还有个 `#pragma once`(编译器认到「这个文件本次编译只处理一次」),大多数主流编译器都支持、写起来更短,但它**不在 ISO C 标准里**(是编译器扩展);追求可移植就用 `#ifndef`、追求省事就用 `#pragma once`,真实项目里两者都常见。

## 契约二:ODR,一个符号只定义一次

第二条契约更深一层,跟链接器直接打交道。规则叫 **ODR(One Definition Rule,单一定义规则)**:**一个非 `inline` 的全局变量或函数,在整个程序里只能有「一处定义」**;「声明」(告诉编译器「这东西存在、长这样」)可以放任意多个翻译单元里(通常写在头文件里、大家 `#include` 共享),但「定义」(真正分配存储、生成代码的那一处)只能有一个。链接器拼 `.o` 的时候,如果一个符号在两个 `.o` 里都有定义,它就不知道该用哪个、直接报 `multiple definition` 拒绝链接。真跑一个最经典的 ODR 违规——两个 `.c` 都定义了同一个全局变量 `counter`:

```c
/* a.c */
#include <stdio.h>
int counter = 5; /* 定义 */
```

```c
/* b.c */
int counter = 10; /* 又一个定义 → 违反 ODR */
```

```c
/* main.c */
#include <stdio.h>
extern int counter; /* 声明:counter 在别处定义 */
int main(void) {
    printf("counter = %d\n", counter);
    return 0;
}
```

分步编译(每个 `.c` 编成 `.o`、最后链接),前三个 `gcc -c` 都过(编译器只看单个翻译单元,不知道别的 `.o` 里也有 `counter`);到链接那一步炸了:

```text
$ gcc -std=c11 -Wall -c a.c -o a.o      # 编译器看不见别人,各自都过
$ gcc -std=c11 -Wall -c b.c -o b.o
$ gcc -std=c11 -Wall -c main.c -o main.o
$ gcc a.o b.o main.o -o p2
/usr/bin/ld: b.o:(.data+0x0): multiple definition of `counter'; a.o:(.data+0x0): first defined here
collect2: error: ld returned 1 exit status
```

链接器 `ld` 发现 `counter` 在 `a.o` 和 `b.o` 里各有一份定义,甩出 `multiple definition of 'counter'`、还指了「`a.o` 里 first defined here」——这是链接器的核心执法:**它要求每个全局符号在整个程序里只有一处定义**。修法是**只留一处定义、别处一律改成声明**:`a.c` 继续提供唯一定义 `int counter = 5;`,`b.c` 那行改成 `extern int counter;`(声明「这个变量在别处定义、我这里只是借用名字」):

```c
/* b.c 修好版 */
extern int counter; /* 只声明、不定义:counter 还是由 a.c 提供唯一定义 */
```

这下 `counter` 全工程只有 `a.c` 里那一份定义,`b.c` 和 `main.c` 都靠 `extern` 声明来引用它,链接器满意,程序跑起来打印 `counter = 5`。**「声明到处放、定义只一份」**——这就是 ODR 的实操纪律,头文件里写的 `extern` 声明、函数原型,都是为了让多个 `.c` 能引用同一个唯一定义而不违反 ODR。

这里千万分清「声明」和「定义」:带 `extern` 且不初始化的是**声明**(`extern int counter;`,不分配存储)、带初始化或没 `extern` 的是**定义**(`int counter = 5;` 或 `int counter;`,分配存储)。头文件里只该放声明(函数原型、`extern` 变量声明、`struct`/`typedef` 定义),**不该放变量的定义**——否则谁 `#include` 它谁就多一份定义,n 个翻译单元就 n 重定义,链接器当场翻脸。

## 契约三:裸 C99 inline,一个悄悄埋雷的历史坑

第三条是 C 的一个历史包袱,特别阴——`inline` 关键字。你想在头文件里写一个「希望被内联优化的小函数」,直觉是写成 `inline int dbl(int x) { return x*2; }` 放头里、各 `.c` `#include` 共享。可 C99 的 `inline` 语义有个反直觉的设定:**一个普通的(不带 `static`、也没配 `extern inline` 的)`inline` 函数,它只算一个「内联定义」、不提供可以被外部链接调用的符号**。于是你在头里写了 `inline int dbl(...)`,`main.c` `#include` 之后调用 `dbl(5)`——编译没问题,链接时却报 `undefined reference to 'dbl'`,因为这个 `inline` 定义没生成一个叫 `dbl` 的全局符号给链接器用。真跑:

```c
/* func.h */
inline int dbl(int x) { /* C99 inline,不是 static inline */
    return x * 2;
}
```

```c
/* main.c */
#include <stdio.h>
#include "func.h"

int main(void) {
    printf("dbl(5) = %d\n", dbl(5));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -I. -c main.c -o main.o
$ gcc main.o -o p3
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0xa): undefined reference to `dbl'
collect2: error: ld returned 1 exit status
```

`undefined reference to 'dbl'`——编译器认得 `dbl`(头里有它的定义、能内联展开),可它没生成一个能被链接器引用的 `dbl` 符号,链接阶段找不到、炸了。这是 C99 `inline` 最坑人的地方:它和 C++ 的 `inline` 语义**完全不同**(C++ 的 `inline` 自动提供外部定义、不会出这问题),从 C++ 过来的人最容易踩。

修法有两条,都成立。最常用的是改成 **`static inline`**——加个 `static`,意思是「每个翻译单元各自编译一份自己的、内部链接、不跨翻译单元共享符号」,于是每个 `.c` 都有自己的 `dbl`、不需要外部符号、链接器没意见:

```c
/* func.h 修好版 */
static inline int dbl(int x) { /* static inline:每个翻译单元自己一份,不用外部符号 */
    return x * 2;
}
```

这回编过、打印 `dbl(5) = 10`。工程里头文件的小函数,几乎一律写 `static inline`(既享受内联、又不踩 C99 裸 `inline` 的雷)。另一条修法是保留 `inline`、再在某一个 `.c` 里加一句 `extern inline int dbl(int);`——这会触发编译器为它生成一份外部定义、给链接器用;但这写法绕、容易忘,日常还是 `static inline` 最省心。记住这条铁律:**头文件里的内联函数,写 `static inline`,别写裸 `inline`**。

## 小结

工程化阶段的开篇,我们钉了跨文件协作的三条契约。**include guard**(`#ifndef`/`#define`/`#endif` 或 `#pragma once`)让同一个头被重复包含时只生效一次,否则 `struct` 重定义。**ODR**(单一定义规则)要求每个非 inline 全局符号全工程只有一处定义——声明(`extern`、函数原型)可以到处放(头文件里),定义只能一份(某个 `.c` 里),否则链接器报 `multiple definition`。**头文件里的内联函数写 `static inline`、别写裸 `inline`**——C99 的裸 `inline` 不提供外部符号、调用方会撞 `undefined reference`(这是 C++ 转 C 的人最常踩的坑)。这三条背后是同一个道理:**头文件是跨翻译单元的契约,链接器是契约的执法者**——契约写对了,几十个 `.c` 才能拼成一个能跑的程序;写错了,链接器一句话不说直接拒绝。

带着这套契约,下一章我们看怎么用「不透明类型」(opaque type)把 `struct` 的细节藏进 `.c`、只把一个句柄放进 `.h`——那是头文件契约的进阶用法,既保护了模块内部不被外人乱碰、又让 ABI 更稳。

## 参考资源

- **Expert C Programming**(Peter van der Linden):讲链接器、符号解析、ODR 的经典,第 5-6 章把「链接器在干什么」讲得最透。
- **Effective C**(Robert C. Seacord):第 2 章「compile with warnings」,头文件/guard/链接错误的工程实践。
- **ISO C**:C99 `inline` 的微妙语义见 §6.7.4(三种 inline:`inline`/`extern inline`/`static inline` 的差别);ODR 在 §6.9(外部定义)。
- **man 页**:gcc/ld 的 `multiple definition` / `undefined reference` 文档,`gcc(1)` 的 `-Wl,--allow-multiple-definition`(只在调试应急用、别当正常手段)。
