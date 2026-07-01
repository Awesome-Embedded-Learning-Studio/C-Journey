---
title: "程序结构与编译四阶段：C 语言视角"
description: "阶段 0 讲了工具链怎么把 .c 变成可执行，这一章从 C 语言本身看「程序到底是怎么组装的」。从一个最小的 main 开始：它的两种标准签名、它的返回值在 C99 之后发生了什么（不写 return 也隐式返回 0，但 C89 下会给你一个垃圾退出码——真跑给你看）。再把「翻译单元」「声明 vs 定义」「链接」这三个 C 工程的骨架概念讲透：一个 .c 加上它展开的头文件就是一个翻译单元，函数原型和 extern 只声明不分配、定义才分配存储，全局符号是 external 链接（nm 里大写 T/B）、加 static 就变 internal 链接（小写 t/b、别的翻译单元看不到）。全程 gcc 真编译 + nm 看符号，并回扣阶段 0 的编译四阶段。"
chapter: 1
order: 1
tags:
  - host
  - syntax
  - toolchain
difficulty: beginner
reading_time_minutes: 13
platform: host
c_standard: [11, 99, 89]
prerequisites:
  - "阶段 0 · 第 1 章：工具链体检"
  - "阶段 0 · 第 2 章：编译四阶段全景（-save-temps）"
related:
  - "阶段 0 · 第 2 章：编译四阶段全景（这一章只回顾，不重复）"
  - "第 9 章：作用域、存储期与 static（static 的深入，含跨翻译单元隔离）"
---

# 程序结构与编译四阶段：C 语言视角

## 引言：从「工具链怎么编」到「C 程序怎么组装」

阶段 0 我们花了整整一大阶段，把「`.c` 怎么一步步变成能跑的可执行」这件事从工具链的角度摸透了——预处理、编译、汇编、链接，以及 gcc 的各种旗标、make、CMake、GDB、sanitizer、CI。现在我们换个视角：从 **C 语言本身**看，一个程序到底是由什么组装起来的、它的骨架是什么。你会发现，前面那些工具链行为（链接报的 `undefined reference`、多文件怎么拼、`.o` 里那些符号），背后对应的都是 C 语言里几个根本概念：**翻译单元、声明 vs 定义、链接**。搞清这几个，你写多文件工程时就不会再「凭感觉」。

## `main`：程序的唯一入口

C 程序从 `main` 开始执行，这是标准规定的（ISO/IEC 9899 §5.1.2.2.1）。`main` 有两种标准签名：

```c
int main(void) {
    ...
} /* 不接命令行参数 */
int main(int argc, char* argv[]) {
    ...
} /* 接命令行参数 */
```

注意返回类型是 `int`——它把程序的退出码返回给操作系统（第 13 章讲 GDB 时你见过，段错误是 139，正常退出是 0）。这里有一个很多人没意识到的版本差异：**`main` 不写 `return`，行为在 C89 和 C99 之后是不一样的**。我们真跑一个故意不写 `return` 的 `main`：

```c
#include <stdio.h>

int main(void) {
    printf("hello\n");
} /* 注意:没有 return 0; */
```

```text
$ gcc -std=c11 mainret.c -o mr && ./mr ; echo "退出码=$?"
hello
退出码=0                  ← C11:main 隐式返回 0(ISO §5.1.2.2.3)

$ gcc -std=c89 -Wall mainret.c -o mr && ./mr ; echo "退出码=$?"
mainret.c:7:1: warning: control reaches end of non-void function [-Wreturn-type]
hello
退出码=6                  ← C89:没这个规定,返回的是寄存器里的垃圾值!
```

**C99 起，标准给了 `main` 一个特权**（§5.1.2.2.3）：如果 `main` 走到 `}` 都没 `return`，就当它 `return 0`——所以 C11 下退出码干干净净是 0。但 **C89 没有这条规定**，不写 `return` 的 `main` 等于在一个返回 `int` 的函数里没返回值，gcc 用 `-Wall` 会警告你 `control reaches end of non-void function`，而退出码是 `6`（寄存器里残留的垃圾值）。所以「`main` 不写 return 也能返回 0」是 C99 之后才有的待遇，写要兼容老标准的代码时别依赖它，老老实实 `return 0`（或 `return EXIT_SUCCESS`）。本课程一律显式写 `return`。

