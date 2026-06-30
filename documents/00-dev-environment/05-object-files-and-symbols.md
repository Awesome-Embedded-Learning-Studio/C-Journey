---
title: "目标文件与符号：用 nm/readelf 透视 .o 里的 T/t/U/D/B"
description: "第 4 章末尾我们留下一个问题：msg 指针那条 .quad .LC0 的真实地址是谁填的？这一章打开 .o 这个目标文件，用 nm/readelf 读它的符号表和重定位表，搞懂 T/t/U/D/B 这些字母、为什么 static 函数是小写 t、什么是 UND（待链接填），并看着链接器把 main.o 里的 U(visible_fn) 填成真实地址——而 printf 却一直留着 U@GLIBC，这就引出了下一章的动态链接。"
chapter: 0
order: 5
tags:
  - host
  - toolchain
  - linker
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "第 4 章：编译阶段看汇编"
related:
  - "第 6 章：链接与静态库"
  - "第 7 章：动态库与 dlopen"
---

> 🟡 状态：待审核（2026-06-30）｜分支：next｜本机 gcc 16.1.1、x86-64、全程 `-std=c11` 真跑。

# 目标文件与符号：用 nm/readelf 透视 .o 里的 T/t/U/D/B

## 引言：`.quad .LC0` 那个坑，到底谁填的

第 4 章我们看汇编时留了个尾巴：`msg` 指针那条 `.quad .LC0`——`.LC0` 是那个字符串的临时标号，编译成 `.o` 之后，这个标号要变成一个**真实的内存地址**。可是单看一个 `foo.o`，它根本不知道自己最后会被加载到内存的哪个位置，那这个地址是谁、什么时候填上的？

答案藏在「目标文件（object file，也就是 `.o`）」这个中间产物里。这一章我们拿 `nm`、`readelf` 把 `.o` 撬开，看清它内部的**符号表**（我定义了什么、我需要谁）和**重定位表**（哪些位置的地址还没定、要链接器来填）。搞懂这个，你就能回答三个工程里高频出现的问题：「这个符号到底定义了没」「为什么报 undefined reference」「static 函数为什么外面调不到」。顺带把第 4 章那条 `.quad .LC0` 的谜底揭开。

先把那条「标准 vs 实现」的分界线再强调一遍（和第 4 章一脉相承）：**符号表、`.o` 格式、`T/U/D/B` 这些字母，全是 ELF 工具链的实现层，ISO C 一个字没提**。C 标准里对应的概念叫**链接性（linkage）**——外部链接（external，全局可见）、内部链接（internal，`static` 限文件内）、无链接（局部变量，§6.2.2）。`nm` 的大小写字母（`T` vs `t`）就是这套语言概念在 ELF 层的落地映射。我们既讲语言概念，也讲工具现实。

## 目标文件是什么：一份「我有什么 / 我缺什么」的清单

`gcc -c` 产出的 `.o` 叫**可重定位目标文件（relocatable object file）**。「可重定位」是关键词：它的代码和数据里，所有地址都还是「相对于自己」的临时值，**没定最终位置**——因为它还不知道自己会和谁链接、被加载到哪。每个 `.o` 随身带两张表：

- **符号表（symbol table）**：一份清单，写着「我**定义**了哪些符号（函数、全局变量）」和「我**引用**了哪些、但自己没定义的符号（要别人提供）」。
- **重定位表（relocation table）**：代码里凡是引用了「地址还没定」的地方，都登记一条「这里需要链接器填一个地址」。

链接器（`ld`，通常由 `gcc` 在背后调）干的事，就是**把一堆 `.o` 的符号表对一遍**：你这个 `.o` 缺的 `visible_fn`，那个 `.o` 正好定义了，好，把真实地址填进所有引用它的位置。填不齐就报 `undefined reference`，撞车（同名强符号定义两次）就报 `multiple definition`。

我们造一对最简单的多文件工程来观察：

```c
/* foo.c —— 提供定义 */
int counter = 7;            /* 已初始化全局 */
int tally;                  /* 零初始化全局 */
static int helper(int);     /* 前向声明:static 函数 */
int visible_fn(int x) {     /* 全局函数 */
    return helper(x) + counter;
}
static int helper(int x) {  /* static 函数:只在 foo.c 内可见 */
    return x * 2;
}
```

