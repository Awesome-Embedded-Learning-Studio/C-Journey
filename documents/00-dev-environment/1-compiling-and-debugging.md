---
title: "编译流程、GDB 与库：把 .c 变成可执行文件的每一步"
description: "拆开 gcc 的黑盒，看清预处理/编译/汇编/链接四阶段；用 GDB 一眼定位空指针崩溃；搞懂静态库与动态库的差别与 dlopen 玩法。"
chapter: 0
order: 1
tags:
  - host
  - toolchain
  - gdb
  - build
difficulty: beginner
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "Git 基础操作"
  - "命令行基础"
related:
  - "CMake 与模块化工程"
  - "Socket 编程（阶段 5）"
---

# 编译流程、GDB 与库：把 .c 变成可执行文件的每一步

## 引言

新手写 C 的第一行命令大概率是 `gcc main.c`，然后收获一个 `a.out`，跑起来能出结果，于是心满意足地继续写下一行。说实话，我当年也是这么过来的——直到有一天程序崩了，报了个 `Segmentation fault`，我盯着这行字完全不知道该往哪看。后来我才意识到，**`gcc main.c` 这一行命令其实是个大黑盒，里面塞了四个完全不同的阶段**，而调试、链接库、看汇编这些活儿，全都要建立在你先把这四个阶段拆开看清楚的基础上。

所以这一章我们要做的就是把这个黑盒撬开。配套示例在 [examples/stage0-compiling-and-debug](../../examples/stage0-compiling-and-debug/)，里面分了三块：`1/` 讲编译流程、`gdb_use/` 讲调试、`2/` 讲库。我们挨个走，而且每一步都会**真的跑一遍**，把产物摆出来看，不靠想象。

## 核心概念：编译其实是四个阶段

很多人以为"编译"就是 `.c` 变成可执行文件这一步，其实严格意义上的"编译"只是其中一环。一个 `.c` 文件走到可执行，要经过四个阶段，每个阶段都对应一个独立的产物，gcc 也给每个阶段留了单独的开关让我们能"半路停下来看一眼"：

```text
hello.c ──[预处理]──▶ hello.i ──[编译]──▶ hello.s ──[汇编]──▶ hello.o ──[链接]──▶ 可执行文件
              gcc -E            gcc -S           gcc -c            ld / gcc
```

为什么要分这么细？因为这四步干的事完全不同：预处理只管文本替换（展开宏、塞头文件、处理 `#ifdef`），编译把 C 翻译成汇编，汇编把翻译成机器码的目标文件，链接再把多个目标文件和库拼到一起、填好地址。**把它们分开，你才能在出问题的时候知道是哪一环的锅**——是宏展开错了，还是链接找不到符号，这俩的排查方向完全不一样。

## 一步步拆开 gcc

我们拿 [1/function.c](../../examples/stage0-compiling-and-debug/1/function.c) 这套多文件工程当靶子。它有个条件编译的宏：

```c
// function.h
#define FIRST_OPTION
#ifdef FIRST_OPTION
#define MULTIPLIER (3.0)
#else
#define MULTIPLIER (2.0)
#endif
```

### 预处理：gcc -E，看看宏到底展开了啥

`gcc -E` 让 gcc 跑完预处理就停下，把结果吐到stdout。我们来看看 `MULTIPLIER` 这个宏被换成了什么：

```text
$ gcc -E 1/function.c | tail -6
float add_and_multiply(float x, float y)
{
 float z = add(x, y);
 z *= (3.0);        ← 宏 MULTIPLIER 已经变成了 (3.0)
 return z;
}
```

你看，`MULTIPLIER` 在预处理阶段就被原样替换成了 `(3.0)`，源码里的宏到这一步已经不存在了。这个能力特别有用——当你的宏死活不生效、或者条件编译没按预期走的时候，`gcc -E` 一跑，真相立现。

### 汇编：gcc -S，看看编译器把 C 翻成了什么

