---
title: "CMake 入门：在 make 之上声明「要构建什么」"
description: "上一章手写的 Makefile 在小项目里够用，但工程一大、一跨平台、一要接第三方库，手维护依赖和平台分支就开始吃力。这一章在 make 之上再封一层 CMake：用一份声明式的 CMakeLists.txt 描述「我要构建什么」，由 CMake 替你生成对应平台的构建文件。拿上一章的 greet 项目真跑：cmake -B build 配置（生成 Makefile）、cmake --build build 编译、out-of-source 构建（源码树保持干净）；看 CMAKE_BUILD_TYPE=Debug/Release 分别给出 -g 和 -O3 -DNDEBUG（呼应第 9 章的 -O/-g）；以及 -G Ninja 换一个底层生成器（同一份 CMakeLists 生成 build.ninja 而非 Makefile）。还点出一个呼应第 9 章的细节：CMake 默认给你 -std=gnu11 不是 c11。"
chapter: 0
order: 12
tags:
  - host
  - cmake
  - build
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [11]
prerequisites:
  - "第 11 章：make 入门（CMake 生成的就是 Makefile）"
  - "第 9 章：标准与优化（-g / -O3 / -std=gnu11 的含义）"
related:
  - "第 10 章：Sanitizer 门禁（CMake 里开 sanitizer 的旗标）"
  - "第 17 章：格式化与质量门（在 CMake 里挂 clang-format、sanitizer、测试）"
---

> 🟡 状态：待审核（2026-06-30）

# CMake 入门：在 make 之上声明「要构建什么」

## 引言：make 够用，但工程一大就吃力

上一章我们手写了一个 Makefile，在三文件的小项目里它干净利落。但你把项目规模放大一点，手写 Makefile 就开始疼：头文件依赖得靠 `-MMD` 自动生成才不漏、跨 Linux/Windows/macOS 要写一堆 `ifdef` 分支、要链接第三方库（OpenSSL、zlib 之类）得自己 `pkg-config` 找路径——这些活每项都不难，但堆在一起维护成本很高，而且换个平台就得重写一遍。

**CMake** 的定位是「构建系统的生成器」：你不直接写「怎么编译」，而是写一份声明式的 `CMakeLists.txt`，描述「我要构建哪些目标、它们依赖什么、要链接哪些库」；然后 CMake 根据你当前的平台和选择的「生成器」，**替你生成**对应平台的构建文件——在 Linux 上默认生成 Makefile，也可以生成 Ninja 文件，在 Windows 上还能生成 Visual Studio 工程。换句话说，CMake 不取代 make，它在 make 之上再封一层：你维护一份 CMakeLists，CMake 替不同平台产出各自的 Makefile/Ninja/VS 工程。这一章我们拿上一章的 greet 项目，真跑一遍 CMake 的最小流程。

## 最小 CMakeLists.txt

把上一章的 `greet.h`、`greet.c`、`main.c` 放进一个新目录，再写一个 `CMakeLists.txt`（CMake 的约定文件名，大小写敏感）：

```cmake
cmake_minimum_required(VERSION 3.10)
project(greet C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(main main.c greet.c)
```

逐行读：`cmake_minimum_required(VERSION 3.10)` 声明这个工程需要的最低 CMake 版本（CMake 4.x 起对很老的版本号不再兼容，所以别写 2.x）；`project(greet C)` 给工程起名，并声明语言是 C；`set(CMAKE_C_STANDARD 11)` 设 C 标准为 C11（对应第 9 章的 `-std=c11` 那条纪律，只是换个地方钉），`CMAKE_C_STANDARD_REQUIRED ON` 保证「不够这个标准的编译器直接报错」而不是悄悄降级；最后 `add_executable(main main.c greet.c)` 声明「我要一个叫 `main` 的可执行文件，源文件是这两个」——注意这里**不再手写编译命令和依赖**，你只说「目标 + 源文件」，剩下的 CMake 全替你算。和上一章那个手写每条 `gcc -c` 的 Makefile 比，这份 CMakeLists 只描述「要什么」，不描述「怎么做」——这就是「声明式」。

## 两步走：先配置，再构建

CMake 的使用固定两步。第一步「配置」，让 CMake 读 CMakeLists、检测编译器、生成构建文件：

