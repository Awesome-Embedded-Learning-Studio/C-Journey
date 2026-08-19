---
title: "阶段 0 课后练习（Homework）"
description: "开发环境与编译阶段的课后练习：17 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战。难度覆盖 L1~L5，题目都做了变式处理，参考答案独立成文件、逐步解答附知识点链接。"
chapter: 0
order: 0
tags:
  - host
  - toolchain
  - build
difficulty: beginner
reading_time_minutes: 20
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 全部章节（第 1~17 章）"
related:
  - "阶段 0 Lab：解剖 hello 的一生"
  - "阶段 0 Project：给 calc 库配齐全套质量门"
---

# 阶段 0 课后练习（Homework）

## 引言

这里的题按章组织，每章两道——一道基础、一道进阶，最后还有两道跨章综合和一道 L5 挑战。每道题都标注了难度档位（L1~L5，见[练习总览](/exercises/)）和它涉及的章节；题目都是「变式」，照抄教材例题抄不出答案，得真动手在终端里跑。

答案在独立的[参考答案](homework-solutions)文件里，按题号一一对应，每步解答都带着知识点链接。建议先把一章的题做完，再翻答案对照。所有实验请在临时目录（如 `/tmp/`）里做，别污染你的源码树——这个规矩本身在第 2 章就讲过。

## 0.1 工具链体检

### 0.1-A {#hw-0-1-a}

难度 **L1** · 涉及[第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)

写出两条命令，分别回答这两个问题：①你命令行里敲的 `gcc`，实际解析到哪个二进制文件？②这个编译器给哪种 CPU/系统产出代码？把两条命令的真实输出贴出来，并用一句话解释它们为什么能回答各自的问题。

