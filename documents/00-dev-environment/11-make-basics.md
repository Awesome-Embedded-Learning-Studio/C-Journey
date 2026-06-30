---
title: "make 入门：让多文件构建只重编该重编的"
description: "前面十章我们都在敲「gcc 单个文件」，这一章走向真正的工程——多文件、有头文件、改一个文件不想全量重编。从 make 的三要素（目标/依赖/命令）讲起，第一坑就地踩（命令行必须用 TAB 缩进，空格直接 missing separator），真跑一个三文件项目（greet.h/greet.c/main.c）的 Makefile：全量构建、改 greet.c 只重编 greet.o 的增量、改 greet.h 两个 .o 都重编的头文件依赖、make: up to date、make clean。再把变量（CC/CFLAGS，命令行 make CC=clang 覆盖）、= vs := 的展开时机、自动变量（$@ $< $^）、模式规则（%.o: %.c）、.PHONY 伪目标一个个真跑一遍。"
chapter: 0
order: 11
tags:
  - host
  - make
  - build
difficulty: beginner
reading_time_minutes: 14
platform: host
c_standard: [11]
prerequisites:
  - "第 1 章：工具链体检（gcc 基本用法）"
  - "第 2 章：编译四阶段全景（.c → .o → 可执行，-c 停在哪）"
related:
  - "第 12 章：CMake 入门（在 make 之上再封一层）"
  - "第 16 章：GitHub Actions（CI 里调用构建）"
---

> 🟡 状态：待审核（2026-06-30）

# make 入门：让多文件构建只重编该重编的

## 引言：从敲一行 gcc 到管理一堆文件

前面十章，我们编译程序的方式一直是「敲一行 gcc」：`gcc hello.c -o hello`。单文件时这没毛病。但真正的工程不会只有一个文件——你会有 `main.c` 负责流程、`greet.c` 负责某个功能、它们共享一个 `greet.h` 头文件（第 5、6 章讲过 `.o` 和链接）。这时候手动管理编译就难受了：你得先 `gcc -c main.c`、`gcc -c greet.c` 编出两个 `.o`，再 `gcc main.o greet.o -o main` 链接；更烦的是，你只改了 `greet.c` 一个文件，却得把所有 `.o` 重新编一遍——文件一多，全量重编动辄几十秒到几分钟，开发节奏全毁。

`make` 解决的就是这两件事：**它自动算出「谁依赖谁」，然后只重新编译那些真正改动过的文件**（这叫增量编译）。你把「目标—依赖—怎么编」写进一个叫 `Makefile` 的文件里，之后敲一句 `make`，它自己判断该重编什么。这一章我们从零写一个多文件的 Makefile，每一步都真跑。

## Makefile 的三要素，和命令行那个 TAB 坑

一个 Makefile 由一条条「规则」组成，每条规则三个要素，缺一不可：

```makefile
目标: 依赖
	命令
```

`目标`是你要生成的文件（比如 `main` 或 `main.o`），`依赖`是生成它需要的文件（源码、头文件、其他 `.o`），`命令`是具体怎么生成（就是一条 gcc 命令）。make 的核心逻辑就一句话：**如果「目标」不存在、或者比它的「依赖」旧，就执行「命令」重新生成它**——这正是增量编译的依据。

这里有个能让你卡半小时的坑，必须就地讲清楚：**命令行行首必须是一个 TAB 字符，不是空格**。听起来是小事，但这是 make 第一大坑，几乎所有新手都会栽一次。我们故意写一个用空格缩进的 `Makefile.bad`（命令行前面是 4 个空格），看 make 怎么反应：

```text
$ make -f Makefile.bad
Makefile.bad:2: *** missing separator.  Stop.
```

`missing separator`——make 在第 2 行找不到它期望的那个「分隔符」（也就是 TAB），直接罢工。这个报错信息对新手完全不友好（它不说「你用了空格，请换成 TAB」），所以你看到 `missing separator` 第一反应就该是「命令行缩进是不是打成空格了」。后面我们所有 Makefile 的命令行，行首都是 TAB；你在自己机器上照着敲时，务必确认编辑器插入的是 TAB（很多编辑器会把 TAB 自动转成空格，记得关掉这个设置）。

