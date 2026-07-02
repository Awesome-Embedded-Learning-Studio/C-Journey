---
title: "CMake 工程化:target 语义、PRIVATE/PUBLIC/INTERFACE 与多配置"
description: "阶段 0 第 12 章把 cmake -B build 两步走跑通了,但真到工程化规模,一道墙就砸上来——我把一堆 .c 抽成库给别人用,凭什么叫「链接我」?凭什么消费者能拿到我的头文件却碰不到我的内部细节?这一章把现代 CMake 的核心心法「target-centric」和传播三态讲透:PRIVATE 只自己编用、PUBLIC 自己+消费者都用、INTERFACE 只消费者用。我亲手造一个最小静态库 greeter,真跑出「消费者能 #include 到 PUBLIC 的 greeter.h、却够不着 PRIVATE 的 prefix.h」,并把 INTERFACE 挂的宏到消费者编译命令里抓出来。再补两个工程里真会咬人的:GLOB_RECURSE 收源「加文件不触发 reconfigure」的坑(故而 clib-utilities 改革改成了显式源表,逐行引它的 CMakeLists),以及 CMAKE_BUILD_TYPE=Debug/Release 多配置怎么自动选 -g 与 -O3 -DNDEBUG、连带 assert 在 Release 下静默的真跑。全 gcc16/clang22 真跑,贴真实输出与 flags.make。"
chapter: 4
order: 5
tags:
  - host
  - cmake
  - build
  - engineering
  - toolchain
difficulty: intermediate
reading_time_minutes: 18
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 12 章:CMake 入门(cmake -B build 两步走、out-of-source 构建)"
  - "阶段 0·第 6 章:链接与静态库(.a / undefined reference / 库顺序)"
  - "第 1 章:头文件契约(include guard、extern 声明、PUBLIC/PRIVATE 的契约层伏笔)"
related:
  - "阶段 0·第 12 章:CMake 入门(本章是它的工程化深化)"
  - "第 1 章:头文件契约(target_* 三态是契约在构建系统层的落地)"
  - "第 2 章:API 设计与不透明类型(把 struct 藏进 .c,target 把内部分进 PRIVATE)"
---

> 🟡 状态:待审核(2026-07-02)

# CMake 工程化:target 语义、PRIVATE/PUBLIC/INTERFACE 与多配置

## 引言:从「能编出来」到「能给别人用」

阶段 0 第 12 章我们把 `cmake -B build` 两步走跑通了——写一份 `CMakeLists.txt`,配置生成 Makefile,再 `cmake --build` 真编译,产物全进 `build/`、源码树干干净净。那是一个**单 target** 的最小场景:`add_executable(main main.c greet.c)`,一个可执行文件从头编到尾。可一旦项目长大,你马上会撞上一堵墙——我把一组 `.c` 抽成了一个**库**,凭什么叫「链接我」?更细的:我的库**内部**偷偷依赖了某个头文件、某个第三方库,调用我的人**根本不该知道**这些内部细节,怎么做到把内部藏起来、只把公开 API 暴露出去?

这堵墙就是现代 CMake 的核心:**target 语义**和它身上的**传播三态**——PRIVATE、PUBLIC、INTERFACE。这一章我们亲手造一个最小的真实静态库 `greeter`,把它身上这三态一条一条真跑给你看:消费者能 `#include` 到 PUBLIC 的头、能拿到 INTERFACE 挂的宏,却**够不着** PRIVATE 的内部头(真跑一个「故意 include 私有头」的消费者,当场编译失败)。再补两个工程里真会咬人的:用 `GLOB_RECURSE` 收源文件这个看似省事的写法,**加文件不触发重新配置**的经典坑(也就是为什么本仓库 `projects/clib-utilities` 的 CMakeLists 改革改成了显式源表——我逐行引它给你看),以及 `CMAKE_BUILD_TYPE=Debug/Release` 多配置怎么替你选 `-g` 与 `-O3 -DNDEBUG`、连带 `assert` 在 Release 下静默的真跑。前置阅读是阶段 0 第 12 章;本章在它之上往「工程化」走一层,不重复讲两步走和 out-of-source,默认你已经会。

## 一切围着 target 转:为什么不再写全局 include_directories

