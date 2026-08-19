---
title: "阶段 0 Project 参考实现"
description: "阶段 0 综合项目（calc 库质量门）的完整参考实现：分层任务逐步讲解，每步标注知识点链接，含 Makefile/CMake/sanitizer/CI/git 全链路真实运行输出。"
chapter: 0
order: 5
tags:
  - host
  - toolchain
  - build
difficulty: intermediate
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 Project 题面"
related:
  - "阶段 0 各章"
---

# 阶段 0 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1 + clang 22.1.8）真实运行得到。建议只读你卡住的那一层；参考实现只是**一种**过关方式，你的实现和它不一样、验收标准对得上，就都是对的。

## 核心任务（L2）：库 + CLI + Makefile {#pj-core}

**思路**：库函数只管「算」，CLI 只管「解析参数 + 调用」，除零检查放在 CLI 层（这是工程上常见的分层——库保持简单，策略在调用侧）。Makefile 用变量 + 模式规则把编译压到两条；`test.sh` 是第一个「测试门」的雏形。

**`include/calc.h`**——头文件契约：include guard 防重复包含，只放声明不放实现。→ 知识点：[第 11 章：make 入门](/00-dev-environment/12-make-basics)（多文件项目的头文件）、[阶段 1 第 1 章](/01-c-basics/01-program-structure-and-compilation)（声明与定义的分离，预告）

```c
#ifndef CALC_H
#define CALC_H

int calc_add(int a, int b);
int calc_sub(int a, int b);
int calc_mul(int a, int b);
int calc_div(int a, int b);

#endif
```

**`src/calc.c`**——四个函数各一行，朴素到不能再朴素；`calc_div` 里埋着的 `INT_MIN / -1` 雷，我们故意留到第三层让 sanitizer 来抓。→ 知识点：[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)（UB 平时不发作，发作就换着花样）

```c
#include "calc.h"

int calc_add(int a, int b) {
    return a + b;
}

int calc_sub(int a, int b) {
    return a - b;
}

int calc_mul(int a, int b) {
    return a * b;
}

int calc_div(int a, int b) {
    return a / b;
}
```

**`src/main.c`**——CLI 解析 `argv`：`argc != 4` 先挡掉错误用法，`atoi` 把参数转整数，`strcmp` 分派到对应函数；除零在调用前拦下（真调了就是 UB 崩溃，不能赌）。`argv` 这套命令行参数在阶段 1 会细讲，这里先照猫画虎，重点是**分层**和**防御**。→ 知识点：[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)（多文件协作）、阶段 1 第 8 章（函数与 `main` 参数，预告）

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: main add|sub|mul|div <a> <b>\n");
        return 1;
    }
    int a = atoi(argv[2]);
    int b = atoi(argv[3]);
    if (strcmp(argv[1], "add") == 0) {
        printf("%d\n", calc_add(a, b));
    } else if (strcmp(argv[1], "sub") == 0) {
        printf("%d\n", calc_sub(a, b));
    } else if (strcmp(argv[1], "mul") == 0) {
        printf("%d\n", calc_mul(a, b));
    } else if (strcmp(argv[1], "div") == 0) {
        if (b == 0) {
            fprintf(stderr, "error: divide by zero\n");
            return 1;
        }
        printf("%d\n", calc_div(a, b));
    } else {
        fprintf(stderr, "unknown op: %s\n", argv[1]);
        return 1;
    }
    return 0;
}
```

**`Makefile`**——`CC`/`CFLAGS` 变量、模式规则、`test` 伪目标、`clean` + `.PHONY` 全齐。→ 知识点：[第 11 章：make 入门](/00-dev-environment/12-make-basics)「变量」「自动变量与模式规则」「`.PHONY` 与 clean」三节

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

main: src/main.o src/calc.o
	$(CC) $(CFLAGS) -o main src/main.o src/calc.o $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: main
	./test.sh

clean:
	rm -f main src/*.o

.PHONY: test clean
```

**`test.sh`**——回归测试：每个用例断言输出，错一个就 `fail=1`，最后 `exit $fail`——这就是「测试门」的最小形态，后面 CI 靠它做硬门。→ 知识点：[第 16 章：GitHub Actions](/00-dev-environment/17-github-actions)（CI 靠退出码当门，这里先埋下伏笔）

