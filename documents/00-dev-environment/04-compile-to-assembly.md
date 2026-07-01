---
title: "编译阶段看汇编：读懂 .text/.data/.bss 与 x86-64 调用约定"
description: "第 2 章我们拿到一个 28 行的 .s 文件——这一章把它读懂。用 gcc -S 看清 C 怎么变成汇编：全局变量怎么落到 .data/.bss、字符串字面量怎么进只读的 .rodata、函数参数怎么按 x86-64 SysV ABI 走 rdi…r9 寄存器、以及 -O2 怎么把局部变量整条吃掉（gdb 里只剩 optimized out）。全程真编译真贴，并划清『ISO C 规定的』和『ABI/实现层规定的』这条线。"
chapter: 0
order: 4
tags:
  - host
  - toolchain
  - asm
difficulty: intermediate
reading_time_minutes: 18
platform: host
c_standard: [11]
prerequisites:
  - "第 2 章：编译四阶段全景（-save-temps）"
related:
  - "第 5 章：目标文件与符号"
  - "第 9 章：标准与优化：-std/-O/-g"
---

# 编译阶段看汇编：读懂 .text/.data/.bss 与 x86-64 调用约定

## 引言：那个 28 行的 .s 里到底写了什么

第 2 章我们用 `-save-temps` 拿到一个 `hello.s`，它只有 28 行，当时我们没细看就跳过了。这章要把它读懂。你可能觉得「我又不写汇编，看这玩意干嘛」——恰恰相反，**你不需要会写汇编，但你需要会读那么几行**。因为往后整本书里，只要一碰到「这个变量为什么没了」「这段代码为什么被优化坏了」「这个崩溃到底崩在哪条指令」「参数到底传进去没有」，最终能给你一个铁证的，往往就是这几行汇编。

这一章我们盯住编译阶段的产物 `.s`，搞清三件事：第一，你的全局变量和字符串在汇编里落到了哪几个「段」（section）；第二，函数参数是怎么传的（这叫调用约定，也就是 ABI）；第三，`-O` 优化级别到底对你的代码做了什么。每一步我们都真编译、真贴输出。

在动刀之前，先把一条**最容易混淆的线**划清楚，它会贯穿全章：

> C 标准（ISO/IEC 9899）只规定了语言的**语义**，对「段」「寄存器」「栈帧布局」**只字未提**。`.text/.data/.bss` 是 **ELF 文件格式 + 链接器/加载器**的约定；参数走 `rdi` 还是栈，是 **System V AMD64 ABI**（Linux/macOS 上 gcc、clang 共同遵循的 psABI）的规定。这些都是 **implementation-defined / 平台 ABI** 的现实，不是 C 语言的规矩。本章讲的是「我这台 x86-64 Linux 机器上的现实」，换到 ARM、换到 Windows MSVC，细节会变——但**读懂汇编、区分标准与 ABI**这套方法不变。

## 先看一张全局图：程序在内存里是分段的

很多人以为一个程序就是「一坨代码 + 一坨数据」。汇编会直接打你脸：它是**分好几段**的，每段的内存**权限**不一样。先认四个最常见的：

| 段 | 放什么 | 权限 | 典型内容 |
|---|---|---|---|
| `.text` | 代码 | 读 + **执行**（不可写） | 编译出的机器指令 |
| `.rodata` | 只读数据 | 读（不可写不可执行） | 字符串字面量、`const` 常量 |
| `.data` | 已初始化的可写全局 | 读 + 写 | `int x = 42;` 这种 |
| `.bss` | 零初始化的可写全局 | 读 + 写 | `int x;`（没给初值的） |

为什么要分段？因为操作系统加载程序时，会按段给内存开**不同的权限**：代码段给你执行权但不许改（防止程序运行时篡改自己的指令），只读数据段干脆连写都不许。这个权限设计，正是后面「写字符串字面量会段错误」的根因。

光说不练假把式，我们编一段故意凑齐这四种落点的代码：

