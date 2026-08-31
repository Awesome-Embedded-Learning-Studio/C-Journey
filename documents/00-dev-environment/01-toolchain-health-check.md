---
title: "工具链体检：还没写一行 C，先把这套家当验明正身"
description: "动手第一篇：先立动机——同一份源码、同一条命令，gcc 认为在编 C23、clang 认为在编 C17，这个分歧当面拆掉，因为后面每一章的输出都站在「工具链听指挥」的前提上，且本仓 CI 就是 gcc × clang 双编译器伺候。IDE 只是壳子，工具链（编译器 gcc/clang、构建 make/cmake/ninja、调试 gdb、格式化 clang-format、版本控制 git）各管一摊；--version 全家福逐件自报家门，-dumpmachine 报出 x86_64-pc-linux-gnu 目标三元组；examples/hello.c 双编译器各编各跑，stat 验出两个产物连大小都不一样——「编译器不同、产物不同」的直觉就此立住；再用 __STDC_VERSION__ 探针（ISO/IEC 9899 §6.10.8 / §6.10.10）当场揭出 gcc 16 默认 gnu23、clang 22 默认 gnu17 的标准分歧，-std=c11 与 -std=c23 显式对齐、-Wall -Wextra 双编译器零警告——「永远显式传 -std」这条全书纪律，就立在这一章的真跑输出上。收尾立住 CI 排障直觉：which gcc 未必是 CI 那一格的编译器，代码从第一天起就要伺候两个编译器。"
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
  - "第 0 章：C 从哪儿来（编译器=翻译官、标准演进）"
related:
  - "第 0 章：C 从哪儿来——编译器与标准的来历"
  - "第 2 章：VSCode + Clangd，把工具链接进编辑器"
  - "第 3 章：编译四阶段与汇编透视（-save-temps）"
  - "第 5 章：对象文件与符号（两个编译器的产物里到底装了什么）"
  - "第 9 章：标准与优化：-std 选项、-O 级别与 -g 调试信息"
  - "第 10 章：Sanitizer 门禁"
---

# 不着急，咱们先给环境做个体检

上一篇咱们聊清了 C 的来历，也认识了两位翻译官；认识归认识，活干得怎么样，得当面验。就像咱们参加工作，起手都是配置环境，如果这一关不过，之后再去进行下一步的开发，会十分的艰难。不过「环境」这两个字背后是一整套替咱们干活的工具，这一章就把它们逐件过一遍：能自报版本、能编出真程序、能在两个编译器手里表现一致。

## 同一条命令，两个答案

上一篇结尾露的那个馅，这一章当面拆：

我们再看同一份源码、同一条命令行、同一台机器，gcc 认为自己在编 C23，clang 认为自己在编 C17。这不是谁坏了，是两家给「不传 `-std` 时按哪个标准编译」设定的默认值压根不一样。为什么把这件事当成第一件正事？因为后面每一章的每一段输出，都站在「工具链听指挥」这个前提上。

标准管不到这道题，根子在这：标准对「按 C11 编译，什么写法合法、什么非法」有明文；可命令行不归它管——咱们敲下编译器的名字、一个旗标（啊哈，熟悉了你就知道我们习惯叫flag，就是命令里 `-std`、`-o` 这种横杠开头的小开关）都不给的时候，默认按哪一年的 C 来编，这道题标准没出，答案各家自己填。于是，就有了接下来这一幕。

这一章正经的大程序一行都不急着写，咱们要做的事朴素得很：把这门课全程要用的家当逐件过一遍——能自报版本、能编出真程序、能在两个编译器手里表现一致。顺带立下全书第一条纪律，它就诞生于本章的一段真实输出。

还有一层原因现在就得摆出来：**本仓库的 CI 同时跑 gcc 和 clang**（CI，持续集成——代码一推进仓库，云端机器就自动替咱们把全部代码编一遍、检查一遍；这套自动检查分了两格，一格用 gcc、一格用 clang，`.github/workflows/ci.yml` 里写得明明白白）。这意味着「在咱们机器上能编过」从来不算数——代码得两个编译器都伺候得住。

## 嘿，你们聚在一起，很开心嘛，做什么呢？

很多朋友入坑是在 Windows 上装 Visual Studio （兄弟们这是我）或者 Dev C++，新建工程、点绿色运行按钮，蹦出 hello world 就觉得「环境配好了」。这类集成开发环境（IDE，把编辑、编译、调试打包在一个软件里）笔者也用过，省心是真的——但它们只是**壳子**：壳子自己一行机器码都不产，背后调用的永远是一整套编译器、链接器、调试器（Dev C++ 背后是 MinGW 的 gcc，Visual Studio 背后是 MSVC）——链接器干的，正是上一篇说的「和现成的库代码拼在一起」的拼接活。这一整套家当合起来有个行话总称：**工具链**，章名里的「工具链体检」、后文的「工具链听指挥」，说的都是它。壳子把这套家当打包藏好，代价就是哪天换了机器、或者代码进了 CI，报错一出，咱们连该往哪儿看都不知道。本课程从头到尾一个姿态：**命令行能跑通，才算真的通。**

