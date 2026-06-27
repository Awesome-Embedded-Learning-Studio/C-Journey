---
title: "符号与链接:读懂 undefined reference,搞懂头文件和库到底是什么"
description: "从「声明 vs 实现」「符号可见性」讲清 C 的符号模型,用真实的 nm 输出和 undefined/multiple definition 报错,把头文件、静态库、动态库、链接顺序这些工程必修课一次打通。"
chapter: 4
order: 2
tags:
  - host
  - build
  - linker
  - toolchain
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0:编译流程、GDB 与库"
  - "Chapter 4:CMake 与模块化工程"
related:
  - "Chapter 0:编译流程、GDB 与库"
---

# 符号与链接:读懂 undefined reference,搞懂头文件和库到底是什么

## 引言

我相信每个写 C 的人都经历过这个让人后背发凉的报错:

```text
undefined reference to `xxx'
collect2: error: ld returned 1 exit status
```

第一反应多半是手忙脚乱问 AI、搜博客。但鲜有人真的想清楚——**为什么会有 `undefined reference`?** 最迷惑的是那种情况:你明明提供了源文件、甚至看到它被链接了,可就是失败。要彻底搞懂它,得回到一个更根本的问题:**我们写的变量和函数,对计算机而言到底意味着什么?编译器工具链是怎么搜集、查找"符号"的?** 这一章就把这条链路(声明 → 符号 → 目标文件 → 链接 → 库)一次打通。所有 `nm`/`gcc` 输出都在本机实测捕获。

## 声明、实现,与符号的可见性

讨论对象是**全局变量和函数**(局部变量不上磁盘,是程序跑起来后 OS/编译器在栈/寄存器上动态安排的,跟链接无关)。对它们,你写的代码本质上在做两件事:

- **声明**:嚷嚷一句"这里存在一个叫 xxx 的东西",但不给值、不给实现,让编译器自己去别处找。
- **实现(定义)**:把声明和具体内容关联起来——全局变量的实现是一份数据,函数的实现是一段代码。**实现本身就包含了声明。**

这里有个关键概念叫**符号可见性**,得分两个时间段看:

- **编译期可见**:没被 `static` 修饰的全局变量和函数,别的 `.c` 文件能访问(外部链接 `external linkage`);加了 `static` 的,只在当前文件可见(内部链接 `internal linkage`)。
- **运行期可见**:不管有没有 `static`,全局变量和函数都伴随程序一生、躺在可执行文件里占空间——`static` 只是限制**谁能访问**,不改变"它存在"这个事实。

来看一段什么花活都有的 demo,把各种符号都摆出来:

```c
// demo.c
int un_g_initialized_var;          /* 未初始化全局 */
int g_initialized_var = 1;         /* 已初始化全局 */
extern int extern_var;             /* 只声明,别处定义 */
static int un_init_local_var;      /* 文件内静态,未初始化 */
static int init_local_var = 1;     /* 文件内静态,已初始化 */
static int local_func(void) { return 1; }  /* 静态函数 */
int func(void) { return 2; }                /* 普通函数 */
extern int extern_func(void);      /* 只声明,别处定义 */
int main(void) { return extern_var + extern_func(); }
```

## 只编译,看目标文件里有什么

`gcc -c` 只编译不链接,得到**可重定位目标文件**(`.o`):

```text
$ gcc -c demo.c -o demo.o
```

编译器在这个阶段干的事:把 C 文本翻成机器码、把全局变量安排好数据——但**允许声明而不实现**(`extern_var`、`extern_func` 这俩没实现,编译器知道,但放你一马,把裁决推给下一步:链接)。所以我们能在 `.o` 里看到"已定义的符号"和"悬而未决的符号(只有名字,等别人填)"。

## nm:把符号表翻出来看

`nm` 工具能把目标文件/可执行文件的符号表打出来。这是上面 `demo.o` 的**真实输出**:

```text
$ nm demo.o
                 U extern_func          ← U = Undefined,等链接时填
                 U extern_var
