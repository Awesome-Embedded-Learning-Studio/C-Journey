---
title: "工具链体检：还没写一行 C，先把这套家当验明正身"
description: "课程第一篇：先讲 C 的诞生、「编译器=翻译官」这层关系，以及 K&R→C89→ISO/IEC 9899→C99/C11/C17/C23 的标准演进和 gcc、clang 两家实现；再在 WSL2/Linux 上把 gcc 16.1.1、clang 22.1.8、make/cmake/ninja、gdb 17.2、clang-format、git 逐个 --version 验明，-dumpmachine 报出 x86_64-pc-linux-gnu；examples/hello.c 双编译器各编各跑，file 确认两边同为 ELF 64-bit PIE 动态链接、cmp 却从第 41 字节起分道扬镳；再用 __STDC_VERSION__ 探针（ISO/IEC 9899 §6.10.8 / §6.10.10）当场揭出 gcc 16 默认 gnu23、clang 22 默认 gnu17 的标准分歧，-std=c11 与 -std=c23 显式对齐、-Wall -Wextra 双编译器零警告——「永远显式传 -std」这条全书纪律，就立在这一章的真跑输出上。"
chapter: 0
order: 1
tags:
  - host
  - toolchain
difficulty: beginner
reading_time_minutes: 14
platform: host
c_standard: [11, 17, 23]
prerequisites:
  - "命令行基础"
related:
  - "第 2 章：VSCode + Clangd，把工具链接进编辑器"
  - "第 3 章：编译四阶段与汇编透视（-save-temps）"
  - "第 9 章：标准与优化：-std 选项、-O 级别与 -g 调试信息"
  - "第 10 章：Sanitizer 门禁"
---

# 不着急，咱们先给环境做个体检

笔者注意到不少教程，很多都是直接从语法开始，而不是从工作环境的介绍开始。就像咱们参加工作，起手都是配置环境，如果这一关不过，之后再去进行下一步的开发，会十分的艰难。不过「环境」这两个字背后是一整套替咱们干活的工具，它们各自是什么、打哪儿来，得先花几分钟讲清楚，后面的体检报告才看得懂。

## C 从哪儿来，编译器又是谁

在开始咱们的C语言旅程之前，笔者想好好地讲讲关于C语言的故事。