[参考答案 →](homework-solutions#hw-0-1-a)

### 0.1-B {#hw-0-1-b}

难度 **L2** · 涉及[第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)

同事跑来诉苦：他本地用 gcc 编一个用了 `bool`/`true` 的程序，一切正常；推到仓库后 CI 里 clang 那格红了，报 `unknown type name 'bool'`。他百思不得其解：「我根本没改代码」。请你分析最可能的原因，并用实验验证（写一个最小程序，在 gcc/clang 各种 `-std` 组合下真跑一遍，贴出关键输出），最后给出修复方案。

[参考答案 →](homework-solutions#hw-0-1-b)

## 0.2 编译四阶段全景

### 0.2-A {#hw-0-2-a}

难度 **L1** · 涉及[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)

用一条 `gcc` 命令，把 `hello.c` 的 `.i`/`.s`/`.o` 三个中间产物一次性全部留下，同时**不污染当前源码目录**。写出你的命令，贴出产物列表（`ls` 输出），并说明每个产物分别是哪一阶段的成果。

[参考答案 →](homework-solutions#hw-0-2-a)

### 0.2-B {#hw-0-2-b}

难度 **L3** · 涉及[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)

两小问。①写一个只调用 `printf("%s\n", "...")` 的程序，用 `gcc -S` 拿到汇编，观察 `main` 里对 `printf` 的调用变成了什么、这是哪个阶段干的、为什么。②把教材里的 AT&T 指令 `movq %rsp, %rbp` 翻译成 Intel 语法，并解释两种语法操作数顺序相反意味着什么——拿 `-masm=intel` 真跑一遍验证你的翻译。

[参考答案 →](homework-solutions#hw-0-2-b)

## 0.3 预处理深入

### 0.3-A {#hw-0-3-a}

难度 **L1** · 涉及[第 3 章：预处理深入](/00-dev-environment/04-preprocessor-deep-dive)

定义一个求平均值的带参宏 `AVG(a, b)`，要求它被 `AVG(2+4, 6)` 这样「带运算表达式的实参」调用时结果依然正确（应为 6）。再用 `gcc -E` 把展开后的文本摆出来自证。提示：教材里 `SQ_BAD` 栽的坑，求平均值一样会栽。

[参考答案 →](homework-solutions#hw-0-3-a)

### 0.3-B {#hw-0-3-b}

难度 **L3** · 涉及[第 3 章：预处理深入](/00-dev-environment/04-preprocessor-deep-dive)

有人写了这样一个「取最大值」宏：`#define MAX(a,b) a>b?a:b`。现在他调用 `MAX(x, y) + 1`，其中 `x=5`、`y=3`。请**先动笔预测**结果，再编译运行验证。如果预测和直觉不一样（你以为会得到 6），用 `gcc -E` 解释到底发生了什么，并给出修正后的宏定义。

[参考答案 →](homework-solutions#hw-0-3-b)

## 0.4 编译阶段看汇编

### 0.4-A {#hw-0-4-a}

难度 **L2** · 涉及[第 4 章：编译阶段看汇编](/00-dev-environment/05-compile-to-assembly)

写出下面这段代码里每个变量/字面量各自落在哪个段（`.text`/`.rodata`/`.data`/`.bss`，注意指针本身和它指向的字符串要分开说），**先预测，再用 `gcc -S` + `grep` 验证**，最后用 `size` 报告 `.data`/`.bss` 的字节数并与你的预测对账。

```c
int counter;              /* ① */
static int limit = 100;   /* ② */
const char* name = "cj";  /* ③ */
int main(void) {
    return counter;
}
```

[参考答案 →](homework-solutions#hw-0-4-a)

### 0.4-B {#hw-0-4-b}

难度 **L4** · 涉及[第 4 章：编译阶段看汇编](/00-dev-environment/05-compile-to-assembly)

教材里 `sum6` 验证了「前 6 个整型参数走 `edi/esi/edx/ecx/r8d/r9d`，第 7 个起走栈」。现在写一个 `sum8`，收 **8 个** `int` 参数返回总和。请**先预测**第 7、8 个参数在栈帧里相对 `rbp` 的偏移，写出理由；再用 `gcc -S -O0` 真跑验证你的预测，把相关汇编行贴出来。

[参考答案 →](homework-solutions#hw-0-4-b)

## 0.5 目标文件与符号

### 0.5-A {#hw-0-5-a}

难度 **L2** · 涉及[第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)

写一个**单个** `.c` 文件，使它编译成 `.o` 后，`nm` 输出里恰好同时出现 `T`、`t`、`D`、`B`、`U` 五种符号各至少一个。贴出你的源码和真实 `nm` 输出，并逐个说明每个符号对应源码里的谁、为什么是这个字母（大小写含义要讲清）。

[参考答案 →](homework-solutions#hw-0-5-a)

### 0.5-B {#hw-0-5-b}

难度 **L3** · 涉及[第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)

同事把工具函数 `helper` 定义成 `static` 放在 `foo.c`，然后在 `main.c` 里写 `extern int helper(int);` 并调用，链接时报 `undefined reference to 'helper'`。他说：「`helper` 明明存在，`foo.o` 里都能 `nm` 看到」。请你复现这个错误（贴真实报错），解释为什么「`nm` 能看到却链不上」，并给出两种修复方案。

[参考答案 →](homework-solutions#hw-0-5-b)

## 0.6 链接与静态库

### 0.6-A {#hw-0-6-a}

难度 **L2** · 涉及[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)

现有三个源文件：`add.c`（定义 `add`）、`use.c`（定义 `use_all`，其内部调用了 `add`）、`main.c`（`main` 只调用 `use_all`）。把 `add.o` 打成 `libadd.a`、`use.o` 打成 `libuse.a`。请写出**正确**的链接命令行，并解释为什么 `libuse.a` 必须排在 `libadd.a` 前面；再故意写反一次，贴出错误信息对比。

[参考答案 →](homework-solutions#hw-0-6-a)

### 0.6-B {#hw-0-6-b}

难度 **L3** · 涉及[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)

执行下面两条命令后，`ar t libmymath.a` 会列出哪些成员？**先预测，再真跑验证**，并解释 `ar r` 的「替换」到底替换什么、没替换什么。

```text
$ ar rcs libmymath.a add.o mul.o
$ ar rcs libmymath.a mul.o sub.o
```

[参考答案 →](homework-solutions#hw-0-6-b)

## 0.7 动态库与 dlopen

### 0.7-A {#hw-0-7-a}

难度 **L2** · 涉及[第 7 章：动态库与 dlopen](/00-dev-environment/08-dynamic-libs-and-dlopen)

写一个 `greet.c`（提供 `void greet(const char* name)`，打印问候），把它编成 `libgreet.so`，再写一个 `loader` 程序：不链接任何 `greet` 符号，运行期用 `dlopen`/`dlsym` 把它捞出来调用。贴出 `file libgreet.so` 的关键词、编译命令、运行输出，并解释链接期和运行期各发生了什么。

[参考答案 →](homework-solutions#hw-0-7-a)

### 0.7-B {#hw-0-7-b}

难度 **L3** · 涉及[第 7 章：动态库与 dlopen](/00-dev-environment/08-dynamic-libs-and-dlopen)

你的 loader 用 `dlopen("./libgreet.so", ...)` 在目录 A 里测试一切正常；换到目录 B 再跑，报 `cannot open shared object file`。请解释原因，并给出**两种**修复方案，各真跑验证一次。提示：想清楚「带斜杠的路径」和「不带斜杠的名字」在 `dlopen` 眼里意味着什么。

[参考答案 →](homework-solutions#hw-0-7-b)

## 0.8 警告旗标进阶

### 0.8-A {#hw-0-8-a}

难度 **L1** · 涉及[第 8 章：警告旗标进阶](/00-dev-environment/09-warning-flags)

写三段**独立**的最小代码，分别让以下三个旗标各亮一次：①`-Wextra`（且 `-Wall` 单独开时不亮）；②`-Wconversion`（且 `-Wall -Wextra` 都不亮）；③`-Wall` 里那个抓「`=` 当 `==`」的旗标。贴出每段的编译命令和警告输出。

[参考答案 →](homework-solutions#hw-0-8-a)

### 0.8-B {#hw-0-8-b}

难度 **L3** · 涉及[第 8 章：警告旗标进阶](/00-dev-environment/09-warning-flags)、[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

写一个函数：局部变量在某个条件分支里才被赋值，另一个分支直接读它（读未初始化变量是 UB）。先用 `-Wall -Wextra -O2` 编译，确认编译器一声不吭；再把它跑起来（多跑几次），观察输出是否每次一样。然后请回答：**有什么工具能真正在运行期抓到它？** 找出可用的一种并真实验证（如果一种工具在本机抓不到，如实报告并解释原因）。

[参考答案 →](homework-solutions#hw-0-8-b)

## 0.9 标准与优化

### 0.9-A {#hw-0-9-a}

难度 **L2** · 涉及[第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)

写一个调用 `strdup("hi")` 的最小程序，分别在 `-std=c11` 和 `-std=gnu11` 下编译，贴出两种结果并解释差异根源（说清是哪个宏在起作用）。然后在 **保持 `-std=c11`** 的前提下，给出一种让程序合法编译的写法并验证。

[参考答案 →](homework-solutions#hw-0-9-a)

### 0.9-B {#hw-0-9-b}

难度 **L4** · 涉及[第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)

教材用 `x + 100 < x` 演示了「靠溢出回绕检测溢出」的翻车。现在换一个变式：有人写 `if (x * 2 < x)` 来检测「x 太大、加倍会溢出」。用 `volatile` 变量喂入 `INT_MAX`，分别在 **gcc 和 clang 的 `-O0` 与 `-O2`** 四个组合下真跑，把四个结果如实贴出来。如果四个结果和教材里「gcc 删掉检查、clang 保留」的戏码不一样——别急着怀疑你的实验，这四个结果的一致性恰恰是本题要你发现的点。再用 `gcc -S` 看一眼 `check` 函数的汇编，回答：gcc 这次到底「删没删」你的检查？综合本题和教材的例子，说说「编译器会不会删掉一段 UB 代码」由什么决定。

[参考答案 →](homework-solutions#hw-0-9-b)

## 0.10 Sanitizer 门禁

### 0.10-A {#hw-0-10-a}

难度 **L2** · 涉及[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

写一个程序：栈上 `char buf[4]`，然后写 `buf[5] = 'x'`。用 `-O1 -g -fsanitize=address,undefined` 编译运行，贴出 ASan 的完整报告，并回答：报错类型是什么？它点名了哪个变量？`-g` 在这份报告里起了什么作用（做个去 `-g` 的对照实验）？

[参考答案 →](homework-solutions#hw-0-10-a)

### 0.10-B {#hw-0-10-b}

难度 **L3** · 涉及[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

写一个程序：`malloc` 一块能放 3 个 `int` 的内存，赋好值后 `free` 掉，然后**再读**它的第 2 个元素。用 ASan 编译运行，贴出完整报告。ASan 会给出一份「三段栈」——请标出三段各自对应你源码的哪一行、分别是什么事件（分配/释放/非法访问），并说明为什么这种 bug 传统调试特别难抓。

[参考答案 →](homework-solutions#hw-0-10-b)

## 0.11 make 入门

### 0.11-A {#hw-0-11-a}

难度 **L1** · 涉及[第 11 章：make 入门](/00-dev-environment/12-make-basics)

做一个三文件项目 `farewell.h` / `farewell.c` / `main.c`（功能自拟，比如打印「goodbye」）。手写一个朴素 Makefile：三个目标、每条规则三要素齐全、头文件写进依赖、带 `clean` 和 `.PHONY`。贴出 `make`、`./main`、`make clean` 的真实输出。

[参考答案 →](homework-solutions#hw-0-11-a)

### 0.11-B {#hw-0-11-b}

难度 **L3** · 涉及[第 11 章：make 入门](/00-dev-environment/12-make-basics)

两小问。①接 0.11-A：项目已构建完成后，执行 `touch farewell.h && make`，**先预测** make 会执行哪几条命令（几条编译、几条链接？），再真跑对答案。②用模式规则和自动变量把 0.11-A 的 Makefile 重写成通用形式，解释 `$@`/`$<`/`$^` 在每条命令里分别展开成什么。

[参考答案 →](homework-solutions#hw-0-11-b)

## 0.12 CMake 入门

### 0.12-A {#hw-0-12-a}

难度 **L2** · 涉及[第 12 章：CMake 入门](/00-dev-environment/13-cmake-basics)

把 0.11-A 的 farewell 项目迁到 CMake：写一份最小 `CMakeLists.txt`（声明最低版本、工程名、C 语言、C11 标准、可执行目标），用 out-of-source 方式配置和构建，贴出两条命令的输出，并确认 `build/` 里生成了什么（点出那份 Makefile）。

[参考答案 →](homework-solutions#hw-0-12-a)

### 0.12-B {#hw-0-12-b}

难度 **L3** · 涉及[第 12 章：CMake 入门](/00-dev-environment/13-cmake-basics)、[第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)

同事的 CMake 项目「明明设了 C11」，但你发现实际编译命令里是 `-std=gnu11`。请解释为什么会这样（真跑验证：找到 CMake 生成的 `flags.make`，贴出其中的标准旗标），说明 `gnu11` 和 `c11` 的实质差别，并给出让 CMake 真正产出 `-std=c11` 的配置。再顺手对比 `Debug` 和 `Release` 两种构建类型各自的旗标差异。

[参考答案 →](homework-solutions#hw-0-12-b)

## 0.13 GDB 基础

### 0.13-A {#hw-0-13-a}

难度 **L2** · 涉及[第 13 章：GDB 基础](/00-dev-environment/14-gdb-basics)

写一个「先算对东西、再崩溃」的程序：`compute(5)` 算三角形数，然后 `volatile int divisor = 0; int r = v / divisor;`。用 `-g -O0` 编译，进 GDB 完成一次完整的事后定位：`run` 后 GDB 报告了什么信号、停在哪一行？`bt` 看到什么？`print` 哪个变量能锁定根因？把 GDB 会话记录完整贴出来。

[参考答案 →](homework-solutions#hw-0-13-a)

### 0.13-B {#hw-0-13-b}

难度 **L3** · 涉及[第 13 章：GDB 基础](/00-dev-environment/14-gdb-basics)

写一个程序：先 `printf("before crash\n")`，然后对 `NULL` 解引用崩溃。①把它的输出重定向到文件再跑，**先预测**文件内容，再真跑验证（大概率你会预测错）；解释为什么。②修改程序（一行），让同样崩溃的场景下文件里能看到那行输出，验证之。③说说为什么「printf 调试法」对崩溃类 bug 不可靠、GDB 为什么可靠。

[参考答案 →](homework-solutions#hw-0-13-b)

## 0.14 GDB 进阶

### 0.14-A {#hw-0-14-a}

难度 **L2** · 涉及[第 14 章：GDB 进阶](/00-dev-environment/15-gdb-advanced)

写一个循环累加 $1+2+...+100$ 的程序。用**条件断点**让 GDB 只在第 50 次循环即将执行 `sum += i` 时停下来。停下的那一刻，`i` 应是多少？`sum` 应是多少（1 到 49 的和，先动笔算）？把 GDB 会话记录贴出来对答案。

[参考答案 →](homework-solutions#hw-0-14-a)

### 0.14-B {#hw-0-14-b}

难度 **L3** · 涉及[第 14 章：GDB 进阶](/00-dev-environment/15-gdb-advanced)

两小问。①写一个程序：循环累加 1 到 10，循环结束后某行代码悄悄把 `sum` 改成 999。用 **watchpoint** 定位这行「案发现场」：设置 `watch` 并配上只盯 `sum == 999` 的条件，贴出 GDB 停在改动点时的 Old/New 值。②用 0.13-A 那个崩溃程序，在 GDB 里 `generate-core-file` 生成 core，然后**不开程序**，直接 `gdb 可执行文件 core文件` 做事后分析，贴出 bt 和关键变量。

[参考答案 →](homework-solutions#hw-0-14-b)

## 0.15 Git 工作流

### 0.15-A {#hw-0-15-a}

难度 **L1** · 涉及[第 15 章：Git 工作流](/00-dev-environment/16-git-workflow)

在一个临时目录里开一个全新 git 仓库，完整走一遍：`init` → 放文件 → `add`/`commit` → 改文件 → `status`/`diff` → 第二次 `commit` → 开分支改一版 → `merge --no-ff` 合回 → `git log --oneline --graph`。贴出最终的历史图，并解释图中哪一行是合并提交、分叉发生在哪个提交。

[参考答案 →](homework-solutions#hw-0-15-a)

### 0.15-B {#hw-0-15-b}

难度 **L2** · 涉及[第 15 章：Git 工作流](/00-dev-environment/16-git-workflow)

三道笔试题。①给三个场景写规范的约定式提交信息：修了一个除零崩溃、新增了 `README.md`、重构了 Makefile 但行为不变。②为一个 C 工程写至少 4 条 `.gitignore` 条目，说明每条挡的是什么。③用你自己的话解释：既然「工作区直接 commit」也能存快照，Git 为什么还要中间隔一个暂存区？

[参考答案 →](homework-solutions#hw-0-15-b)

## 0.16 GitHub Actions

### 0.16-A {#hw-0-16-a}

难度 **L2** · 涉及[第 16 章：GitHub Actions](/00-dev-environment/17-github-actions)

写一份最小 workflow 的 YAML 骨架：名字、触发条件（push 到 main 和 pull_request 到 main）、一个 build job——`runs-on` 用什么、两个 step（checkout 和编译命令）。再把编译 step 改成 **gcc/clang 矩阵**：写出 `strategy.matrix` 部分，解释 `fail-fast: false` 的意义。

[参考答案 →](homework-solutions#hw-0-16-a)

### 0.16-B {#hw-0-16-b}

难度 **L3** · 涉及[第 16 章：GitHub Actions](/00-dev-environment/17-github-actions)

场景：「本地 `make` 全过，CI 红了」。列出**至少 4 条**可能原因，每条注明它对应 CI 里哪个 job、以及你第一步会怎么排查。提示：把第 1 章「本地 gcc ≠ CI 编译器」、第 8 章 `-Werror`、第 10 章 sanitizer、第 17 章格式门都过一遍脑子。

[参考答案 →](homework-solutions#hw-0-16-b)

## 0.17 格式化与质量门

### 0.17-A {#hw-0-17-a}

难度 **L1** · 涉及[第 17 章：格式化与质量门](/00-dev-environment/18-format-and-quality-gate)

写一个故意不合规的 `messy.c`：`int x=42;`（没空格）、`if(x==42){printf("%d\n",x);}`（单行堆叠）。用 `clang-format --dry-run --Werror` 检查并贴报错（注意退出码）；再 `clang-format -i` 原地修好；最后再跑一次检查确认退出码 0，贴出修复后的文件。

[参考答案 →](homework-solutions#hw-0-17-a)

### 0.17-B {#hw-0-17-b}

难度 **L2** · 涉及[第 17 章：格式化与质量门](/00-dev-environment/18-format-and-quality-gate)

两道笔试题。①本项目 `.clang-format` 里 `SortIncludes: Never` 和 `AllowShortIfStatementsOnASingleLine: Never` 两个设置看起来很「反常规」（别的项目常开自动排序、允许单行 if）。说说这两个设置各是为了什么。②格式门在「编辑器、本地提交、远端 CI」三个层面各是怎么守住的？为什么要有三层？

[参考答案 →](homework-solutions#hw-0-17-b)

## 0.C 跨章综合与挑战

### 0.C-1 {#hw-0-c-1}

难度 **L3** · 涉及[第 10 章](/00-dev-environment/11-sanitizer-gate)、[第 11 章](/00-dev-environment/12-make-basics)、[第 17 章](/00-dev-environment/18-format-and-quality-gate)

一条龙综合题。做一个两文件项目：`calc.c`（`add`/`mul`）+ `main.c`（`main` 里**故意埋一个栈越界写**）。依次完成：①写 Makefile（带 `clean`/`.PHONY`）并构建通过——注意，如果你此时直接 `./main` 会崩，别慌，把崩溃现象原样记下来，它是本题的伏笔；②`clang-format --dry-run --Werror` 检查代码格式；③用 sanitizer 构建（`-fsanitize=address,undefined`）运行，贴出它抓到的报告并定位到源码行；④修复 bug；⑤重跑 sanitizer 确认全绿。把五步的命令和输出按顺序贴全。

[参考答案 →](homework-solutions#hw-0-c-1)

### 0.C-2 {#hw-0-c-2}

难度 **L4** · 涉及[第 5 章](/00-dev-environment/06-object-files-and-symbols)、[第 6 章](/00-dev-environment/07-linking-and-static-libs)

黑盒诊断题。`math_utils.c` 定义了 `add`、`mul` 和一个 `static int helper(int)`；`main.c` 里声明并调用了 `add`、`helper`（写了 `extern`）、以及一个**哪都没实现**的 `sub`。链接后报错。请：①复现并贴出完整报错；②逐条分析每个 `undefined reference` 的成因（这两个错成因不同！）；③给出正确的修复。再补一问：如果把这个项目打包成 `libmath.a` 并把链接顺序排成错误的顺序，会得到什么样的报错——顺手真跑验证。

[参考答案 →](homework-solutions#hw-0-c-2)

### 0.C-3 {#hw-0-c-3}

难度 **L5** · 涉及[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)、[第 6 章](/00-dev-environment/07-linking-and-static-libs)、[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)

挑战题（改编自「徒手链接」经典练习）。这次**不调用 gcc 的一站式编译**，手动走完全部四个阶段并让程序真正跑起来：用 `gcc -E` 预处理、`gcc -S` 编译、**`as`** 汇编、最后**直接调用 `ld`** 完成链接（提示：链接器需要启动文件 `crt1.o`/`crti.o`/`crtn.o`、libc 和动态链接器，它们的路径可以用 `gcc -print-file-name=...` 探出来；注意现代 gcc 默认产 PIE 代码，`ld` 需要匹配的启动文件和选项）。把每一阶段的命令、关键产物、`ld` 那条命令的完整写法，以及最终 `./hello` 的输出全部贴出来。卡住很正常——这一题的正确姿势就是「照着报错信息一步步把缺的部件补上」。

[参考答案 →](homework-solutions#hw-0-c-3)