000000000000000b T func                 ← T = .text 里的全局函数
0000000000000000 D g_initialized_var    ← D = .data 里的全局变量
0000000000000004 d init_local_var       ← d = .data 里的静态变量(小写=文件内)
0000000000000000 t local_func           ← t = .text 里的静态函数
0000000000000016 T main
0000000000000000 B un_g_initialized_var ← B = .bss 里的全局变量
0000000000000004 b un_init_local_var    ← b = .bss 里的静态变量
```

字母大小写有讲究,**大写=外部可见(全局),小写=文件内(static)**:

| 字母 | 段 | 含义 |
|---|---|---|
| `T` / `t` | `.text` | 代码(函数),大写=全局,小写=`static` |
| `D` / `d` | `.data` | 已初始化数据,大写=全局,小写=`static` |
| `B` / `b` | `.bss` | 未初始化数据(运行时清零),大写=全局,小写=`static` |
| `U` | `*UND*` | 未定义——只有名字,等链接器找实现 |
| `W` / `w` | | 弱符号(weak) |

> 记住这条:**未初始化全局变量进 `.bss`(不占磁盘,运行时清零),已初始化进 `.data`**。这是为什么 `un_g_initialized_var` 是 `B`、`g_initialized_var` 是 `D`。

## 链接:解决每一个悬而未决的符号

现在把 `extern_var`/`extern_func` 的实现放到另一个文件:

```c
// demo_extern.c
int extern_var = 10;
int extern_func(void) { return 3; }
```

分别编译再链接:

```text
$ gcc -c demo_extern.c -o demo_extern.o
$ gcc demo_extern.o demo.o -o demo_exe      ← 链接两个 .o
```

链接成功了。再 `nm` 看可执行文件,原来的 `U`(未定义)已经被填成了真实地址:

```text
$ nm demo_exe | grep extern
0000000000001119 T extern_func    ← 不再是 U,有了 .text 里的地址
0000000000004010 D extern_var     ← 不再是 U,有了 .data 里的地址
```

这就是链接器的核心职责:**在每个目标文件之间,把"只有名字的引用"和"真实的定义"配对上。** 配不上的,就是那个让人头秃的报错:

```text
$ gcc demo.o -o demo_bad          ← 只给 demo.o,没给 extern 的实现
/usr/bin/ld: demo.o: in function `main':
demo.c:(.text+0x1b): undefined reference to `extern_func'
collect2: error: ld returned 1 exit status
```

**解法只有一条**:找到那个符号所在的源文件/库,链接时一起提供。这就是所有(非动态库场景下)`undefined reference` 的根本治法。

## 反过来:重复定义(multiple definition)

如果同一个符号在两个文件里都有定义,链接器也没法裁决"到底信谁",于是:

```text
$ gcc demo_extern.o demo_dup.o -o demo_dup_exe   ← 两个 .o 都定义了 extern_func
/usr/bin/ld: demo_dup.o: multiple definition of `extern_func';
             demo_extern.o:demo_extern.c:(.text+0x0): first defined here
collect2: error: ld returned 1 exit status
```

记住一个关键分工:**编译器一次只编译一个文件,它管不了别的源文件;整个程序(可执行文件/库)的符号裁决,是链接器说了算。** 所以"未定义"和"重复定义"都是链接期才报的——编译期它放你过去,链接期才秋后算账。

## 头文件和库,到底是什么

把上面的逻辑推到底,你会"重新发明"两个概念:

- **头文件(`.h`)**:就是一组**符号的声明**——你向编译器担保"这些符号存在,实现在别处"。
- **库**:把一堆可重定位文件(`.o`)打包到一起的集合,链接时按需取用。

所以"写 `#include "xxx.h"` + 链接 `-lxxx`" 这套日常操作,本质就是:**用头文件告诉编译器符号长什么样,用库告诉链接器符号的实现打包在哪。**

### 静态库(`.a` / `.lib`)

`ar` 把多个 `.o` 打成 `libxxx.a`。链接器拿着一张"未决符号表"在库里翻:**找到一个符号,就把包含它的那个 `.o` 整个链接进来**(粒度是 `.o`,不是单个符号)。新拉进来的 `.o` 可能又带来新的未决符号,链接器继续翻,直到全部解决或确认找不到。

