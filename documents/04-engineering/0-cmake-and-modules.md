---
title: "CMake 与模块化：让多文件工程不再靠手搓 gcc"
description: "从手写一长串 gcc 命令的痛，到用 CMake 组织一个多模块 C 库；拆解 GLOB_RECURSE、target_include_directories、build 目录隔离，并照着真实库看清工程组织。"
chapter: 4
order: 0
tags:
  - host
  - build
  - cmake
  - engineering
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程、GDB 与库"
  - "Chapter 2：指针与内存"
related:
  - "Chapter 0：编译流程、GDB 与库"
  - "用 C 造泛型容器（阶段 3）"
---

# CMake 与模块化：让多文件工程不再靠手搓 gcc

## 引言

前几章我们的程序都很小，两三个 `.c` 文件，编译的时候敲一行 `gcc a.c b.c c.c -o demo` 就完事了。可一旦工程长到几十个文件、分了好几个模块、头文件散落在不同目录，这行命令就会膨胀成又臭又长的一串，而且每加一个文件你就得改一遍——这种体力活干两次你就想骂人。所以真实工程没有人在命令行手搓 gcc，大家都用**构建系统**，C 语言世界里最流行的就是 CMake。这一章我们就拿 [clib-utilities](../../projects/clib-utilities/) 这个真实的多模块库当样本，看 CMake 是怎么把一坨散乱的 `.c`/`.h` 组织成"一条命令全编译"的。

## 核心概念：CMake 不是编译器

第一个要纠正的误解——**CMake 本身不编译代码**，它只是个"构建系统生成器"。你写一份 `CMakeLists.txt` 描述"我要编译哪些文件、头文件在哪、产物叫什么"，然后跑 `cmake`，它根据这份描述生成出一份 `Makefile`（或者 Ninja、VS 工程文件），最后还是交给 `make` 去真正编译。所以标准流程永远是两步：

```bash
mkdir build && cd build    # 1. 进一个独立的 build 目录
cmake ..                   # 2. 配置：读 CMakeLists.txt，生成 Makefile
make                       # 3. 构建：真正调 gcc 编译
```

我们对着 [clib-utilities](../../projects/clib-utilities/CMakeLists.txt) 真跑一次配置阶段，输出是这样的：

```text
$ cd build && cmake ..
-- Configuring done (0.5s)
-- Generating done (0.0s)
-- Build files have been written to: .../build
```

看到没，配置阶段它没碰编译器，只是把 Makefile 写好了。这一步分离出来有个大好处——**生成的所有中间产物都待在 `build/` 目录里**，源码目录永远干干净净，想重来直接 `rm -rf build`，一了百了。这就是所谓的 out-of-source 构建，比早期"在源码目录里直接 make、搞得满地 `.o`"文明太多了。

## 多模块工程怎么组织

clib-utilities 不是把所有 `.c` 塞一个目录，而是按职责拆成了三个模块，每个模块内部又是"头文件和源码分家"：

```text
clib-utilities/
├── Basic_Utils/         # 基础工具：Pair、比较、错误处理、内存封装…
│   ├── Includes/        #   对外公开的 .h
│   └── Sources/         #   内部实现的 .c
├── BasicDataStructure/  # 数据结构：String、DynamicArray、LinkList、Range
│   ├── Includes/
│   └── Sources/
├── SystemRelated/       # 系统相关：动态加载、互斥锁、线程、错误码
│   ├── Includes/
│   └── Sources/
├── test/                # 各模块的测试
├── main.c               # 测试入口
└── CMakeLists.txt       # 把上面这些粘起来的总指挥
```

**为什么要这么分？** 因为工程化的核心是"接口与实现分离"。`Includes/` 里放的是给别人用的头文件（公开 API），`Sources/` 里放的是实现细节。别的模块要用你，只 `#include` 你的头文件就行，不需要也不应该关心你的 `.c` 里写了什么。这种分离让模块边界清晰，依赖关系不会乱成一锅粥。

## CMakeLists 的三个关键招式

现在打开 [CMakeLists.txt](../../projects/clib-utilities/CMakeLists.txt) 看它怎么把这三个模块粘起来。最关键的就三招。

第一招，**用 `file(GLOB_RECURSE …)` 自动收集源文件**，不用一个个手写文件名：

```cmake
file(GLOB_RECURSE CCSTD_BasicUtils_SRC_FILE ${CCSTD_BasicUtils_SRC_DIR}/*.c)
file(GLOB_RECURSE CCSTD_BasicDataStructure_SRC_FILE ${CCSTD_BasicDataStructure_SRC_DIR}/*.c)
file(GLOB_RECURSE CCSTD_SystemRelated_SRC_FILE ${CCSTD_SystemRelated_SRC_DIR}/*.c)
```

