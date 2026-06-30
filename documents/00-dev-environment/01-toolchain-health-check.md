---
title: "工具链体检：在本机把 gcc/clang/gdb/make/cmake 都跑通"
description: "课程第 0 章第一篇：在 WSL2/Linux 上逐个验证 gcc、clang、gdb、make、cmake、ninja 真能用，亲手跑出第一条 gcc hello.c，并当场揭开 gcc 16 默认 C23、clang 22 默认 C17 的标准分歧——这就是全课程『永远显式钉 -std』纪律的由来。"
chapter: 0
order: 1
tags:
  - host
  - toolchain
difficulty: beginner
reading_time_minutes: 12
platform: host
c_standard: [11, 17, 23]
prerequisites:
  - "命令行基础"
related:
  - "第 2 章：编译四阶段全景（-save-temps）"
  - "第 9 章：标准与优化：-std 选项、-O 级别与 -g 调试信息"
---

> 🟡 状态：待审核（2026-06-30）｜分支：next｜这是本课程的声音锚点件之一，请重点校准语气与方法。

# 工具链体检：在本机把 gcc/clang/gdb/make/cmake 都跑通

## 引言：为什么第一件事是「体检」

很多人学 C 的路径是这样的：装个 VS Code，再装个 C/C++ 插件，点一下运行按钮，屏幕上蹦出 `hello world`，于是心满意足地觉得自己「环境配好了」。说实话，我当年差不多也是这么入坑的——直到有一天换台机器、或者把代码丢进 CI（持续集成），同样的代码突然报一堆看不懂的错，才意识到一个挺扎心的事实：**VS Code 只是个壳，真正在背后替你干活的是 gcc、ld、gdb 这一套命令行工具链，而你压根没确认过它们到底在不在、是不是你以为的那个版本。**

所以这一章我们要做的事情特别朴素，但特别重要：**在本机上把这门课全程要用的工具链逐个验证一遍**，能亲口说出每件工具的版本和职责，并亲手敲出第一条真实的 `gcc hello.c -o hello`，把产物摆出来看。

还有一层你必须现在就知道的理由：**本仓库的 CI 是同时跑 gcc 和 clang 两个编译器的**（一个矩阵 job，两个编译器各编一遍）。这意味着你的代码不能只在「你机器上的那个 gcc」上过得去，它还得在 clang 上也过得去。这两位对 C 标准的默认取值**根本不一样**——这是本章要当场拆给你看的第一个、也是最大的一个坑。先把这个雷排了，后面整本书才站得住。

## 工具链各件都是干什么的（带本机真实版本）

我们先把这趟旅程要用的家当列清楚。下面这张表里的版本号不是我从网上抄的，是我在自己这台 WSL2 机器上现敲 `--version` 一个个抓出来的（下一节你就会看到怎么做）：

| 工具 | 本机版本 | 干什么 |
|---|---|---|
| **gcc** | 16.1.1 | 编译器，把 `.c` 翻成可执行。本仓 CI 的编译器之一。 |
| **clang** | 22.1.6 | 另一个编译器，报错信息更友好。本仓 CI 的第二个编译器。 |
| **make** | 4.4.1 | 构建自动化，读 `Makefile` 决定该编什么。 |
| **cmake** | 4.3.4 | 构建系统「生成器」，产出 Makefile 或 ninja 文件。 |
| **ninja** | 1.13.2 | 一个更快的构建后端，本仓 CI 用 cmake + ninja。 |
| **gdb** | 17.2 | 调试器，程序崩了靠它定位到源码行。 |
| **clang-format** | 22.1.6 | 代码格式化，本仓用根目录的 `.clang-format` 统一风格。 |
| **git** | 2.54.0 | 版本控制。 |

你不需要把版本背下来，但你得知道**每件工具的职责边界**：编译器（gcc/clang）负责翻译代码，构建工具（make/cmake/ninja）负责「哪些文件该重编、按什么顺序编」，调试器（gdb）负责出事之后查现场。这几件东西各管一摊，混为一谈是新手最常见的误解。

## 第一步：把版本都打一遍

体检嘛，最直接的就是让每件工具自报家门。`--version` 几乎是所有命令行工具的通用开关：

```text
$ gcc --version
gcc (GCC) 16.1.1 20260430
Copyright (C) 2026 Free Software Foundation, Inc.
...

$ clang --version
clang version 22.1.6
Target: x86_64-pc-linux-gnu
...

$ make --version | head -1
GNU Make 4.4.1

$ cmake --version | head -1
cmake version 4.3.4

$ gdb --version | head -1
GNU gdb (GDB) 17.2
```