> ⚠️ **链接顺序很重要**:链接器**不走回头路**。如果 `b.o` 依赖 `liby`,而 `liby` 里某个 `.o` 又依赖 `libx`,你必须按"被依赖的库放后面"的顺序写(`-lx -ly` 表示 x 依赖 y)。顺序反了就链接失败。这就是为什么 CMake 里 `target_link_libraries` 的顺序、以及"依赖必须层层递进不能循环"这么要紧。

### 动态库 / 共享库(`.so` / `.dll` / `.dylib`)

静态库的缺点:每个可执行文件都塞一份相同代码(`printf` 等被复制千万遍),既占磁盘,库修了 bug 你还得全部重编。

动态库的解法:链接时发现某符号来自 `.so`,**不把实现塞进可执行文件**,只记一张"欠条"(这符号归哪个库)。程序**运行时**,由一个小链接器(`ld.so`)把这些欠条即时兑现、把库映射进来。于是所有程序共享一份 `libc.so`,库升级了换文件即可,不用重编。

和静态库的关键差别在**粒度**:静态库按 `.o` 取用,动态库是**整个库映射进地址空间**。

## 附注:如果你把 C 和 C++ 混用(name mangling)

本仓库是纯 C,但你的 C 代码很可能被 C++ 调用(或反过来)。这时会遇到一个经典坑:C 编译器**不做名称修饰**,`int_max` 就是 `int_max`;而 C++ 为了支持重载/命名空间,会把 `int_max(int,int)` 修饰成 `_Z7int_maxii` 这种。于是 C++ 那边找 `_Z7int_maxii`,C 编译的库里只有 `int_max`——`undefined reference` 又来了。

解法:在 C++ 侧用 `extern "C"` 告诉编译器"这个符号按 C 的方式命名,别修饰":

```cpp
extern "C" int int_max(int a, int b);   // C++ 里声明 C 函数
```

这也是为什么你会在很多 C 库的头文件里看到 `#ifdef __cplusplus extern "C" { ... }`——就是为了能被 C++ 安全包含。

## 小结

把这条链路记牢,`undefined reference` 再也不会让你发怵:

1. **声明 vs 实现**:头文件给声明,库/源文件给实现。
2. **符号可见性**:`static` = 文件内(小写 nm 字母),不加 = 全局(大写);未初始化进 `.bss`,已初始化进 `.data`。
3. **编译器只管单文件、放行未定义;链接器才做全局裁决**——所以未定义/重复定义都是链接期报。
4. **库 = 打包的 `.o` 集合**:静态库按 `.o` 取用、链接顺序敏感;动态库运行时兑现、整库映射。
5. **解 `undefined reference` 的唯一根本办法**:找到符号实现所在的源文件或库,链接时提供。

## 练习

1. 写两个 `.c` 文件:一个定义 `int add(int,int)`,另一个 `main` 调用它(只 `extern` 声明)。先只编译不链接看 `nm`,再链接成功,对比 `add` 符号从 `U` 变 `T`。
2. 给一个全局变量加 `static`,用 `nm` 看它从大写变 小写,体会内部链接。
3. 故意只链接 `main` 所在的 `.o`(不给 `add` 的实现),复现 `undefined reference`;再把 `add` 打成静态库 `libmymath.a`,用 `-L. -lmymath` 链接成功。
4. 制造一次 `multiple definition`,读懂报错里 "first defined here" 指向哪里。
5. 用 `ar -t libmymath.a` 看静态库里有哪些 `.o`,理解"按 `.o` 取用"的粒度。

## 参考资源

- [Beginner's Guide to Linkers](https://www.lurklurk.org/linkers/linkers.html) —— 本章链接顺序例子的来源,强烈推荐。
- `man nm` / `man ld` —— 符号表与链接器手册。
- *程序员的自我修养——链接、装载与库*(俞甲子 等)—— 中文里讲链接/装载最透彻的一本。

---
*整理自作者《深入理解 C/C++ 的编译与链接技术》笔记,按 C-Journey 写作规范重写;所有 nm/gcc 输出在本机实测捕获。*