`GLOB_RECURSE` 会递归地把指定目录下所有 `.c` 找出来。这样你新加一个源文件，理论上不用改 CMakeLists。注意是"理论上"——下面踩坑环节会讲它的坑。

第二招，**把收集到的源文件凑成一个可执行文件**：

```cmake
add_executable(TEST_RES ${CCSTD_COMPILE_ALL_SRC} main.c)
```

第三招，**告诉编译器头文件去哪找**，这样源码里 `#include "CCPair.h"` 才能找到：

```cmake
target_include_directories(TEST_RES PUBLIC
    ${CCSTD_Test_DIR}
    ${CCSTD_BasicUtils_INC_DIR}
    ${CCSTD_SystemRelated_INC_DIR}
    ${CCSTD_BasicDataStructure_INC_DIR})
```

这三招一出，一个"源文件自动收集、头文件路径自动设置、产物自动生成"的工程就搭起来了。以后加文件、加模块，改动量极小。

## 常见坑（真正的坑在后面）

> **坑 1：`GLOB_RECURSE` 不会自动感知新文件。** 这是 CMake 最经典的坑——你新建了一个 `foo.c`，再敲 `make`，它会告诉你"没变化"，因为 CMake 是在 `cmake` 配置阶段 glob 的，生成 Makefile 之后就不再看了。新文件得重新跑一次 `cmake ..` 才会被纳入。正式项目里官方其实不推荐用 GLOB 收集源文件，建议显式列出，就是为了避免这个陷阱。

> **坑 2：`build/` 目录忘了加 `.gitignore`。** build 目录里全是生成的中间产物（`.o`、`Makefile`、`CMakeCache.txt`），绝不能提交进仓库。新建 CMake 工程第一件事就是把 `build/` 写进 `.gitignore`。

> **坑 3：`target_include_directories` 别漏。** 很多人头文件找不到、报 `No such file or directory`，就是因为没告诉 CMake 头文件在哪个目录。`-I` 路径得在这里设，而不是指望源码里写死绝对路径。

## 读库练手：把它修到能编译

说实话，得给你交个底——这个库是我早期的代码，在现在比较新的 GCC 14 严格模式下**当前编译不过**，但这反而成了个绝佳的练手题。`make` 的时候会卡在两个地方，我故意不直接修，留给你：

第一处，[CCSTDLibs_MyCompiles.h](../../projects/clib-utilities/Basic_Utils/Includes/CCSTDLibs_MyCompiles.h) 第 27 行写的是 `#ifndef (__linux__)`，编译器直接报 `macro names must be identifiers`。原因是 `#ifndef` 后面期望的是一个裸标识符，不能给它套个括号。把括号去掉就行——这是典型的"手滑多打了括号"。

第二处，[CCPair.c](../../projects/clib-utilities/Basic_Utils/Sources/CCPair.c) 里 `DEFAULT_DENY(p, NUL_PTR)` 把一个 `void*` 指针塞给了 `exit(int)`，GCC 14 把这种"整数从指针来"的隐式转换从 warning 升成了 error。要么改宏的实现，要么在 CMake 里给这个 target 加 `-Wno-int-conversion`。

把这两处搞定，`make` 就能跑通，`./TEST_RES` 就会执行 `main.c` 里启用的那个测试。这种"拿到一份旧代码、读懂它、修到能跑"的能力，恰恰是工程化阶段最该练的——比从零写一个新库实在多了。

## 小结

- [ ] CMake 不编译代码，它生成 Makefile；标准流程是 `cmake ..` 配置 + `make` 构建
- [ ] out-of-source 构建：产物全进 `build/`，源码目录保持干净
- [ ] 工程化三招：`GLOB_RECURSE` 收集源文件、`add_executable` 凑产物、`target_include_directories` 设头文件路径
- [ ] 接口与实现分离：`Includes/`（公开）与 `Sources/`（实现）分家
- [ ] `GLOB_RECURSE` 不自动感知新文件，新增源文件要重跑 `cmake`

## 练习

- [ ] 把上面两个编译障碍修掉，让 `./TEST_RES` 跑起来
- [ ] 给这个工程加一个新的 `Logging/` 模块（`Includes/` + `Sources/`），照着现有三招把它接进 CMakeLists
- [ ] 试着把 `GLOB_RECURSE` 换成显式列出源文件，体会官方推荐做法的好处

## 参考资源

- [CMake 官方教程](https://cmake.org/cmake/help/latest/guide/tutorial/)——从最小工程一步步往上加
- Eric Nizer 的《Professional CMake: A Practical Guide》——CMake 进阶必读
- 本仓库的 [clib-utilities/CMakeLists.txt](../../projects/clib-utilities/CMakeLists.txt)——一份真实的多模块 CMake 工程样本

---
*配套示例整理自 2023–2024 学习存档。*
