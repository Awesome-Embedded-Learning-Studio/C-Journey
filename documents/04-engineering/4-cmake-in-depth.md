---
title: "CMake 进阶：target_*、PRIVATE/PUBLIC/INTERFACE、find_package 与 install/export"
description: "从 add_library 造一个静态库开始，拆透 target_* 三件套与 PRIVATE/PUBLIC/INTERFACE 的传播语义，用真实 find_package 与 cmake --install 把库 export 成可被 find_package 消费的产物。"
chapter: 4
order: 4
tags:
  - host
  - build
  - cmake
  - engineering
  - linker
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 4：CMake 与模块化（0-cmake-and-modules 基础）"
related:
  - "CMake 与模块化：让多文件工程不再靠手搓 gcc"
  - "Chapter 4：符号与链接"
---

# CMake 进阶：target_*、PRIVATE/PUBLIC/INTERFACE、find_package 与 install/export

## 引言

上一篇我们让多文件工程跑起来了：`GLOB_RECURSE` 收源文件、`add_executable` 凑产物、`target_include_directories` 设头文件路径。但工程一长大，几个真问题就砸到你脸上：

- 我把一堆 `.c` 抽成了**库**给别人用，可他们凭什么能 `#include` 到我的头文件？凭什么能链接到我的 `.a`？
- 我的库内部偷偷依赖了另一个库，调用我的人**根本不该知道**这个内部细节，怎么做到？
- 别人写好的库（OpenCV、OpenSSL、我自己装的 greeter），`find_package` 一行就找来了，它到底是怎么找到的？
- 我 `make` 编出来了 `.a`，凭什么叫"安装"？`cmake --install` 又干了什么神奇的事，让另一台机器上的 `find_package` 能消费我？

这篇就把这四件事一次拆透。**所有 cmake 输出都是本机实测捕获的**，CMake 4.3、GCC 16、Linux。我们先动手造一个最小的真实静态库 `greeter`，把它 export 出去，再用一个独立的 `app` 工程 `find_package` 把它吃回来——全链路打通，没有任何省略。

## 核心心法：一切围着 target 转

上一篇你可能注意到了一个细节：我用了 `target_include_directories(TEST_RES PUBLIC ...)`，而不是全局的 `include_directories(...)`。这不是风格偏好，这是 CMake 现代化的一条分水岭。

老的写法（`include_directories`、`add_definitions`、`link_directories`）是**全局的**——你设一次，所有 target 都吃，包括将来后加的、八竿子打不着的 target。工程小的时候无所谓，工程一大就乱成一锅粥：你根本不知道某个 `-I` 路径是给谁用的。

现代 CMake 的核心心法是 **target-centric（以目标为中心）**。所有属性都挂在具体的 target 上：`target_include_directories`、`target_compile_definitions`、`target_compile_options`、`target_link_libraries`。你给 `greeter` 这个 target 设的头文件路径，只对 `greeter` 和"明确链接了 greeter 的人"生效，别人一律不管。这才有清晰的边界、才有可复用、才有 `install/export`。

而 target 属性之所以能这么挂，靠的是下面这个三元组。

## PRIVATE / PUBLIC / INTERFACE：到底谁传染给谁

这是整篇最关键、也最容易讲糊的概念。CMake 给 target 挂属性时，每个属性都要标三个关键字之一。先上人话定义：

| 关键字 | 这个属性给**我自己**用吗？ | 这个属性会**传染给链接我的人**吗？ |
|--------|--------------------------|------------------------------------|
| `PRIVATE` | 是 | 否 |
| `INTERFACE` | 否 | 是 |
| `PUBLIC` | 是 | 是 |

记住这张表就够了。`PUBLIC` 就是 `PRIVATE` + `INTERFACE` 的并集：我自己用，也传染给上游。

拿一个具体场景翻译：库 `A` 链接了库 `B`。

- **`A` 把 `B` 标成 PRIVATE**：`A` 的实现里调了 `B`，但 `A` 的公开头文件里**没有**任何来自 `B` 的类型或函数。调用 `A` 的人完全感知不到 `B` 的存在。典型例子：一个网络库 `A` 内部用某个 SSL 库 `B` 做加密，但对外只暴露 `send()/recv()`，调用者根本不知道底下用的什么 SSL。
- **`A` 把 `B` 标成 INTERFACE**：`A` 自己的 `.c` 里**根本没用到** `B`，但 `A` 的公开头文件里**出现了** `B` 的类型。所以谁要 `#include` `A` 的头，就得同时拿到 `B` 的头和库——典型是 header-only 的薄封装库，自己没实现，纯粹转发调用。
- **`A` 把 `B` 标成 PUBLIC**：`A` 实现里用了 `B`，**而且** `A` 的公开 API 里也露出了 `B` 的类型。调用者既要链接 `B`（因为运行时需要），又要能找到 `B` 的头（因为编译时要解析类型）。