这里有个细节值得停一下：`gcc -dumpmachine` 和 `clang -dumpmachine` 能告诉你编译器的**目标三元组**（target triple），也就是它给哪种 CPU/系统产代码：

```text
$ gcc -dumpmachine
x86_64-pc-linux-gnu
$ clang -dumpmachine
x86_64-pc-linux-gnu
```

两个都是 `x86_64-pc-linux-gnu`——64 位 x86、Linux、GNU ABI。这一串后面看汇编、讲调用约定（参数走哪些寄存器）的时候会反复用到，先混个眼熟。

> **踩坑预警**：别把「装了 IDE」等同于「装了工具链」。VS Code 的 C/C++ 插件只是个**前端壳子**，它自己不会编译，背后调用的还是系统里的 gcc/clang/gdb。哪天你在纯命令行里敲 `gcc` 报 `command not found`，那就是工具链没装到位，装再多插件也白搭。本课程的姿态是：**命令行能跑通，才算真的通**。

## 第二步：hello.c 用 gcc 和 clang 各编各跑

光看版本号不过瘾，我们真的编一个程序出来。下面这段 `hello.c` 大概是全宇宙最朴素的 C 程序了：

```c
#include <stdio.h>

int main(void) {
    printf("hello from C\n");
    return 0;
}
```

现在我们分别用 gcc 和 clang 把它编出来、跑一遍：

```text
$ gcc hello.c -o hello_gcc && ./hello_gcc
hello from C
$ clang hello.c -o hello_clang && ./hello_clang
hello from C
```

两个编译器，同一份 `hello.c`，都吐出了 `hello from C`。到这一步，工具链的基本盘就算验过了。再看一眼产物本身是什么东西——`file` 命令能告诉你一个文件的真身：

```text
$ file hello_gcc
hello_gcc: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
BuildID[sha1]=f7a538cd..., for GNU/Linux 4.4.0, not stripped
```

读一下这串信息：它是一个 **ELF** 格式（Linux 的可执行文件格式）的 **64 位**可执行文件，**动态链接**（运行时要找动态链接器 `/lib64/ld-linux-x86-64.so.2`），没被 strip（还带着符号，方便调试）。这些词后面讲链接、讲调试时会逐一展开，这里先有个印象：**`gcc hello.c -o hello` 产出的不是什么魔法，而是一个结构清清楚楚的 ELF 文件**。

> 顺手提一句：clang 编出来的 `hello_clang` 用 `file` 看结构几乎一模一样（同样是 ELF 64-bit pie、dynamically linked），唯独 `BuildID` 不同——**同一份源码，两个编译器产出的是两个不同的二进制**。这个直觉后面讲「为什么 CI 要跑两个编译器」时会用到。

## 真正的第一大坑：gcc 和 clang 默认的 C 标准根本不一样

接下来这一段，是整本书我想让你最先记住的一件事。

C 语言不是一成不变的死语法，它有一串还在演进的标准：C89 → C99 → C11 → C17 → C23，每一代都加特性、改规则。当你敲 `gcc hello.c` 的时候，gcc 会**默认**按某个标准来编译——问题是，**gcc 默认的那个标准，和 clang 默认的那个标准，并不一样**。空口无凭，我们写个小探针程序，让它把编译器当前认定的 C 标准版本号打印出来：

```c
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);
#else
    printf("__STDC_VERSION__ 未定义（C89/90）\n");
#endif
    return 0;
}
```

`__STDC_VERSION__` 是 C 标准预定义的一个宏（预定义宏，详见 ISO/IEC 9899），它的值就是当前编译器认定的 C 标准版本号，约定如下：C99 = `199901L`、C11 = `201112L`、C17 = `201710L`、C23 = `202311L`。我们**不传任何 `-std`**，让两个编译器都用各自的默认值来编它：

```text
$ gcc std_probe.c -o std_gcc && ./std_gcc
__STDC_VERSION__ = 202311L
$ clang std_probe.c -o std_clang && ./std_clang
__STDC_VERSION__ = 201710L
```

看出问题了吗？**同一份 `std_probe.c`、同一个命令行写法（都没传 `-std`），gcc 认为它在编 C23（`202311L`），clang 认为它在编 C17（`201710L`）。** 这不是 bug，是两个编译器各自的默认值就这么设定的——gcc 16 默认 `gnu23`，clang 22 默认 `gnu17`（`gnu` 表示在纯标准基础上开了 GNU 扩展，这个区别留到第 9 章细讲）。