## 第一个 Makefile：一个三文件项目

我们建一个最小但完整的多文件项目。一个头文件 `greet.h` 声明函数，`greet.c` 实现它，`main.c` 调用它：

```c
/* greet.h */
#ifndef GREET_H
#define GREET_H
void greet(const char *name);
#endif
```
```c
/* greet.c */
#include "greet.h"
#include <stdio.h>
void greet(const char *name) {
    printf("hello, %s!\n", name);
}
```
```c
/* main.c */
#include "greet.h"
int main(void) {
    greet("make");
    return 0;
}
```

给它们写一个 Makefile。先看一个把每一步都显式写出来的朴素版本，把三要素看清楚：

```makefile
main: main.o greet.o
	gcc -o main main.o greet.o

main.o: main.c greet.h
	gcc -std=c11 -Wall -Wextra -c main.c

greet.o: greet.c greet.h
	gcc -std=c11 -Wall -Wextra -c greet.c
```

（再次提醒：上面 `gcc` 开头那几行，行首是 **TAB**，不是空格。）读一遍这几条规则：最终目标 `main` 依赖 `main.o` 和 `greet.o`，靠一条链接命令生成；`main.o` 依赖 `main.c` 和 `greet.h`（注意头文件也是依赖，这点等下讲增量时会用到），靠 `gcc -c` 生成；`greet.o` 同理。make 默认从第一条规则的目标（`main`）开始，按依赖关系自动决定先编什么。

现在真跑一下（后面我们用更精炼的写法，但先看这个能不能跑通）：

```text
$ make
gcc -std=c11 -Wall -Wextra -c main.c
gcc -std=c11 -Wall -Wextra -c greet.c
gcc -o main main.o greet.o
$ ./main
hello, make!
```

make 看出 `main` 依赖两个 `.o`，而它们还不存在，于是先把 `main.c`、`greet.c` 各编成 `.o`（第 2 章讲的 `-c` 停在汇编后），最后链接成 `main`。跑一下，输出 `hello, make!`。

## 增量编译：make 的核心价值

全量构建谁都会，make 真正的价值在「第二次以后」。我们先什么都不改，再敲一次 make：

```text
$ make
make: 'main' is up to date.
```

`'main' is up to date`——make 看了一遍，`main` 和所有 `.o` 都比它们的依赖新，啥也不用做。这就是你要的：没改东西，它绝不浪费时间重编。

现在模拟「只改了 `greet.c`」（用 `touch` 把它的修改时间刷新成现在），再 make：

```text
$ touch greet.c && make
gcc -std=c11 -Wall -Wextra -c greet.c    ← 只重编了 greet.o!
gcc -o main main.o greet.o               ← 然后 re-link
```

看清楚发生了什么：**只有 `greet.c` 被重新编译，`main.c` 纹丝不动**，最后重新链接一次。因为 make 发现只有 `greet.c` 比它的产物 `greet.o` 新，`main.c` 没变所以 `main.o` 不用动。在一个有几十上百个源文件的工程里，这个「只重编改过的」能把你每次构建的时间从「全量几分钟」压到「几秒」——这就是 make 值得学的根本理由。

再看一个新手容易漏的：改了**头文件**会怎样？我们 `touch greet.h`：

```text
$ touch greet.h && make
gcc -std=c11 -Wall -Wextra -c main.c     ← 两个 .o 都重编了
gcc -std=c11 -Wall -Wextra -c greet.c
gcc -o main main.o greet.o
```

两个 `.o` **都**重编了。因为我们在规则里写了 `main.o: main.c greet.h` 和 `greet.o: greet.c greet.h`——头文件 `greet.h` 是两者的依赖，它一变，两个都得重编。这正是前面强调「头文件也要写进依赖」的原因：如果你忘了把 `greet.h` 写进 `main.o` 的依赖，改了头文件 make 不会重编 `main.o`，结果你用的还是旧头文件编出来的 `.o`，bug 就这么藏进去了。手动维护这种头文件依赖很容易漏，所以大项目会借助编译器的 `-MMD` 选项自动生成依赖（这个我们点到为止，CMake 那章会再提）。

