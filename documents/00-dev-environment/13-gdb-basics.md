---
title: "GDB 基础：在程序崩掉的地方停下来看现场"
description: "程序段错误了，直接跑只收获一个 Segmentation fault、连 printf 的调试输出都看不到——这一章用 GDB 在崩掉的地方停下来。拿一个「算完阶乘再 NULL 解引用」的程序真跑：gdb run 触发 SIGSEGV，它告诉你崩在 crash.c 第几行、当时的调用栈、以及 p 是 (int*)0x0、x 是 120；再演示断点（break）、运行（run）、单步（next/step）、查看变量（print/display）、调用栈（backtrace）、继续（continue）这套基础流程。顺便说清一个坑：为什么直接跑时连 printf 的输出都丢了（stdout 非终端全缓冲 + 段错误未刷新）——这正是靠 printf 调试崩溃不可靠、GDB 直接读变量才靠谱的原因。"
chapter: 0
order: 13
tags:
  - host
  - gdb
  - debug
difficulty: beginner
reading_time_minutes: 13
platform: host
c_standard: [11]
prerequisites:
  - "第 9 章：标准与优化（-g 调试信息，GDB 的前提）"
  - "第 10 章：Sanitizer 门禁（出错自动报的「被动」路子，对照本章「主动」停下来看）"
related:
  - "第 14 章：GDB 进阶（watchpoint、条件断点、core dump、TUI）"
  - "第 4 章：编译阶段看汇编（-O2 下 <optimized out>，所以调试回 -O0）"
---

# GDB 基础：在程序崩掉的地方停下来看现场

## 引言：程序段错误了，然后呢

调试 C 程序，你迟早会撞上一个段错误（`Segmentation fault`）。直接跑，你的全部线索就是终端吐出来的一句 `Segmentation fault` 和一个 139 的退出码——崩在哪？为什么崩？当时变量的值是什么？一概不知。更气人的是，你可能想「我加个 printf 打印一下」，结果发现连 printf 的输出都看不到（这个坑等下真跑时你会亲眼看到）。

**GDB**（GNU Debugger）就是干这件事的：它让程序在你的掌控下运行，崩了就停在崩溃的那一行，让你查看「现场」——调用栈、当时每个变量的值、甚至内存。这一章我们拿一个故意会段错误的程序，把 GDB 最基础的那套流程（启动、断点、运行、单步、查看）真跑一遍。这套是后面所有调试的地基。

先说明一个大前提（第 9 章讲过、这里再用）：**用 GDB 调试，程序必须带 `-g`，且用 `-O0` 编译**。没有 `-g`，GDB 只能给你一串内存地址、看不出对应源码第几行；开了 `-O2`，变量被优化没了你会满屏 `<optimized out>`。所以本章所有例子都是 `gcc -g -O0`。

## 靶子程序：算完阶乘，然后 NULL 解引用

我们写一个先做正经计算、再故意段错误的程序，用来当 GDB 的靶子：

```c
#include <stdio.h>

int compute(int n) {
    int result = 1;
    for (int i = 1; i <= n; i++) {
        result *= i;
    }
    return result;
}

int main(void) {
    int* p = NULL;
    int x = compute(5);
    printf("5! = %d\n", x);
    *p = x; /* NULL 解引用 → SIGSEGV */
    return 0;
}
```

`main` 先调 `compute(5)` 算出 5 的阶乘 120，打印出来，然后 `*p = x`——但 `p` 是 `NULL`，对空指针解引用会触发段错误。先带上 `-g -O0` 编译出来，直接跑一次看会发生什么：

```text
$ gcc -std=c11 -Wall -Wextra -g -O0 crash.c -o crash
$ ./crash
Segmentation fault
$ echo $?
139
```