这意味着什么？意味着一段依赖较新标准特性的代码（比如 C23 才有的写法），在你机器上的 gcc 下能编过，丢进 CI 的 clang 那一格就 may 报错——反之亦然。**「在我机器上能跑」在这门课里不成立，因为我们的 CI 有两个编译器盯着你。**

解药特别简单，也特别重要：**永远在命令行里显式钉死 `-std=cXX`**，别吃默认值。你看，一旦我们把标准显式钉到 `c11`，两个编译器立刻对齐了：

```text
$ gcc -std=c11 std_probe.c -o std_gcc_c11 && ./std_gcc_c11
__STDC_VERSION__ = 201112L
$ clang -std=c11 std_probe.c -o std_clang_c11 && ./std_clang_c11
__STDC_VERSION__ = 201112L
```

两边都是 `201112L`（C11），齐刷刷的。从这一章起，本书所有示例都会显式写 `-std=cXX`，理由就是上面这两段真跑出来的输出。请把这个习惯也焊死成你的肌肉记忆。

> **踩坑预警**：默认的 `-std` 是会**随编译器版本漂移**的。gcc 16 默认 `gnu23`，但你换台装着 gcc 11 的机器，默认可能就是 `gnu17`。**永远不要假设「不传 `-std` 就是某个固定标准」**——要么显式传，要么在构建脚本里写死。第 9 章我们会把 `-std`、`-O` 优化级别、`-g` 调试信息一起系统讲透。

## 还有两个小坑，顺手排了

**「我用的 gcc 到底是哪个 gcc？」** `which` 能告诉你命令行里敲 `gcc` 时实际解析到哪个路径，而 CI 里真正调用的编译器则由 `CC` 环境变量决定，两者不一定一致：

```text
$ which gcc clang
/usr/sbin/gcc
/usr/sbin/clang
$ echo "CC=[$CC]"
CC=[]
```

我这台机器上 `gcc` 解析到 `/usr/sbin/gcc`（你的机器很可能是 `/usr/bin/gcc`，这很正常）；而 `$CC` 当前是空的，说明我没有额外指定，命令行里敲 `gcc` 就用上面那个。CI 的情况不一样：它会在矩阵里把 `CC=gcc` 和 `CC=clang` 分别设进去，所以**「我本地敲的 gcc」和「CI 这一格用的编译器」可能是同一个名字、不同的实际二进制**。排查「本地过、CI 红」时，先确认两边用的是不是同一个编译器、同一个版本。

**「gcc 当成唯一编译器」** 这一章我们反复让 clang 露脸，不是凑数。本书 CI 的 build 矩阵就是 gcc × clang， sanitizer 那一格还专门用 clang。所以你写代码时要心里有数：**不是「gcc 能编就行」，是「gcc 和 clang 都得能编、行为还得一致」**——尤其是碰了实现定义行为（implementation-defined）的时候，两个编译器可能给出不同结果，那才是真正要小心的地方（这块在第 2 章的 UB 巡讲里会大量出现）。

## 小结

到这里，工具链体检就做完了。请你现在能不假思索地回答这几条：

- **每件工具的职责**：gcc/clang 是编译器，make/cmake/ninja 是构建，gdb 是调试，clang-format 是格式化——别混。
- **CI 跑两个编译器**：你的代码得在 gcc 和 clang 上都过得去，不是只伺候一个。
- **默认 `-std` 会漂移、且 gcc≠clang**：gcc 16 默认 C23、clang 22 默认 C17（本章真跑验证）；**永远显式钉 `-std=cXX`**。
- **IDE ≠ 工具链**：命令行能跑通，才算真的通。
- **`which gcc` ≠ CI 实际调用的编译器**：CI 由 `$CC` 决定，排查「本地过 CI 红」先对编译器。

体检过关，我们就可以开始撬开 gcc 这个黑盒了——下一章，我们用 `-save-temps` 把 `.c → .i → .s → .o → 可执行` 这四个阶段一次性全停下来给你看。

## 参考资源

- GCC 16 release notes（默认 `-std` 变更、各版本特性）
- Clang 22 release notes（默认 `-std`、与 GCC 的差异）
- ISO/IEC 9899 预定义宏 `__STDC_VERSION__`（各标准版本取值约定）
- 本仓库 `.github/workflows/ci.yml`（gcc/clang 矩阵 job 的真实写法，第 16 章逐行拆）