```bash
#!/usr/bin/env bash
fail=0
check() {
    expected="$1"; shift
    out="$(./main "$@")" || { echo "FAIL: main $* 退出码非 0"; fail=1; return; }
    if [ "$out" != "$expected" ]; then
        echo "FAIL: main $* -> got '$out', want '$expected'"
        fail=1
    else
        echo "ok:   main $* -> $out"
    fi
}
check 5   add 2 3
check 7   sub 10 3
check 12  mul 3 4
check 4   div 12 3
check 0   div 1 5
./main div 1 0 2>/dev/null; [ $? -eq 1 ] && echo "ok:   div by zero -> exit 1" || { echo "FAIL: div by zero"; fail=1; }
exit $fail
```

**验证输出**：

```text
$ make && ./main add 2 3
gcc -std=c11 -Wall -Wextra -Iinclude -c src/main.c -o src/main.o
gcc -std=c11 -Wall -Wextra -Iinclude -c src/calc.c -o src/calc.o
gcc -std=c11 -Wall -Wextra -Iinclude -o main src/main.o src/calc.o
5
$ make test
./test.sh
ok:   main add 2 3 -> 5
ok:   main sub 10 3 -> 7
ok:   main mul 3 4 -> 12
ok:   main div 12 3 -> 4
ok:   main div 1 5 -> 0
ok:   div by zero -> exit 1
```

## 进阶任务（L3）：CMake 迁移 {#pj-cmake}

**思路**：同一份源码，CMakeLists 只描述「要什么」。`CMAKE_C_EXTENSIONS OFF` 是第 12 章的功课——不关的话拿到的是 `-std=gnu11`。`target_include_directories` 声明头文件搜索路径，CMake 自动处理依赖顺序。

**`CMakeLists.txt`**：

```cmake
cmake_minimum_required(VERSION 3.10)
project(calc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(main src/main.c src/calc.c)
target_include_directories(main PRIVATE include)
```

**验证输出**：

```text
$ cmake -B build
$ cmake --build build
[ 66%] Building C object CMakeFiles/main.dir/src/calc.c.o
[100%] Linking C executable main
[100%] Built target main
$ ./build/main mul 6 7
42
$ grep '^C_FLAGS' build/CMakeFiles/main.dir/flags.make
C_FLAGS = -std=c11        ← 严格 C11,不是 gnu11(EXTENSIONS OFF 生效)
```

知识点：[第 12 章：CMake 入门](/00-dev-environment/13-cmake-basics)「最小 CMakeLists.txt」「Debug / Release」两节（`CMAKE_C_EXTENSIONS` 默认 ON 的坑）。

## 再进阶任务（L4）：sanitizer 门与修复 {#pj-sanitize}

**思路**：`INT_MIN / -1` 的结果 $2147483648$ 超出 `int` 表示范围，是 ISO C §6.5 第 5 段的 UB。普通构建下 x86 的 `idiv` 指令直接陷阱 → SIGFPE；sanitizer 构建下 UBSan 先把这条 UB 的来龙去脉讲清楚，ASan 再补上崩溃现场。

**实验一：普通构建**——直接 `Floating point exception`（退出码 136 = 128+8），一个字的源码定位都没有。→ 知识点：[第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)（UB 表现随环境漂移）、[第 13 章](/00-dev-environment/14-gdb-basics)（退出码 128+N 的含义）

```text
$ ./main div -2147483648 -1; echo "exit=$?"
exit=136                      ← SIGFPE,只有一句浮点异常
```

**实验二：sanitizer 构建**——UBSan 精确到 `src/calc.c:16:14`，ASan 给出调用链（`calc_div` 被 `main.c:25` 调用）。→ 知识点：[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)「UBSan」「ASan」两节

```text
$ make CFLAGS="-std=c11 -Wall -Wextra -Iinclude -O1 -g -fsanitize=address,undefined" \
      LDFLAGS="-fsanitize=address,undefined"
$ ./main div -2147483648 -1
src/calc.c:16:14: runtime error: division of -2147483648 by -1 cannot be represented in type 'int'
==355==ERROR: AddressSanitizer: FPE ...
    #0 0x... in calc_div src/calc.c:16
    #1 0x... in main src/main.c:25
```

**修复**——在库里显式处理这条边界（`#include <limits.h>` 拿 `INT_MIN`），返回 0 并保持函数不崩；sanitizer 构建重跑全绿。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)（修复后非 0 退出码的门就过了）