> 请允许我碎碎念，笔者上大学才接触编程，好巧不巧，这就是我的第一门接触的编程语言，也是我至今为止使用的最多的编程语言之一，另一个是隔壁C++，我也有一个仓库专门记录和讲解相关的内容：[TAMCPP](https://github.com/Awesome-Embedded-Learning-Studio/Tutorial_AwesomeModernCPP)

C 这门语言是被真实的工程难题逼出来的，跟课堂没关系。把时间拨回 1972 年前后，新泽西州的贝尔实验室，Dennis Ritchie 在这里造出了 C。更早的时候，他的同事 Ken Thompson 用一门叫 B 的小语言给刚出生的 UNIX 写过最早的一批工具（内核那会儿还是汇编），而 B 又能一路追溯到英国的 BCPL。名字也懒得另起——顺着 B 往下数一位正好是 C，至于这一位是顺着字母表数的，还是顺着 BCPL 的字母数的，Ritchie 自己都没把话说死。实验室当时的难处很实际：系统软件全靠汇编写，写得苦、读着更苦，还和特定 CPU 死死绑在一起。Ritchie 要的是一门「像汇编一样贴着机器、又像高级语言一样给人读」的语言——按他自己的回忆，当初没把「搬家到别的机器」当头号目标，可移植这层好处是后来才显出来的。1973 年，UNIX 内核（操作系统里最贴硬件、最核心的那部分）的大半就用这门新语言重写了；再往后 UNIX 真能从一种机型搬进另一种机型，靠的正是这身 C 打的底子。

读到这里，新手朋友心里多半冒出一个更实际的问题：屏幕上这几行英文，是怎么变成一个能跑的程序的？这就要请出全书从头到尾打交道的关键角色——**编译器**。咱们写下的 C 代码，说到底是写给人看的文本，一行行英文单词加符号；CPU 不认识这些，它只认自家指令集里的机器码——一条条用数字编码的指令（前面说的汇编，就是把这些数字指令写成人勉强能读的样子）。人话和机器话中间隔着一次翻译，编译器干的就是这趟活：把整份 `.c` 从头到尾翻成目标 CPU 认识的指令序列，再和现成的库代码拼在一起，交出一份操作系统能直接加载运行的文件。咱们后面在终端里敲 `gcc hello.c -o hello_gcc`，就是在给这位翻译官派活——`gcc`，就是本章两位主角里头一个亮相的。这套翻译拆成几步、每步的中间产物长什么样，第 3 章拿 `-save-temps` 逐站停车细看；眼下记住一句话就够：**C 程序不是「跑」出来的，是「翻」出来的。**

语言落地了，正经的规矩却等了十几年才来。中间那段时间，管用的「官方标准」其实是一本书：1978 年 Kernighan 和 Ritchie 合写的《The C Programming Language》，江湖人称 K&R。书写成什么样，写编译器的人就照着各自的理解去实现，理解稍有出入，细节就走样；走样攒了十来年，这门越来越大的语言，终于到了该立规矩的时候。第一版正式标准由美国国家标准协会 ANSI 在 1989 年定下，史称 C89；第二年国际标准化组织 ISO 接手，规矩从此挂在 ISO/IEC 9899 这个名下——本章后面引用的 `§6.10.8` 条款，出处就是它。往后是一串年份：C99、C11、C17、C23，一代加一批特性、改一批规则。这串年份马上就要在本章动真格。

标准本身一行程序也编不出来——把 ISO/IEC 9899 做成真能干活的软件，靠的是各家自己的实现。这个行当里资格最老的是自由软件基金会 GNU 项目的 gcc：1987 年发布第一个版本，名字原本是 GNU C Compiler，后来同一副骨架上能编的语言越接越多，才改叫 GNU Compiler Collection（编译器集合），Linux 世界的老功臣。另一家是 LLVM 项目的 clang，2007 年前后由 Apple 起头，如今是 macOS 上的默认编译器，报错信息出了名地好读。同一份 ISO C 标准，两家各做各的实现——一段合格的 C 程序，按理在哪边都该编得过。「按理」两个字，笔者特意挑的。

为什么特意挑这两个字？因为照同一份标准干活，不等于每个决定都做得一样。标准对「按 C11 编译，什么写法合法、什么非法」有明文；可命令行不归它管——咱们敲下编译器的名字、一个旗标都不给（旗标，就是命令里 `-std`、`-o` 这种横杠开头的小开关）的时候，默认按哪一年的 C 来编，这道题标准没出，答案各家自己填。于是，就有了接下来的这一幕。

## 同一条命令，两个答案

同一份源码、同一条命令行、同一台机器——gcc 认为自己在编 C23，clang 认为自己在编 C17。这不是谁坏了，是两家给「不传 `-std` 时按哪个标准编译」设定的默认值压根不一样。这个分歧，笔者打算放到全书第一篇就当面拆掉，因为后面每一章的每一段输出，都站在「工具链听指挥」这个前提上。

于是这一章正经的大程序一行都不急着写，咱们要做的事朴素得很：把这门课全程要用的家当逐件过一遍——能自报版本、能编出真程序、能在两个编译器手里表现一致。顺带立下全书第一条纪律，它就诞生于本章的一段真实输出。

还有一层原因现在就得摆出来：**本仓库的 CI 同时跑 gcc 和 clang**（CI，持续集成——代码一推进仓库，云端机器就自动替咱们把全部代码编一遍、检查一遍；这套自动检查分了两格，一格用 gcc、一格用 clang，`.github/workflows/ci.yml` 里写得明明白白）。这意味着「在咱们机器上能编过」从来不算数——代码得两个编译器都伺候得住。

## 家当清单：每件工具干什么

很多朋友入坑是在 Windows 上装 Visual Studio 或者 Dev C++，新建工程、点绿色运行按钮，蹦出 hello world 就觉得「环境配好了」。这类集成开发环境（IDE，把编辑、编译、调试打包在一个软件里）笔者也用过，省心是真的——但它们只是**壳子**：壳子自己一行机器码都不产，背后调用的永远是一整套编译器、链接器、调试器（Dev C++ 背后是 MinGW 的 gcc，Visual Studio 背后是 MSVC）——链接器干的，正是开篇那句「和现成的库代码拼在一起」的拼接活。这一整套家当合起来有个行话总称：**工具链**，章名里的「工具链体检」、后文的「工具链听指挥」，说的都是它。壳子把这套家当打包藏好，代价就是哪天换了机器、或者代码进了 CI，报错一出，咱们连该往哪儿看都不知道。本课程从头到尾一个姿态：**命令行能跑通，才算真的通。**

下面这张表的版本号不是从网上抄的，是笔者在自己这台 WSL2 机器（WSL2，Windows 里跑一个真 Linux 的子系统，不用装双系统）上现敲 `--version` 抓出来的（快照抓于 2026-08-29；这台机器用的是 Arch Linux 这种滚动发行版，小版本号会随时间漂，各自机器上跑出更新的数字很正常——重点是每件工具都能自报家门）：

| 工具 | 本机版本 | 职责 |
|---|---|---|
| **gcc** | 16.1.1 | 编译器，把 `.c` 翻成可执行。本仓 CI 的编译器之一。 |
| **clang** | 22.1.8 | 另一个编译器，报错信息更友好。CI 的第二个编译器。 |
| **make** | 4.4.1 | 构建自动化，读 `Makefile` 决定编什么、按什么顺序。 |
| **cmake** | 4.4.2 | 构建系统「生成器」，产出 Makefile 或 ninja 文件。 |
| **ninja** | 1.13.2 | 更快的构建后端，本仓 CI 用 cmake + ninja。 |
| **gdb** | 17.2 | 调试器，程序崩了靠它定位到源码行。 |
| **clang-format** | 22.1.8 | 代码格式化，本仓用根目录 `.clang-format` 统一风格。 |
| **git** | 2.55.0 | 版本控制。 |

版本号不用背，要长在身上的是**职责边界**：编译器（gcc/clang）管翻译，构建工具（make/cmake/ninja）管「哪些文件该重编、怎么串起来」，调试器（gdb）管出事之后查现场，格式化（clang-format）管风格统一。新手最常见的误解就是把这几摊混成一团——比如编译报错了去查 make 的配置。

## 让每件工具自报家门

`--version` 几乎是所有命令行工具的通用自检开关，环境装没装好，敲一遍就知道。两位主角：

```text
$ gcc --version | head -2
gcc (GCC) 16.1.1 20260728
Copyright (C) 2026 Free Software Foundation, Inc.

$ clang --version | head -2
clang version 22.1.8
Target: x86_64-pc-linux-gnu
```

构建、调试和配套工具也各报一遍：

```text
$ make --version | head -1
GNU Make 4.4.1
$ cmake --version | head -1
cmake version 4.4.2
$ ninja --version
1.13.2
$ gdb --version | head -1
GNU gdb (GDB) 17.2
$ clang-format --version
clang-format version 22.1.8
$ git --version
git version 2.55.0
```

版本之外还有一串更值得记的信息：`-dumpmachine` 让编译器报出**目标三元组**（target triple），也就是它给哪种 CPU/系统产代码：

```text
$ gcc -dumpmachine
x86_64-pc-linux-gnu
$ clang -dumpmachine
x86_64-pc-linux-gnu
```

两个都是 `x86_64-pc-linux-gnu`——64 位 x86、Linux、GNU ABI。这串名字后面讲汇编、讲调用约定（参数走哪些寄存器）时会反复出场，咱们先混个眼熟。

## hello.c：两个编译器各编各跑

光看版本不过瘾，咱们真的编一个程序出来。靶子是仓库里的 [examples/hello.c](../../examples/hello.c)，大概是全宇宙最朴素的 C 程序：

```c
#include <stdio.h>

/* 工具链体检(阶段 0 第 1 章)里第一条 gcc 命令的靶子程序 */
int main(void) {
    printf("hello from C\n");
    return 0;
}
```

在临时目录里（本课程的实验都在 `/tmp` 这类地方做，别污染源码树）分别用 gcc 和 clang 编出来、跑一遍：

```text
$ gcc hello.c -o hello_gcc && ./hello_gcc
hello from C
$ clang hello.c -o hello_clang && ./hello_clang
hello from C
```

两个编译器、同一份 `hello.c`，都吐出 `hello from C`——基本盘验过了。但产物本身值得多看两眼，`file` 能报出一个文件的真身：

```text
$ file hello_gcc
hello_gcc: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
BuildID[sha1]=8161c8e94abdb632f3c884b3c396f64dcfe73cc1,
for GNU/Linux 4.4.0, not stripped
$ file hello_clang
hello_clang: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
BuildID[sha1]=5094530f9020904e4b09798b764b8f6bc2469e2e,
for GNU/Linux 4.4.0, not stripped
```

读这串信息：两边都是 **ELF**（Linux 的可执行文件格式）、64 位、PIE（地址无关可执行）、动态链接（运行时找 `/lib64/ld-linux-x86-64.so.2` 这个动态链接器）、未 strip（还带着符号，方便调试）。末尾的 `for GNU/Linux 4.4.0` 别误会成「只能在 4.4 内核上跑」——那是 ELF 里 `.note.ABI-tag` 段声明的「最早兼容的内核 ABI 版本」，工具链构建时写死的；笔者的内核是 6.x，照跑不误。

再盯一眼两个 `BuildID`：不一样。用 `stat` 和 `cmp` 把两个二进制放在一起对——尺寸只差 16 字节，内容却从第 41 个字节起就分道扬镳：

```text
$ stat -c '%n %s bytes' hello_gcc hello_clang
hello_gcc 15968 bytes
hello_clang 15984 bytes
$ cmp hello_gcc hello_clang
hello_gcc hello_clang differ: byte 41, line 1
```

**同一份源码，两个编译器产出的是两个不同的二进制**——这个直觉先记下，等咱们聊到「CI 为什么要跑两个编译器」时它会再次出场。

## 最大的坑：gcc 和 clang 默认的 C 标准根本不一样

接下来这段，是全书里笔者最想让咱们提前记住的东西。

咱们开篇已经见过这串还在演进的标准：C89 → C99 → C11 → C17 → C23。敲 `gcc hello.c` 时不传 `-std`，编译器会按某个**默认**标准来编——问题就在这：gcc 的默认和 clang 的默认，不是一个。空口无凭，咱们写个小探针，让它把编译器当前认定的标准版本号打印出来：

```c
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);
#else
    printf("__STDC_VERSION__ 未定义(C89/90)\n");
#endif
    return 0;
}
```

`__STDC_VERSION__` 是 C 标准预定义的宏（预定义宏在 ISO/IEC 9899 里有专门一节：C99 到 C17 是 §6.10.8，C23 重编号成了 §6.10.10），它的值就是编译器当前认定的 C 标准版本号，取各版标准的定稿年月：C99 = `199901L`、C11 = `201112L`、C17 = `201710L`、C23 = `202311L`。顺带把一笔容易疑惑的账算平：C23 的正式标准号其实是 ISO/IEC 9899:2024，但它 2023 年 11 月就定稿了，宏值取定稿年月，所以是 `202311L`——C17 的 `201710L` 同理。

**不传任何 `-std`**，让两个编译器各用各的默认值来编：

```text
$ gcc std_probe.c -o std_gcc && ./std_gcc
__STDC_VERSION__ = 202311L
$ clang std_probe.c -o std_clang && ./std_clang
__STDC_VERSION__ = 201710L
```

同一份 `std_probe.c`、同一类命令行，gcc 认为它在编 C23（`202311L`），clang 认为它在编 C17（`201710L`）。这不是 bug——gcc 16 的默认方言是 `gnu23`，clang 22 的默认是 `gnu17`，两家各自就这么设定的（`gnu` 前缀表示在纯标准之上开了 GNU 扩展，这层区别第 9 章细讲）。

后果很具体：一段用了 C23 才有的写法的代码，在咱们机器的 gcc 下能编过，推进 CI 的 clang 那一格就红；反过来，一段默认指望老标准的代码，换了台新 gcc 的机器也可能不知不觉换了方言。**「在我机器上能编」这门课里从来就不成立，因为 CI 里有两个编译器盯着。**而且默认值还会随版本漂——gcc 15 才把默认从 `gnu17` 提到 `gnu23`，上一次变更是 gcc 8（`gnu11` → `gnu17`）；换台装着 gcc 11 的机器，默认就又是另一回事了。

解药简单得不像话：**永远在命令行里显式传 `-std=cXX`**，不吃任何默认值。传了，两个编译器立刻对齐——对齐到 C11：

```text
$ gcc -std=c11 std_probe.c -o std_gcc_c11 && ./std_gcc_c11
__STDC_VERSION__ = 201112L
$ clang -std=c11 std_probe.c -o std_clang_c11 && ./std_clang_c11
__STDC_VERSION__ = 201112L
```

对齐到 C23 也一样齐刷刷：

```text
$ gcc -std=c23 std_probe.c -o std_gcc_c23 && ./std_gcc_c23
__STDC_VERSION__ = 202311L
$ clang -std=c23 std_probe.c -o std_clang_c23 && ./std_clang_c23
__STDC_VERSION__ = 202311L
```

从这一章起，全书所有示例都显式写 `-std`，依据就是上面这几段真跑输出。其实连探针程序都可以省——`-dM -E` 让预处理器把内置宏全部吐出来，一行命令就能问出默认方言：

```text
$ gcc -dM -E -x c /dev/null | grep __STDC_VERSION__
#define __STDC_VERSION__ 202311L
$ clang -dM -E -x c /dev/null | grep __STDC_VERSION__
#define __STDC_VERSION__ 201710L
```

顺手把刚立的纪律用上：按 `-std=c17` 加上 `-Wall -Wextra` 把 `hello.c` 重编一遍，两个编译器都一声不吭、退出码 0——零警告，这一项体检才算绿：

```text
$ gcc -Wall -Wextra -std=c17 hello.c -o hello_gcc17; echo "exit=$?"
exit=0
$ clang -Wall -Wextra -std=c17 hello.c -o hello_clang17; echo "exit=$?"
exit=0
```

⚠️ 从今天起把它焊进肌肉记忆：凡编译，显式传 `-std=cXX`——默认方言随厂家和版本各自漂移，读代码的人不该猜谜。

## 本地能过、CI 却红：两个常识先立住

**「命令行里的 gcc，和 CI 那一格用的编译器，未必是同一个东西。」**`which` 能报出敲 `gcc` 时实际解析到哪个路径；CI 里真正调用谁，则由 `CC` 环境变量说了算：

```text
$ which gcc clang
/usr/sbin/gcc
/usr/sbin/clang
$ echo "CC=[$CC]"
CC=[]
```

笔者这台机器上 `gcc` 解析到 `/usr/sbin/gcc`（很多朋友的机器上是 `/usr/bin/gcc`，都正常）；`$CC` 是空的，说明没额外指定，命令行里敲 `gcc` 用的就是上面那个。CI 不一样：矩阵会把 `CC=gcc` 和 `CC=clang` 分别注入各格。所以「本地敲的 gcc」和「CI 这格调用的编译器」可能同名、不同二进制——排查「本地过、CI 红」时，头一件事是确认两边到底是不是同一个编译器、同一个版本。

**「gcc 不是唯一裁判。」**本章反复让 clang 露脸，不是凑数——本仓 CI 的 build 矩阵就是 gcc × clang，sanitizer 那一格（让 UB 和内存错误在运行期现形的插桩工具，第 10 章专门讲）还专门用 clang。更得心里有数的是：一旦碰上实现定义行为（implementation-defined）乃至未定义行为（undefined behavior，UB），同一段代码在两个编译器手里完全可能给出不同结果——UB 之下标准撒手不管，崩、不崩、好像没事，哪种结果都不保证，那才是真正要小心的地方（第 9 章真跑过一个活样本：同一段靠溢出回绕「检测溢出」的代码，gcc 和 clang 在同一档 `-O` 下结论不同）。

## 小结

体检做完，几样东西应该已经长在身上了。gcc/clang 是编译器、make/cmake/ninja 是构建、gdb 是调试、clang-format 是格式化，各管一摊，不混为一谈。开篇讲的那个道理也算一件：C 程序不是「跑」出来的，是「翻」出来的，翻译的规矩就订在 ISO/IEC 9899 那份文件里。本仓 CI 双编译器伺候，代码从来不是只写给一个 gcc 的。最要紧的一条：默认 `-std` 随厂家和版本漂移，gcc 和 clang 默认还对不上（本章真跑：gcc 16 默认 C23、clang 22 默认 C17），所以**永远显式传 `-std=cXX`**。另外两个常识：IDE 不等于工具链，命令行跑得通才算真的通；`which gcc` 报的未必是 CI 那一格的编译器，排查「本地过、CI 红」时头一件事是对 `$CC`。

体检过关，就可以撬 gcc 这个黑盒了。动刀之前有个更舒服的选择：下一章咱们把这套工具链原样接进 VSCode + Clangd，让跳转、补全、断点长在编辑器里（它属于「推荐」档）；急着看黑盒内部的读者也可以直接去第 3 章——那里用 `-save-temps` 把 `.c → .i → .s → .o → 可执行` 这四个阶段一次性全停下来给咱们看。

想动手的话，阶段 0 的练习里有两道正对着本章：Homework 的 0.1-A（用两条命令报出本机 `gcc` 的位置和目标三元组）和 0.1-B（「本地 gcc 过、CI 的 clang 红」排障题，正好把本章的默认方言分歧用一遍）；Project 的收尾场景也会拿「本地 gcc ≠ CI 编译器」当排查线索。练习区随阶段收口统一回归上线，先把题记在这里。

## 参考资源

- [GCC 15 release notes](https://gcc.gnu.org/gcc-15/changes.html)——默认 C 方言从 `gnu17` 改成 `gnu23` 的变更起点；上一次默认值变更（`gnu11` → `gnu17`）记在 [GCC 8 changes](https://gcc.gnu.org/gcc-8/changes.html)
- Clang 的默认 C 标准：release notes 不写，以 `man clang` 为准（原文「The default C language standard is gnu17」），或用 `clang -dM -E -x c /dev/null | grep __STDC_VERSION__` 当场实测（本章已实测）
- ISO/IEC 9899 预定义宏 `__STDC_VERSION__`：C99–C17 见 §6.10.8，C23 起重编号为 §6.10.10
- 本仓库 `.github/workflows/ci.yml`（gcc/clang 矩阵 job 的真实写法，第 15 章逐行拆）
- 本仓库 `examples/hello.c`（本章靶子程序的仓库存档；`examples/stage0-compiling-and-debug/` 是第 3～7、13 章的配套实验）