## 翻译单元：一个 `.c` 就是一个翻译单元

一个 C 程序可以拆成多个 `.c` 文件，**每个 `.c` 文件（加上它 `#include` 的所有头文件展开后的内容）构成一个「翻译单元」（translation unit）**（ISO/IEC 9899 §5.1.1.1）。编译器是「一个翻译单元一个翻译单元地」编译的——你 `gcc -c counter.c` 编出来的 `counter.o`，对应的就是 `counter.c` 这个翻译单元编译后的产物；它对 `main.c` 那个翻译单元里有什么**一无所知**。把多个翻译单元拼成一个程序，是「链接」那一阶段干的活（阶段 0 第 2、6 章讲过）。

我们写一个最小的两翻译单元程序来看清这件事。一个头文件 `counter.h` 放共享的声明、`counter.c` 放定义、`main.c` 使用它们：

```c
/* counter.h */
#ifndef COUNTER_H
#define COUNTER_H
extern int counter; /* 声明:counter 在别处定义,这里不分配存储 */
void inc(void);     /* 函数原型:声明 */
#endif
```
```c
/* counter.c */
#include "counter.h"
int counter = 0; /* 定义:在这里分配存储 */
void inc(void) {
    counter++;
}
```
```c
/* main.c */
#include <stdio.h>
#include "counter.h"
int main(void) {
    inc();
    inc();
    printf("counter = %d\n", counter);
    return 0;
}
```

两个翻译单元分别 `-c`、再链接：

```text
$ gcc -std=c11 -Wall -c counter.c -o counter.o    # 编译 counter 这个翻译单元
$ gcc -std=c11 -Wall -c main.c -o main.o          # 编译 main 这个翻译单元
$ gcc counter.o main.o -o cprog                    # 链接两个 .o
$ ./cprog
counter = 2
```

`counter.o` 和 `main.o` 是各自独立编译出来的，它们怎么知道彼此的 `counter`、`inc`?这就是接下来要讲的「声明 vs 定义」和「链接」。

## 声明 vs 定义

上面那个例子里有个关键区分：**声明（declaration）告诉编译器「有这么个东西、它什么类型」，定义（definition）才真正「把它造出来、分配存储」**。看 `counter`：在 `counter.h` 里是 `extern int counter;`——这只是声明，`extern` 的意思是「这个变量在别处定义、我这里只是引用它的类型」，**不分配存储**；真正的定义在 `counter.c` 里：`int counter = 0;`，这一行才给 `counter` 分配了一块内存。函数也一样：`void inc(void);` 是函数原型（声明），`void inc(void) { counter++; }` 是函数体（定义）。

这个区分决定了一个工程怎么组织：**头文件（`.h`）放声明、源文件（`.c`）放定义**。因为头文件会被多个翻译单元 `#include`，如果头文件里放了定义（比如 `int counter = 0;`），每个 include 它的 `.c` 都会生成一个 `counter` 的定义，链接时就会撞「multiple definition」（阶段 0 第 6 章讲过这个报错）。所以头文件里只放声明（`extern`、函数原型），让定义只在一个 `.c` 里出现一次——这是 C 多文件工程的基本规矩。

## 链接：external / internal / none

那么 `main.o` 引用的 `counter`、`inc`，怎么找到 `counter.o` 里的定义？靠的是**链接（linkage）**（ISO/IEC 9899 §6.2.2）。C 里有三种链接性。一个全局的函数或变量（没加 `static`）是 **external 链接**——它能被其他翻译单元看到、引用；`main.o` 里 `counter`/`inc` 正是靠 external 链接，在链接阶段被解析到 `counter.o` 的定义。我们用 `nm` 看符号表，能直接看到这个「能不能被外部引用」的区别（`nm` 的字母大小写是关键）：

```text
$ nm counter.o
0000000000000000 B counter     ← 大写 B:external 链接(在 .bss,因为初始化为 0)
0000000000000000 T inc         ← 大写 T:external 链接(函数,在 .text)
$ nm main.o | grep -E 'counter|inc'
                 U counter     ← U:undefined,本翻译单元引用、等链接时填地址
                 U inc
```