## 变量：把编译器和旗标抽出来

朴素版 Makefile 里，`gcc -std=c11 -Wall -Wextra` 在每条规则里重复写，改个旗标得改三处。用变量抽出来：

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

main: main.o greet.o
	$(CC) $(CFLAGS) -o main main.o greet.o
```

用 `变量名 = 值` 定义，用 `$(变量名)` 引用。把编译器放进 `CC`、旗标放进 `CFLAGS` 是约定俗成的名字——因为 make 用它们，你可以在命令行直接覆盖。比如想用 clang 编一次试试，不用改 Makefile：

```text
$ make CC=clang
clang -std=c11 -Wall -Wextra -c main.c
clang -std=c11 -Wall -Wextra -c greet.c
clang -o main main.o greet.o
```

`make CC=clang` 把 Makefile 里的 `CC` 临时换成 `clang`——命令行传入的变量优先级最高，会覆盖文件里的定义。这是个很常用的手法：CI 里用同一份 Makefile、靠命令行切编译器；或者临时换个 `CFLAGS=-O2` 跑性能版本。

关于变量的赋值符号，有一个能坑人的细节：`=` 和 `:=` 的**展开时机不同**。`=` 是「递归展开」，变量的值在**被用到的那一刻**才展开；`:=` 是「立即展开」，在**赋值的那一刻**就定死。看一个对照真跑：

```makefile
A = $(B)
B = hello
C := $(D)
D = world
all:
	@echo "A (递归 =) => [$(A)]"
	@echo "C (立即 :=) => [$(C)]"
```

```text
$ make -f Mk-var
A (递归 =) => [hello]
C (立即 :=) => []
```

`A = $(B)`，而 `B` 在 `A` 之后才定义成 `hello`——但因为 `=` 是用到时才展开，`$(A)` 真正被 echo 的那一刻 `B` 早就有了，所以输出 `hello`。反过来 `C := $(D)`，赋值那一刻 `D` 还没定义，`C` 当场就被定成了空字符串，后面 `D = world` 改不动它了，所以 `$(C)` 是空的。日常简单变量用 `=` 就行；但当你引用的变量可能在后面才定义、或者要避免重复展开的开销时，`:=`（「现在就定死」）更可控。另外还有 `?=`（只在没定义时才赋值，常用于给默认值）、`+=`（追加），用到再查。

## 自动变量与模式规则：写一次编所有

朴素版里，每个 `.o` 都单独写一条规则，文件一多就啰嗦。make 提供两个武器消灭这种重复：**自动变量**和**模式规则**。

先看自动变量——make 在执行命令时，会替你填好几个「指代当前规则」的符号：

- `$@`：当前规则的目标名
- `$<`：第一个依赖
- `$^`：所有依赖（去重）

再看模式规则——用 `%` 当通配符，一条规则覆盖所有同类的 `.c → .o`。把两者合起来，整个 Makefile 的编译部分能压成一条：

```makefile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

读法：「任何 `X.o` 都依赖 `X.c`，生成命令是 `$(CC) $(CFLAGS) -c 那个.c -o 那个.o`」。`$<` 展开成触发它的那个 `.c`（比如 `main.c`），`$@` 展开成目标 `main.o`。我们回头看一眼真跑时 make 实际执行的命令，自动变量已经替我们填好了：

```text
gcc -std=c11 -Wall -Wextra -g -c main.c -o main.o      ← $< = main.c,$@ = main.o
gcc -std=c11 -Wall -Wextra -g -c greet.c -o greet.o
gcc -std=c11 -Wall -Wextra -g -o main main.o greet.o   ← $@ = main,$^ = main.o greet.o
```

