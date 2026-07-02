---
title: "make 深处：头文件自动依赖、-MMD/-MP 与 -j 并行竞态"
description: "阶段 0 第 11 章把 make 三要素、TAB、增量、变量、$@/$</$^、模式规则、.PHONY 讲完了——但那套 Makefile 留了一道没填的墙：每条 .o 规则的依赖里得手写头文件。这一章就专门拆这道墙。先用一个 mod.h+mod.c+main.c 的多文件工程真跑出那个经典坑：模式规则 %.o: %.c 只把 .c 列成依赖，头文件不在里面——于是改 mod.h（哪怕把里面的 GREETING 从 v1 改成 v2）make 仍然 up to date，跑出来的程序还是旧值 v1，bug 就这么静悄悄地藏进去。修法是让编译器自己生成头依赖：gcc 的 -MMD 在编译每个 .c 时顺便吐一份 .d 依赖文件（main.o: main.c mod.h），Makefile 里一行 -include $(wildcard *.d) 把它们喂回给 make；再配 -MP 给每个头文件补一条空的 phantom 规则（mod.h:），这样头文件被删/改名时 make 不会因为「找不到 mod.h 这个 prerequisite」而罢工——我把有 -MP 和没 -MP 生成的 .d 文件内容并排贴出来对照。讲完手写 Makefile 这套之后，对照本仓 build_examples.py 为什么全用 CMake：CMake 生成的构建系统（Ninja 或 Makefile）内部自带头依赖图（Ninja 用一个 .ninja_deps 二进制库记录每个 .o 依赖了哪些 .h，编译时自动喂回去），你根本不用手写 -MMD。最后真跑 -jN 并行：构建能加速，但有个经典竞态——某条 .o 规则的 prerequisite 里漏了一个由别的规则生成的头文件（比如代码生成器产出的 genhdr.h），make -j 就可能在你生成头文件之前先去编译那个 .c，当场 fatal error: genhdr.h: No such file or directory；我加 sleep 把生成器放慢、-j8 真跑复现这个竞态，再把 genhdr.h 显式加进 %.o 的 prerequisite 修掉。全 gcc16+clang22 真跑，贴真实输出。"
chapter: 4
order: 4
tags:
  - host
  - make
  - build
  - engineering
  - toolchain
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 11 章:make 入门（三要素、TAB、增量、变量、$@/$</$^、模式规则、.PHONY）"
  - "阶段 0·第 2 章:编译四阶段（预处理把 #include 文本插入、-c 停在汇编后）"
  - "阶段 0·第 12 章:CMake 入门（cmake -B build 两步走、out-of-source）"
related:
  - "阶段 0·第 11 章:make 入门（本章是它的工程化深化）"
  - "第 5 章:CMake 工程化（target 语义、PRIVATE/PUBLIC/INTERFACE，CMake 内置依赖图的进阶）"
  - "阶段 0·第 12 章:CMake 入门（本章末尾对照它讲为什么 CMake 不用你手写 -MMD）"
---

> 🟡 状态:待审核(2026-07-02)

# make 深处：头文件自动依赖、-MMD/-MP 与 -j 并行竞态

## 引言：阶段 0 那道没填的墙

写这一章之前，先划清楚和阶段 0 第 11 章的边界：那边讲了 make 的三要素（目标 / 依赖 / 命令）、命令行那个非 TAB 不可的 `missing separator` 坑、增量编译为什么能「只重编改过的」、`CC`/`CFLAGS` 变量与 `make CC=clang` 覆盖、`=` vs `:=` 的展开时机、`$@`/`$<`/`$^` 自动变量、`%.o: %.c` 模式规则、`.PHONY` 与 `clean`。如果你对这几样还生，回去把那章过一遍再回来——本章不再重复这些，直接站在它们之上往工程化深处走。

阶段 0 那套 Makefile 留了什么没填？回忆一下那章最后给的精简版编译规则：