```c
#include <stdio.h>

int init_global = 42;          /* 进 .data */
int zero_global;               /* 进 .bss(零初始化,不占文件空间) */
const char* msg = "in rodata"; /* 指针进 .data,字符串字面量进 .rodata */

__attribute__((noinline)) int sum6(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

int main(void) {
    int local = init_global + zero_global;
    printf("sum6=%d local=%d msg=%s\n", sum6(1, 2, 3, 4, 5, 6, 7), local, msg);
    return 0;
}
```

先 `gcc -S` 拿到汇编，顺手用 `size` 看一眼目标文件里 `.text/.data/.bss` 各占多少字节：

```text
$ gcc -std=c11 -O0 -S layout.c -o layout.s
$ gcc -std=c11 -O0 -c layout.c -o layout.o
$ gcc -std=c11 -O0 layout.o -o layout && ./layout
sum6=28 local=42 msg=in rodata

$ size layout.o
   text	   data	    bss	    dec	    hex	filename
    352	     12	      4	    368	    170	layout.o
```

`.data` 是 12 字节、`.bss` 是 4 字节——我们先记住这两个数，等下对着汇编一一验证。现在把汇编里**段声明和数据落点**这几行抠出来看：

```text
$ grep -nE '^\s*\.(section|data|bss|text|rodata|zero|long|quad|string)' layout.s
2:  .text
4:  .data
6:      .type  init_global, @object
9:      .long  42                    ← init_global:已初始化,落 .data
11: .bss
13:     .type  zero_global, @object
16:     .zero  4                     ← zero_global:零初始化,落 .bss
18: .section  .rodata
20:     .string "in rodata"          ← 字符串字面量,落只读的 .rodata
21: .section  .data.rel.local,"aw"
26:     .quad  .LC0                  ← msg 指针本身:要重定位,落 .data 的子段
63: .section  .rodata
65:     .string "sum6=%d local=%d msg=%s\n"   ← printf 的格式串,也是 .rodata
```

挨个对：

- `init_global = 42` → `.data` 段里一条 `.long 42`（4 字节）。✅
- `zero_global`（没给初值）→ `.bss` 段里一条 `.zero 4`（4 字节）。✅ 这条特别值得停一下：`.bss` 的意思是「这里留 4 个字节的零」，**它在目标文件里只记了「我要 4 字节」，并没有真写 4 个零进去**——所以 `.bss` 不占文件空间，程序加载时操作系统才在内存里给它清零。这就是为什么有人发现「可执行文件比你想象的还小」。
- 字符串字面量 `"in rodata"` 和 `"sum6=%d..."` → 都进了 `.section .rodata`，`.string` 伪指令。✅
- `msg` 这个**指针**本身（注意，是那个 8 字节的指针变量，不是它指向的字符串）→ 落到了一个叫 `.data.rel.local` 的段。这是 `.data` 的一个子段（`size` 也把它算进 `.data` 那 12 字节里），因为指针的值要在链接时**重定位**（填上 `.LC0` 那个字符串的真实地址）。于是 `.data` 的 12 字节 = `init_global`(4) + `msg` 指针(8)，对上了。

你看，**一个变量落哪个段，取决于它「初始化了没」和「是不是只读」**，跟它的类型关系倒不大。这套直觉后面看可执行文件体积、查段错误都直接用得上。

这里有个反直觉的地方：别以为 `const` 修饰的全局就一定进 `.rodata`。`const` 是写给编译器看的「语义约束」，**一个 `const` 变量最终进哪段，取决于它有没有被初始化、有没有被取地址、链接器怎么摆**——很多 `const` 全局实际落在 `.data` 或 `.rodata` 的子段里。判断它到底在哪，靠 `readelf -S` / `objdump`，不靠猜。这章我们只摆事实，`const` 的精细语义留到第 2 编讲限定符时再展开。

## 函数参数怎么传：x86-64 SysV 调用约定