这套语义真正要解决的问题是 **usage requirement（使用要求）的传播**：你链接了一个库，CMake 自动把"正确使用这个库所需的一切"——头文件路径、编译宏、链接依赖——给你配齐，不用你手写一堆 `-I`、`-D`、`-l`。而到底传不传、传给谁，就是这三个关键字控制的。

有一个静态库特有的细节值得知道：**静态库（`.a`）本身不记录它依赖了谁**（不像动态库能用 `ldd` 查出依赖链）。所以当 `A` 是静态库、`B` 标成 PRIVATE 时，CMake 在最终把 `A` 链进可执行文件时，**仍然会把 `B` 一并加进链接命令**——因为 `A` 的 `.o` 里引用了 `B` 的符号，不带上 `B` 链接就过不去。也就是说，PRIVATE 对静态库只在"编译/头文件层面"是私有的，"链接层面"CMake 会替你兜底。这正是你不用担心、但要心里有数的事。

## 造一个库：add_library 三种类型

回到动手。上一篇全是 `add_executable`，现在该造库了。`add_library` 的骨架是：

```cmake
add_library(<name> [STATIC | SHARED | MODULE] [source...])
```

类型三选一，对应三种产物：

- **`STATIC`**：静态库，产物是 `lib<name>.a`（Linux）。链接进可执行文件，运行时不依赖。
- **`SHARED`**：动态/共享库，产物是 `lib<name>.so`（Linux）。运行时动态加载，能被多个进程共享。
- **`MODULE`**：插件式，运行时由 `dlopen` 之类显式加载，**不**参与链接。Windows 上 dll 若不导出符号就得用 MODULE。

不写类型时，CMake 由全局变量 `BUILD_SHARED_LIBS` 决定（默认 OFF，即静态）。

## 实战：greeter 静态库 + 独立 app 全链路

我们把上面所有概念串成一个能跑的真实工程。结构如下：

```text
/tmp/imp_cmake/
├── greeter/                  # 我们的库工程
│   ├── include/greeter.h     #   公开头文件（PUBLIC）
│   ├── src/prefix.h          #   内部头文件（PRIVATE）
│   ├── src/prefix.c
│   ├── src/greeter.c
│   ├── greeterConfig.cmake.in#   Config 模板（后面讲）
│   └── CMakeLists.txt
└── app/                      # 消费者工程，完全独立
    ├── main.c
    └── CMakeLists.txt
```

设计意图很明确：`greeter` 的公开 API 只是 `greeter_greet()`，公开头是 `include/greeter.h`；内部实现细节 `prefix.h/greet_prefix()`藏在 `src/`，调用者根本不该看见。这就是 PUBLIC vs PRIVATE 的天然演示场景。

### 库的源码

`greeter/include/greeter.h`（PUBLIC API）：

```c
#ifndef GREETER_H
#define GREETER_H

#include <stddef.h>

/* PUBLIC API: callers of greeter see this function. */
int greeter_greet(const char *who, char *out, size_t out_len);

#endif /* GREETER_H */
```

`greeter/src/prefix.h`（PRIVATE 内部助手）：

```c
#ifndef PREFIX_H
#define PREFIX_H

/* Internal helper, NOT in the public API. */
const char *greet_prefix(void);

#endif /* PREFIX_H */
```

`greeter/src/prefix.c`：

```c
#include "prefix.h"

const char *greet_prefix(void)
{
    return "Hello";
}
```

`greeter/src/greeter.c`（实现里用到了私有助手）：

```c
#include "greeter.h"
#include "prefix.h"

#include <stdio.h>

int greeter_greet(const char *who, char *out, size_t out_len)
{
    int written = snprintf(out, out_len, "%s, %s!", greet_prefix(), who);
    return written;
}
```

### 库的 CMakeLists：target_* 三件套齐活