先纠正一个最容易混的误解:**CMake 不是编译器,它也不「编译」任何东西**——它是构建系统的生成器,你写一份声明式的 `CMakeLists.txt` 描述「我要构建什么、依赖什么、链接谁」,它替你生成 Makefile(或 Ninja 文件),最后还是交给 `make`/`ninja` 去调 `gcc`/`clang` 真编译。阶段 0 第 12 章讲过这套两步走,这里不重复,我们直接进工程化的关键。

老式 CMake 写法有一堆**全局**命令:`include_directories(...)`、`add_definitions(...)`、`link_directories(...)`。这些命令设一次,后面**所有** target 都吃——包括将来新加的、八竿子打不着的。工程小的时候无所谓,工程一大就乱成一锅粥:你根本说不清某个 `-I` 路径到底是给谁用的,排查头文件冲突时能让你翻半天。

现代 CMake 的心法是 **target-centric(以目标为中心)**。所谓 target,就是 `add_executable` / `add_library` 声明出来的那个「构建目标」——一个可执行文件、一个静态库、一个动态库,都是一个 target。所有属性都挂在**具体哪个 target** 身上:`target_include_directories`、`target_compile_definitions`、`target_compile_options`、`target_link_libraries`。给 `greeter` 这个 target 设的头文件路径,只对 `greeter` 自己和「明确链接了 `greeter` 的人」生效,别的 target 一概不管。这才有了清晰的边界,才谈得上封装、复用、跨工程传播。这条心法是下面三态的舞台。

## 传播三态:PRIVATE / PUBLIC / INTERFACE 到底谁传给谁

这是整章最关键、也最容易讲糊的概念。CMake 给 target 挂属性(头文件路径、编译宏、链接库、编译选项)时,每个属性都要标三个关键字之一。先上人话定义:

| 关键字 | 这个属性**给我自己**用吗? | 会**传染给链接我的人**吗? |
|---|---|---|
| `PRIVATE` | 是 | 否 |
| `INTERFACE` | 否 | 是 |
| `PUBLIC` | 是 | 是 |

`PUBLIC` 就是 `PRIVATE` + `INTERFACE` 的并集:我自己用,也传染给上游。翻译成一句工程话——**`PRIVATE` 是「只自己编用」、`PUBLIC` 是「自己+消费者都用」、`INTERFACE` 是「只消费者用」**。记住这一句和这张三列表,下面整章都是它的注脚。

这套语义真正要解决的事有个正式名字:**usage requirement(使用要求)的传播**。你链接了一个库,CMake 自动把「正确使用这个库所需的一切」——头文件在哪、要预定义什么宏、还要带上哪些下游库——给你配齐,不用你手写一堆 `-I`/`-D`/`-l`。而到底传不传、传给谁,就是这三个关键字控制的。我们用一个最小静态库把它跑出来,你就什么都明白了。

## 造一个库:target_* 三态齐活

我们写一个最小的静态库 `greeter`——它的公开 API 只有一个 `greeter_greet()`,公开头 `greeter.h` 放在 `include/`;内部实现细节 `prefix.h`/`greet_prefix()` 藏在 `src/`,调用者根本不该看见。这就是 PUBLIC vs PRIVATE 的天然演示场景:公开头要传染给消费者(PUBLIC)、内部头只自己用(PRIVATE)。工程结构长这样:

```text
/tmp/cj/p4ch5/greeter/
├── include/greeter.h     # 公开头(PUBLIC)
├── src/prefix.h          # 内部头(PRIVATE)
├── src/prefix.c          # 内部实现
├── src/greeter.c         # 主实现(用了内部 prefix)
└── CMakeLists.txt
```

公开头 `greeter/include/greeter.h` 只声明一个函数,这是消费者唯一该看到的东西:

```c
/* greeter/include/greeter.h */
#ifndef GREETER_H
#define GREETER_H

#include <stddef.h>

/* PUBLIC API: anyone linking greeter sees this. */
int greeter_greet(const char* who, char* out, size_t out_len);

#endif /* GREETER_H */
```

内部头 `greeter/src/prefix.h` 藏了个助手函数,这是 greeter 自己的实现细节:

```c
/* greeter/src/prefix.h */
#ifndef GREETER_PREFIX_H
#define GREETER_PREFIX_H

/* Internal helper, NOT in the public API. */
const char* greet_prefix(void);

#endif /* GREETER_PREFIX_H */
```

内部实现 `greeter/src/prefix.c`:

```c
/* greeter/src/prefix.c */
#include "prefix.h"

const char* greet_prefix(void) {
    return "Hello";
}
```

主实现 `greeter/src/greeter.c`,它**同时** `#include` 了公开头和内部头——这一点很要紧,后面验证时就靠它:

```c
/* greeter/src/greeter.c */
#include "greeter.h"
#include "prefix.h"

#include <stdio.h>

int greeter_greet(const char* who, char* out, size_t out_len) {
    int written = snprintf(out, out_len, "%s, %s!", greet_prefix(), who);
    return written;
}
```

现在看 `CMakeLists.txt` 怎么把这三态全挂上去。逐行读下来,每个关键字都对应前面那张表:

```cmake
cmake_minimum_required(VERSION 3.15)

project(greeter
        VERSION 1.0
        DESCRIPTION "Minimal C static lib for the CMake target-propagation demo"
        LANGUAGES C)

# Explicit source list (not GLOB_RECURSE): see Ch5 for why.
add_library(greeter STATIC
    src/greeter.c
    src/prefix.c)

# PUBLIC  : greeter.h lives here; anyone linking greeter must see it.
# PRIVATE : prefix.h lives here; only greeter's own .c need it.
target_include_directories(greeter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# INTERFACE: propagates UP to consumers, greeter itself never uses it.
target_compile_definitions(greeter INTERFACE GREETER_VIA_CMAKE=1)
```

逐句对一遍。`add_library(greeter STATIC ...)` 明确造一个**静态库**,产物会是 `libgreeter.a`。`target_include_directories` 一条命令里同时挂了两个路径、各自标了不同关键字:`include/` 标 **PUBLIC**,因为 `greeter.h` 在这里,任何链接 greeter 的人都得能 `#include "greeter.h"`,所以这条路径「我自己用、也传染给上游」;`src/` 标 **PRIVATE**,因为 `prefix.h` 在这里,但只有 greeter 自己的 `.c` 要 `#include "prefix.h"`,消费者根本碰不到——所以「只我自己用,不传染」。最后那条 `target_compile_definitions(greeter INTERFACE GREETER_VIA_CMAKE=1)` 纯粹为了演示 **INTERFACE**——这个宏 greeter 自己的代码里压根没用,但会作为使用要求传染给任何链接 greeter 的 target,我们待会儿从消费者侧的编译命令里亲眼抓出来。

注意那条注释 `# Explicit source list (not GLOB_RECURSE)`——这一笔是个伏笔,后面讲 GLOB 坑时回来对。现在先把这个库配置 + 构建出来,gcc 和 clang 都跑一遍:

```text
$ cd greeter && cmake -B build
-- The C compiler identification is GNU 16.1.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cj/p4ch5/greeter/build
$ cmake --build build
[ 33%] Building C object CMakeFiles/greeter.dir/src/greeter.c.o
[ 66%] Building C object CMakeFiles/greeter.dir/src/prefix.c.o
[100%] Linking C static library libgreeter.a
[100%] Built target greeter
$ CC=clang cmake -B build-clang && cmake --build build-clang
... Built target greeter
```

`libgreeter.a` 出来了,gcc 16.1.1 和 clang 22.1.6 都编过。看到那行 `Building C object` 了没——`project(... LANGUAGES C)` 把工程锁死在 C,CMake 不会去探测 C++ 编译器,干净利落。

## 消费者侧:PUBLIC 给得到、PRIVATE 给不到、INTERFACE 传上来

光造库还不够,关键是「链接它的人」拿到什么。我们写一个独立的消费者 `app`,它链接 `greeter`、调 `greeter_greet()`:

```c
/* app/main.c */
#include "greeter.h" /* PUBLIC header: consumer can reach it */

#include <stdio.h>

int main(void) {
    char buf[64];
    greeter_greet("C-Journey", buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
```