```text
$ cmake -B build
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features - done
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cj/ch12/build
```

`-B build` 是把所有生成物放进 `build/` 子目录（这叫 **out-of-source 构建**），不污染源码树。配置阶段的输出是 CMake 在「探」环境：它找到一个能用的 C 编译器（`/usr/sbin/cc`）、探明它的特性，然后把构建文件写进 `build/`。第二步「构建」，真正开始编译：

```text
$ cmake --build build
[ 33%] Building C object CMakeFiles/main.dir/main.c.o
[ 66%] Building C object CMakeFiles/main.dir/greet.c.o
[100%] Linking C executable main
[100%] Built target main
$ ./build/main
hello, make!
```

`cmake --build build` 是跨生成器的统一构建命令（它内部会去调 make 或 ninja）。那个 `[33%] [66%] [100%]` 是 CMake 给的进度百分比，比裸 make 的输出友好。跑出来的 `./build/main` 输出和上一章一样是 `hello, make!`——因为底层编译的还是同一个 gcc，只是中间多了一层 CMake 替你管。

我们看一眼 `build/` 里到底生成了什么：

```text
$ ls build/
CMakeCache.txt  CMakeFiles/  Makefile  cmake_install.cmake  main
```

**里面有一个 `Makefile`**——这就是关键：CMake 在 Linux 上默认生成的就是一份 Makefile（而且是一份相当复杂、带全套依赖追踪的 Makefile）。所以「CMake 和 make 的关系」一句话说清：**你写 CMakeLists，CMake 生成 Makefile，make（或 cmake --build）去编译**。CMake 没有取代 make，它取代的是「你手写 Makefile」这件事。那个 `CMakeCache.txt` 是 CMake 缓存的配置（编译器路径、选项等），下次再配置时直接复用、不必重新探测；这也意味着你改了 CMakeLists 后要重新跑 `cmake -B build` 让它更新缓存，只跑 `cmake --build` 是不够的——这是个新手常踩的点。

## Debug / Release：CMAKE_BUILD_TYPE

CMake 内置了几种「构建类型」，最常用的是 Debug 和 Release。区别直接体现在编译旗标上——和我们第 9 章手拧的 `-g`、`-O` 是一回事，只是 CMake 按构建类型替你选好了：

```text
$ cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug
$ cmake -B build-rel -DCMAKE_BUILD_TYPE=Release
```

`-D` 是在命令行给 CMake 变量赋值。配置完后，CMake 给两种类型分别生成的旗标（存在它生成的 `flags.make` 里）是这样的：

```text
Debug:   C_FLAGS = -g           -std=gnu11
Release: C_FLAGS = -O3 -DNDEBUG -std=gnu11
```

Debug 给 `-g`（带调试信息，对应第 9 章「调试用 -O0 -g」），Release 给 `-O3 -DNDEBUG`（`-O3` 是激进优化、`-DNDEBUG` 关掉 `assert` 宏，对应「发布构建」）。所以开发期用 Debug 调试、要发布或跑性能基准时切 Release，靠的就是这一个 `CMAKE_BUILD_TYPE`，不必自己去 if/else 旗标——这正是 CMake 比手写 Makefile 省心的地方。

这里有个直接呼应第 9 章的细节要划重点：两种类型里都带了 `-std=gnu11`，**不是 `-std=c11`**。因为 CMake 的 `CMAKE_C_EXTENSIONS` 这个开关默认是 `ON`，它给的是带 GNU 扩展的 `gnu11`。如果你像本课程第 1 章那样要求「严格 c11、不要 GNU 扩展」，得显式 `set(CMAKE_C_EXTENSIONS OFF)`，CMake 才会给你 `-std=c11`。这种「CMake 默认纵容扩展」的行为，和第 9 章讲的「gcc 默认 `-std=gnuXX`」是一脉相承的——默认姿态都是「能让你过就让你过」，要严格得自己动手。

## 生成器：同一份 CMakeLists，换一个底层

CMake 最有特色的一点是「生成器」可换。刚才默认生成的是 Makefile（Unix Makefiles 生成器）；我们换一个，让它生成 Ninja 的构建文件：

```text
$ cmake -B build-ninja -G Ninja
$ ls build-ninja/
build.ninja  CMakeCache.txt  CMakeFiles/  ...
```