```cmake
cmake_minimum_required(VERSION 3.15)

project(greeter
        VERSION 1.0
        DESCRIPTION "Tiny C static lib demo for the CMake-in-depth tutorial"
        LANGUAGES C)

add_library(greeter STATIC
    src/greeter.c
    src/prefix.c)

# PUBLIC  : greeter.h lives here, and callers of greeter MUST see it.
# PRIVATE : prefix.h lives here too, but only greeter's own .c need it.
target_include_directories(greeter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# INTERFACE: a usage requirement that propagates UP to anyone linking greeter.
# Here it is a compile flag the consumer inherits without greeter using it itself.
target_compile_definitions(greeter INTERFACE GREETER_VIA_CMAKE=1)
```

这里每行都对应前面讲的语义，逐句对一遍：

1. `add_library(greeter STATIC ...)`——明确造一个**静态库**，产物会是 `libgreeter.a`。
2. `target_include_directories` 的 **PUBLIC** 分支挂了 `include/`：因为 `greeter.h` 在这里，任何链接 greeter 的人都得能 `#include "greeter.h"`，所以这个路径**我自己用、也传染给上游**。
3. 同一条命令的 **PRIVATE** 分支挂了 `src/`：`prefix.h` 在这里，但只有 greeter 自己的 `.c` 要 `#include "prefix.h"`，调用者根本碰不到——所以**只我自己用，不传染**。
4. `target_compile_definitions(greeter INTERFACE GREETER_VIA_CMAKE=1)`：纯粹为了演示 **INTERFACE**——这个宏 greeter 自己的代码里压根没用，但它会作为使用要求传染给任何链接 greeter 的 target。我们后面会用消费者侧的编译命令亲眼验证它确实传过去了。

那两个 `$<BUILD_INTERFACE:...>` / `$<INSTALL_INTERFACE:...>` 是**生成器表达式**。同一份 `include/` 目录，在"自己 build 时"用的是源码树的绝对路径，在"被安装后别人消费时"用的是相对于安装前缀的 `include`。这一个 target 同时服务 build 树和 install 树，靠的就是这个表达式切换。

### 配置 + 构建：真实输出

```text
$ cd greeter && cmake -B build -DCMAKE_INSTALL_PREFIX=/tmp/imp_cmake/prefix
-- The C compiler identification is GNU 16.1.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.9s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/imp_cmake/greeter/build
```

```text
$ cmake --build build
[ 33%] Building C object CMakeFiles/greeter.dir/src/greeter.c.o
[ 66%] Building C object CMakeFiles/greeter.dir/src/prefix.c.o
[100%] Linking C static library libgreeter.a
[100%] Built target greeter
```

`libgreeter.a` 出来了。注意编译的是 `Building C object`——`project(... LANGUAGES C)` 把这个工程锁死在 C，CMake 不会去探测 C++ 编译器，干净利落。

## install：把库"种"到前缀里

光 build 出 `.a` 不叫安装。安装是按 `install()` 规则，把产物 + 头文件 + 让 `find_package` 能找到的元信息，一起拷到一个**安装前缀**下，形成一棵规范的目录树。