消费者侧的 `CMakeLists.txt` 用 `target_link_libraries(demo PRIVATE greeter)` 一行声明依赖——这里特意写成 `PRIVATE`,意思是 demo 把 greeter 当内部实现细节,没有第三方会再链接 demo。靠 `add_subdirectory` 把同源的 greeter target 引进来(真实工程里这步通常是 `find_package`,见参考资源里的 install/export 文档,这里不展开):

```cmake
cmake_minimum_required(VERSION 3.15)

project(demo_app LANGUAGES C)

set(GREETER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../greeter)

add_executable(demo main.c)

# PRIVATE: demo uses greeter internally; nothing links demo further.
target_link_libraries(demo PRIVATE greeter)

# Let demo's CMake find the greeter target defined in the sibling tree.
add_subdirectory(${GREETER_DIR} ${CMAKE_BINARY_DIR}/greeter_build)
```

配置、构建、跑:

```text
$ cd app && cmake -B build && cmake --build build
[ 20%] Building C object greeter_build/CMakeFiles/greeter.dir/src/greeter.c.o
[ 40%] Building C object greeter_build/CMakeFiles/greeter.dir/src/prefix.c.o
[ 60%] Linking C static library libgreeter.a
[ 60%] Built target greeter
[ 80%] Building C object CMakeFiles/demo.dir/main.c.o
[100%] Linking C executable demo
[100%] Built target demo
$ ./build/demo
Hello, C-Journey!
```

跑通了,`greeter_greet()` 拼出 `Hello, C-Journey!`。但光看输出还不够——我们当初给 `greeter` 设的三态到底生效没有?得扒 CMake 生成的 `flags.make`(它记着每个 target 真正用的编译选项)来验证。先看 INTERFACE 那个宏到底有没有传上来:

```text
$ grep GREETER app/build/CMakeFiles/demo.dir/flags.make
C_DEFINES = -DGREETER_VIA_CMAKE=1
```

`-DGREETER_VIA_CMAKE=1` 确确实实出现在了 demo 的编译命令里。注意 greeter 自己的 `.c` 里**根本没用**这个宏——它是我们专门挂给 INTERFACE 演示的,只在消费者侧出现。这就是 INTERFACE usage requirement 跨 target 传播的活证据:库定义时写一次,消费者 link 进来后自动继承,**开发者一行 `-D` 都没手写**。再看 include 路径,PUBLIC 的 `include/` 给到了、PRIVATE 的 `src/` 没给:

```text
$ grep -oE '\-I[^ ]*' app/build/CMakeFiles/demo.dir/flags.make
-I/tmp/cj/p4ch5/greeter/include
```

只有 `greeter/include` 这一条 PUBLIC 路径,`greeter/src` 那条 PRIVATE 路径**正确地缺席**。光说「缺席」还不够有说服力,我们写个**故意**去 include 私有头的消费者,看它编译时是不是真的够不着:

```c
/* leak_attempt/main.c -- deliberately try to include a PRIVATE header */
#include "prefix.h" /* PRIVATE: should NOT be reachable from a consumer */

#include <stdio.h>

int main(void) {
    printf("prefix = %s\n", greet_prefix());
    return 0;
}
```

配置阶段它不报错(那时候还没真编译),`cmake --build` 一上来就炸:

```text
$ cd leak_attempt && cmake -B build && cmake --build build
...
[ 80%] Building C object CMakeFiles/leak.dir/main.c.o
/tmp/cj/p4ch5/leak_attempt/main.c:2:10: fatal error: prefix.h: No such file or directory
    2 | #include "prefix.h" /* PRIVATE: should NOT be reachable from a consumer */
      |          ^~~~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/leak.dir/build.make:79: CMakeFiles/leak.dir/main.c.o] Error 1
```

`fatal error: prefix.h: No such file or directory`——消费者就是拿不到 PRIVATE 的 `src/` 目录,`#include "prefix.h"` 这一行当场编译失败。这就是 target 传播三态真正干的事:你把内部细节标进 PRIVATE,CMake 在构建系统层就替你把封装守住了,消费者连「碰一下」都做不到。这件事在第 1 章里是契约层的纪律(头文件只放声明、内部细节藏起来),到了 CMake 这里是构建系统层的**强制执行**——两章合起来,才是工程化「接口与实现分离」的完整闭环。