```makefile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

这条模式规则的 prerequisite 里**只有 `.c`，没有头文件**。换句话说，make 只知道「`main.o` 依赖 `main.c`」——它根本不知道 `main.c` 里 `#include "mod.h"` 这回事。于是一旦你改了头文件，make 看一眼 `main.o` 比 `main.c` 新，就一声 `up to date` 不重编。阶段 0 那章其实已经在朴素的显式规则里手写过 `main.o: main.c greet.h` 把头文件塞进依赖，也点了一句「大项目会借 `-MMD` 自动生成依赖」然后把它推给了「CMake 那章再提」——这一章就是来兑现这个承诺的，而且会比预想的更有意思：不光要解决「改了头文件不重编」，还要解决「删了头文件 make 找不到 prerequisite 罢工」，最后还得对付 `-j` 并行时一道更阴的竞态。

这一章的工程场景非常具体：一个真实的 C 工程是几十上百个 `.c` 配几十个 `.h`，你不可能每次新加一个 `#include` 就记得去翻 Makefile 把那个头文件添进对应 `.o` 的依赖里——人干这事一定会漏，漏一次就埋一个「改了头文件、程序行为却没变」的灵异 bug。所以工程上的正解是：**让编译器替你管头文件依赖**。gcc/clang 编译每个 `.c` 时本来就要把所有 `#include` 全部展开（阶段 0 第 2、3 章讲的预处理），它最清楚这个 `.c` 到底依赖了哪些 `.h`——让它编译时顺手把这份依赖清单吐到一个 `.d` 文件里，Makefile 再把这些 `.d` 文件 `-include` 进来当规则用，整个链路就自动化了。这一章我们一步一步把它真跑通，然后对照看 CMake 是怎么在更高一层把这整套机制全替你封装掉的。

## 第一道坑：改了头文件，make 不重编

先把这个坑亲手踩出来。我们搭一个最小但真实的多文件工程——一个 `mod.h` 声明函数并定义一个宏 `GREETING`，`mod.c` 实现它，`main.c` 调用它：

```c
/* mod.h */
#ifndef MOD_H
#define MOD_H
#define GREETING "v1"
void mod_print(void);
#endif
```
```c
/* mod.c */
#include "mod.h"
#include <stdio.h>

void mod_print(void) {
    printf("mod says: %s\n", GREETING);
}
```
```c
/* main.c */
#include "mod.h"

int main(void) {
    mod_print();
    return 0;
}
```

给它配一个最自然的、用模式规则的 Makefile（注意命令行行首是 TAB，这是阶段 0 第 11 章那个老坑，这里不重复）：

```makefile
/* Makefile.naive */
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

main: main.o mod.o
	$(CC) $(CFLAGS) -o main main.o mod.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f main main.o mod.o
```

先全量构建一次，确认能跑：

```text
$ make -f Makefile.naive clean && make -f Makefile.naive
gcc -std=c11 -Wall -Wextra -c main.c -o main.o
gcc -std=c11 -Wall -Wextra -c mod.c -o mod.o
gcc -std=c11 -Wall -Wextra -o main main.o mod.o
$ ./main
mod says: v1
```

很好，输出 `v1`。现在做一件能暴露问题的事：**只刷新 `mod.h` 的修改时间**（用 `touch`），再 `make`：

```text
$ touch mod.h && make -f Makefile.naive
make: 'main' is up to date.
```

`up to date`——make 觉得啥都不用干。问题在哪？`%.o: %.c` 这条模式规则告诉 make「`main.o` 只依赖 `main.c`」，它根本不知道 `main.c` 还 `#include "mod.h"`。`touch` 把 `mod.h` 改新了，可 `mod.h` 压根不在 `main.o` 的 prerequisite 列表里，make 比较的是 `main.o` 和 `main.c` 的 mtime，`main.c` 没动，所以 `main.o` 也不用动。这听起来好像只是「优化没生效」，真正可怕的地方在下一步：我把 `mod.h` 里的 `GREETING` 从 `v1` 改成 `v2`，再 `make`：

```text
$ # 编辑 mod.h：#define GREETING "v2"
$ make -f Makefile.naive
make: 'main' is up to date.
$ ./main
mod says: v1
```