下面这张表的版本号不是从网上抄的，是笔者在自己这台 WSL2 机器（WSL2，Windows 里跑一个真 Linux 的子系统，不用装双系统）上现敲 `--version` 抓出来的（快照抓于 2026-08-29；这台机器用的是 Arch Linux 这种滚动发行版，小版本号会随时间漂，各自机器上跑出更新的数字很正常——重点是每件工具都能自报家门）：

| 工具             | 本机版本 | 职责                                                |
| ---------------- | -------- | --------------------------------------------------- |
| **gcc**          | 16.1.1   | 编译器，把 `.c` 翻成可执行。本仓 CI 的编译器之一。  |
| **clang**        | 22.1.8   | 另一个编译器，报错信息更友好。CI 的第二个编译器。   |
| **make**         | 4.4.1    | 构建自动化，读 `Makefile` 决定编什么、按什么顺序。  |
| **cmake**        | 4.4.2    | 构建系统「生成器」，产出 Makefile 或 ninja 文件。   |
| **ninja**        | 1.13.2   | 更快的构建后端，本仓 CI 用 cmake + ninja。          |
| **gdb**          | 17.2     | 调试器，程序崩了靠它定位到源码行。                  |
| **clang-format** | 22.1.8   | 代码格式化，本仓用根目录 `.clang-format` 统一风格。 |
| **git**          | 2.55.0   | 版本控制。                                          |

版本号不用背，要长在身上的是**职责边界**：编译器（gcc/clang）管翻译，构建工具（make/cmake/ninja）管「哪些文件该重编、怎么串起来」，调试器（gdb）管出事之后查现场，格式化（clang-format）管风格统一。新手最常见的误解就是把这几摊混成一团——比如编译报错了去查 make 的配置。

## 让每件工具自报家门

上一篇两位翻译官已经报过到了，这里让其余的家当也各报一遍——还是那个 `--version` 开关，谁装好了、装的哪个版本，一敲便知：

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

版本之外还有一串更值得记的信息：`-dumpmachine` 让编译器报出**目标三元组**（target triple），翻译成人话就是「我给哪种机器产代码」：

```text
$ gcc -dumpmachine
x86_64-pc-linux-gnu
$ clang -dumpmachine
x86_64-pc-linux-gnu
```

两个都是 `x86_64-pc-linux-gnu`——64 位的 x86 处理器、Linux 系统。两位翻译官服务的是同一位客户，这很正常；这串名字后面讲汇编时会反复出场，咱们先混个眼熟。

## hello.c：两个编译器各编各跑

光看版本不过瘾，咱们真的编一个程序出来。靶子是仓库里的 [examples/hello.c](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/examples/hello.c)，大概是全宇宙最朴素的 C 程序：

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

两个编译器、同一份 `hello.c`，都吐出 `hello from C`——基本盘验过了。产物本身也值得瞄一眼，`stat` 能报出一个文件的基本信息，拿它把两份成品摆在一起：

```text
$ stat -c '%n %s bytes' hello_gcc hello_clang
hello_gcc 15968 bytes
hello_clang 15984 bytes
```

同一份源码，两位翻译官交出的两个文件连大小都不一样。这完全不影响它们干一样的活（两边都正确打印了 `hello from C`），但「**编译器不同，产物就是两个不同的文件**」这个直觉，现在就该立住——这两个文件里到底装了什么、拿什么工具能亲眼看进去，第 5 章（对象文件与符号）当场拆给咱们看。

## 最大的坑：gcc 和 clang 默认的 C 标准根本不一样

接下来这段，是全书里笔者最想让咱们提前记住的东西。

上一篇咱们已经见过这串还在演进的标准：C89 → C99 → C11 → C17 → C23。敲 `gcc hello.c` 时不传 `-std`，编译器会按某个**默认**标准来编——问题就在这：gcc 的默认和 clang 的默认，不是一个。空口无凭，咱们写个小探针，让它把编译器当前认定的标准版本号打印出来：

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

给没见过这种写法的朋友递个底：`#ifdef`、`#else`、`#endif` 这些 `#` 开头的行是**预处理指令**——编译器正式开工之前，先有个「预处理」的阶段按这些行做文字层面的裁剪，`#ifdef X` 就是在问「`X` 这个宏，你定义了没有」，定义了留这段、没定义留那段。宏（macro）可以想成编译器备好的一摞名片：`__STDC_VERSION__` 这张名片上写的，正是「我按哪一年的 C 标准在干活」，取各版标准的定稿年月——C99 = `199901L`、C11 = `201112L`、C17 = `201710L`、C23 = `202311L`。顺带把一笔容易疑惑的账算平：C23 的正式标准号其实是 ISO/IEC 9899:2024，但它 2023 年 11 月就定稿了，宏值取定稿年月，所以是 `202311L`——C17 的 `201710L` 同理。（这些预定义宏在 ISO/IEC 9899 里有专门一节：C99 到 C17 是 §6.10.8，C23 重编号成了 §6.10.10，先混个脸熟，第 4 章讲预处理时再回来对表。）

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

从这一章起，全书所有示例都显式写 `-std`，依据就是上面这几段真跑输出。