```c
/* main.c —— 使用定义 */
#include <stdio.h>
int visible_fn(int);        /* 声明:本文件没定义,要用别人的 */
int main(void) {
    printf("r=%d\n", visible_fn(5));
    return 0;
}
```

分别编成 `.o`，先看 `foo.o` 的符号：

```text
$ gcc -std=c11 -Wall -c foo.c -o foo.o
$ gcc -std=c11 -Wall -c main.c -o main.o
$ nm foo.o
0000000000000000 D counter
000000000000001f t helper
0000000000000000 B tally
0000000000000000 T visible_fn
```

`nm` 输出的最后一列是符号名，**倒数第二列那个大写字母是符号类型**。这一组输出把最常见的几种全凑齐了：

| 字母 | 含义 | 这里对应 |
|---|---|---|
| `T` | 定义在 `.text` 段的**全局**函数 | `visible_fn` |
| `t` | 定义在 `.text` 段的**局部**函数（`static`） | `helper` |
| `D` | 定义在 `.data` 段的**全局**已初始化变量 | `counter` |
| `B` | 定义在 `.bss` 段的**全局**零初始化变量 | `tally` |
| `U` | **未定义**（自己没定义，要链接器找别人要） | 见下面的 `main.o` |

> 记忆窍门：**大写 = 全局（external linkage，外部链接）；小写 = 局部（internal linkage，文件内 `static`）**。对应 ISO C §6.2.2 的链接性：全局符号是外部链接、`static` 是内部链接。同一个字母大小写之分，就是「链接器能不能跨文件看见它」之分。

这里最值得停一下的是 `helper`——它是**小写 `t`**，因为加了 `static`。这意味着：

> **踩坑预警**：`static` 函数/全局是 `.o` 里的**局部符号**（小写字母），**链接器跨文件时看不到它**。你在 `main.c` 里声明 `extern int helper(int);` 然后调它，链接器翻遍所有 `.o` 都找不到一个全局的 `helper`，报 `undefined reference to 'helper'`——尽管 `foo.o` 里明明有个叫 `helper` 的函数，但它是 `static`、对链接器隐形。`static` 的本意就是「这是我这个文件私有的」，跨文件要用就得去掉 `static`。

## UND：自己没有、等链接器填的符号

现在看 `main.o`。它只声明了 `visible_fn`、调了 `printf`，但**都没自己定义**：

```text
$ nm main.o
0000000000000000 T main
                 U printf
                 U visible_fn
```

`T main` 是它定义的；`U printf` 和 `U visible_fn` 是 `U`（undefined）——**「我自己没有，请链接器帮我找」**。注意 `U` 行没有地址（左边空着），因为还没定。

`readelf -s` 给的是同一份符号表、更详细的视图，能看到一列 `UND`：

```text
$ readelf -s main.o | grep -iE 'FUNC|OBJECT|UND|main|visible|printf'
   4: 0000000000000000    45 FUNC    GLOBAL DEFAULT    1 main
   5: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND visible_fn
   6: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND printf
```

`UND`（undefined）就是 `nm` 里的 `U`。`main` 那行 `FUNC GLOBAL` 说它是个 45 字节的全局函数，落在第 1 节（`.text`）。`visible_fn`/`printf` 都是 `UND`，等着被填。

那「填」具体填在哪？看**重定位表** `readelf -r`：

```text
$ readelf -r main.o
Relocation section '.rela.text' at 0x1c0 contains 3 entries:
  Offset          Type           Sym. Name + Addend
00000000000a     R_X86_64_PLT32  visible_fn - 4
000000000013     R_X86_64_PC32   .rodata - 4
000000000022     R_X86_64_PLT32  printf - 4
```

这张表的意思是：`main.o` 的 `.text` 段里有三处「地址还没定、要链接器填」：

- 偏移 `0x0a`：一条**调用 `visible_fn`** 的指令，类型 `R_X86_64_PLT32`（PC 相对、走 PLT 的函数调用重定位）。
- 偏移 `0x13`：引用 **`.rodata`**（那个格式串 `"r=%d\n"`），类型 `R_X86_64_PC32`（PC 相对的地址引用）。
- 偏移 `0x22`：一条**调用 `printf`** 的指令，同样 `R_X86_64_PLT32`。