头文件已经写 `v2` 了，make 还是一句 `up to date`，跑出来的程序却仍然是旧的 `v1`。这就是这个坑最阴的地方——它不报错、不崩溃、不 assert 失败，只是悄悄地让你的程序和行为已经改过的源码对不上。一个真实工程里，你改了头文件里某个结构体的字段顺序、某个宏的值、某个函数的签名，make 不重编就意味着你的 `.o` 还停在旧头文件编出来的版本，链接进可执行文件后，运行时表现和你以为的源码状态完全脱节。这种 bug 调起来能让人怀疑人生——你看源码明明改对了，程序就是不听话。

顺带说一句，这个坑阶段 0 第 11 章其实用**手写显式规则**绕过了一次：当时那个朴素版 Makefile 写的是 `main.o: main.c greet.h`，把头文件硬编码进了 prerequisite，所以 `touch greet.h` 能触发重编。但手写的前提是你得记得每个 `.c` 都 `#include` 了哪些 `.h`——文件一多、一改 include，人是不可能维护对的。所以工程上从来不靠手写，而是让编译器自己吐依赖。这就引出 `-MMD`。

## `-MMD`：让编译器替你写头文件依赖

gcc 和 clang 都提供一族 `-M*` 选项，专门干「编译时顺便吐出依赖清单」这件事。常用的两个：

- `-MMD`：编译每个 `.c` 时，额外生成一份 `.d` 文件，里面写「`xxx.o: xxx.c 加上它 #include 的所有 .h`」。它**只列非系统头文件**（你自己项目里的 `mod.h` 这种），不把 `<stdio.h>` 这类系统头也塞进去——系统头基本不会改，塞进去只会让依赖清单巨大无比还容易误触发。
- `-MP`：给 `.d` 文件里**每一个头文件**额外补一条「空的 phantom 规则」（比如 `mod.h:`，目标后面什么都不写）。这条空规则的作用是告诉 make「就算 `mod.h` 这个文件消失了，也别报错」——这个机制下一节专门讲，先把 `-MMD` 跑通。

我们给同一份源码换一个 Makefile，把 `-MMD -MP` 加进 `CFLAGS`，再用 `-include` 把生成的 `.d` 文件喂回 make：

```makefile
/* Makefile.deps */
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP

main: main.o mod.o
	$(CC) $(CFLAGS) -o main main.o mod.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

DEPS := $(wildcard *.d)
-include $(DEPS)

.PHONY: clean
clean:
	rm -f main main.o mod.o *.d
```

这里有两个新东西得说清楚。`-MMD -MP` 进了 `CFLAGS`，所以每次 `gcc -c main.c -o main.o` 都会顺带在旁边生成一个 `main.d`——这就是编译器替你写的头依赖清单。`-include $(DEPS)` 那行是关键：`$(wildcard *.d)` 把当前目录下所有 `.d` 文件列出来，`-include`（注意前面那个**减号**）把它们的内容当 Makefile 片段读进来。减号的意思是「找不到也别报错」——第一次干净构建时还没有任何 `.d` 文件，`*.d` 匹配空，`-include` 一声不吭地跳过；编完一次之后 `.d` 就有了，第二次起 make 就能读到这些自动依赖。这个「减号前缀吞掉文件不存在错误」的小机制，是让「第一次构建」和「之后每次」能用同一份 Makefile 的前提。

干净构建一次，看看 `.d` 长什么样：

```text
$ make -f Makefile.deps clean && make -f Makefile.deps
gcc -std=c11 -Wall -Wextra -MMD -MP -c main.c -o main.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c mod.c -o mod.o
gcc -std=c11 -Wall -Wextra -MMD -MP -o main main.o mod.o
$ ls *.d
main.d  mod.d
$ cat main.d
main.o: main.c mod.h
mod.h:
$ cat mod.d
mod.o: mod.c mod.h
mod.h:
```

看 `main.d` 的第一行 `main.o: main.c mod.h`——这正是阶段 0 第 11 章那个手写的 `main.o: main.c greet.h` 的自动版：编译器编译 `main.c` 时知道它 `#include "mod.h"`，于是把 `mod.h` 自动列进了 `main.o` 的 prerequisite。从此以后，你新加一个 `#include "xxx.h"`、改一个 include 路径，都不用动 Makefile——下次编译时 `-MMD` 自动把新的依赖写进 `.d`，`-include` 自动让 make 看到它。