```cmake
include(GNUInstallDirs)

install(TARGETS greeter
        EXPORT greeterTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

几个要点：

- **`GNUInstallDirs`** 提供了一组符合平台惯例的目录变量：`CMAKE_INSTALL_LIBDIR`（Linux 上是 `lib` 或 `lib64`）、`CMAKE_INSTALL_INCLUDEDIR`（`include`）、`CMAKE_INSTALL_BINDIR`（`bin`）。用它而不是硬编码 `lib`，库才能在不同发行版上落对位置。
- **`install(TARGETS ...)`** 的几条 `DESTINATION` 按产物类型分流：`LIBRARY` 给动态库、`ARCHIVE` 给静态库（`.a` 就是 archive）、`RUNTIME` 给可执行文件、`INCLUDES` 给头文件目录。本例是静态库，所以真正落盘的是 `ARCHIVE`。
- **`EXPORT greeterTargets`** 是 export 的关键：它让 CMake 把 `greeter` 这个 target 的全部属性（含 PUBLIC/INTERFACE 的传播信息）记录进一个 export 集，待会儿生成成 `greeterTargets.cmake`。
- `CMAKE_INSTALL_PREFIX` 是整棵安装树的根，Unix 默认 `/usr/local`，本例显式指到 `/tmp/imp_cmake/prefix`。

### 真实的 cmake --install 输出

```text
$ cmake --install build
-- Install configuration: ""
-- Installing: /tmp/imp_cmake/prefix/lib/libgreeter.a
-- Installing: /tmp/imp_cmake/prefix/include
-- Installing: /tmp/imp_cmake/prefix/include/greeter.h
-- Installing: /tmp/imp_cmake/prefix/lib/cmake/greeter/greeterTargets.cmake
-- Installing: /tmp/imp_cmake/prefix/lib/cmake/greeter/greeterTargets-noconfig.cmake
-- Installing: /tmp/imp_cmake/prefix/lib/cmake/greeter/greeterConfig.cmake
-- Installing: /tmp/imp_cmake/prefix/lib/cmake/greeter/greeterConfigVersion.cmake
```

注意后半段那几个 `lib/cmake/greeter/*.cmake`——它们才是让"别人能 find_package"的命根子。装完之后的树长这样：

```text
/tmp/imp_cmake/prefix/
├── include/greeter.h
├── lib/libgreeter.a
└── lib/cmake/greeter/
    ├── greeterConfig.cmake
    ├── greeterConfigVersion.cmake
    └── greeterTargets.cmake
```

## export：让 find_package 能认出你的库

光有 `.a` 和 `.h`，`find_package(greeter)` 还是找不到——它要的是 `greeterConfig.cmake` 这类"包配置文件"。所以我们得多写两段，把它们生成并装走。

第一段，**生成 target 导出 + 版本文件 + 包配置文件**：

```cmake
install(EXPORT greeterTargets
        FILE greeterTargets.cmake
        NAMESPACE greeter::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/greeter)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/greeterConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/greeterConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/greeterConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/greeter)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/greeterConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/greeterConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/greeter)
```

逐句对：

- **`install(EXPORT ... NAMESPACE greeter::)`**：把前面 `EXPORT greeterTargets` 收集的 target 写成 `greeterTargets.cmake`，并给每个 target 加上 `greeter::` 命名空间前缀。所以消费侧拿到的不是 `greeter`，而是 **`greeter::greeter`**——命名空间能避免多库同名 target 撞车。
- **`CMakePackageConfigHelpers`** 提供两个帮手。`write_basic_package_version_file` 生成版本兼容性文件（`SameMajorVersion` 意思是主版本号相同就算兼容）；`configure_package_config_file` 把你手写的 `.in` 模板渲染成真正的 `greeterConfig.cmake`。
- 模板 `greeter/greeterConfig.cmake.in` 极简，作用就是 `include` 进那个 target 导出文件：

```cmake
@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/greeterTargets.cmake")

check_required_components(greeter)
```

`@PACKAGE_INIT@` 会被渲染成一堆让这个 config 文件能"自己定位自己"的代码，是 `configure_package_config_file` 的标配。

### export 文件里到底存了什么

打开装好的 `greeterTargets.cmake`，核心就这几行（其余是防护和路径计算）：

```cmake
# Create imported target greeter::greeter
add_library(greeter::greeter STATIC IMPORTED)

set_target_properties(greeter::greeter PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS "GREETER_VIA_CMAKE=1"
  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include;${_IMPORT_PREFIX}/include"
)
```

看清楚没——我们当初给 `greeter` 设的 **INTERFACE** 属性（`GREETER_VIA_CMAKE=1`）和 PUBLIC 的 include 目录，**全部被忠实记录进了导出文件**。这就是 usage requirement 能跨越工程边界传播的根本：它不是临时的 build 树状态，而是被序列化进了 `.cmake`，谁 `find_package` 进来，谁就拿到这个 `IMPORTED` target 及其全部 INTERFACE 属性。注意这里**没有** PRIVATE 的 `src/` 目录——它正确地没被导出，调用者根本看不见内部细节。

## find_package：Module 模式 vs Config 模式

现在消费者该上场了。`find_package` 有两种找包模式，先讲清楚再上代码。

### Module 模式（默认/基础）

CMake 找一个叫 **`Find<PackageName>.cmake`** 的脚本。这些脚本是 CMake **自带的**，专门给一批常见库写的"找包配方"，搜 `CMAKE_MODULE_PATH` 和 CMake 安装目录下的 `Modules/`。可以用这个命令看 CMake 自带了哪些：

```text
$ cmake --help-module-list | grep -E '^Find'
FindALSA
FindASPELL
FindAVIFile
FindArmadillo
FindBISON
FindBLAS
FindBZip2
FindBacktrace
...
```

Module 模式找包成功后，习惯上给你留一组变量：`<P>_FOUND`、`<P>_INCLUDE_DIRS`、`<P>_LIBRARIES`。用 `cmake -P` 直接跑一个找 zlib 的脚本，就能看清楚这套变量约定（本沙箱没装 zlib 开发头，正好演示"找不到"的真实输出）：

```text
-- Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
-- ZLIB_FOUND = FALSE
-- ZLIB_INCLUDE_DIRS =
-- ZLIB_LIBRARIES = ZLIB_LIBRARY-NOTFOUND
```

注意那行 `Could NOT find ZLIB`——这就是 CMake 自带的 `FindZLIB.cmake` 在 Module 模式下跑出来的真实输出，它按约定填了 `ZLIB_FOUND/ZLIB_INCLUDE_DIRS/ZLIB_LIBRARIES` 这几个变量。

### Config 模式（高级/全签名）

Module 模式找不到（或你显式指定 `CONFIG`/`NO_MODULE`），就回退到 Config 模式：找 **`<PackageName>Config.cmake`** 或 `<lower>-config.cmake`。这种文件**不是 CMake 自带的**，而是**库自己安装时一起装出来的**（就像我们上一步装的 `greeterConfig.cmake`）。搜的路径多得多，最常生效的是 `PATH` 环境变量、`CMAKE_PREFIX_PATH`，以及精确的 `<PackageName>_DIR`。

一句话总结：**Module 模式靠 CMake 自带的 `FindXXX.cmake` 找系统常见库；Config 模式靠库自己装的 `XXXConfig.cmake` 找任何"懂得自我描述"的库。** 我们自己 export 的 greeter 走的就是 Config 模式。

### 消费者侧：find_package + target_link_libraries

`app/CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.15)

project(demo_app LANGUAGES C)

# find_package our own installed greeter (Config mode).
find_package(greeter 1.0 REQUIRED CONFIG)

add_executable(demo main.c)

# PRIVATE: demo uses greeter internally; nothing that links demo needs to know.
target_link_libraries(demo PRIVATE greeter::greeter)
```

`app/main.c`：

```c
#include "greeter.h"

#include <stdio.h>

int main(void)
{
    char buf[64];
    greeter_greet("C-Journey", buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
```

注意三个细节：

1. `find_package(greeter 1.0 REQUIRED CONFIG)`——要 1.0 版、找不到就报错停、明确走 Config 模式。
2. `target_link_libraries(demo PRIVATE greeter::greeter)`——这里 link 的是带命名空间的 imported target，不是文件名。**全程没写一个 `-I`、没写一个 `-lgreeter`**，全靠 CMake 从导出的 target 自动配齐头文件路径、链接库、还有那个 INTERFACE 宏。
3. `PRIVATE` 表明 demo 把 greeter 当内部实现细节，没有第三方会再 link demo。

### 告诉 find_package 去哪找

find_package 默认不会去 `/tmp/imp_cmake/prefix` 找，得用 `CMAKE_PREFIX_PATH` 指一下：

```text
$ cd app && cmake -B build -DCMAKE_PREFIX_PATH=/tmp/imp_cmake/prefix
-- The C compiler identification is GNU 16.1.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.6s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/imp_cmake/app/build
```

注意配置阶段**没有任何 `greeter` 相关的 message**——`REQUIRED` 找到包时是安静的，只有找不到才会喊。看到 `Configuring done` 就说明 `greeterConfig.cmake` 被找到了、imported target 建好了。

```text
$ cmake --build build
[ 50%] Building C object CMakeFiles/demo.dir/main.c.o
[100%] Linking C executable demo
[100%] Built target demo
```

```text
$ ./build/demo
Hello, C-Journey!
```

全链路打通。库的 `greet_prefix()` 返回 "Hello"，拼出来正是这句话。

### 验证 INTERFACE 真的传过去了

光看到输出还不够。我们当初给 `greeter` 挂的那个 `INTERFACE GREETER_VIA_CMAKE=1` 宏，到底有没有传染到消费者 `demo` 的编译命令里？直接扒消费者编译时用的 flags 文件：

```text
$ grep GREETER app/build/CMakeFiles/demo.dir/flags.make
GREETER_VIA_CMAKE
```

`GREETER_VIA_CMAKE` 确确实实出现在了 `demo` 的编译命令里。这就是 INTERFACE usage requirement 跨工程传播的活证据：库定义时写一次，安装时序列化进 export 文件，消费者 find_package 进来后自动继承，**开发者一行都没手写**。这就是现代 CMake target-centric 模型真正省心的地方。

## 常见踩坑

> **坑 1：把内部头文件目录标成 PUBLIC。** 新手常犯：图省事把整个 `src/` 也塞进 PUBLIC，结果调用者能 `#include "prefix.h"` 碰到内部细节，封装直接泄漏。内部目录必须 PRIVATE。

> **坑 2：忘了写 `$<INSTALL_INTERFACE:...>`，安装后头文件路径错。** PUBLIC 里若只写源码绝对路径，build 树没问题；但一旦装到别的机器，那条绝对路径根本不存在，`find_package` 消费时头文件就找不到。用生成器表达式让 build 树和 install 树各取各的路径。

> **坑 3：link 了 imported target 却报 undefined reference。** 八成是 `find_package` 没真找到（比如 `CMAKE_PREFIX_PATH` 指错），但没用 `REQUIRED`，CMake 静默把 `greeter::greeter` 当成了普通库名去 `-lgreeter::greeter`。养成 `find_package(... REQUIRED)` 的习惯，找不到立即死，比链接阶段才报错好查得多。

> **坑 4：动态库忘了设 `SOVERSION`/版本，或 Windows dll 不导出符号却用 SHARED。** SHARED 在 Linux 下要考虑版本化；Windows 上若 dll 没有任何导出，必须用 MODULE 而非 SHARED，否则链接器拿不到任何符号。

> **坑 5：`install(TARGETS)` 没分清 LIBRARY/ARCHIVE/RUNTIME。** 静态库产物走 `ARCHIVE`，动态库走 `LIBRARY`，可执行文件走 `RUNTIME`。漏掉对应那行，装出来文件就不知去向。用 `GNUInstallDirs` 的变量比硬编码路径稳。

## 小结

- [ ] 现代 CMake 心法是 **target-centric**：属性挂在具体 target 上，而非全局。
- [ ] **PRIVATE** = 只自己用；**INTERFACE** = 只传染给上游；**PUBLIC** = 既自己用又传染。
- [ ] `target_include_directories` / `target_compile_definitions` / `target_link_libraries` 三件套，全部带 `PRIVATE/PUBLIC/INTERFACE`。
- [ ] 静态库本身不记依赖，PRIVATE 在链接层面 CMake 会兜底带上依赖库。
- [ ] `add_library(STATIC|SHARED|MODULE)` 三种库类型对应 `.a` / `.so` / 插件。
- [ ] `install(TARGETS ... EXPORT ...)` + `GNUInstallDirs` 把产物、头文件、export 元信息种到前缀。
- [ ] export 出来的 `XXXTargets.cmake` 会忠实记录 target 的 INTERFACE 属性——这是 usage requirement 跨工程传播的根本。
- [ ] `find_package`：**Module 模式**找 CMake 自带的 `FindXXX.cmake`；**Config 模式**找库自带的 `XXXConfig.cmake`。自己 export 的库走 Config。
- [ ] 消费侧 `find_package(... REQUIRED CONFIG)` + `target_link_libraries(... PRIVATE greeter::greeter)`，全程不写一个 `-I`/`-l`。

## 练习

- [ ] 把本工程的 `greeter` 从 `STATIC` 改成 `SHARED`，重新 install，用 `ldd app/build/demo` 观察动态库依赖（注意要设 `SOVERSION` 和 rpath 才能直接跑）。
- [ ] 给 `greeter` 再挂一个真正会被自己 `.c` 用到的 PRIVATE `target_compile_options`（比如 `-Wall`），观察它是否传染给消费者（应该不会）。
- [ ] 故意把 `find_package` 的 `REQUIRED` 去掉、`CMAKE_PREFIX_PATH` 指错，复现坑 3，看链接器报什么错。
- [ ] 在 `greeter` 里加一个 `option(GREETER_BUILD_TESTS ...)`，用上一篇学的 `option` + `if` 控制是否编译一个测试可执行文件。

## 参考资源

- [CMake 官方教程：Adding Usage Requirements](https://cmake.org/cmake/help/latest/guide/tutorial/Adding%20Usage%20Requirements.html)——PUBLIC/PRIVATE/INTERFACE 官方演示
- [CMake 官方教程：Installing and Testing](https://cmake.org/cmake/help/latest/guide/tutorial/Installing%20and%20Testing.html)——install/export 起点
- [Craig Scott 关于 PRIVATE/PUBLIC/INTERFACE 的经典解释](https://gitlab.kitware.com/cmake/community/-/wikis/FAQ)——本篇那张三列表的思想来源
- [find_package 官方文档](https://cmake.org/cmake/help/latest/command/find_package.html)——Module/Config 模式的权威说明

---
*整理自作者笔记,按 C-Journey 写作规范重写;所有 cmake 输出本机实测捕获。*