再往下走一步，`gcc -S` 把 C 编译成汇编语言（AT&T 语法），存成 `.s` 文件：

```text
$ gcc -S 1/function.c -o function.s && head function.s
	.file	"function.c"
	.text
	.globl	nCompletionStatus
	.bss                ← 注意这个段
	.align 4
	.type	nCompletionStatus, @object
	.size	nCompletionStatus, 4
nCompletionStatus:
	.zero	4
```

这里有个值得停下来说的点：那个没初始化的全局变量 `nCompletionStatus`，被放进了 `.bss` 段而不是 `.data` 段。**为什么？因为 `.bss` 段不占可执行文件的实际空间**——它的值全都是 0，没必要存进去，加载时操作系统直接分配一坨清零的内存就行。所以你看 `nCompletionStatus` 后面跟的是 `.zero 4`（4 字节全零），而不是一串初始值。这就是为什么一个声明了几 MB 全局数组的程序，可执行文件本身却没那么大。

### 目标文件与链接：多个 .c 怎么拼起来

`gcc -c` 生成目标文件 `.o`（机器码，但还不能跑，因为地址没填），最后一步链接把 `function.o` 和 `main.o` 拼成可执行文件。`main.c` 里用 `extern` 引用了 `function.c` 的全局变量 `nCompletionStatus`，又调用了 `add_and_multiply`——这些跨文件的引用，就是在链接这一步被"对上号"的：

```bash
gcc -c function.c main.c      # 各自生成 .o
gcc function.o main.o -o demo # 链接成可执行
```

链接找不到符号的时候，你会看到 `undefined reference to 'xxx'` 这种经典报错——这就是链接阶段的锅，不是编译阶段的，别去 `.c` 文件里瞎找。

## GDB：崩溃了别慌，先 bt

编译搞定了，但程序跑起来崩了怎么办？[gdb_use/error.c](../../examples/stage0-compiling-and-debug/gdb_use/error.c) 就是一个故意写崩的例子，它干了一件最经典的事——解引用空指针：

```c
#define NULL ((void*)0)
int main()
{
    char* p = NULL;
    *p = 123;        // 往地址 0 写东西，必崩
    return -1;
}
```

先用 `-g` 把调试信息编进去（这一步千万别省，不然 gdb 看不到源码行号），然后让 gdb 接管它：

```text
$ gcc -g gdb_use/error.c -o err
$ gdb ./err
(gdb) run
Program received signal SIGSEGV, Segmentation fault.
0x0000555555555129 in main () at gdb_use/error.c:5
5		*p = 123;
(gdb) bt
#0  0x0000555555555129 in main () at gdb_use/error.c:5
```

看到没有，gdb 直接告诉你崩溃发生在 `error.c` 的第 5 行，就是 `*p = 123` 这一句。一个段错误，从"两眼一黑"到"精确定位到行"，就靠 `run` 加一个 `bt`（backtrace，调用栈回溯）。这就是 GDB 最日常、也最救命的一个用法。

> **踩坑预警**：我第一次编译这个目录的时候，顺手敲了 `gcc -g gdb_use/*.c -o demo`，结果 linker 直接报 `multiple definition of 'main'`。原因很简单——`error.c` 和 `main.c` **各自都有一个 `main` 函数**，它们是两个独立的练习程序，不是一套工程。所以这个目录下的程序得**一个一个单独编译**，不能用 `*.c` 一锅端。这种"多个 main"的坑在拼装旧代码时特别常见，看到 `multiple definition of 'main'` 第一反应就该是：是不是把几个独立程序混到一块儿编译了。

## 库：静态库 vs 动态库

写到这一步，你的程序还都是"所有代码塞一起"的形态。但真实工程里，我们会把一堆通用函数打包成**库**，让别人（或者将来的自己）拿来就用。库分两种，差别很关键。

**静态库（`.a`）** 是在**链接时**就把用到的代码复制一份进你的可执行文件。好处是产物独立、拷到哪都能跑；代价是体积大，而且库更新了你得重新编译所有用到它的程序。用 `ar` 把一堆 `.o` 打包：