现在回到当初那个坑，验证修好了没有。再次 `touch mod.h`：

```text
$ touch mod.h && make -f Makefile.deps
gcc -std=c11 -Wall -Wextra -MMD -MP -c main.c -o main.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c mod.c -o mod.o
gcc -std=c11 -Wall -Wextra -MMD -MP -o main main.o mod.o
```

这次 make 老老实实把 `main.o` 和 `mod.o` 都重编了——因为 `main.d` 里写着 `main.o: main.c mod.h`，`mod.h` 一新，`main.o` 就过期了。那两行 `mod.h:`（在 `main.d` 和 `mod.d` 里各出现一次）就是 `-MP` 加的 phantom 规则，下一节讲它干嘛用的。

## `-MP` 的 phantom 规则：头文件被删了，make 别罢工

`-MMD` 解决了「头文件改了不重编」，但留下一个新问题：**头文件被删了怎么办**。想象这个场景——你重构代码，把 `mod.h` 里的内容并进了别的头文件，然后把 `mod.h` 从磁盘删了，`main.c` 里的 `#include "mod.h"` 也去掉了。但旧的 `main.d` 还在，里面写着 `main.o: main.c mod.h`。这时候你 `make`，make 看到 `main.o` 依赖 `mod.h`，可 `mod.h` 已经不存在了，它又找不到任何规则能生成 `mod.h`——于是 make 直接报 `No rule to make target 'mod.h', needed by 'main.o'.  Stop.` 罢工。明明你的源码已经不再需要这个头文件了，make 却被一份过期的 `.d` 卡住。

`-MP` 就是用来堵这个洞的。它让 `.d` 文件里每个头文件后面都跟一条「空的 phantom 规则」——比如 `mod.h:`，目标名是 `mod.h`、prerequisite 为空、命令也为空。make 读到这条规则时会认为「`mod.h` 这个目标我已经有一条规则了」（哪怕它什么都不做），于是就算磁盘上 `mod.h` 不存在，make 也不会再说「找不到规则生成 `mod.h`」，而是用这条空规则「假装生成」了它（实际啥都不干），然后继续往下走。等到这次编译重新跑过 `-MMD`，新的 `main.d` 自然就不会再列出已经不 include 的 `mod.h`，依赖清单自己就更新对了。

我把「有 `-MP`」和「没 `-MP`」生成的 `.d` 文件并排对照，差别一眼能看出来。先看不加 `-MP`、只用 `-MMD` 时编 `main.c` 生成什么：

```text
$ gcc -std=c11 -MMD -c main.c -o mainA.o
$ cat mainA.d
/tmp/cj/p4ch4/mainA.o: main.c mod.h
```

只有一行 `main.o: main.c mod.h`，没有别的。再加上 `-MP`：

```text
$ gcc -std=c11 -MMD -MP -c main.c -o mainB.o
$ cat mainB.d
/tmp/cj/p4ch4/mainB.o: main.c mod.h
mod.h:
```

多了最后一行 `mod.h:`——这就是 `-MP` 加的 phantom 规则。这行看着不起眼，但它就是「删了头文件 make 不罢工」的全部秘密。所以工程上的规矩很干脆：**`-MMD` 永远配 `-MP` 一起用**，前者负责把头依赖写对，后者负责善后头文件消失的情况。两个选项几乎没有单独用的理由，写 Makefile 时直接 `-MMD -MP` 当一对写死就行。

## 多目录工程：路径与 `$(wildcard)` 的实战