顺带说一个静态库特有的细节,免得你后面踩到时慌。静态库(`.a`)本身**不记录**它依赖了哪些别的库——不像动态库能 `ldd` 查出依赖链。所以当 `greeter` 是静态库、它内部 PRIVATE 依赖了某库 B 时,CMake 在最终把 greeter 链进可执行文件的那一刻,**仍然会把 B 一并加进链接命令**,因为 greeter 的 `.o` 里引用了 B 的符号、不带上 B 链接过不去。也就是说,PRIVATE 对静态库只在「编译/头文件层面」是私有的,「链接层面」CMake 会替你兜底带上依赖库——这是你不用操心、但要心里有数的事。

## GLOB_RECURSE 的坑:加文件不触发重新配置

现在回到 `CMakeLists.txt` 里那条 `# Explicit source list (not GLOB_RECURSE)` 的伏笔。很多人第一次写 CMake,源文件列表都爱这么收——

```cmake
file(GLOB_RECURSE DEMO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_executable(demo main.c ${DEMO_SRC})
```

`GLOB_RECURSE` 递归地把指定目录下所有 `.c` 找出来,看起来特别省事:新加一个源文件,理论上不用改 CMakeLists。可这「理论上」三个字正是坑所在。**CMake 只在 `cmake -B build`(配置阶段)那一回 glob 一次,生成 Makefile 之后就再也不看那个目录了。** 于是你新加了一个 `beta.c`、改了 `main.c` 去调它,然后只跑 `cmake --build build`——构建系统根本不知道有新文件,链接时当场报 `undefined reference to 'beta'`。我把它真跑出来给你看,先用只有 `alpha.c` 的版本配好、跑通:

```c
/* globdemo_bad/src/alpha.c */
int alpha(void) {
    return 1;
}
```

```c
/* globdemo_bad/main.c */
#include <stdio.h>

int alpha(void);

int main(void) {
    printf("alpha=%d\n", alpha());
    return 0;
}
```

```cmake
cmake_minimum_required(VERSION 3.15)
project(globdemo_bad LANGUAGES C)

# GLOB_RECURSE WITHOUT CONFIGURE_DEPENDS -- the classic trap.
file(GLOB_RECURSE DEMO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_executable(demo main.c ${DEMO_SRC})
```

```text
$ cd globdemo_bad && cmake -B build && cmake --build build
-- Build files have been written to: /tmp/cj/p4ch5/globdemo_bad/build
[100%] Linking C executable demo
[100%] Built target demo
$ ./build/demo
alpha=1
```

跑通了。现在加一个 `beta.c`、把 `main.c` 改成同时调 `alpha()` 和 `beta()`,然后**只重新构建、不重新配置**:

```c
/* globdemo_bad/src/beta.c */
int beta(void) {
    return 99;
}
```

```c
/* globdemo_bad/main.c -- updated to call beta() too */
#include <stdio.h>

int alpha(void);
int beta(void);

int main(void) {
    printf("alpha=%d beta=%d\n", alpha(), beta());
    return 0;
}
```

```text
$ cmake --build build            # 只 build,没 reconfigure
/usr/bin/ld: CMakeFiles/demo.dir/main.c.o: in function `main':
main.c:(.text+0xa): undefined reference to `beta'
collect2: error: ld returned 1 exit status
make[2]: *** [CMakeFiles/demo.dir/build.make:117: CMakeFiles/demo.dir/main.c.o] Error 1
```

`undefined reference to 'beta'`——这就是 GLOB 的经典坑:`beta.c` 在文件系统里明明躺着,可 CMake 上回 glob 出来的源表里没有它,Makefile 也没编它,链接时找不到 `beta` 符号就炸了。修法是手动重跑一次配置,CMake 重新 glob 一遍,新文件就进来了:

```text
$ cmake -B build                 # 重新配置 → 重新 glob
$ cmake --build build
[ 25%] Building C object CMakeFiles/demo.dir/src/beta.c.o
[ 50%] Linking C executable demo
[100%] Built target demo
$ ./build/demo
alpha=1 beta=99
```