`counter.o` 里 `counter` 是大写 `B`、`inc` 是大写 `T`——**大写表示 external 链接**（对外可见）；`main.o` 里它们是 `U`（undefined），表示「我引用了它、但定义在别处」，链接器负责把这个 `U` 填成 `counter.o` 里那个真实的地址。如果加上 `static`，就变成第二种 **internal 链接**——只在本翻译单元可见、别的翻译单元引用不到。对比一下:

```c
static int secret = 42; /* static:internal 链接,别的翻译单元看不到 */
int visible = 7;        /* 外部链接:别的翻译单元可 extern 引用 */
```

```text
$ gcc -std=c11 -c static.c -o static.o
$ nm static.o
0000000000000000 d secret    ← 小写 d:internal 链接(只在本文件)
0000000000000004 D visible   ← 大写 D:external 链接
```

**小写字母表示 internal 链接**：`secret` 是 `d`（小写），它对其他翻译单元是不可见的。所以 `static` 用在文件作用域的变量/函数上，效果是「把这个名字限制在本文件内」——你可以用它实现「模块私有」的辅助函数和全局状态，不用担心和别的翻译单元里同名的东西撞车。第三种 **none 链接**（无链接）就是普通的局部变量（函数内部的 `int x;`），它没有链接性可言、只在自己的作用域里存在——这个留到第 9 章讲作用域和存储期时细说。记一个口诀：**大写 external、小写 internal、`U` 是等着别人填的引用**。

## 编译四阶段：回顾一眼

最后把阶段 0 第 2 章的编译四阶段在这里对齐一下（不重复展开，细节回去看那一章）：源码先经**预处理**（展开宏和 `#include`，得到一个翻译单元的完整文本）、再**编译**成汇编、**汇编**成 `.o`、最后**链接**把多个 `.o` 拼起来。这一章讲的「翻译单元」就是预处理之后的那个完整文本；讲的「链接」就是第四阶段在干的事——它把 `main.o` 里那些 `U`（undefined）符号，逐一解析到 `counter.o` 里的定义上，程序才完整。理解了 C 语言的这套骨架，你后面写多文件工程、查链接错误，就有了理论抓手。

## 小结

从 C 语言视角看，一个程序是这样组装的：每个 `.c`（连同展开的头文件）是一个**翻译单元**（§5.1.1.1），编译器一个一个地编译、最后由链接阶段拼起来。`main` 是唯一入口（§5.1.2.2.1），返回 `int` 作退出码，**C99 起 `main` 不写 `return` 隐式返回 0**（§5.1.2.2.3，我们真跑 C11 退出码 0），但 C89 没这规定（真跑给你一个垃圾退出码 6 + `-Wreturn-type` 警告），所以兼容老标准就显式 `return`。**声明 vs 定义**是工程组织的核心：声明（函数原型、`extern` 变量）只说「有这东西」、不分配存储，定义才分配；所以头文件放声明、源文件放定义，避免 `multiple definition`。**链接**（§6.2.2）决定一个名字跨翻译单元可不可见：普通全局是 external（`nm` 大写 `T`/`B`/`D`，别的翻译单元能引用、`U` 等着填它）、加 `static` 是 internal（小写 `t`/`b`/`d`，只在本文件，可实现模块私有）、局部变量是无链接（none）。这一章是阶段 1 的开篇，接下来我们从 C 的类型系统（整型家族、`sizeof`）开始，一头扎进语言的细节。

## 参考资源

- ISO/IEC 9899:2011 §5.1.1.1（翻译单元）、§5.1.2.2.1（程序启动 / main）、§5.1.2.2.3（main 的返回，C99 起隐式 return 0）、§6.2.2（链接性 external/internal/none）、§6.7（声明）、§6.9（外部定义）
- `nm` 手册（符号类型：大写 = external、小写 = internal、`U` = undefined）
- 阶段 0 · 第 2 章：编译四阶段全景（这一章只回顾）；第 6 章：链接与静态库（`undefined reference` / `multiple definition`）
- 第 9 章：作用域、存储期与 static（static 的深入，含跨翻译单元隔离验证）