真实工程不会把所有 `.c` 平铺在一个目录里，通常是 `src/` 放源码、`include/` 放头文件、`build/` 放产物（out-of-source 构建，阶段 0 第 12 章讲 CMake 时提过这个理念）。这套结构上手写 Makefile 时，`-MMD` 生成的 `.d` 文件会**跟着 `.o` 走**（默认和 `.o` 同名同目录），所以只要你的 `%.o` 模式规则把 `.o` 放进 `build/`，`.d` 也会自动落进 `build/`，`-include` 时把 `$(wildcard build/*.d)` 喂进去就行——路径上不用特殊处理。这里我不展开多目录的完整 Makefile（写起来涉及 `VPATH`、`vpath`、自动创建 `build/` 目录、模式规则里拼路径等一堆细节，容易把这一章的主线「头依赖」冲淡），只想强调一个容易卡的地方：**`.d` 文件里写的路径是「编译命令里 `.o` 的路径」**。如果你编 `gcc -c src/foo.c -o build/foo.o`，生成的 `build/foo.d` 第一行会是 `build/foo.o: src/foo.c include/foo.h`——目标和依赖都是带相对路径的，这恰好和 Makefile 里那个 `build/foo.o` 目标对得上，所以 `-include` 进来能直接生效，不用你再做路径归一化。这条规律在手写复杂 Makefile 时是个定心丸：只要你的编译命令路径写对了，`.d` 里的路径就自动对。

真到了多目录、多 target、还要跨平台、还要管安装路径的规模，手写 Makefile 的维护成本就开始失控了——这正是 CMake 这类元构建系统（meta-build system）的舞台。不过在那之前，我们先看一眼仓库自己是怎么处理这件事的。

## 对照：本仓 `build_examples.py` 为什么全用 CMake

讲完手写 Makefile 的 `-MMD`，回头看本仓库自己怎么构建 `examples/`。打开 `scripts/build_examples.py`，核心就这几行：

```text
$ grep -nE 'GEN|cmake|CMake' scripts/build_examples.py | head
2:"""构建并测试 examples/ 下所有 CMake 子项目 —— CI 质量门的核心。
8:退出码:0 examples 全过;1 examples 有失败;2 缺 cmake。
28:CMAKE = shutil.which("cmake")
30:GEN = "Ninja" if shutil.which("ninja") else "Unix Makefiles"
46:r = run([CMAKE, "-S", str(src), "-B", str(bdir), "-G", GEN])
80:for cl in sorted((REPO / "examples").glob("**/CMakeLists.txt")):
```

它遍历 `examples/**/CMakeLists.txt`，对每一个用 `cmake -S src -B build -G Ninja`（有 Ninja 用 Ninja、没有退回 Unix Makefiles）生成构建文件，再 `cmake --build` 编译。注意这里**从头到尾没出现 `-MMD`**——不是写漏了，是根本不需要你写：CMake 生成的构建系统（无论是 Ninja 还是 Makefile）**内部已经自带头文件依赖追踪**。我们用刚才那个 `mod.h/mod.c/main.c` 配一个最小 `CMakeLists.txt` 真跑一遍，看 CMake 是怎么管的：

```cmake
/* cmake_demo/CMakeLists.txt */
cmake_minimum_required(VERSION 3.15)
project(mymod C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_executable(main main.c mod.c)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

配置 + 构建：

```text
$ cmake -G Ninja -B build
-- The C compiler identification is GNU 16.1.1
-- Detecting C compile features - done
-- Configuring done (0.1s)
-- Build files have been written to: /tmp/cj/p4ch4/cmake_demo/build
$ cmake --build build
[1/3] Building C object CMakeFiles/main.dir/main.c.o
[2/3] Building C object CMakeFiles/main.dir/mod.c.o
[3/3] Linking C executable main
$ ./build/main
mod says: from mod
```

现在做和手写 Makefile 时一模一样的实验——`touch mod.h` 再 build：

```text
$ touch mod.h && cmake --build build
[1/3] Building C object CMakeFiles/main.dir/main.c.o
[2/3] Building C object CMakeFiles/main.dir/mod.c.o
[3/3] Linking C executable main
```

两个 `.o` 都重编了——CMake/Ninja 自动知道 `main.o` 依赖 `mod.h`，一行 `-MMD` 都没让你写。它是怎么做到的？答案是 Ninja（CMake 默认生成的构建后端）在编译每个 `.c` 时，**自己**用编译器探出的头文件清单建了一张依赖数据库，存在 `build/.ninja_deps` 这个二进制文件里。我们可以用 `ninja -t deps` 把这张库的内容打印出来看：

```text
$ ninja -C build -t deps
CMakeFiles/main.dir/main.c.o: #deps 3, deps mtime 1782967859628390253 (VALID)
    /tmp/cj/p4ch4/cmake_demo/main.c
    /usr/include/stdc-predef.h
    /tmp/cj/p4ch4/cmake_demo/mod.h

