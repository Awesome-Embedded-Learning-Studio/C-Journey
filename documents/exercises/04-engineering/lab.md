---
title: "阶段 4 Lab：给 statslib 配齐全套质量门"
description: "工程化阶段动手实验：把一个三文件的小统计库 statslib 从「能编」推到「可信」——头文件契约体检、Makefile 头依赖、CMake 三态、Unity 测试、sanitizer 抓真 bug、gcov 覆盖率，六步加一道 malloc 失败注入的 L5 挑战，每步有验收标准。"
chapter: 4
order: 2
tags:
  - host
  - engineering
  - testing
  - build
difficulty: intermediate
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 第 1~16 章（重点：第 1、4、5、7、10、13 章）"
related:
  - "阶段 4 Homework"
  - "阶段 4 Project"
---

# 阶段 4 Lab：给 statslib 配齐全套质量门

## 实验目标

阶段 4 的十六章立了十几道防线，每一章都是「为了讲这件事、搭一个最小例子」。这个 Lab 把镜头对准**同一个**三文件小库 `statslib`——它只有两个函数（求均值、求极差），但你要把它从头到尾推过六道门：头文件契约体检、Makefile 头依赖、CMake 三态、Unity 测试、sanitizer（这一步会真的抓到一个我们埋进去的 use-after-free，就像收官章抓 clib 那样）、gcov 覆盖率。做完你会对「把一个库从能编推到可信」这句话有肌肉记忆：每一道门都不是摆设，它们一道接一道地拦住不同层的错误。

所有实验在 `/tmp` 下独立目录做。每步有验收标准；卡住先回题面标注的章节链接读教材，再不行看[实验参考](lab-solutions)。

## 步骤 1：工具链体检与头文件契约（L1）{#lab-1}

**目标**：确认本机工具链，钉死头文件三条契约。

1. 跑 `gcc --version`、`clang --version`、`cmake --version`、`gdb --version`、`valgrind --version`（valgrind 可能没装，没装就如实记下），贴出各版本号。
2. 纸面答三问：① 新建一个 `statslib.h`，它的 include guard 三行怎么写？② 用一句话复述 ODR，并说明 `int stats_mean(...)` 这样的函数**原型**放进头文件为什么不算违反 ODR；③ 头文件里若要放内联辅助函数，该写 `static inline` 还是裸 `inline`？为什么？

**验收标准**：贴出各工具版本；三问答案写进同一个文本文件 `notes.md` 里。

