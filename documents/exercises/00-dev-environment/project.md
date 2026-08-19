---
title: "阶段 0 Project：给 calc 库配齐全套工程化质量门"
description: "阶段 0 综合项目：把一个四则运算库 calc 从「能编」做到「可信」——Makefile 增量构建、CMake 迁移、sanitizer 抓 UB（INT_MIN / -1 除溢出）、格式门、CI workflow 与 git 全程。任务分四层，难度 L2~L5。"
chapter: 0
order: 4
tags:
  - host
  - toolchain
  - build
difficulty: intermediate
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 全部章节"
related:
  - "阶段 0 Homework"
  - "阶段 0 Lab"
---

# 阶段 0 Project：给 calc 库配齐全套工程化质量门

## 项目定位

Homework 是点、Lab 是线，Project 是面：把一个四则运算库 `calc` 从「能编」一路做到「可信」。你手里有阶段 0 的全部工具——gcc/clang 的旗标纪律、make 的增量构建、CMake 的声明式配置、sanitizer 的运行期兜底、clang-format 的格式门、GitHub Actions 的 CI、git 的版本管理。这个项目把它们全部用上：先让库跑起来（核心），再把它迁到 CMake（进阶），然后埋一个真实世界最常见的 UB 让 sanitizer 抓给你看（再进阶），最后配好 CI 和 git 全程（终极）。

任务分四层，每层都有验收标准。建议一层做完、验收对上了再做下一层。卡住了看[参考实现](project-solutions)——它是按任务分层组织的，可以只读你卡住的那一层。

## 任务分层

### 核心任务（L2）：库 + CLI + Makefile {#pj-core}

**L1 热身**：先把四个函数声明写进 `include/calc.h`、用[阶段 1 第 1 章](/01-c-basics/01-program-structure-and-compilation)「声明 vs 定义」的规矩（预告，做阶段 0 项目时还没学到）搭好 `src/calc.c` 与 `src/main.c` 骨架——不实现逻辑，只求两个 `.c` 各自 `gcc -c` 零警告通过。

做一个项目目录（比如 `calc-project/`），结构如下：`include/calc.h`（四个函数声明）、`src/calc.c`（实现 `calc_add/calc_sub/calc_mul/calc_div`）、`src/main.c`（CLI：`./main add 2 3` 这类用法，解析 `argv` 并调用对应函数；`div` 遇到除数为 0 要报错并以退出码 1 结束）。再加一个 Makefile：变量 `CC`/`CFLAGS`、模式规则 `%.o: %.c`、头文件目录 `-Iinclude`、`clean` 与 `.PHONY`。顺手写一个 `test.sh` 回归脚本：跑 6 条断言（5 个常规用例 + 1 个「除零报错退出码 1」用例），断言每个输出，错一个就非 0 退出；Makefile 里加 `test` 目标调它。

**验收标准**：`make` 全绿；`./main add 2 3` 输出 `5`；`make test` 六条断言全 ok（含 `div 1 0` 报错退出码 1 那条）；`make clean` 后目录回到只有源码。

[参考实现 →](project-solutions#pj-core)

### 进阶任务（L3）：CMake 迁移 {#pj-cmake}

写一份 `CMakeLists.txt`：最低版本、工程名 + C 语言、`CMAKE_C_STANDARD 11` + `CMAKE_C_STANDARD_REQUIRED ON`、**`CMAKE_C_EXTENSIONS OFF`**、`add_executable`、`target_include_directories` 指到 `include/`。配置并构建出 `build/main`，跑一个用例验证。

**验收标准**：贴出 `cmake -B build` 与 `cmake --build build` 的关键输出、`./build/main` 的运行结果，以及 `flags.make` 里的 `C_FLAGS`——它必须是 `-std=c11`，**不是** `-std=gnu11`。

[参考实现 →](project-solutions#pj-cmake)

### 再进阶任务（L4）：sanitizer 门与修复 {#pj-sanitize}

`calc_div` 现在是朴素的 `return a / b;`——它藏着一个经典的 UB：`INT_MIN / -1`（结果 `2147483648` 超出 `int` 范围）。做两个实验：①普通构建下跑 `./main div -2147483648 -1`，记下现象和退出码；②用 sanitizer 构建（`-fsanitize=address,undefined`）再跑同一条命令，贴出报告。然后修复 `calc_div`（对 `INT_MIN / -1` 显式处理，需要 `#include <limits.h>`），sanitizer 构建下重跑确认全绿。最后用 `clang-format --dry-run --Werror` 把 `src/*.c` 和 `include/*.h` 全查一遍（先故意搞乱一行再修回来也行，体会一下格式门怎么拦）。

**验收标准**：贴出两个实验的完整输出、修复后的全绿结果、格式门退出码 0。能说清普通构建和 sanitizer 构建对同一个 bug 的两种「报法」有什么不同。

[参考实现 →](project-solutions#pj-sanitize)

### 终极挑战（L5）：CI workflow + git 全程 {#pj-ci}

给项目写 `.github/workflows/ci.yml`：三个 job——`build`（gcc/clang 矩阵，`make clean && make && make test`）、`sanitize`（clang + `-fsanitize=address,undefined` 再跑测试）、`format-check`（`clang-format --dry-run --Werror`）。再写 `.gitignore`（挡 `main`、`build/`、`src/*.o`）。最后 `git init -b main`，按约定式提交把项目提交进去，贴出 `git log`。本地把你 workflow 里的 format-check 和 sanitize 两个 job 的核心命令复现一遍，确认退出码 0 就是 GitHub 上那个绿勾。

**验收标准**：贴出 ci.yml 全文（或骨架 + 关键块）、`.gitignore`、`git log --oneline`，以及两个 job 的本地复现结果。

[参考实现 →](project-solutions#pj-ci)

## 提交物清单

项目目录 + 每层的终端记录（命令 + 输出）。最后一层做完，写一段 200 字以内的小结：说说「能编」和「可信」之间，你在这个项目里到底补上了哪几道门。