第 4 章那条 `.quad .LC0` 之谜，答案就在这里——`.LC0` 在 `.o` 里留了一条重定位条目，链接器扫到它、算出字符串的真实地址，回填进那条 `.quad`。重定位类型（`PLT32`/`PC32`/绝对重定位等）决定「怎么算、填几字节」，是 ABI 细节，本章你只要记住「**有 `U` 就一定伴随重定位条目，等链接器回填**」即可。

## 链接：看着 `U` 变成真实地址

把两个 `.o` 链接成可执行，跑一下：

```text
$ gcc foo.o main.o -o prog && ./prog
r=17
```

`r=17` 是对的：`visible_fn(5) = helper(5) + counter = 10 + 7 = 17`。现在看链接之后符号表发生了什么：

```text
$ nm prog | grep -iE 'visible_fn| main$| counter| tally| helper| printf'
0000000000004018 D counter
0000000000001158 t helper
0000000000001166 T main
                 U printf@GLIBC_2.2.5
0000000000004020 B tally
0000000000001139 T visible_fn
```

对比链接前后，最关键的变化：

- `counter`、`tally`、`main`、`visible_fn`、`helper` 现在**都拿到了真实的虚拟地址**（`0x4018`、`0x4020`、`0x1166`、`0x1139`、`0x1158`）。`main.o` 里那个 `U visible_fn` 没了——链接器在 `foo.o` 找到了定义，把地址回填进 `main` 里所有调用 `visible_fn` 的地方。**这就是「`U` 被解析」的全过程。**
- 但 `printf` **仍然是 `U`**——准确说是 `U printf@GLIBC_2.2.5`。为什么？因为 `printf` 不在任何 `.o` 里，它在 **glibc 动态库**中。静态链接阶段不填它，而是留一个带版本号（`@GLIBC_2.2.5`）的标记，等程序**启动时由动态链接器**去 libc.so 里找、再填地址。

这个 `printf@GLIBC` 留着 `U` 的现象，正是**静态链接 vs 动态链接**的分水岭，也是第 7 章（动态库与 `dlopen`）的入口。先记住这个画面，后面会接上。

> **踩坑预警**：别拿 C++ 的经验套纯 C。C++ 有 name mangling（`int foo(int)` 编译后符号叫 `_Z3fooi`），所以 C++ 工程里 `nm` 看到的是一堆乱码符号；**纯 C 的符号不 mangle**（`visible_fn` 就是 `visible_fn`）。混语言时要用 `extern "C"`（C++ 那边）让符号退回 C 的平坦命名，否则链接器对不上名。另外，符号可见性 `-fvisibility=hidden` 会把本来全局的符号变成局部（影响动态库导出哪些符号），第 7 章细讲。

## 小结

到这，`.o` 这个黑盒就被我们撬开了。请你记住这几条：

- **`.o` 是可重定位的**：地址都是临时的，靠随身带的**符号表 + 重定位表**等链接器回填。
- **`nm` 字母**：`T/t`（text 函数，大写全局/小写 static）、`D/d`（data）、`B/b`（bss）、`U`（未定义）。**大写 = 外部链接（全局），小写 = 内部链接（`static`）**，对应 ISO C §6.2.2 链接性。
- **`U` = 我没有、等链接器找**：`readelf -s` 里显示 `UND`，必伴随 `readelf -r` 里的重定位条目。
- **`static` 符号链接器跨文件看不见**：跨文件调 `static` 函数 → `undefined reference`。
- **链接把 `U` 填成真实地址**；但来自动态库的符号（如 `printf@GLIBC`）仍留 `U`，运行期由动态链接器解析——这是下一章的引子。
- **纯 C 不 name-mangle**，别拿 C++ 的 mangled 符号经验往 C 上套。

下一章我们正式打开「链接」这一步：亲手制造并诊断 `undefined reference` 和 `multiple definition`，搞懂链接器按命令行顺序找符号的规则，再把几个 `.o` 打包成静态库 `.a`。

## 参考资源

- ELF 格式规范（`/usr/include/elf.h`、Oracle *Linker and Libraries Guide*）：符号表 `.symtab`、重定位条目 `.rela.text` 的字段定义
- `man nm` / `man readelf` / `man objdump`（符号类型字母、重定位类型的完整清单）
- ISO/IEC 9899:2011 §6.2.2（标识符的链接性：external / internal / no linkage）
- x86-64 psABI：`R_X86_64_PLT32` / `R_X86_64_PC32` 等重定位类型的语义