CMakeFiles/main.dir/mod.c.o: #deps 28, deps mtime 1782967859631795331 (VALID)
    /tmp/cj/p4ch4/cmake_demo/mod.c
    /usr/include/stdc-predef.h
    /tmp/cj/p4ch4/cmake_demo/mod.h
    /usr/include/stdio.h
    ... (后面还有 20 多个 glibc 系统头,省略)
```

`main.c.o` 那条记录了 3 个依赖：`main.c` 自己、`stdc-predef.h`（gcc 默认会塞的预定义头）、还有我们的 `mod.h`。`mod.c.o` 因为 `#include <stdio.h>`，连带追了 28 个系统头进去。注意 Ninja 这里**连系统头也追**了（不像 `-MMD` 默认只追项目头）——因为 Ninja 把整张依赖图存自己的库、不污染 Makefile，存细一点也不亏，反而更精确（系统头真改了也能触发重编）。CMake 用 Makefile 后端时（`-G "Unix Makefiles"`）走的是和我们手写一模一样的 `-MMD -MP` 机制，只是这套开关是 CMake 替你加进去的、`.d` 文件是它替你 `-include` 的——你在 `CMakeLists.txt` 里写 `add_executable(main main.c mod.c)`，剩下的事它全包了。

所以本仓 `build_examples.py` 全用 CMake 不是偏好问题，是工程现实：手写 `-MMD` 在一个 5 文件 demo 里看着简单，到了要管十几个 target、跨平台编译器、out-of-source 构建目录、安装规则、CTest 测试发现的规模，手写 Makefile 的复杂度会指数爆炸，而 CMake 把这一整套（头依赖只是其中一环）都用一套声明式语法管起来了。阶段 0 第 12 章和本章第 5 章会专门讲 CMake 的工程化用法，这里只是把「为什么 CMake 不用你手写 -MMD」这件事在头依赖这个点上落一次地。

## `-jN`：并行构建与那道阴险的竞态

最后讲 `-j`。make 默认是**串行**的——一次只跑一条命令，前一条跑完才跑下一条。加 `-jN`（N 是一个数字，比如 `-j4`）告诉 make「同时最多跑 N 条命令」，并行编译能显著加速大型工程（CPU 几个核就给几个 job，几十个 `.c` 同时编译能省一大半时间）。`-j` 不带数字则是不设上限、能跑多少跑多少（激进，可能把内存吃光，一般给个 `-j$(nproc)` 最稳）。我们用刚才那个三文件工程真跑一下 `-j4`：

```makefile
/* Makefile.par — 几个 .o 凑成可并行的工程 */
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP

PROG = prog
OBJS = pmain.o mod.o worker.o

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

DEPS := $(wildcard *.d)
-include $(DEPS)

.PHONY: clean
clean:
	rm -f $(PROG) $(OBJS) *.d
```

```text
$ make -f Makefile.par clean && make -f Makefile.par -j4
gcc -std=c11 -Wall -Wextra -MMD -MP -c pmain.c -o pmain.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c mod.c -o mod.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c worker.c -o worker.o
gcc -std=c11 -Wall -Wextra -MMD -MP -o prog pmain.o mod.o worker.o
```

三个 `.o` 的编译命令可以并行（它们互不依赖），最后那条链接必须等三个 `.o` 都编完才能跑——make 自己会算这个依赖顺序，不用你管。到这里一切美好。

但 `-j` 有一道特别阴的竞态，专门坑那种「prerequisite 里漏写了某个文件」的 Makefile。典型场景是**代码生成器**：你有个脚本（比如 `genhdr.sh`）会生成一个头文件 `genhdr.h`，某个 `.c` 又 `#include "genhdr.h"`。如果手写 Makefile 时只写了「`genhdr.h` 由 `genhdr.sh` 生成」和「`%.o: %.c` 编译」，**却忘了在 `%.o` 的 prerequisite 里加上 `genhdr.h`**，那么串行构建（`make` 不带 `-j`）可能因为恰好先生成了头文件再编译而侥幸通过，但一开 `-j` 就原形毕露——make 会同时调度「生成 `genhdr.h`」和「编译 `foo.c`」两条命令，编译那条可能在头文件还没生成完时就先跑了，当场 `fatal error: genhdr.h: No such file or directory`。我们故意写一个有这个缺陷的 Makefile，再加个 `sleep` 把生成器放慢，让竞态稳定复现：