重新配置之后,`beta.c.o` 这一回才编出来。这就是为什么本仓库 `projects/clib-utilities/CMakeLists.txt` 改革时**放弃了 GLOB_RECURSE、改成显式列源表**。我们逐行引它,每一处都是上面这套语义的实战落地。第 4 行起注释就直接挑明:

```cmake
# 显式源表(不用 GLOB_RECURSE):CMake 官方不推荐 GLOB 收源——
# 加/删文件不会自动触发重新 configure,列出来更稳、更可读、可审查。
set(CLUIB_SRC
    BasicDataStructure/Sources/CCBasicString.c
    BasicDataStructure/Sources/CCDynamicArray.c
    ...
    SystemRelated/Sources/CCThread.c)
```

这一坨 `set(CLUIB_SRC ...)` 看着是体力活——每加一个 `.c` 都得来这改一行——但换来的恰恰是「加文件一定触发重新配置、源表可一眼审查、不会出现『明明加了文件却编不进来』的玄学」。下面这一行接上,把这份显式源表喂给 `add_library` 造静态库:

```cmake
add_library(clib_utils STATIC ${CLUIB_SRC})
```

接着是 `target_include_directories`,公开头目录全标 **PUBLIC**(消费者要用这些头):

```cmake
target_include_directories(clib_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/BasicDataStructure/Includes
        ${CMAKE_CURRENT_SOURCE_DIR}/Basic_Utils/Includes
        ${CMAKE_CURRENT_SOURCE_DIR}/SystemRelated/Includes)
```

然后这两行是 PUBLIC/PRIVATE 的对比标本——C 标准 `c_std_11` 标 **PUBLIC**(消费者也得按 C11 编译),而 `-Wall -Wextra` 这些**警告旗标**标 **PRIVATE**(警告是我自己开发期要的,不该强加给消费者,否则别人一链接你就被你的一堆 warning 烦死):

```cmake
target_compile_features(clib_utils PUBLIC c_std_11)
target_compile_options(clib_utils PRIVATE -Wall -Wextra)
```

最后一处尤其值得品。POSIX 下这个库用了 `pthread`(线程/互斥锁)和 `dl`(动态加载),标的是 **PUBLIC**:

```cmake
if(NOT WIN32)
    target_link_libraries(clib_utils PUBLIC pthread dl)
endif()
```

为什么是 PUBLIC 而不是 PRIVATE?因为这个库的**公开头文件里露出了** pthread/dl 的类型(比如某个公开 struct 里嵌了个 `pthread_mutex_t`)——消费者只要 `#include` 了这些头,就也得能链接 pthread/dl 才能编过。这正是我们前面那张三列表里 PUBLIC 的定义:「实现里用了**而且**公开 API 里也露出了 → 既要自己用又要传染给上游」。如果这个库的公开头里**根本没碰** pthread/dl、只在 `.c` 里偷偷用,那就该标 PRIVATE——把内部依赖藏起来,消费者无感知。这种「PUBLIC 还是 PRIVATE」的判断,标准就一条:**消费者的编译/链接会不会因为缺了这个依赖而失败**。会,就 PUBLIC;不会,就 PRIVATE。整个 clib-utilities 这份 CMakeLists 就是这套语义的活教材,值得逐行读一遍。

顺带补一个真有人会问的:CMake 给 GLOB 留了个 `CONFIGURE_DEPENDS` 选项,写法是 `file(GLOB_RECURSE DEMO_SRC CONFIGURE_DEPENDS .../*.c)`,加上它之后 CMake 每次构建前会重新检查那个目录、有新文件就自动重新配置。这确实能盖住「加文件不触发」的坑,但 CMake 官方文档自己也说它**不保证在所有生成器上都生效**,而且每次构建多一次目录扫描有性能开销——所以官方的推荐姿态仍然是**显式列源表**,把 `CONFIGURE_DEPENDS` 当「实在不想手维护源表时的妥协」。本仓库 clib-utilities 走的就是显式源表这条路,稳、可读、可审查,一劳永逸。

## 多配置:CMAKE_BUILD_TYPE 怎么替你选旗标