现在看第二个问题：`sum6` 收了 7 个 `int` 参数，它们是怎么从 `main` 传进 `sum6` 的？看 `sum6` 的函数体（AT&T 语法，寄存器带 `%`、立即数带 `$`、操作数顺序是 `源, 目的`）：

```text
sum6:
    pushq  %rbp
    movq   %rsp, %rbp          ← 函数序言:建立栈帧
    movl   %edi, -4(%rbp)      ← a  来自 %edi
    movl   %esi, -8(%rbp)      ← b  来自 %esi
    movl   %edx, -12(%rbp)     ← c  来自 %edx
    movl   %ecx, -16(%rbp)     ← d  来自 %ecx
    movl   %r8d, -20(%rbp)     ← e  来自 %r8d
    movl   %r9d, -24(%rbp)     ← f  来自 %r9d
    ...                          (把 a~f 加起来,中间结果放 %edx)
    movl   16(%rbp), %eax      ← g  从栈上取!
    addl   %edx, %eax
```

（`-O0` 下 gcc 老实地把每个寄存器参数先倒进栈帧里存着，再慢慢加，所以看起来啰嗦——重点不在它怎么加，在**前 6 个参数用寄存器、第 7 个参数用栈**这件事。）

读出来就是 **x86-64 System V ABI** 的规矩：整型/指针参数**前 6 个**依次走寄存器 `rdi、rsi、rdx、rcx、r8、r9`（这里参数是 `int`，所以用它们 32 位的低位 `edi、esi、edx、ecx、r8d、r9d`）；**从第 7 个参数起**，多余的走**栈**。你看 `g` 就是 `movl 16(%rbp), %eax`——从栈帧偏移 `+16` 处取出来的。

为什么是 `16(%rbp)`？画一下进入 `sum6` 那一刻的栈（地址往下增长）：

```text
高地址
  [ 第 7 个参数 g ]   ← rbp + 16
  [ 返回地址        ] ← rbp + 8   (call 指令自动压入)
  [ 调用者保存的 rbp ] ← rbp       (pushq %rbp 压入)
低地址  ← rsp
```

所以 `g` 在 `rbp+16`，返回地址在 `rbp+8`。这套「前 6 走寄存器、第 7 起走栈、栈帧长这样」就是 **ABI**。再说一遍那个关键区分：**`rdi` 这些名字、参数走哪个寄存器、栈帧怎么排，C 标准一个字都没规定**——这是 x86-64 Linux/macOS 上大家共同遵守的 System V psABI。你换到 32 位 x86、换到 ARM、换到 Windows 的 `__fastcall`，规则全变；但「函数靠一套调用约定传参」这件事不变。读汇编时记住前 6 个寄存器，你就能判断「参数到底传进函数没有」。

> 浮点参数走的是另一套寄存器 `xmm0..xmm7`，结构体、变长参数还有更细的规则——本章点到「整型的寄存器 + 栈」为止，够你看懂后续章节的汇编了。完整规则见 System V AMD64 ABI psABI 文档。

## `-O` 优化级别：局部变量为什么「消失」了

最后看一个会让你调试时血压拉满的现象。写个简单函数，两个局部变量：

```c
__attribute__((noinline)) int compute(int x) {
    int a = x * 3;
    int b = a + 1;
    return b;
}
```

先在 `-O0`（不优化，默认就是它）下看汇编：

```text
$ gcc -std=c11 -O0 -S opt.c -o opt_O0.s   (然后看 compute 函数体)
compute:
    pushq  %rbp
    movq   %rsp, %rbp
    movl   %edi, -20(%rbp)    ← x  存到栈上 -20
    movl   -20(%rbp), %edx
    movl   %edx, %eax
    addl   %eax, %eax
    addl   %edx, %eax         ← 算出 a = x*3
    movl   %eax, -8(%rbp)     ← a  存到栈上 -8
    movl   -8(%rbp), %eax
    addl   $1, %eax           ← b = a+1
    movl   %eax, -4(%rbp)     ← b  存到栈上 -4
```

