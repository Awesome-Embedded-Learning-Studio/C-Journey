---
title: "阶段 0 Lab：解剖 hello 的一生"
description: "阶段 0 动手实验：把一个 hello 程序从 .c 一路解剖到可执行文件与崩溃现场——四阶段停靠、预处理侦探、nm/readelf 透视、静态库顺序陷阱、sanitizer 抓 UB、GDB 定位崩溃，最后在 strip 后的裸二进制上做汇编级定位（L5 挑战）。"
chapter: 0
order: 2
tags:
  - host
  - toolchain
  - debug
difficulty: beginner
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 第 1~14 章"
related:
  - "阶段 0 Homework"
  - "阶段 0 Project"
---

# 阶段 0 Lab：解剖 hello 的一生

## 实验目标

Homework 是一道道独立的题，这个 Lab 则是一条贯穿的线：我们从零写一个 `hello`，然后亲手把它解剖一遍——看它怎么从 `.c` 变成可执行文件（四阶段）、它的宏和条件编译在预处理站发生了什么、它的 `.o` 里记着什么账（符号表与重定位表）、它被打包成库后链接顺序怎么咬人，最后再解剖它的两种「死法」：被 sanitizer 当场抓住的 UB，和被 GDB 事后定位的段错误。做完这个 Lab，阶段 0 的 17 章就不是 17 个孤立知识点，而是一条你亲手摸过的流水线。