注意一个细节：你可能预期 `compute` 算完后那行 `printf("5! = 120\n")` 的输出该先打出来，但实际常常**什么都没有**——直接就是一句 `Segmentation fault`。这是因为 `printf` 的输出走的是 `stdout`，当它不是直接连终端（比如被重定向、或在脚本里跑）时是**全缓冲**的，数据先攒在内存缓冲区里、等攒满或程序正常退出才刷出去；而段错误是信号把进程直接打死的，**缓冲区根本没机会刷新**，于是 printf 那行就丢了。这恰恰说明了「靠 printf 调试段错误」有多不靠谱——你连最后一行 printf 都看不到。GDB 不依赖你的 printf，它直接读内存里变量的真实值，这才是调试崩溃的正路。

## 在 GDB 里看崩溃现场

现在把程序交给 GDB 跑。我们让 GDB 直接 `run`，崩了之后看调用栈和变量：

```text
$ gdb -q ./crash
(gdb) run
...
Program received signal SIGSEGV, Segmentation fault.
0x00005555555551a8 in main () at crash.c:15
15	    *p = x; /* NULL 解引用 → SIGSEGV */
```

（`gdb` 启动时那几行关于 debuginfod 的提示我们略掉了——它问你要不要联网下载调试符号，命令行里跑选 `n` 跳过即可，不影响。）看 GDB 给的信息：程序收到 `SIGSEGV`（段错误信号），崩溃位置精确到 **`main () at crash.c:15`**——也就是 `*p = x` 那一行，GDB 还贴心地把那行源码打印出来给你对照。比起直接跑时那句干巴巴的 `Segmentation fault`，现在你明确知道「崩在第 15 行」。

接下来看「现场」。先看调用栈（`backtrace`，简写 `bt`），再看当时的变量：

```text
(gdb) bt
#0  0x00005555555551a8 in main () at crash.c:15
(gdb) print p
$1 = (int *) 0x0
(gdb) print x
$2 = 120
(gdb) info locals
p = 0x0
x = 120
```

这几条是 GDB 最常用的「看现场」命令。`bt` 列出调用栈（这个例子只有 `main` 一层，所以只有 `#0`；如果是在某个深层函数里崩的，`bt` 会一层层列出谁调用了谁，你能顺着栈帧往上查）。`print p`（简写 `p`）打印变量 `p` 的值——`(int *) 0x0`，它就是个空指针，这就是崩的根因；`print x` 显示 `120`，说明 `compute(5)` 确实算对了。`info locals` 一次性把当前栈帧的所有局部变量列出来。到这里，这个段错误的来龙去脉就全清楚了：第 15 行对值为 `0x0`（NULL）的 `p` 解引用。**整个过程没写一行 printf**，这就是 GDB 的价值。

## 断点、单步、查看：主动控制执行

上一节是「让程序自己崩，我们事后看现场」。但很多 bug 不是段错误，而是「逻辑错了、程序正常跑完但结果不对」——这时你要主动控制程序的执行节奏，在可疑的地方停下来、一步一步走、边走边看变量。这套命令是：断点（`break`）、运行（`run`）、单步（`next`/`step`）、查看（`print`/`display`）、继续（`continue`）。

我们重新进 GDB，这次主动在 `compute` 函数设断点：

```text
(gdb) break compute
Breakpoint 1 at 0x1140: file crash.c, line 4.
(gdb) run
...
Breakpoint 1, compute (n=5) at crash.c:4
4	    int result = 1;
```

`break compute`（简写 `b`）在 `compute` 函数的入口设了一个断点，GDB 报告它落在 `crash.c:4`。然后 `run`（简写 `r`）启动程序——程序执行到断点就停住，GDB 告诉你「停在 `compute (n=5)` 的第 4 行」，连参数 `n=5` 都显示出来了。注意这次是从头跑、在断点处**主动**停下，不是崩了才停。

停下来之后，你就可以查看和单步了：

```text
(gdb) print n
$1 = 5
(gdb) next
5	    for (int i = 1; i <= n; i++) {
(gdb) print result
$2 = 1
(gdb) continue
...
Program received signal SIGSEGV, Segmentation fault.
```