`-G Ninja` 选 Ninja 生成器，产物是 `build.ninja`（**没有** Makefile 了）。同一份 CMakeLists.txt，一个字不改，换个 `-G` 就把底层构建工具从 make 换成了 ninja。Ninja 专为速度设计，大项目里增量构建比 make 快得多，所以现在很多工程（包括 LLVM）默认用 Ninja。构建命令还是那条跨生成器的 `cmake --build build-ninja`：

```text
$ cmake --build build-ninja
[1/3] Building C object CMakeFiles/main.dir/main.c.o
[2/3] Building C object CMakeFiles/main.dir/greet.c.o
[3/3] Linking C executable main
```

注意进度格式从 `[33%]` 变成了 `[1/3]`——因为这回是 ninja 在跑，输出风格也跟着底层工具走了。这种「一份 CMakeLists、多个生成器、多个平台」的能力，是 CMake 成为主流的核心原因：你只维护一份声明式的构建描述，跨平台和换构建工具的脏活都交给 CMake。

## 往大一点想：target、依赖、找库

我们这个例子太小，看不出 CMake 比 make 强在哪。但只要你把项目想象得再大一点，CMake 的价值就浮现了：多个目标用 `add_executable` / `add_library` 各建一个，目标之间的依赖用 `target_link_libraries(main greet)` 一行声明（CMake 自动算传递顺序，不像第 6 章手动链接时要操心库的排列顺序）；头文件搜索路径用 `target_include_directories(main PRIVATE include/)`；接第三方库用 `find_package(OpenSSL REQUIRED)` 然后链接——这些在 make 里全是手写、还容易跨平台出错的事，CMake 用几条声明就涵盖了。这些属于「CMake 进阶」，我们在用到时（尤其第 17 章把 clang-format、sanitizer、测试挂进 CMake 时）再展开，这里你只要建立一个印象：CMake 的声明式写法能撑住工程化规模，而手写 Makefile 撑不住。

## 小结

CMake 是「构建系统的生成器」：你写一份声明式的 `CMakeLists.txt` 描述「要构建什么目标、依赖什么」，CMake 替你生成对应平台和生成器的构建文件——在 Linux 上默认是一份 Makefile，也能 `-G Ninja` 换成 `build.ninja`，所以 CMake 不是取代 make，而是取代「你手写 Makefile」，关系是「CMakeLists → CMake → Makefile/Ninja → make/ninja 编译」。用法固定两步：`cmake -B build` 配置（探测环境、生成构建文件、写 CMakeCache.txt 缓存），`cmake --build build` 构建（跨生成器的统一命令），改了 CMakeLists 要重跑配置、光 build 不够；用 `-B` 做的是 out-of-source 构建，产物全在 build 子目录、源码树保持干净。构建类型 `CMAKE_BUILD_TYPE` 按 Debug/Release 自动选旗标（我们真跑看到 Debug 给 `-g`、Release 给 `-O3 -DNDEBUG`，对应第 9 章手拧的 `-g`/`-O`）；但要留心 CMake 默认的 `CMAKE_C_EXTENSIONS=ON` 给的是 `-std=gnu11` 不是 `-std=c11`，要严格 c11 得显式 OFF——这和 gcc 默认纵容 GNU 扩展是一回事。下一章我们从构建转到调试，拿起 GDB，看怎么在程序跑崩的地方停下来、一步一步看现场。

## 参考资源

- CMake 官方教程与 `cmake --help-command`（`cmake_minimum_required`/`project`/`set`/`add_executable`/`target_*`/`find_package`）
- CMake 手册：`CMAKE_BUILD_TYPE`（Debug/Release/RelWithDebInfo/MinSizeRel）、`CMAKE_C_STANDARD`/`CMAKE_C_EXTENSIONS`、生成器（`-G`，Unix Makefiles / Ninja）、`cmake --build` 跨生成器构建
- 第 9 章：标准与优化（`-g`/`-O3`/`-std=c11 vs gnu11` 的含义，CMake 的旗标就是它们的封装）
- 第 11 章：make 入门（CMake 默认生成的 Makefile 长什么样、`cmake --build` 内部在调 make）
- 第 17 章：格式化与质量门（CMake 里挂 clang-format、sanitizer、CTest 的实战）