所有实验都在 `/tmp` 下的独立目录做（`-save-temps` 会弄脏目录，这个坑第 2 章讲过）。每一步都有验收标准，输出对得上才算过；卡住了先回[题面标注的章节链接](#lab-1)读教材，再不行就看[实验参考](lab-solutions)。

## 步骤 1：四阶段停靠 {#lab-1}

难度 **L1** · 涉及[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)

**目标**：把 `hello` 的四段流水线（预处理 → 编译 → 汇编 → 链接）在每一站停下来看产物。

1. 在 `/tmp/cj-ex0-lab/` 下写一个带宏的 `labhello.c`（内容自拟，比如 `#define GREET "hello from lab"` 然后 `printf("%s\n", GREET)`）。
2. 依次执行 `gcc -std=c11 -E`、`-S`、`-c`，最后 `gcc labhello.o -o labhello`，每站之后用 `wc -l` 或 `file` 观察产物。
3. 完整跑一遍 `./labhello`。

**验收标准**：贴出 `.i` 的行数（远大于源码行数）、`.s` 的行数、`file` 对 `.o` 和可执行文件分别给出的关键词（`relocatable` / `pie executable`），以及程序输出。你能说出这四个产物各对应哪一站。

[实验参考 →](lab-solutions#lab-1)

## 步骤 2：预处理侦探 {#lab-2}

难度 **L2** · 涉及[第 3 章：预处理深入](/00-dev-environment/04-preprocessor-deep-dive)

**目标**：用 `gcc -E` 当场拆穿「条件编译走了哪一支」。

1. 写一个 `#ifdef LAB_MODE / #else / #endif` 的程序，两支分别打印不同内容。
2. 不带 `-D` 编译运行一次；带 `-DLAB_MODE` 编译运行一次。
3. 对两种编译命令各做一次 `gcc -E`，用 `grep -c` 统计两支文本各自「存活」的条数（0 还是 1）。

**验收标准**：贴出两次运行的不同输出，以及四次 `grep -c` 的结果；能说清「被删掉的那一支去了哪里」。

[实验参考 →](lab-solutions#lab-2)

## 步骤 3：符号与重定位透视 {#lab-3}

难度 **L2** · 涉及[第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)

**目标**：用 `nm` 和 `readelf -r` 读懂一个 `.o` 随身带的两张表。

1. 写 `lab_lib.c`：一个已初始化全局 `counter = 7`、一个零初始化全局 `tally`、一个 `static int helper(int)`、一个全局函数 `visible_fn`（内部调用 `helper`）。
2. 写 `lab_main.c`：`main` 只声明并调用 `visible_fn`，再调 `printf`。
3. 分别 `gcc -c`，对两个 `.o` 各跑一次 `nm`；对 `lab_main.o` 跑 `readelf -r`。

**验收标准**：贴出两份 `nm` 输出，逐个符号说出字母含义（`T/t/D/B/U`）；贴出 `readelf -r`，说出三条重定位条目各对应源码里哪一处引用。

[实验参考 →](lab-solutions#lab-3)

## 步骤 4：静态库与顺序陷阱 {#lab-4}

难度 **L3** · 涉及[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)

**目标**：亲手打包 `.a`，再故意把链接顺序写反，看报错长什么样。

1. `ar rcs liblab.a lab_lib.o`，用 `ar t` 确认成员。
2. 用**正确**顺序链接：`gcc lab_main.o -L. -llab -o lab_ok`，运行。
3. 用**错误**顺序链接：`gcc -L. -llab lab_main.o -o lab_bad`，把报错贴下来。

**验收标准**：贴出正确顺序的运行结果（应输出 `r=17`）、错误顺序的报错全文；能解释为什么「库在前」时链接器一个成员都不抽。

[实验参考 →](lab-solutions#lab-4)

## 步骤 5：sanitizer 抓 UB {#lab-5}

难度 **L3** · 涉及[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

**目标**：埋一个越界写，让 ASan/UBSan 当场把它钉死在源码行上。

1. 写 `ub_lab.c`：栈上 `int vals[2] = {1, 2};`，然后 `vals[3] = 42;`。
2. 用 `-O1 -g -fsanitize=address,undefined` 编译运行，贴出完整报告。
3. 修复越界（`vals[3]` → `vals[1]`），重编重跑，确认退出码 0。

**验收标准**：贴出报告中的报错类型（`stack-buffer-overflow`）、精确到源码行列的定位、以及修复后全绿的输出；能说出报告里 UBSan 和 ASan 各自报了哪一段。

[实验参考 →](lab-solutions#lab-5)

## 步骤 6：GDB 崩溃定位 {#lab-6}

难度 **L4** · 涉及[第 13 章：GDB 基础](/00-dev-environment/14-gdb-basics)（缓冲丢失的分析在那一章的「靶子程序」一节）

**目标**：一个段错误程序，用 GDB 事后定位根因，全程不写 printf。

1. 写 `crash_lab.c`：`compute(4)` 算阶乘，打印结果，然后对 `NULL` 指针解引用写值。
2. `-g -O0` 编译；先直接跑一次，记下退出码。
3. 进 GDB：`run` 看它停在哪一行，`bt` 看栈，`print` 定位根因。

**验收标准**：贴出 GDB 报告的崩溃行号、`print p` 和 `print x` 的值；能解释为什么直接跑时连 `printf` 的输出都可能看不见（想想缓冲）。

[实验参考 →](lab-solutions#lab-6)

## 附加挑战（L5）：strip 后的汇编级定位 {#lab-l5}

**目标**：把符号表整个剥掉，在只剩机器码的裸二进制上做汇编级崩溃定位——这题改编自「无符号调试」经典练习，是给想看看山顶的人准备的。

1. `strip` 掉 `crash_lab` 的符号表，对比 `strip` 前后 `file` 输出。
2. 在 GDB 里跑这个 strip 版：`run` 崩掉后，观察 `bt` 变成了什么（一堆 `??`）；用 `x/i $pc` 看**崩在的那条指令本身**，用 `p/x $rax` 看它写入的寄存器值。
3. 结合第 4 章的汇编知识，从那条指令判断根因（这条指令要往哪写？那个寄存器里是什么？）。

**验收标准**：贴出 strip 版的 `bt`（应只剩 `??` 和地址）、`x/i $pc` 的指令、`p/x $rax` 的值；用一句话说清「没有符号表时，你凭什么还能定位这个 bug」。

[实验参考 →](lab-solutions#lab-l5)

## 提交物清单

一个目录（比如 `lab-stage0/`）装下：全部源码文件、每个步骤的终端记录（命令 + 输出，可存成 `stepN.log`）、以及一段 200 字以内的小结——用你自己的话写清楚「hello 的一生」你看到了哪几站、每站的核心产物是什么。