`print n` 确认传进来的 `n` 是 5；`next`（简写 `n`）执行**一行**代码（执行了 `int result = 1;`，停在下一行 `for`），所以 `print result` 现在能看到 `1`；`continue`（简写 `c`）放开手脚继续跑，直到下一个断点或程序结束——这里没有别的断点，于是它一路跑到 `*p = x` 又段错误停下。

这里要分清两个单步命令：`next`（`n`）是「单步、但**不进入**函数调用」（把函数调用当成一行整体执行过去），`step`（`s`）是「单步、且**进入**函数内部」。调试时想跳过已经确认没问题的函数就用 `next`、想钻进某个可疑函数细看就用 `step`。如果你盯着的变量想每步都自动显示，用 `display result`（它会在每次停下来时自动打印，不用每次手敲 `print`），取消用 `undisplay`。

## 常用命令速查

把这一章用到的命令收拢一下（GDB 命令都支持简写，括号里是简写）：

- `run`（`r`）：从头运行程序，到断点或崩溃或结束。
- `break 位置`（`b`）：设断点，位置可以是函数名（`b compute`）、行号（`b crash.c:15`）、甚至文件:行号。
- `next`（`n`）/ `step`（`s`）：单步一行，前者不进函数、后者进函数。
- `continue`（`c`）：继续跑到下一个断点或结束。
- `print 变量`（`p`）：打印变量/表达式的值；`info locals` 打印所有局部变量。
- `backtrace`（`bt`）：看调用栈；`frame N` 切到第 N 层栈帧。
- `list`（`l`）：看当前位置附近的源码，方便对照。
- `quit`（`q`）：退出 GDB。

这套命令能覆盖绝大多数「停下来看现场」的调试场景。更进阶的手段——在变量被改时停下来的 `watch`、带条件的 `break ... if`、崩溃后还能查的 core dump、分屏看源码/汇编的 TUI——我们留到第 14 章。

## 小结

GDB 让你在程序崩掉或可疑的地方停下来，直接读内存里的变量值和调用栈，而不依赖容易被段错误吞掉的 printf。用它的前提是 `-g -O0` 编译（第 9 章讲过：没 `-g` 只有地址、开了 `-O2` 变量 `<optimized out>`）。最经典的用法是事后定位段错误：`gdb ./prog` 进去 `run`，崩了它会精确告诉你崩在哪个文件第几行（我们的例子是 `main () at crash.c:15`），再用 `bt` 看调用栈、`print p`/`info locals` 看当时变量（`p = 0x0`、`x = 120`），根因立刻浮现。主动调试靠断点（`break`）+ 运行（`run`）+ 单步（`next` 不进函数 / `step` 进函数）+ 查看（`print`/`display`/`info locals`）+ 继续（`continue`）这套流程，配合 `list` 对照源码。别忘了那个诚实的小坑：直接跑段错误时连 printf 的输出都可能丢（`stdout` 非终端全缓冲、被信号打死没刷新），所以崩了别指望 printf 救你、上 GDB。下一章我们继续往深了挖，看 watchpoint、条件断点、core dump 这些更趁手的兵器。

## 参考资源

- GDB 手册（`help` 命令在 GDB 内随时可查）：`run`/`break`/`next`/`step`/`continue`/`print`/`display`/`backtrace`/`frame`/`info locals`/`list`
- 第 9 章：标准与优化（`-g` 调试信息、`-O0` 才好调试、`-O2` 下 `<optimized out>`）
- 第 4 章：编译阶段看汇编（为什么 `-O2` 下 GDB 看不到变量）
- 第 10 章：Sanitizer 门禁（ASan/UBSan 自动报错的「被动」路子，和本章「主动停下来」互补）
- 第 14 章：GDB 进阶（`watch`、条件断点、core dump、TUI）