工程化的另一件常事是「同一个项目要编出不同配置」——开发期带调试信息(`-g`)好上 GDB,发布期激进优化、关掉 `assert`(`-O3 -DNDEBUG`)。阶段 0 第 9 章我们手拧过 `-g`/`-O`/`-DNDEBUG`,在 CMake 里这一整套靠一个变量 `CMAKE_BUILD_TYPE` 来切。它内置几种构建类型,最常用的就是 Debug 和 Release。我们写一个最小例子,它的 `main` 里有一条 `assert(1 == 2)`——Debug 下会触发、Release 下因为 `-DNDEBUG` 而静默:

```c
/* multiconfig/demo.c */
#include <assert.h>
#include <stdio.h>

int main(void) {
    /* NDEBUG (set by Release) makes assert a no-op. */
    assert(1 == 2 && "this assert fires under Debug, silent under Release");
    printf("ran to completion\n");
    return 0;
}
```

`CMakeLists.txt` 极简,什么都不用标——构建类型是配置期用 `-D` 传进去的:

```cmake
cmake_minimum_required(VERSION 3.15)
project(multiconfig LANGUAGES C)

add_executable(demo demo.c)
```

分别配两个 build 目录、各传一个 `CMAKE_BUILD_TYPE`,然后把 CMake 实际给的旗标从生成的 `flags.make` 里扒出来对比。先看 Debug:

```text
$ cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build-dbg
$ grep '^C_FLAGS' build-dbg/CMakeFiles/demo.dir/flags.make
C_FLAGS = -g
$ ./build-dbg/demo; echo "exit=$?"
demo: /tmp/cj/p4ch5/multiconfig/demo.c:7: main: Assertion `1 == 2 && ...' failed.
exit=134
```

Debug 给的就是 `-g`(带调试信息,对应阶段 0 第 9 章「调试用 `-O0 -g`」),`NDEBUG` 没定义,所以 `assert(1 == 2)` 当场触发、程序 abort、退出码 134(128+SIGABRT 的 6)。再看 Release:

```text
$ cmake -B build-rel -DCMAKE_BUILD_TYPE=Release
$ cmake --build build-rel
$ grep '^C_FLAGS' build-rel/CMakeFiles/demo.dir/flags.make
C_FLAGS = -O3 -DNDEBUG
$ ./build-rel/demo; echo "exit=$?"
ran to completion
exit=0
```

Release 给的是 `-O3 -DNDEBUG`——`-O3` 激进优化,`-DNDEBUG` 定义了 `NDEBUG` 宏、`assert` 在 `<assert.h>` 里被这个宏关成了空操作(`((void)0)`),所以同一段代码这回直接跑到底、打印 `ran to completion`、退出 0。这就是 `CMAKE_BUILD_TYPE` 的全部价值:**开发期 Debug 调试、要发布或跑性能基准时切 Release,靠这一个变量切换,旗标 CMake 替你选好**——不用自己去 `if/else` 一堆 `-g`/`-O3`,这正是 CMake 比手写 Makefile 省心的地方。

这里有个直接呼应阶段 0 第 9 章的细节要划重点。你大概注意到上面 `C_FLAGS` 那行里**没有** `-std=` 那一条——这个 demo 没设 `CMAKE_C_STANDARD`,CMake 用的就是编译器默认的 `-std=gnu11`(gnu 扩展开着),和我们第 9 章讲的「gcc 默认纵容 GNU 扩展、要严格 c11 得显式动手」一脉相承。要在 CMake 里钉死严格 C11,得显式写 `set(CMAKE_C_STANDARD 11)` 配 `set(CMAKE_C_STANDARD_REQUIRED ON)` 再配 `set(CMAKE_C_EXTENSIONS OFF)`——三件套齐了,CMake 才给你 `-std=c11` 而不是 `-std=gnu11`。这套纪律阶段 0 第 12 章讲过、第 9 章也讲过,这里不重复,只提醒一句:CMake 默认的姿态和 gcc 一样,是「能让你过就让你过」,要严格得自己动手。

还有一个多配置的坑值得提一嘴:`CMAKE_BUILD_TYPE` 这套是**单配置生成器**(Unix Makefiles、Ninja)的玩法,你**必须**在配置期用 `-D` 指定一个类型、一个 build 目录对应一个类型。如果你用 **多配置生成器**(Visual Studio、Ninja Multi-Config、Xcode),配置期不指定类型、构建期用 `cmake --build build --config Debug` 来选——同一份工程、同一个 build 目录里能同时存在多套配置产物。这两种生成器的切换点不一样,新手在 Visual Studio 上跑 `cmake -DCMAKE_BUILD_TYPE=Release` 发现旗标没变,八成就是这个坑——单配置的那套在多配置生成器上**不生效**。Linux 上默认的 Unix Makefiles 是单配置,我们这套写法没问题;哪天换 Ninja Multi-Config,记得改用 `--config` 切。

## 小结

工程化这一章我们把阶段 0 第 12 章那个「能编出来」的最小流程,推进到了「能给别人用」的工程化规模,核心就两件事。第一件是 **target 语义和传播三态**:现代 CMake 一切围着 target 转,属性挂在具体 target 身上而不是全局;给属性标 `PRIVATE`(只自己编用)、`PUBLIC`(自己+消费者都用)、`INTERFACE`(只消费者用),CMake 就会替你把「正确使用这个库所需的一切」按这三态自动传播——头文件路径、编译宏、链接库,消费者 link 进来就配齐,不用手写一个 `-I`/`-D`/`-l`。我们真跑出消费者能 `#include` 到 PUBLIC 的 `greeter.h`、能继承 INTERFACE 挂的 `GREETER_VIA_CMAKE` 宏,却**够不着** PRIVATE 的 `prefix.h`(`fatal error: prefix.h: No such file or directory`)——契约层的「接口与实现分离」到 CMake 这里被构建系统强制执行了。第二件是两个工程里真会咬人的:**GLOB_RECURSE 收源「加文件不触发重新配置」**的经典坑,故而本仓库 clib-utilities 改成了逐行列出的显式源表(我们逐行引了它的 CMakeLists,从 PUBLIC 的头目录、PUBLIC 的 `pthread dl`、到 PRIVATE 的 `-Wall -Wextra`,每处都是三态语义的实战落地);以及 **CMAKE_BUILD_TYPE 多配置**替你选 `-g`(Debug)与 `-O3 -DNDEBUG`(Release),连带 `assert` 在 Release 下静默的真跑——但要留心 CMake 默认给的是 `-std=gnu11` 不是 `-std=c11`,要严格 C11 得显式关 `CMAKE_C_EXTENSIONS`,还有单配置 vs 多配置生成器切换配置的姿势不一样。把这些吃透,你写出来的 CMakeLists 就不再是「能跑」的脚本,而是「有封装、有边界、可复用」的工程描述。