在 `-O0` 下，`x`、`a`、`b` **三个局部变量老老实实全在栈帧里**（`-20(%rbp)`、`-8(%rbp)`、`-4(%rbp)`），每算一步就写回内存。这又慢又啰嗦，但有个天大的好处：**每个变量都在栈上有一个稳定的家，gdb 能停下来、能 `print a` 看到它的值**。这也是为什么我们调试时一律用 `-O0 -g`。

现在把优化开到 `-O2`，再看同一个函数：

```text
$ gcc -std=c11 -O2 -S opt.c -o opt_O2.s
compute:
    leal  1(%rdi,%rdi,2), %eax     ← 整个函数就剩这一条
    ret
```

`a` 和 `b` **整个消失了**。`leal 1(%rdi,%rid,2)` 一条指令就算出了 `rdi*3+1`（`%rdi,%rdi,2` 表示 `rdi + rdi*2 = 3*rdi`，再加 `1`），结果直接放进返回值寄存器 `%eax`，然后 `ret`。整个 `.text` 段从 `-O0` 的 185 字节缩到 `-O2` 的 127 字节（`compute` 这部分从十几条指令塌成两条）。

变量去哪了？被优化器**吃掉了**——因为它判定 `a`、`b` 只是中间结果，不影响程序的「可观察行为」。这里有个重要的标准概念：**ISO C 的 as-if 规则**（程序执行 / 可观察行为，见 ISO/IEC 9899 §5.1.2.3）——只要最终的可观察行为（输入输出、volatile 访问、系统调用等）一致，编译器**爱怎么改就怎么改**你的代码，包括删掉变量、改写顺序、提前算好。`a`、`b` 不影响可观察行为，所以合法地没了。

代价是调试：你在 `-O2` 下用 gdb 调这个函数，`print a` 大概率收获一句 `<optimized out>`，断点停的行号也会乱跳（因为代码已经被重排、合并）。**这就是为什么本课程调试一律回到 `-O0 -g`，做性能基准才上 `-O2`。**

> 上面是书里真跑的 `-O2` 汇编。想自己改 C 代码、当场看它变成什么汇编？试试这个(默认 `-O2`,点「看 x86-64 汇编」就行;改改算式或优化级别,看 `lea`/`mov`/`ret` 怎么变):

<OnlineCompilerDemo
  title="亲手玩:改 C 代码,看它编成什么 x86-64 汇编"
  description="这就是上面那段 compute 函数。-O2 下被塌成一条 lea。改改算式(比如 *5、+7)、或把 -O2 改成 -O0 看局部变量怎么回到栈上,体会 as-if 规则和优化的关系。"
  allow-run="true"
  allow-x86-asm="true"
  run-options="-std=c11 -O0 -g"
  x86-options="-std=c11 -O2"
  sourcePath="/demos/compute_asm.c"
/>

这里得提醒一句：`-O2` 不只是「让程序变快」，它会**改变可观察行为的边界**——尤其碰到未定义行为（UB）的时候。一段依赖 UB 的代码（比如有符号溢出、越界），在 `-O0` 下可能「看起来正常」，一上 `-O2`，优化器「假定它不会 UB」就直接把你的溢出检查整段删掉，输出变得完全错误。这正是后面为什么要上 sanitizer 的根本原因，第 10 章会专讲。

## 最阴的一个坑：写字符串字面量，C 连警告都不给

讲完段布局，回头看一个经典崩溃。下面这段代码，你觉得会发生什么？

```c
#include <stdio.h>

int main(void) {
    char* s = "hello"; /* "hello" 落在 .rodata(只读) */
    s[0] = 'H';        /* 写只读段 -> ? */
    printf("%s\n", s);
    return 0;
}
```

先编译——注意，我**开了 `-Wall -Wextra`**：