顺手把刚立的纪律用上：按 `-std=c17` 加上 `-Wall -Wextra` 把 `hello.c` 重编一遍——`-Wall` 和 `-Wextra` 是把警告开到最严的两个开关（警告，就是编译器觉得「这段代码可疑」但不拦着编译的提示，第 8 章专门讲）。两个编译器都一声不吭、退出码 0，零警告，这一项体检才算绿：

```text
$ gcc -Wall -Wextra -std=c17 hello.c -o hello_gcc17; echo "exit=$?"
exit=0
$ clang -Wall -Wextra -std=c17 hello.c -o hello_clang17; echo "exit=$?"
exit=0
```

⚠️ 从今天起把它焊进肌肉记忆：凡编译，显式传 `-std=cXX`——默认方言随厂家和版本各自漂移，读代码的人不该猜谜。

## 本地能过、CI 却红，从编译器查起

真碰上「本地过、CI 红」，头一个该确认的事：命令行里的 gcc，和 CI 那一格调用的，未必是同一个编译器。`which` 能报出敲 `gcc` 时实际解析到哪个路径：

```text
$ which gcc clang
/usr/sbin/gcc
/usr/sbin/clang
```

笔者这台机器上 `gcc` 解析到 `/usr/sbin/gcc`（很多朋友的机器上是 `/usr/bin/gcc`，都正常）。CI 不一样——那边真正调用谁，由一个叫 `CC` 的环境变量说了算，矩阵会把 `CC=gcc` 和 `CC=clang` 分别注入各格。所以「本地敲的 gcc」和「CI 这格调用的编译器」可能同名、不同二进制；排查时头一件事就是把两边的编译器、版本对上。

顺着这条再往深一层，就到了本章反复让 clang 露脸的原因——不是凑数，本仓 CI 的 build 矩阵本来就是 gcc × clang 两格。两位翻译官毕竟不是同一个人，同一段 C 代码在它们手里，完全可能给出不一样的结果——什么时候只是「各有风格」、什么时候是真正危险的写法，第 9、10 章拿真例子细说。眼下先把一条姿势焊住：咱们写的代码，从第一天起就得伺候两个编译器。

## 体检过关：工具链听指挥了

体检做完，几样东西应该已经长在身上了。gcc/clang 是编译器、make/cmake/ninja 是构建、gdb 是调试、clang-format 是格式化，各管一摊，不混为一谈。本仓 CI 双编译器伺候，代码从来不是只写给一个 gcc 的。最要紧的一条：默认 `-std` 随厂家和版本漂移，gcc 和 clang 默认还对不上（本章真跑：gcc 16 默认 C23、clang 22 默认 C17），所以**永远显式传 `-std=cXX`**。IDE 不等于工具链，命令行跑得通才算真的通；往后碰上「本地过、CI 红」，拿 `which gcc` 和 CI 那一格的编译器对一对，往往一查一个准。至于「C 程序是『翻』出来的、翻译的规矩订在 ISO/IEC 9899 里」——上一篇讲过的道理，这一章咱们算是亲手摸过它的边了。

体检过关，就可以撬 gcc 这个黑盒了。动刀之前有个更舒服的选择：下一章咱们把这套工具链原样接进 VSCode + Clangd，让跳转、补全、断点长在编辑器里（它属于「推荐」档）；急着看黑盒内部的读者也可以直接去第 3 章——那里用 `-save-temps` 把 `.c → .i → .s → .o → 可执行` 这四个阶段一次性全停下来给咱们看。

想动手的话，阶段 0 的练习里有两道正对着本章：Homework 的 0.1-A（用两条命令报出本机 `gcc` 的位置和目标三元组）和 0.1-B（「本地 gcc 过、CI 的 clang 红」排障题，正好把本章的默认方言分歧用一遍）；Project 的收尾场景也会拿「本地 gcc ≠ CI 编译器」当排查线索。练习区随阶段收口统一回归上线，先把题记在这里。

## 参考资源

- [GCC 15 release notes](https://gcc.gnu.org/gcc-15/changes.html)——默认 C 方言从 `gnu17` 改成 `gnu23` 的变更起点；上一次默认值变更（`gnu11` → `gnu17`）记在 [GCC 8 changes](https://gcc.gnu.org/gcc-8/changes.html)
- Clang 的默认 C 标准：release notes 不写，以 `man clang` 为准（原文「The default C language standard is gnu17」），或用 `clang -dM -E -x c /dev/null | grep __STDC_VERSION__` 当场实测——这条 `-dM -E` 也是个进阶小查法：不开探针程序，一行命令让预处理器把内置宏全吐出来、直接问出默认方言
- ISO/IEC 9899 预定义宏 `__STDC_VERSION__`：C99–C17 见 §6.10.8，C23 起重编号为 §6.10.10
- 本仓库 `.github/workflows/ci.yml`（gcc/clang 矩阵 job 的真实写法，第 15 章逐行拆）
- 本仓库 `examples/hello.c`（本章靶子程序的仓库存档；`examples/stage0-compiling-and-debug/` 是第 3～7、13 章的配套实验）