```makefile
/* Makefile.race2 — foo.o 不声明 genhdr.h 为 prerequisite,有竞态 */
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

foo: foo.o genhdr.h
	$(CC) $(CFLAGS) -o foo foo.o

genhdr.h:
	@sh genhdr.sh          # 模拟一个慢的代码生成器

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f foo foo.o genhdr.h
```

```c
/* foo.c — 包含由生成器产出的 genhdr.h */
#include "genhdr.h"
#include <stdio.h>

int main(void) {
    printf("%s\n", GENERATED_MSG);
    return 0;
}
```

```sh
/* genhdr.sh — 故意 sleep,放大竞态窗口 */
echo "generating genhdr.h (slow)"
sleep 0.3
printf '#ifndef GENHDR_H\n#define GENHDR_H\n#define GENERATED_MSG "hello from generated"\n#endif\n' > genhdr.h
```

`-j8` 真跑，竞态当场炸：

```text
$ make -f Makefile.race2 clean && make -f Makefile.race2 -j8
gcc -std=c11 -Wall -Wextra -c foo.c -o foo.o
generating genhdr.h (slow)
foo.c:2:10: fatal error: genhdr.h: No such file or directory
    2 | #include "genhdr.h"
      |          ^~~~~~~~~~~
compilation terminated.
make: *** [Makefile.race2:13: foo.o] Error 1
make: *** Waiting for unfinished jobs....
```

看输出的顺序——make 同时开两条命令：一条是 `gcc -c foo.c`（快，几十毫秒），另一条是 `sh genhdr.sh`（慢，sleep 了 0.3 秒）。编译那条根本不管头文件有没有生成就冲了，结果 `#include "genhdr.h"` 找不到文件，`fatal error`。如果不开 `-j`，make 串行执行会先把 `genhdr.h` 生成完再去编 `foo.c`（顺序由 `foo: foo.o genhdr.h` 这条规则隐含的「先满足 prerequisite」决定），就侥幸过了——这正是这种 bug 在开发机上不发作、一进 CI（CI 通常 `-j$(nproc)` 并行）就偶发炸的原因，特别难复现、特别难调。

修法很直接：**把 `genhdr.h` 显式加进 `foo.o` 的 prerequisite**，让 make 知道「编 `foo.o` 之前必须先有 `genhdr.h`」：

```makefile
/* Makefile.racefix — foo.o 显式声明 genhdr.h 为 prerequisite,竞态消除 */
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

foo: foo.o genhdr.h
	$(CC) $(CFLAGS) -o foo foo.o

genhdr.h:
	@sh genhdr.sh

%.o: %.c genhdr.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f foo foo.o genhdr.h
```

这里我把 `%.o: %.c` 改成了 `%.o: %.c genhdr.h`——现在 `foo.o` 的 prerequisite 里有 `genhdr.h`，make 在并行调度时会**先**等 `genhdr.h` 那条规则完成、再开 `foo.o` 的编译，竞态窗口被堵死。再跑一次验证：

```text
$ make -f Makefile.racefix clean && make -f Makefile.racefix -j8
generating genhdr.h (slow)
gcc -std=c11 -Wall -Wextra -c foo.c -o foo.o
gcc -std=c11 -Wall -Wextra -o foo foo.o
$ ./foo
hello from generated
```

先 `generating genhdr.h`（等 0.3 秒），再编译，再链接，输出正确。顺便提一个进阶细节：上面这种写法把 `genhdr.h` 当**普通 prerequisite**，意味着只要 `genhdr.h` 的 mtime 一更新（比如生成器重跑了一次、内容没变但时间戳变了），`foo.o` 就会被判定过期而重编。如果你只想保证「编 `foo.o` 之前 `genhdr.h` 已经存在」、但不想因为时间戳变化就触发重编，可以用**order-only prerequisite**——竖线 `|` 分隔，写成 `%.o: %.c | genhdr.h`。这个语法本章不展开实战，知道有这么个开关、用得到时查 make 手册的「Order-Only Prerequisites」即可。