[实验参考 →](lab-solutions#lab-1)

## 步骤 2：Makefile 头依赖（L2）{#lab-2}

**目标**：用 `-MMD -MP` 让 make 替你管头文件依赖。

1. 搭 `statslib` 工程：`statslib.h`（guard + 两个原型）、`statslib.c`（求均值 `stats_mean`、求极差 `stats_range`，返回码风格：失败返 -1、结果走出参）、`demo.c`（调两个函数打印）。
2. 手写 Makefile：`CC`/`CFLAGS` 变量（`-std=c11 -Wall -Wextra -MMD -MP`）、模式规则、`DEPS := $(wildcard *.d)` + `-include`、`clean`。
3. 实验：干净构建跑出结果；`touch statslib.h` 再 `make`，观察谁被重编；把 `statslib.h` 里的一个注释改掉再 `make`，观察是不是又全部重编。

**验收标准**：贴出构建输出、`touch` 后的重编输出；一句话说清 `-MMD` 为什么能替你「记得」每个 `.o` 依赖哪些头。

[实验参考 →](lab-solutions#lab-2)

## 步骤 3：CMake 三态与多配置（L3）{#lab-3}

**目标**：把 statslib 迁进 CMake，验证 target 传播三态和 Debug/Release 旗标切换。

1. 写 `CMakeLists.txt`：`add_library(stats STATIC statslib.c)`、`target_include_directories` 把当前目录标 PUBLIC、`target_compile_options` 把 `-Wall -Wextra` 标 PRIVATE、`target_compile_features` 把 `c_std_11` 标 PUBLIC；配一个 `add_executable(demo demo.c)` 链 `stats`。
2. 各配一个 Debug 和 Release 的 build 目录，构建后 `grep C_FLAGS` 两个目录的 `flags.make`，贴出两行。
3. 在 `demo.c` 里加一条 `assert(0 && "should fire under Debug")`，Debug 版和 Release 版各跑一次贴退出码。

**验收标准**：贴出两份 `C_FLAGS` 与两次运行退出码；说出 `NDEBUG` 是谁带进来的、对 `assert` 干了什么。

[实验参考 →](lab-solutions#lab-3)

## 步骤 4：Unity 测试 + CTest（L3）{#lab-4}

**目标**：把「演示」升级成「测试」——断言、隔离、总账、CTest 红绿。

1. 写 `mini_unity.h/.c`（照着教材第 7 章 ~30 行的教学版复刻：`TEST_ASSERT_EQUAL_INT`、`TEST_ASSERT_TRUE`、`setjmp`/`longjmp` 隔离、`UNITY_BEGIN/END`）。
2. 写 `test_stats.c`：五条用例——均值正常、均值空数组返 -1、极差正常、极差单元素返 0、极差空数组返 -1。
3. CMake 里 `enable_testing()` + `add_test`（把 demo 去掉或留着都行），build 后 `ctest --output-on-failure`。

**验收标准**：贴出 ctest 输出和 Unity 自己的 `5 Tests 0 Failures`；说清 `longjmp` 那一跳为什么「一条 FAIL 不拖死全家」。

[实验参考 →](lab-solutions#lab-4)

## 步骤 5：sanitizer 抓到真 bug（L4）{#lab-5}

**目标**：验证「ctest 全绿 ≠ 没有内存 bug」，并修掉它。

`statslib.c` 的 `stats_range` 里埋着一颗雷：它 `malloc` 了一个临时副本算完 `lo`/`hi`，`free` 之后又读了一下副本的第一个元素做「防御性校验」（结果恰好不受影响，所以普通构建下测试全绿）。用 CI 的姿势（`CC=clang CFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g LDFLAGS=-fsanitize=address,undefined`）重新 configure + build + ctest，贴出 ASan 报告（指出报错类型、行号、`freed by thread T0 here` 那段）。然后把那个越界读修掉，重跑 ctest 贴全绿输出。顺手跑一遍格式门：`clang-format --dry-run --Werror` 查 `*.c *.h`。

**验收标准**：贴出「绿 → sanitizer 红 → 修后绿」三段真实输出 + 格式门退出码；说出 sanitizer 这道门为什么是收官章「最值钱的一刀」。

[实验参考 →](lab-solutions#lab-5)

## 步骤 6：gcov 覆盖率（L4）{#lab-6}

**目标**：把「测试盖到了哪」量化成数字，再补用例提上去。

1. 用 `-DCMAKE_C_FLAGS="--coverage -g -O0" -DCMAKE_EXE_LINKER_FLAGS="--coverage"` 重新配置构建跑 ctest，对 `statslib.c` 跑 `gcov -b`，贴四个数字（Lines / Branches / Taken at least once / Calls）。
2. 打开 `statslib.c.gcov`，找出 `#####` 的行——它们对应哪些没测到的分支？
3. 补用例把死分支救活（想想哪些入参路径没测过），重跑 ctest + gcov，贴新数字。

**验收标准**：贴出前后两份 gcov 数字；指出你补的用例分别救活了哪条分支。

[实验参考 →](lab-solutions#lab-6)

## 附加挑战（L5）：malloc 失败注入 {#lab-l5}

**目标**：用 `--wrap` 把「malloc 失败」这个几乎不可能自然发生的分支测掉（教材外补充：故障注入 fault injection 的思路，源自 Test Double 那套「按脚本返回」的哲学；`--wrap` 本身见教材第 8 章）。

`stats_range` 里 `malloc` 失败会返 -1——这条分支平时的测试根本走不到。写一个 `wrap_malloc.c`：`__wrap_malloc` 平时透传给 `__real_malloc`，当外部 `arm_failure()` 一下之后、下一次调用直接返回 `NULL`。测试程序在调用 `stats_range` 前 `arm_failure()`，断言它返回 -1；再测一次正常路径确认没被污染。链接加 `-Wl,--wrap,malloc`。gcc 和 clang 双跑。

**验收标准**：贴出两个编译器的输出；说清为什么「arm 一下、下一次就失败」的设计能保证别的 `malloc` 调用不被误伤（结合 `__real_malloc` 透传说）。

[实验参考 →](lab-solutions#lab-l5)

## 提交物清单

一个目录装下全部源码（`statslib.h/.c`、`demo.c`、`test_stats.c`、`mini_unity.h/.c`、`wrap_malloc.c`、`Makefile`、`CMakeLists.txt`）、每步终端记录（`stepN.log`）、以及 200 字以内的小结——用你自己的话说清「ctest 全绿」和「代码可信」之间，你在这六步里看明白的差距是什么。