```bash
gcc -c src.c
ar rcs libsrc.a src.o          # 打包成静态库
gcc main.c -L. -lsrc -o demo   # 链接时用 -l 指定
```

**动态库（`.so`）** 则是**运行时**才去加载，可执行文件里只留一个引用，不复制代码。好处是省空间、能被多个程序共享、更新库不用重编译程序；代价是运行时得找得到这个 `.so`。我们用 [2/](../../examples/stage0-compiling-and-debug/2/) 这套来真跑一遍，先生成库，再用 `dlopen` 在运行时加载它：

```bash
# 1. 把 src.c 编译成动态库（-fPIC 是关键，下面解释）
gcc -shared -fPIC -o libsrc.so 2/src.c
# 2. dyuseLib.c 用 dlopen 在运行时加载它
gcc 2/dyuseLib.c -o dyuse -ldl
./dyuse
```

实跑输出如下，`dlopen` 打开了 `libsrc.so`，`dlsym` 拿到了 `add` 和 `printMsg` 两个符号，然后真的调了起来：

```text
Load symbols all success
1 + 2 = 3
```

## 常见坑（真正的坑在后面）

> **坑 1：`multiple definition of 'main'`。** 上面提过了，多个独立程序混在一起编译就会撞。看到这个报错，先确认你编译的是不是"一套工程"。

> **坑 2：动态库不加 `-fPIC` 直接报错。** `-fPIC` 是 Position Independent Code（位置无关代码），动态库因为要被加载到不确定的地址，必须编成位置无关的，否则链接器会拒绝。生成 `.so` 的时候这俩参数 `-shared -fPIC` 基本绑死，别只写一半。

> **坑 3：`dlopen("./libsrc.so")` 找不到文件。** [dyuseLib.c](../../examples/stage0-compiling-and-debug/2/dyuseLib.c) 里写的是相对路径，所以**运行时你的工作目录里必须有这个 `libsrc.so`**。换个工作目录跑就 `Load failed`。真实工程里要么用绝对路径、要么装到系统库目录、要么设 `LD_LIBRARY_PATH`。

> **坑 4：`dlopen` 编译报 `undefined reference to dlopen`。** 因为 `dlopen` 这一族函数在 `libdl` 里，编译时得在末尾加 `-ldl`。注意是**末尾**——gcc 的链接顺序是从左往右找符号，`-ldl` 写在源文件前面会找不到。

## 小结

把黑盒撬开之后，这几件事应该清楚了：

- [ ] 编译有四阶段：预处理(`-E`)→编译(`-S`)→汇编(`-c`)→链接
- [ ] `gcc -E` 看宏展开、`gcc -S` 看汇编，是排查预处理/编译问题的利器
- [ ] 未初始化全局变量进 `.bss` 段，不占文件空间
- [ ] 崩溃了先 `gcc -g` 带调试信息，再用 gdb `run` + `bt` 定位
- [ ] 静态库链接时复制，动态库运行时加载（`-shared -fPIC` + `dlopen`/`dlsym`）

## 练习

- [ ] 用 `gcc -E` 看看 `#include <stdio.h>` 之后你的源文件膨胀成了多少行（会吓你一跳）
- [ ] 给 `gdb_use/main.c` 下个断点，单步走一遍，观察 `arr` 和指针 `p` 的变化
- [ ] 把 `2/src.c` 也打包成静态库 `libsrc.a`，写个程序链接它，对比和动态库用法的区别

## 参考资源

- `man 1 gcc`——gcc 的选项多到吓人，但 `-E/-S/-c` 这几个是基石
- `man 1 gdb`——调试器入口，`run/bt/break/next/print` 是五件套
- Ian Lance Taylor 的《Linkers and Loaders》系列文章——想搞懂链接到底干了啥的经典读物
- Brendan Gregg 的 gdb 速查表——排查崩溃时的实战参考

---
*配套示例整理自 2023–2024 学习存档。*