```text
$ gcc -std=c11 -Wall -Wextra ro_seg.c -o ro_seg
$                       ← 一片寂静,一个警告都没有!
$ ./ro_seg ; echo "退出码=$?"
退出码=139              ← 128 + 11 = SIGSEGV,段错误
```

**编译期静默，运行期直接段错误。** 这比你想的狠——很多人以为「这种事编译器总会提醒我一句吧」，但在 C 里它**不会**。原因有二：

第一，字符串字面量 `"hello"` 落在 `.rodata` 只读段，`char *s = "hello"` 让 `s` 指向那块只读内存，`s[0] = 'H'` 试图写它，操作系统直接给你一个 `SIGSEGV`。汇编能佐证它确实在只读段：

```text
$ gcc -std=c11 -S ro_seg.c -o ro_seg.s
$ grep -nE '\.rodata|\.string' ro_seg.s
3:  .section  .rodata
5:      .string "hello"        ← 字面量在只读段,铁证
```

第二，**C 标准下字符串字面量的类型本来就是 `char[]`（不是 `const char[]`）**，所以 `char *s = "hello"` 在 C 里完全合法、连 `-Wall` 都不吭声；只是**试图修改它的行为是未定义的**（ISO/IEC 9899 §6.4.5 字符串字面量：修改字符串字面量数组的行为未定义）。C++ 在这点上比 C 严（C++ 里字面量是 `const char[]`，赋给 `char*` 直接编译报错），C 则把锅全甩给运行期。

那想让编译器在这件事上提醒你一句呢？得手动加 `-Wwrite-strings`（它不在 `-Wall`/`-Wextra` 里）——这个开关会把字符串字面量当成 `const char[]`，于是 `char *s = "hello"` 立刻报警。所以本课程后续示例里，凡是不该被改的字符串，我们都老老实实写 `const char *s`，从源头堵住这个雷。

## 小结

到这，编译阶段产出的 `.s` 你就能读个大概了。最核心的一张图：程序是分段的——`.text` 放可执行的代码、`.rodata` 放只读数据、`.data` 放已初始化的可写全局、`.bss` 放零初始化的全局（它不占文件空间），变量落哪段看「初始化了没」「只读不」，用 `size`/`readelf -S`/`objdump` 验、不靠猜。再强调一次那条分界线：段布局和寄存器都是 ABI、不是 C 标准，ISO C 对 `.text/.data`、对 `rdi` 只字未提，这是 ELF 加 System V AMD64 ABI 的现实，换平台就会变；具体到 x86-64 SysV，整型参数前 6 个走 `rdi/rsi/rdx/rcx/r8/r9`、第 7 个起走栈，栈帧里 `rbp+8` 是返回地址。优化层面记住 `-O0` 下局部变量全在栈上（好调试），`-O2` 会按 as-if 规则（§5.1.2.3）吃掉不影响可观察行为的变量（gdb 里就是 `<optimized out>`），所以调试一律 `-O0 -g`、做基准才上 `-O2`。最后那个最阴的：字符串字面量不可改（§6.4.5），写它就是 UB，在 `.rodata` 上表现为 `SIGSEGV`，而 C 默认连警告都不给，得 `-Wwrite-strings` 才提醒你。

下一章我们再往下一层，用 `nm`/`objdump`/`readelf` 拆开 `.o` 这个目标文件，看里面的符号表和重定位——那一步会解释清楚 `msg` 指针那个 `.quad .LC0` 到底是怎么被链接器填上真实地址的。

## 参考资源

- System V AMD64 ABI psABI（x86-64 函数调用约定、寄存器用法的权威来源）
- DWARF / ELF 格式规范（`.text/.data/.bss/.rodata` 的文件层定义）
- ISO/IEC 9899:2011 §5.1.2.3（程序执行 / 可观察行为，as-if 规则）、§6.4.5（字符串字面量不可修改）
- GCC 手册：`-O` 各级别、`-Wwrite-strings`、`-masm=intel`