```c
#include <limits.h>

#include "calc.h"

int calc_add(int a, int b) {
    return a + b;
}

int calc_sub(int a, int b) {
    return a - b;
}

int calc_mul(int a, int b) {
    return a * b;
}

int calc_div(int a, int b) {
    if (a == INT_MIN && b == -1) {
        return 0;
    }
    return a / b;
}
```

```text
$ make CFLAGS="... -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
$ ./main div -2147483648 -1; echo "exit=$?"
0
exit=0
$ ./main add 2 3
5
```

**格式门**——先把 `if (argc != 4) {` 故意改乱成 `if(argc!=4){`，`--dry-run --Werror` 立刻红（退出码 1）；`-i` 修回后再查，退出码 0。→ 知识点：[第 17 章：格式化与质量门](/00-dev-environment/18-format-and-quality-gate)「clang-format 怎么用」一节

```text
$ clang-format --dry-run --Werror src/main.c
src/main.c:8:7: error: code should be clang-formatted [-Wclang-format-violations]
    if(argc!=4){
      ^
$ echo $?
1
$ clang-format -i src/main.c
$ clang-format --dry-run --Werror src/*.c include/*.h && echo "修复后格式合规,exit=0"
修复后格式合规,exit=0
```

## 终极挑战（L5）：CI workflow + git 全程 {#pj-ci}

**思路**：三层防线（本地旗标 → sanitizer → CI）的最后一块拼图——把门挂到每次 push/PR 上。`build` job 用 gcc/clang 矩阵把「换个编译器就炸」挡在门外；`sanitize` job 复刻第三层的实验；`format-check` 复刻格式门。

**`.github/workflows/ci.yml`**（骨架，注释里的要点就是第 16 章的内容）：

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    name: 编译 (${{ matrix.cc }})
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        cc: [gcc, clang]
    steps:
      - uses: actions/checkout@v4
      - name: 选择编译器
        run: echo "CC=${{ matrix.cc }}" >> $GITHUB_ENV
      - name: 编译并跑测试
        run: make clean && make && make test

  sanitize:
    name: Sanitizer(ASan + UBSan)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: sanitizer 构建并跑测试
        env:
          CC: clang
          CFLAGS: -std=c11 -Wall -Wextra -Iinclude -O1 -g -fsanitize=address,undefined
          LDFLAGS: -fsanitize=address,undefined
        run: make clean && make && make test

  format-check:
    name: clang-format 检查
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装 clang-format
        run: sudo apt-get update && sudo apt-get install -y clang-format
      - name: 格式门
        run: clang-format --dry-run --Werror src/*.c include/*.h
```

**`.gitignore`**——可执行产物、CMake 构建目录、目标文件全挡在版本库外。→ 知识点：[第 15 章：Git 工作流](/00-dev-environment/16-git-workflow)「远程、协作」一节

```gitignore
main
build/
src/*.o
```

**git 全程与本地复现**——`git init -b main`（默认分支名取决于你的 git 配置，本机默认是 `master`，所以要显式 `-b`）；提交信息按约定式提交；然后本地复现 format-check 与 sanitize 两个 job 的核心命令，退出码 0 就是 GitHub 上那个绿勾。→ 知识点：[第 15 章](/00-dev-environment/16-git-workflow)（约定式提交）、[第 16 章](/00-dev-environment/17-github-actions)「本地把这些 job 跑一遍」一节

```text
$ git init -b main
$ git add . && git commit -m "feat: calc 库与 CLI,带 Makefile 和回归测试"
$ git log --oneline
480be08 feat: calc 库与 CLI,带 Makefile 和回归测试

$ clang-format --dry-run --Werror src/*.c include/*.h && echo "format-check: exit 0(绿)"
format-check: exit 0(绿)

$ CC=clang make CFLAGS="-std=c11 -Wall -Wextra -Iinclude -O1 -g -fsanitize=address,undefined" \
      LDFLAGS="-fsanitize=address,undefined"
$ make test
ok:   main add 2 3 -> 5
ok:   main sub 10 3 -> 7
ok:   main mul 3 4 -> 12
ok:   main div 12 3 -> 4
ok:   main div 1 5 -> 0
ok:   div by zero -> exit 1
```

到这里，「能编」和「可信」之间的门就配齐了：旗标纪律（`-std` 显式钉死、`-Wall -Wextra`）、增量构建（make/CMake）、运行期兜底（ASan/UBSan）、格式真相源（clang-format）、自动化（CI 三个 job）与版本管理（git）。这正是阶段 1 往后的每一行代码都要走的路。