链接那条 `$(CC) $(CFLAGS) -o $@ $^` 里，`$@` 是 `main`、`$^` 是两个 `.o`。一条模式规则 + 三个自动变量，就把「编每个 .c」和「链接」都写尽了。

顺带一提，make 其实**自带**了一套内置规则（叫隐式规则/自动推导）：你哪怕只写一行 `main: main.o greet.o` 不写任何命令，make 也会自己用 `$(CC) $(CFLAGS) -c` 去编 `.c`、用 `$(CC)` 去链接——因为它内置了「`.c` 怎么变 `.o`」「`.o` 怎么链接成可执行」的默认动作。这也是为什么 `CC`、`CFLAGS` 这两个变量名要按约定起：内置规则认的就是它们。我们上面显式写出来，是为了让你看清每一步；实际项目里很多人靠内置规则省事，但你得知道它默认做了什么，不然出问题没法查。

## `.PHONY` 与 clean

最后讲一个必不可少的规则——`clean`，用来删掉所有编译产物、回到「只有源码」的干净状态：

```makefile
clean:
	rm -f main main.o greet.o

.PHONY: clean
```

```text
$ make clean
rm -f main main.o greet.o
```

`clean` 不是一个要生成的文件，它只是「执行一段命令」的标签——这种目标叫**伪目标**。为什么要写 `.PHONY: clean` 显式声明它是伪目标？想象一下，万一你的项目目录里碰巧有个文件就叫 `clean`（不是不可能），make 看到 `clean` 这个目标时会发现「文件已存在且最新」，于是「up to date」啥也不干，你的清理就失效了。`.PHONY` 告诉 make「`clean` 别管同名文件、永远执行它的命令」，把这个隐患堵掉。养成习惯：`clean`、`all`、`install`、`test` 这类「动作型」目标，统统加进 `.PHONY`。

## 小结

make 用一行行「目标:依赖 + TAB 开头的命令」组成的 Makefile，把多文件构建管理起来，核心价值是**增量编译**——它按「目标比依赖旧才重编」的规则，只重新编译真正改动过的文件（我们真跑验证：改 `greet.c` 只重编 `greet.o`，改头文件 `greet.h` 因为是多个 `.o` 的共同依赖会触发相关 `.o` 全重编，没改动时 `up to date` 一声不干），把全量重编的时间砍到只有改动的那部分。第一坑要记牢：命令行行首必须是 **TAB 不是空格**，否则报 `missing separator`，看到它就先查缩进。变量用 `CC`/`CFLAGS` 约定名，`$(...)` 引用，且能被命令行 `make CC=clang` 覆盖；赋值符号里 `=` 是用到时才递归展开、`:=` 是赋值时立即定死（我们真跑看到 `A=$(B)` 后定义 B 仍能取到值、`C:=$(D)` 在 D 未定义时就定成空）。重复的编译规则用「模式规则 `%.o: %.c`」+ 自动变量 `$@`（目标）、`$<`（第一个依赖）、`$^`（所有依赖）压缩成一条，make 还自带 `.c→.o`、链接的内置规则（认 `CC`/`CFLAGS`）能进一步省事；`clean` 这种「动作型」目标要用 `.PHONY` 声明成伪目标，免得目录里碰巧有同名文件导致它失效。make 在小项目里够用又直接，但项目一大、跨平台需求一来，手写 Makefile 维护头文件依赖和平台分支就开始吃力——下一章我们在 make 之上再封一层，看 CMake 怎么用更声明式的方式管这些。

## 参考资源

- GNU Make 手册：规则的语法、`$@`/`$<`/`$^` 自动变量、`%` 模式规则、`= vs := vs ?= vs +=`、`.PHONY`、内置（隐式）规则与自动推导、`-MMD` 自动依赖生成
- 第 2 章：编译四阶段全景（`-c` 停在汇编后、`.o` 再链接成可执行，对应 Makefile 里每条 `.o` 规则）
- 第 6 章：链接与静态库（`main.o greet.o -o main` 这一步链接到底做了什么）
- 第 12 章：CMake 入门（在 make 之上生成 Makefile，管跨平台和依赖）