带着这套 target 心法,后续章节我们会把 sanitizer(阶段 0 第 10 章)、CTest 测试、clang-format 质量门都挂进 CMake 的 target 上——它们全是 target 身上的属性,而不是全局开关,这条心法是一以贯之的。

## 参考资源

- **CMake 官方教程:[Adding Usage Requirements](https://cmake.org/cmake/help/latest/guide/tutorial/Adding%20Usage%20Requirements.html)**——PUBLIC/PRIVATE/INTERFACE 三态的官方演示,本章那张三列表的思想来源。
- **CMake 官方手册**:`target_include_directories` / `target_compile_definitions` / `target_link_libraries`(`cmake --help-command target_link_libraries`)、`CMAKE_BUILD_TYPE`、`CMAKE_C_STANDARD`/`CMAKE_C_EXTENSIONS`、`file(GLOB ... CONFIGURE_DEPENDS)`。
- **[Professional CMake: A Practical Guide](https://crascit.com/professional-cmake/)**(Craig Scott):target-centric 心法、usage requirement 传播、install/export 全签名讲得最系统的一本。
- **本仓库 [projects/clib-utilities/CMakeLists.txt](../../projects/clib-utilities/CMakeLists.txt)**:一份改革过的真实多模块 CMake 工程,显式源表 + target_* 三态 + `pthread dl` PUBLIC 的活教材,本章逐行引用了它。
- **阶段 0 第 9 章:标准与优化**——`-g`/`-O3`/`-DNDEBUG`/`-std=c11 vs gnu11` 的含义,CMake 的旗标就是它们的封装。
- **阶段 0 第 12 章:CMake 入门**——`cmake -B build` 两步走、out-of-source 构建,本章是它的工程化深化。