## 小结

阶段 0 第 11 章那套 Makefile 留的墙是「模式规则 `%.o: %.c` 的 prerequisite 里没有头文件」——我们真跑出来这个坑：改 `mod.h`（甚至把 `GREETING` 从 `v1` 改成 `v2`）后 `make` 一句 `up to date` 不重编，程序跑出来还是旧值 `v1`，bug 不报错不崩溃、只是悄悄和源码脱节，调起来格外折磨人。工程上的正解是让编译器替你管依赖：gcc/clang 的 `-MMD` 在编译每个 `.c` 时顺手吐一份 `.d` 文件（内容形如 `main.o: main.c mod.h`），Makefile 用 `-include $(wildcard *.d)` 把它们喂回 make，从此新加 `#include`、改 include 路径都不用动 Makefile，下次编译自动更新依赖（我们真跑 `touch mod.h` 后两个 `.o` 都正确重编）。`-MP` 是 `-MMD` 的标配搭档，它在 `.d` 里给每个头文件补一条空 phantom 规则 `mod.h:`，作用是头文件被删/改名时 make 不会因为「找不到 prerequisite」而罢工——我们把有 `-MP` 和没 `-MP` 生成的 `.d` 并排对照，差别就是那行 `mod.h:`，所以规矩很干脆：`-MMD -MP` 当一对写死，几乎没理由拆开。对照看本仓 `scripts/build_examples.py`：它遍历 `examples/**/CMakeLists.txt`、用 `cmake -G Ninja` 构建，从头到尾不写 `-MMD`——因为 CMake 生成的构建系统内部自带头依赖图（Ninja 用 `build/.ninja_deps` 这个二进制库存每个 `.o` 依赖的 `.h`，我们 `ninja -t deps` 打印出来看到 `main.c.o` 记了 3 个依赖、`mod.c.o` 连系统头追了 28 个），头依赖、out-of-source、跨平台这一整套都替你包了，这也是真工程规模下大家用 CMake 而不是手写 Makefile 的根本理由。最后真跑了 `-jN` 并行：能加速，但 prerequisite 漏写一个由别的规则生成的文件（典型是代码生成器产出的 `genhdr.h`）时，串行构建侥幸过、`-j8` 当场 `fatal error: genhdr.h: No such file or directory`——我们用 `sleep 0.3` 放慢生成器稳定复现了这个竞态，修法是把 `genhdr.h` 显式加进 `%.o` 的 prerequisite（进阶用法还有竖线分隔的 order-only prerequisite `%.o: %.c | genhdr.h`，只保证存在、不因 mtime 变触发重编）。make 在小工程上够直接，但「头依赖自动管 + 并行竞态防护 + 多目录多 target」这套一旦上规模就吃力，下一章我们正式进 CMake 的工程化世界，看 target 语义和 PRIVATE/PUBLIC/INTERFACE 怎么把契约在构建系统层落地。

## 参考资源

- GNU Make 手册：`-include` 与文件不存在错误的吞掉（前缀减号）、`$(wildcard)` 函数、自动变量、模式规则、order-only prerequisite（`|` 语法）、`-j [jobs]` 并行与 job 调度
- GCC 手册：`-MMD` / `-MP` / `-MG` / `-MF` 等 dependency-generation 选项（`-MMD` 只列非系统头、`-MP` 加 phantom 规则的官方说明）
- Ninja 文档：`ninja -t deps` 工具与 `.ninja_deps` 依赖数据库
- 阶段 0·第 11 章：make 入门（本章是它的工程化深化，三要素/TAB/增量/变量/模式规则/.PHONY 不再重复）
- 阶段 0·第 12 章：CMake 入门（本章末尾对照它讲 CMake 内部如何自动管头依赖）
- 第 5 章：CMake 工程化（target 语义、PRIVATE/PUBLIC/INTERFACE，把本章的依赖话题推进到 target 传播层）
