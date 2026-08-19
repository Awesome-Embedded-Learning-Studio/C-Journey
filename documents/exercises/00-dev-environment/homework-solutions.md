---
title: "阶段 0 课后练习参考答案（Homework）"
description: "阶段 0（开发环境与编译）课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出（gcc 16 / clang 22 / WSL Arch 实跑）。"
chapter: 0
order: 1
tags:
  - host
  - toolchain
  - build
difficulty: beginner
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 课后练习（Homework）"
related:
  - "阶段 0 各章"
---

# 阶段 0 课后练习参考答案（Homework）

> 所有命令与输出都在 WSL Arch（gcc 16.1.1 + clang 22.1.8）下真实运行得到。地址类信息（ASLR 地址、BuildId）每次运行不同，不影响结论。

## 0.1-A {#hw-0-1-a}

**难度 L1** · 题面见 [homework](homework#hw-0-1-a)

**思路**：两道命令分别回答「路径」和「目标平台」两个问题。

1. `which gcc` 告诉你在 shell 里敲 `gcc` 时，PATH 环境变量把它解析到了哪个二进制文件。这一步的意义在于：IDE 的「gcc」和命令行敲的「gcc」可能是不同路径的二进制，排查「本地过、CI 红」第一步就是对齐这个。→ 知识点：[第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)「还有两个小坑」一节（`which gcc` 与 CI 的 `$CC` 不一定一致）
2. `gcc -dumpmachine` 输出编译器产码的目标三元组（target triple）——`架构-厂商-系统-ABI`。它回答的是「这台 gcc 给哪种 CPU/系统产代码」，后面读汇编、讲调用约定（参数走哪些寄存器）全依赖这个信息。→ 知识点：[第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)「第一步：把版本都打一遍」一节

**验证输出**：

```text
$ which gcc clang
/usr/sbin/gcc
/usr/sbin/clang
$ gcc -dumpmachine
x86_64-pc-linux-gnu
$ clang -dumpmachine
x86_64-pc-linux-gnu
```

两个编译器目标三元组一致（`x86_64-pc-linux-gnu`），说明它们给同一类目标产码——但这不代表行为一致，第 1 章那个「默认 `-std` 不一样」的坑还在等着。

## 0.1-B {#hw-0-1-b}

**难度 L2** · 题面见 [homework](homework#hw-0-1-b)

**思路**：`bool`/`true` 是 C23 才收编的原生关键字，C11/C17 里要靠 `<stdbool.h>`。同事「本地 gcc 能过」是因为没钉 `-std`，吃到了 gcc 16 的默认 `gnu23`；CI 的 clang 默认 `gnu17`，`bool` 自然不认识。

1. 写最小探针（注意**不** `#include <stdbool.h>`，这就是同事代码的样子）并逐组合实验。→ 知识点：[第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)「C23：`bool`/`true` 终于成了关键字」一节
2. 观察 gcc 默认与 clang 默认的差异，本质是默认 `-std` 档位不同。→ 知识点：[第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)「真正的第一大坑」一节（gcc 16 默认 gnu23、clang 22 默认 gnu17）
3. 修复：显式钉 `-std=c23`（两边都支持 `bool` 关键字），或钉 `-std=c11` 并 `#include <stdbool.h>`（最稳、兼容性最好）。

**验证输出**：

```text
$ gcc boolprobe.c -o b_gcc_default && ./b_gcc_default
ready                    ← gcc 默认 gnu23,bool 是关键字,过了
$ clang boolprobe.c -o b_clang_default
boolprobe.c:3:11: error: unknown type name 'bool'      ← clang 默认 gnu17,炸了
boolprobe.c:3:24: error: use of undeclared identifier 'true'
$ gcc -std=c11 boolprobe.c -o b_gcc_c11
boolprobe.c:3:11: error: unknown type name 'bool'      ← 钉到 c11,连 gcc 也炸
boolprobe.c:2:1: note: 'bool' is defined in header '<stdbool.h>'; ...
$ gcc -std=c23 boolprobe.c -o b_gcc_c23 && ./b_gcc_c23
ready
$ clang -std=c23 boolprobe.c -o b_clang_c23 && ./b_clang_c23
ready                    ← 钉死标准后,两个编译器行为对齐
```

要点：同事的错不是「代码错了」，而是「没钉标准、赌了默认值」。这正是第 1 章立的纪律——永远显式 `-std=cXX`。

## 0.2-A {#hw-0-2-a}

**难度 L1** · 题面见 [homework](homework#hw-0-2-a)

**思路**：`-save-temps` 把中间产物留在**当前目录**，所以在临时目录里跑是正解。一个命名细节：当 `-o` 的名字与源文件**基名相同**时（`-o hello` 配 `hello.c`），产物叫平名 `hello.i`；只有 `-o` 名字不同（如 `-o saveexe`）才拼复合名 `saveexe-hello.i`——两种命名都别让它们散在源码树里。

1. 在 `/tmp` 下开工作目录并放源码。→ 知识点：[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)「`-save-temps` 会弄脏你的目录」一节（产物命名规则与污染风险）
2. `gcc -std=c11 -save-temps hello.c -o hello`：一条命令同时产出 `.i`（预处理后）、`.s`（汇编文本）、`.o`（可重定位目标文件）和可执行 `hello`。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「核心概念：四阶段、四产物、四个停靠开关」一节
3. 用 `ls` 确认产物齐全，并对照四阶段表说出每个文件是哪个阶段的成果。

**验证输出**：

```text
$ mkdir -p /tmp/cj-ex0-ch2 && cd /tmp/cj-ex0-ch2
$ gcc -std=c11 -save-temps hello.c -o hello
$ ls -1 hello hello.i hello.s hello.o
hello
hello.i     ← 预处理产物
hello.s     ← 编译产物(汇编文本)
hello.o     ← 汇编产物(可重定位目标文件)
$ ./hello
hello from C
```

## 0.2-B {#hw-0-2-b}

**难度 L3** · 题面见 [homework](homework#hw-0-2-b)

**思路**：①`printf("%s\n", "常量")` 会被 gcc 识别成「只打一个字符串加换行」的模式，在**编译**阶段悄悄换成更快的 `puts`——只有看汇编才看得见。②AT&T 的 `movq %rsp, %rbp` 操作数顺序是「源, 目的」，Intel 语法反过来是「目的, 源」。

1. 编译拿到 `.s`，在 `main` 里找到 `call puts@PLT`，说明这是编译阶段的优化，链接期 `puts` 作为动态库符号还会以 `@PLT` 形式调用。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「第二站」一节（printf→puts 的观察）、[第 5 章](/00-dev-environment/06-object-files-and-symbols)（PLT 重定位的伏笔）
2. AT&T → Intel 翻译：`movq %rsp, %rbp`（把 rsp 放进 rbp）→ Intel 写作 `mov rbp, rsp`（rbp 从 rsp 取值）。注意三处差异：`%` 前缀没了、`q` 后缀没了（Intel 从寄存器名推断大小）、**操作数顺序反了**。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「AT&T 还是 Intel」一节（四条规则）

**验证输出**：

```text
$ gcc -std=c11 -S hello2.c -o att.s
$ grep -A7 '^main:' att.s
main:
	pushq	%rbp
	movq	%rsp, %rbp
	leaq	.LC0(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT        ← 源码里的 printf 变成了 puts
	movl	$0, %eax
$ gcc -std=c11 -S -masm=intel hello2.c -o intel.s
$ grep -A7 '^main:' intel.s
"main":
	push    rbp
	mov     rbp, rsp        ← movq %rsp, %rbp 的 Intel 写法
	lea     rax, .LC0[rip]
	mov     rdi, rax
	call    "puts"@PLT
	mov     eax, 0
```

## 0.3-A {#hw-0-3-a}

**难度 L1** · 题面见 [homework](homework#hw-0-3-a)

**思路**：预处理是纯文本替换，`AVG(2+4, 6)` 里的实参 `2+4` 会被**原样**塞进定义体，所以每个参数和整体都必须加括号。

1. 错误版 `(a + b) / 2`：实参原样代入后变成 `(2 + 4 + 6) / 2`——好在整体加了括号，本例恰好蒙对。但把除号换成别的运算符结构就会翻车，这正是括号不能省的根源。→ 知识点：[第 3 章：预处理深入](/00-dev-environment/04-preprocessor-deep-dive)「带参宏的参数只是文本」一节（`SQ_BAD` 的优先级教训）
2. 正确版 `((a) + (b)) / 2`：参数、整体都包住，`AVG(2+4, 6)` 展开为 `((2 + 4) + (6)) / 2` = 6。用 `-E` 把展开文本摆出来自证。→ 知识点：[第 3 章](/00-dev-environment/04-preprocessor-deep-dive)（`gcc -E` 万能钥匙）

**验证输出**：

```text
$ gcc -std=c11 avg.c -o avg && ./avg
AVG_BAD(2+4,6)  = 6
AVG_GOOD(2+4,6) = 6
$ gcc -std=c11 -E avg.c | grep AVG | grep printf
    printf("AVG_BAD(2+4,6)  = %d\n", (2 + 4 + 6) / 2);
    printf("AVG_GOOD(2+4,6) = %d\n", ((2 + 4) + (6)) / 2);
```

要点：求平均值的「和」整体有括号，所以本例两种写法都没炸；但 `AVG_BAD` 的参数没加括号，换成 `AVG(2+4, 6*2)` 这类实参就会暴露。规矩是死规矩：**参数加括号、整体加括号**。

## 0.3-B {#hw-0-3-b}

**难度 L3** · 题面见 [homework](homework#hw-0-3-b)

**思路**：`MAX(x, y) + 1` 展开成 `x > y ? x : y + 1`——三目运算符优先级**低于** `+`，所以 `+ 1` 被卷进了「假分支」。

1. 先展开再推理：`5 > 3 ? 5 : (3 + 1)`，条件为真取 `5`，结果 5 而不是直觉的 6。→ 知识点：[第 3 章](/00-dev-environment/04-preprocessor-deep-dive)「带参宏的参数只是文本」一节（宏展开无语法意识，运算符优先级反咬）
2. 修正版 `((a) > (b) ? (a) : (b))`：整体加括号把宏与外面的运算符隔开，展开成 `((5) > (3) ? (5) : (3)) + 1` = 6。→ 知识点：同上（整体括号 + 参数括号双保险）

**验证输出**：

```text
$ gcc -std=c11 max.c -o max && ./max
MAX_BAD(x,y)+1  = 5     ← 直觉以为是 6,实际 5
MAX_GOOD(x,y)+1 = 6
$ gcc -std=c11 -E max.c | grep MAX | grep printf
    printf("MAX_BAD(x,y)+1  = %d\n", x > y ? x : y + 1);
    printf("MAX_GOOD(x,y)+1 = %d\n", ((x) > (y) ? (x) : (y)) + 1);
```

## 0.4-A {#hw-0-4-a}

**难度 L2** · 题面见 [homework](homework#hw-0-4-a)

**思路**：段归属看两件事——「初始化了没」和「只读不」；`static` 改的是链接性，不改落段。

1. 预测：`counter` 没给初值 → `.bss`；`limit` 已初始化 → `.data`（**static 不影响落段**，它只是内部链接）；`name` 是指针变量，指向 `.rodata` 的字符串，指针本身因需要重定位落在 `.data` 的子段 `.data.rel.local`；`"cj"` 字面量 → `.rodata`。→ 知识点：[第 4 章：编译阶段看汇编](/00-dev-environment/05-compile-to-assembly)「先看一张全局图」与「真跑」两节
2. `gcc -S` + grep 验证每个落点；再 `size` 对账：`.data` = `limit`(4) + `name` 指针(8) = 12，`.bss` = `counter`(4)。→ 知识点：[第 4 章](/00-dev-environment/05-compile-to-assembly)（`size` 验段、不靠猜）

**验证输出**：

```text
$ gcc -std=c11 -O0 -S layout2.c -o layout2.s
$ grep -nE '^\s*\.(data|bss|rodata|section|long|zero|string|quad)' layout2.s
4:	.bss
9:	.zero	4              ← counter:零初始化,落 .bss
10:	.data
15:	.long	100            ← limit:static 但已初始化,照样落 .data
17:	.section	.rodata
19:	.string	"cj"           ← 字符串字面量,只读段
20:	.section	.data.rel.local,"aw"
25:	.quad	.LC0           ← name 指针本身,待重定位,落 .data 子段
$ size layout2.o
   text	   data	    bss	    dec	    hex	filename
    119	     12	      4	    135	     87	layout2.o
```

对账：`.data` 12 字节 = `limit` 4 字节 + `name` 指针 8 字节；`.bss` 4 字节 = `counter`。预测全中。

## 0.4-B {#hw-0-4-b}

**难度 L4** · 题面见 [homework](homework#hw-0-4-b)

**思路**：x86-64 System V ABI 里，整型参数前 6 个走 `rdi/rsi/rdx/rcx/r8/r9`，第 7 个起走栈；栈帧里 `rbp+8` 是返回地址，所以第一个栈参数在 `rbp+16`，此后每个参数 8 字节一个槽位。

1. 预测：`g`（第 7 参数）在 `rbp+16`，`h`（第 8 参数）在 `rbp+24`。理由：`call` 压入返回地址在 `rbp+8`，栈参数从 `rbp+16` 开始依序排。→ 知识点：[第 4 章](/00-dev-environment/05-compile-to-assembly)「函数参数怎么传」一节（前 6 寄存器、第 7 起栈、栈帧图）
2. `gcc -S -O0` 验证：`-O0` 下参数都会被老实倒进栈帧，能清晰看到前 6 个存到 `-4..-24(%rbp)`（寄存器参数的本地副本），`g`/`h` 从 `16(%rbp)`/`24(%rbp)` 取。→ 知识点：[第 4 章](/00-dev-environment/05-compile-to-assembly)（`-O0` 好读汇编的原因）

**验证输出**：

```text
$ gcc -std=c11 -O0 sum8.c -o sum8 && ./sum8
sum8=36
$ grep -nE '%rbp' sum8.s
13:	movl	%edi, -4(%rbp)    ← a
14:	movl	%esi, -8(%rbp)    ← b
15:	movl	%edx, -12(%rbp)   ← c
16:	movl	%ecx, -16(%rbp)   ← d
17:	movl	%r8d, -20(%rbp)   ← e
18:	movl	%r9d, -24(%rbp)   ← f
30:	movl	16(%rbp), %eax    ← g!第 7 个,从栈上取,预测命中
32:	movl	24(%rbp), %eax    ← h!第 8 个,rbp+24,预测命中
```

## 0.5-A {#hw-0-5-a}

**难度 L2** · 题面见 [homework](homework#hw-0-5-a)

**思路**：按符号表规则反推源码——要 `T`/`t` 得有全局/静态函数，要 `D`/`B` 得有已初始化/零初始化的全局变量，要 `U` 得引用一个自己不定义的符号（`printf` 最顺手）。

1. 设计源码并编译为 `.o`（用 `-c` 停在汇编后）。→ 知识点：[第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)「目标文件是什么」一节
2. 逐个对 `nm` 字母：大写=外部链接（跨文件可见），小写=内部链接（`static` 限文件内）；`U` 是「我没有、等链接器找」。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)「nm 输出的字母表」与「记忆窍门」两节（对应 ISO C §6.2.2 链接性）

**验证输出**：

```c
#include <stdio.h>
int data_v = 42;          /* 已初始化全局 → D */
int bss_v;                /* 零初始化全局 → B */
static int helper_f(void) { /* static 函数 → t */
    return 1;
}
int global_f(void) {      /* 全局函数 → T */
    printf("%d\n", helper_f());  /* 自己没定义 → U */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -c five.c -o five.o
$ nm five.o
0000000000000000 B bss_v      ← .bss 段,零初始化全局
0000000000000000 D data_v     ← .data 段,已初始化全局
000000000000000b T global_f   ← .text 段,全局函数
0000000000000000 t helper_f   ← .text 段,static 函数(小写!)
                 U printf     ← 未定义,等链接器填
```

## 0.5-B {#hw-0-5-b}

**难度 L3** · 题面见 [homework](homework#hw-0-5-b)

**思路**：`nm` 能看到 `t helper` 是因为 `.o` 里确实有符号条目；链不上是因为 `static` 给了它内部链接，链接器做符号解析时**跨文件看不见**小写符号。

1. 复现：`foo.o` 里 `helper` 是小写 `t`；`main.o` 里是 `U helper`；链接时链接器翻遍所有输入找不到**全局**的 `helper` → `undefined reference`。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)「最值得停一下的是 helper」一节（`static` 对链接器隐形）
2. 修复一：去掉 `static`，`helper` 变成大写 `T`，链接立刻成功。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)（大小写 = 链接性）
3. 修复二：保留 `static`（封装意图不动），加一个非 `static` 的公开包装函数 `helper_pub` 对外提供服务。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)（内部链接的工程用法）

**验证输出**：

```text
$ nm foo.o
0000000000000000 t helper      ← 小写:内部链接
000000000000000e T visible_fn
$ gcc foo.o main.o -o bad
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0xf): undefined reference to `helper'   ← 链不上
$ gcc foo_fix.o main.o -o ok1 && ./ok1      ← 修复一:去 static
10 6
$ gcc foo_fix2.o main.o -o ok2 && ./ok2     ← 修复二:公开包装函数
10 6
```

## 0.6-A {#hw-0-6-a}

**难度 L2** · 题面见 [homework](homework#hw-0-6-a)

**思路**：链接器从左到右单趟扫描；`libuse.a` 的成员 `use.o` 引用了 `add`，只有先处理了引用方，后到的库才知道该抽哪些成员。

1. 正确顺序 `gcc main.o -L. -luse -ladd -o ok`：`main.o` 登记缺 `use_all` → `libuse.a` 抽出 `use.o`（它又缺 `add`）→ `libadd.a` 抽出 `add.o` 补上。→ 知识点：[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)「库顺序陷阱」一节
2. 反序 `-ladd -luse`：轮到 `libadd.a` 时还没人声明要 `add`，一个成员都不抽；`libuse.a` 抽出 `use.o` 后缺的 `add` 已无人提供 → `undefined reference`。→ 知识点：同上（被依赖的放右边，库按依赖序排）

**验证输出**：

```text
$ gcc main.o -L. -luse -ladd -o ok && ./ok
use_all=5
$ gcc main.o -L. -ladd -luse -o bad
/usr/bin/ld: ./libuse.a(use.o): in function `use_all':
use.c:(.text+0x19): undefined reference to `add'   ← 报错点居然在库里
```

注意报错位置是 `libuse.a(use.o)`——这就是「库之间也要按依赖排序」的直接证据。

## 0.6-B {#hw-0-6-b}

**难度 L3** · 题面见 [homework](homework#hw-0-6-b)

**思路**：`ar r` 的 `r` 是「替换**同名**成员、追加**新名**成员」——它不是「重写整个归档」。

1. 第一条命令后归档是 `add.o mul.o`；第二条命令里 `mul.o` 与已有成员同名 → 被替换，`sub.o` 是全新成员 → 追加。→ 知识点：[第 6 章](/00-dev-environment/07-linking-and-static-libs)「把 `.o` 打包成静态库」一节（`ar rcs` 三字母含义）
2. 所以预测 `ar t` 输出 `add.o mul.o sub.o`（`add.o` 不会被「清走」）。真跑验证。→ 知识点：同上（`ar t` 列成员）

**验证输出**：

```text
$ ar rcs libmymath.a add.o mul.o
$ ar rcs libmymath.a mul.o sub.o
$ ar t libmymath.a
add.o
mul.o     ← 同名替换(内容换新)
sub.o     ← 新成员追加
```

要点：想清空重来用 `rm libmymath.a` 再 `ar rcs`，别指望 `ar r` 帮你「整体重写」。

## 0.7-A {#hw-0-7-a}

**难度 L2** · 题面见 [homework](homework#hw-0-7-a)

**思路**：`-fPIC -shared` 产出位置无关的动态库；loader 链接期根本不知道 `greet`，运行期 `dlopen`/`dlsym` 才把它捞出来。

1. 编库：`gcc -std=c11 -fPIC -shared -o libgreet.so greet.c`，`file` 验证是 `shared object`。→ 知识点：[第 7 章：动态库与 dlopen](/00-dev-environment/08-dynamic-libs-and-dlopen)「编一个动态库」一节（`-fPIC` 是位置无关的硬要求）
2. 写 loader：`dlopen` 开库拿句柄、`dlsym` 按字符串名字找符号、强转成函数指针调用、`dlclose` 收尾。→ 知识点：[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)「运行期加载」一节（注意 `void*` 转函数指针靠 POSIX 保证，ISO C 不管）

**验证输出**：

```text
$ gcc -std=c11 -fPIC -shared -o libgreet.so greet.c
$ file libgreet.so | cut -d, -f1-2
libgreet.so: ELF 64-bit LSB shared object, x86-64
$ gcc -std=c11 -Wall -Wextra loader.c -o loader -ldl
$ ./loader
hello, dlopen!
```

## 0.7-B {#hw-0-7-b}

**难度 L3** · 题面见 [homework](homework#hw-0-7-b)

**思路**：`dlopen` 参数带斜杠（`./libgreet.so`）时**按字面路径找、不做库搜索**——相对路径是相对**进程当前工作目录**解释的，换 cwd 就找不到。

1. 复现：换目录再跑，报 `cannot open shared object file`。→ 知识点：[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)「两个真坑」一节（`dlopen` 路径别写相对的）
2. 修复一：写绝对路径（`dlopen("/tmp/.../libgreet.so", ...)`），换 cwd 后依然成功。→ 知识点：同上
3. 修复二：只传库名不带斜杠（`dlopen("libgreet.so", ...)`），此时才走库搜索路径（含 `LD_LIBRARY_PATH`）。→ 知识点：[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)（带不带斜杠的两种查找语义）

**验证输出**：

```text
$ mkdir elsewhere
$ (cd elsewhere && ../loader)
dlopen failed: ./libgreet.so: cannot open shared object file: No such file or directory
$ sed "s|./libgreet.so|/tmp/cj-ex0-hw/libgreet.so|" loader.c > loader_abs.c
$ gcc -std=c11 -Wall -Wextra loader_abs.c -o loader_abs -ldl
$ (cd elsewhere && ../loader_abs)
hello, dlopen!     ← 修复一:绝对路径,换 cwd 照样跑
$ # 修复二:只传库名不带斜杠,靠 LD_LIBRARY_PATH 指路
$ gcc -std=c11 -Wall -Wextra loader2.c -o loader2 -ldl   ← dlopen("libgreet.so", ...)
$ ./loader2
dlopen failed: libgreet.so: cannot open shared object file: No such file or directory
$ LD_LIBRARY_PATH=/tmp/cj-ex0-hw ./loader2
hello, search path!   ← 不带斜杠才走库搜索路径,LD_LIBRARY_PATH 指路成功
```

## 0.8-A {#hw-0-8-a}

**难度 L1** · 题面见 [homework](homework#hw-0-8-a)

**思路**：三件武器各自管不同的雷：`-Wextra` 补 `-Wall` 的缺口（未用参数）、`-Wconversion` 抓数据截断、`-Wall` 里的 `-Wparentheses` 抓「`=` 当 `==`」。

**验证输出**：

```text
$ gcc -std=c11 -Wall -c w1.c -o w1.o          ← 只有 -Wall:无声
$ gcc -std=c11 -Wall -Wextra -c w1.c -o w1.o
w1.c:1:18: warning: unused parameter 'y' [-Wunused-parameter]   ← -Wextra 补的刀
$ gcc -std=c11 -Wall -Wextra -c w2.c -o w2.o  ← 依然无声
$ gcc -std=c11 -Wall -Wextra -Wconversion -c w2.c -o w2.o
w2.c:2:12: warning: conversion from 'long int' to 'int' may change value [-Wconversion]
$ gcc -std=c11 -Wall -c w3.c -o w3.o
w3.c:2:9: warning: suggest parentheses around assignment used as truth value [-Wparentheses]
```

知识点对应：[第 8 章：警告旗标进阶](/00-dev-environment/09-warning-flags) 依次为「`-Wall` 不够：`-Wextra` 补的那一刀」「`-Wconversion`」「`-Wall` 本来就能抓」三节。

## 0.8-B {#hw-0-8-b}

**难度 L3** · 题面见 [homework](homework#hw-0-8-b)、[题面第 10 章](/00-dev-environment/11-sanitizer-gate)

**思路**：读未初始化变量是 UB（§6.3.2.1¶2：不确定值的左值转换），`-Wuninitialized` 是 best-effort 静态分析，条件分支里的路径它分析不过来。

1. `-Wall -Wextra -O2` 编译 `cond()`——一声不吭，验证「没警告 ≠ 没 UB」。→ 知识点：[第 8 章](/00-dev-environment/09-warning-flags)「`-Wuninitialized`：警告是有盲区的」一节
2. 运行两次：本机恰好都打出 `10`（`u` 那个栈槽里恰好留着 10，纯属巧合——`flag=1` 路径这两次都没执行过）——**恰好一致也是 UB**，换优化级别、换编译器就可能变。→ 知识点：[第 9 章](/00-dev-environment/10-standards-and-optimization)「`-O` 让未定义行为现形」一节（UB 没有保证）
3. 运行期兜底实验：**clang 的 MSan**（`-fsanitize=memory`，运行期插桩）当场报 `WARNING: MemorySanitizer: use-of-uninitialized-value`——注意本机用 `-O0` 才报、`-O1` 这次恰好没报（优化级别会改变 MSan 的可见性，又是「别拿测过当保证」的活例）。本机 valgrind 对动态链接程序启动即死（Arch 的老问题，`-static` 绕法在阶段 4 讲）；gcc 的 `-fanalyzer` 是**编译期**静态分析、也能抓到，作为对照附在输出末尾。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)（运行期插桩是兜底，MSan 专治未初始化）、[第 8 章](/00-dev-environment/09-warning-flags)（不同静态分析工具盲区不同）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -O2 cond.c -o cond      ← 编译:无声!
$ ./cond
cond(0) = 10     ← 两次一样,但这是巧合不是保证
cond(0) = 10
$ clang -std=c11 -O0 -g -fsanitize=memory cond.c -o cond_msan
$ ./cond_msan
==263==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x... (/tmp/.../cond_msan+...) ...
SUMMARY: MemorySanitizer: use-of-uninitialized-value (...)
$ gcc -std=c11 -fanalyzer cond.c -o cond_an
cond.c:7:12: warning: use of uninitialized value 'u' [CWE-457] [-Wanalyzer-use-of-uninitialized-value]
    7 |     return u;
      |            ^
  'cond': events 1-5
    3 |     int u;
      |         ^
      |         |
      |         (1) region created on stack here
```

## 0.9-A {#hw-0-9-a}

**难度 L2** · 题面见 [homework](homework#hw-0-9-a)

**思路**：`strdup` 是 POSIX 函数不是 ISO C；`-std=c11` 定义 `__STRICT_ANSI__`，glibc 借此把 POSIX 声明藏起来。

1. `-std=c11` 下报 `implicit declaration of function 'strdup'`（GCC 14 起这是 error 不是 warning）。→ 知识点：[第 9 章](/00-dev-environment/10-standards-and-optimization)「`cXX` 和 `gnuXX`」一节
2. `-std=gnu11` 下 `__STRICT_ANSI__` 未定义，glibc 亮出 `strdup`，编过。→ 知识点：同上（分水岭是 `__STRICT_ANSI__` 这个宏）
3. 保持 `-std=c11` 的合法写法：在**所有 `#include` 之前** `#define _POSIX_C_SOURCE 200809L`，显式声明「我要 POSIX」。→ 知识点：同上（显式开口子，别退回 gnu 模式）

**验证输出**：

```text
$ gcc -std=c11 -Wall dup.c -o dup_c11
dup.c:4:15: error: implicit declaration of function 'strdup'; did you mean 'strcmp'?
$ gcc -std=gnu11 -Wall dup.c -o dup_gnu11 && ./dup_gnu11
hi
$ gcc -std=c11 -Wall -D_POSIX_C_SOURCE=200809L dup.c -o dup_posix && ./dup_posix
hi
```

## 0.9-B {#hw-0-9-b}

**难度 L4** · 题面见 [homework](homework#hw-0-9-b)

**思路**：四个组合全返回 1，和教材里 `x + 100 < x` 的戏码不一样——这不是实验做错了，而是本题要你发现的点：**编译器会不会删掉 UB 代码，取决于它恰好有没有对应的化简模式**。

1. 四个组合真跑：`x * 2` 在 x86 上回绕成 `-2`，`-2 < INT_MAX` 成立，四个组合都返回 1。→ 知识点：[第 9 章](/00-dev-environment/10-standards-and-optimization)「`-O` 让未定义行为现形」一节（有符号溢出是 UB，§6.5¶5）
2. 看汇编：gcc `-O0` 下 `addl %eax, %eax; cmpl %eax, -4(%rbp); jle`——它**算了**回绕后的值再比；`-O2` 下 `leal (%rdi,%rdi), %eax; cmpl %edi, %eax; setl`——也算了。对比教材：`x + 100 < x` 被 gcc 前端认成「正数加正数必大于自身」的恒假式而整段删除。为什么这个没删？因为 gcc 的 fold 模式表里恰好没有「`X*2 < X` 恒假」这条。→ 知识点：[第 9 章](/00-dev-environment/10-standards-and-optimization)（as-if 规则 §5.1.2.3 允许删，但删不删取决于实现细节）
3. 结论：UB 代码的表现是「实现巧合」——同一个编译器，换个写法，一种被删、一种留着；你无法预测、更不能拿「我 `-O0` 测过」当保证。真正的兜底是运行期插桩。→ 知识点：[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)（UBSan 当场抓溢出，不靠赌）

**验证输出**：

```text
$ for cc in gcc clang; do for o in O0 O2; do
    $cc -std=c11 -$o mulov.c -o m && printf "%-9s -> " "$cc -$o" && ./m; done; done
gcc -O0   -> check(INT_MAX) = 1
gcc -O2   -> check(INT_MAX) = 1
clang -O0 -> check(INT_MAX) = 1
clang -O2 -> check(INT_MAX) = 1
$ gcc -std=c11 -O0 -S mulov.c -o mulov_O0.s && awk '/^check:/,/^main:/' mulov_O0.s
check:
	movl	%edi, -4(%rbp)
	movl	-4(%rbp), %eax
	addl	%eax, %eax        ← 老老实实算了 2x
	cmpl	%eax, -4(%rbp)    ← 拿回绕后的值比较
	jle	.L2
	movl	$1, %eax
...
$ gcc -std=c11 -O2 -S mulov.c -o mulov_O2.s && awk '/^check:/,/^main:/' mulov_O2.s
check:
	leal	(%rdi,%rdi), %eax   ← -O2 也算,没删
	cmpl	%edi, %eax
	setl	%al
	movzbl	%al, %eax
	ret
```

对比教材 `x + 100 < x`：gcc 连 `-O0` 都把检查删掉、直接 `movl $0, %eax`。两种戏码都是合法演出——这就是 UB。

## 0.10-A {#hw-0-10-a}

**难度 L2** · 题面见 [homework](homework#hw-0-10-a)

**思路**：`buf[5]` 越界写，UBSan 先报边界、ASan 再报栈越界；`-g` 提供「地址 → 源码行」的地图。

1. 编译运行，ASan 报 `stack-buffer-overflow`，且点明 `WRITE of size 1` 与越界位置。→ 知识点：[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)「ASan：内存错误的照妖镜」一节（redzone 机制）
2. `-g` 对照：带 `-g` 时栈帧显示 `main /tmp/.../oob2.c:4`（精确到文件行）；去掉 `-g` 后只剩 `main (/tmp/.../oob2_nog+0x1349)`——函数名还在（符号表），但**源码行号没了**，只剩二进制偏移。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)（「精确到源码行的定位全靠 `-g`」）、[第 9 章](/00-dev-environment/10-standards-and-optimization)「`-g`」一节

**验证输出**：

```text
$ gcc -std=c11 -O1 -g -fsanitize=address,undefined oob2.c -o oob2 && ./oob2
oob2.c:4:8: runtime error: index 5 out of bounds for type 'char [4]'   ← UBSan 先报
=================================================================
==511==ERROR: AddressSanitizer: stack-buffer-overflow ...
WRITE of size 1 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex0-hw/oob2.c:4       ← 带 -g:精确到行
...
$ gcc -std=c11 -O1 -fsanitize=address,undefined oob2.c -o oob2_nog && ./oob2_nog
    #0 0x58b11f65e349 in main (/tmp/cj-ex0-hw2/oob2_nog+0x1349) ...   ← 没 -g:只剩偏移
```

## 0.10-B {#hw-0-10-b}

**难度 L3** · 题面见 [homework](homework#hw-0-10-b)

**思路**：use-after-free 是 C 最难抓的 bug 之一——释放后那块内存可能被改作他用，表现完全随机。ASan 的 shadow memory 把每次分配和释放都记账，一访问就翻旧账。

1. 三段栈对应关系：非法读在 `uaf3.c:9`（`printf("%d\n", p[1])`）、`free` 在 `uaf3.c:8`、`malloc` 在 `uaf3.c:4`——分配、释放、非法访问的完整生命周期一次摆全。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)「最惊艳的是 ASan 对 use-after-free 的定位」一节
2. 传统调试难抓的原因：这类 bug 常常「碰巧能跑」，等内存被复用后才炸，且炸点远离事发点；ASan 靠插桩 + shadow memory 把复现门槛压到「只要跑一遍」。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)「sanitizer 怎么工作」一节

**验证输出**：

```text
==526==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex0-hw/uaf3.c:9     ← 非法访问点
...
freed by thread T0 here:
    #1 0x... in main /tmp/cj-ex0-hw/uaf3.c:8     ← free 点
previously allocated by thread T0 here:
    #1 0x... in main /tmp/cj-ex0-hw/uaf3.c:4     ← malloc 点
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj-ex0-hw/uaf3.c:9 in main
```

## 0.11-A {#hw-0-11-a}

**难度 L1** · 题面见 [homework](homework#hw-0-11-a)

**思路**：Makefile 三要素（目标/依赖/命令）一条条写；命令行行首是 TAB；头文件写进依赖；`clean` 用 `.PHONY` 防同名文件。

1. 三个源文件 + 三条规则：`main` 依赖两个 `.o`，每个 `.o` 依赖自己的 `.c` **和共享头文件**。→ 知识点：[第 11 章：make 入门](/00-dev-environment/12-make-basics)「第一个 Makefile」一节
2. `clean` 是动作型目标，`.PHONY` 保证目录里真有个叫 `clean` 的文件时它也不会失效。→ 知识点：[第 11 章](/00-dev-environment/12-make-basics)「`.PHONY` 与 clean」一节

**参考 Makefile 与验证输出**：

```makefile
main: main.o farewell.o
	gcc -o main main.o farewell.o

main.o: main.c farewell.h
	gcc -std=c11 -Wall -Wextra -c main.c

farewell.o: farewell.c farewell.h
	gcc -std=c11 -Wall -Wextra -c farewell.c

clean:
	rm -f main main.o farewell.o

.PHONY: clean
```

```text
$ make
gcc -std=c11 -Wall -Wextra -c main.c
gcc -std=c11 -Wall -Wextra -c farewell.c
gcc -o main main.o farewell.o
$ ./main
goodbye, make!
$ make clean
rm -f main main.o farewell.o
```

## 0.11-B {#hw-0-11-b}

**难度 L3** · 题面见 [homework](homework#hw-0-11-b)

**思路**：①头文件是两个 `.o` 的共同依赖，它一变两条编译都要重跑；②模式规则 `%.o: %.c` 加三个自动变量能把编译部分压成一条。

1. `touch farewell.h` 后 `make`：两条编译 + 一条链接，共三条命令。原因：`farewell.h` 比 `main.o` 和 `farewell.o` 都新，两个 `.o` 全部重编，随后重新链接。→ 知识点：[第 11 章](/00-dev-environment/12-make-basics)「增量编译」一节（头文件依赖漏写的后果）
2. 重写版：`$(CC) $(CFLAGS) -o $@ $^` 里 `$@`=目标 `main`、`$^`=全部依赖（两个 `.o`）；模式规则里 `$<`=触发的那个 `.c`、`$@`=对应的 `.o`。→ 知识点：[第 11 章](/00-dev-environment/12-make-basics)「自动变量与模式规则」一节
3. `=` 递归展开（用到时才求值）vs `:=` 立即展开（赋值时定死）：`A = $(B)` 后定义 `B` 仍能取到值，`C := $(D)` 在 `D` 未定义时就定成空。→ 知识点：[第 11 章](/00-dev-environment/12-make-basics)「变量」一节

**验证输出**：

```text
$ touch farewell.h && make
gcc -std=c11 -Wall -Wextra -c main.c      ← 两个 .o 都重编(预测命中)
gcc -std=c11 -Wall -Wextra -c farewell.c
gcc -o main main.o farewell.o             ← 最后重新链接

$ make clean && make                      ← 模式规则版
gcc -std=c11 -Wall -Wextra -c main.c -o main.o
gcc -std=c11 -Wall -Wextra -c farewell.c -o farewell.o
gcc -std=c11 -Wall -Wextra -o main main.o farewell.o
goodbye, make!

$ make -f mkvar
A (递归 =) => [hello]
C (立即 :=) => []
```

## 0.12-A {#hw-0-12-a}

**难度 L2** · 题面见 [homework](homework#hw-0-12-a)

**思路**：CMake 是声明式的：只说「要什么目标、用什么源、钉什么标准」，编译命令由它生成。

1. 最小 `CMakeLists.txt` 四段：最低版本、工程名+语言、C11（`CMAKE_C_STANDARD_REQUIRED ON` 防止悄悄降级）、可执行目标。→ 知识点：[第 12 章：CMake 入门](/00-dev-environment/13-cmake-basics)「最小 CMakeLists.txt」一节
2. 两步走：`cmake -B build` 配置（探测编译器、生成构建文件、写 `CMakeCache.txt`），`cmake --build build` 构建（跨生成器统一命令）。`-B` 是 out-of-source，产物全在 `build/`。→ 知识点：[第 12 章](/00-dev-environment/13-cmake-basics)「两步走」一节

**验证输出**：

```text
$ cmake -B build
-- Configuring done (2.6s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cj-ex0-hw/fw-cmake/build
$ cmake --build build
[ 33%] Building C object CMakeFiles/main.dir/main.c.o
[ 66%] Building C object CMakeFiles/main.dir/farewell.c.o
[100%] Linking C executable main
[100%] Built target main
$ ./build/main
goodbye, make!
$ ls build/
CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake  main   ← 里面那份 Makefile
```

## 0.12-B {#hw-0-12-b}

**难度 L3** · 题面见 [homework](homework#hw-0-12-b)

**思路**：CMake 的 `CMAKE_C_EXTENSIONS` 默认 `ON`，所以哪怕钉了 `CMAKE_C_STANDARD 11`，实际旗标仍是 `-std=gnu11`。

1. 打开 CMake 生成的 `flags.make` 验证：Debug 给 `-g -std=gnu11`、Release 给 `-O3 -DNDEBUG -std=gnu11`——注意 Release 还顺手关了 `assert`（`-DNDEBUG`）。→ 知识点：[第 12 章](/00-dev-environment/13-cmake-basics)「Debug / Release」一节、[第 9 章](/00-dev-environment/10-standards-and-optimization)（`-O3`/`-DNDEBUG`/`-g` 的含义）
2. 要真正的 `-std=c11`：`set(CMAKE_C_EXTENSIONS OFF)`——**必须放在 `add_executable` 之前**（它影响目标创建时的属性；放最后补一条实测无效，这也是个真坑）。→ 知识点：[第 12 章](/00-dev-environment/13-cmake-basics)（`CMAKE_C_EXTENSIONS`）、[第 9 章](/00-dev-environment/10-standards-and-optimization)（`c11` 与 `gnu11` 的分水岭 `__STRICT_ANSI__`）

**验证输出**：

```text
$ grep '^C_FLAGS' build-dbg/CMakeFiles/main.dir/flags.make
C_FLAGS = -g -std=gnu11                ← 默认带 GNU 扩展
$ grep '^C_FLAGS' build-rel/CMakeFiles/main.dir/flags.make
C_FLAGS = -O3 -DNDEBUG -std=gnu11
$ # 在 add_executable 之前加 set(CMAKE_C_EXTENSIONS OFF) 后:
$ grep '^C_FLAGS' build-strict/CMakeFiles/main.dir/flags.make
C_FLAGS = -g -std=c11                  ← 这下才是严格 C11
$ ./build-strict/main
goodbye, cmake!
```

## 0.13-A {#hw-0-13-a}

**难度 L2** · 题面见 [homework](homework#hw-0-13-a)

**思路**：除零在 x86 上触发 SIGFPE（浮点/算术异常），GDB 在崩溃点停住，`bt` 看栈、`print` 读变量即可定位根因——全程不写一行 printf。

1. `run` 后 GDB 报告 `SIGFPE, Arithmetic exception`，停在 `divzero.c:8`（`int r = v / divisor;`）。→ 知识点：[第 13 章：GDB 基础](/00-dev-environment/14-gdb-basics)「在 GDB 里看崩溃现场」一节
2. `print divisor` = 0 锁定根因（除数为零）；`print v` = 15 说明 `compute(5)` 本身算对了——bug 定位在「除数」而不是「被除数」。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)（`bt`/`print` 看现场）
3. 直接跑时退出码 136 = 128 + 8（SIGFPE 的信号编号），和第 13 章段错误的 139（128+11）是同一套算术。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)「引言」一节（139 的来历；「128+N」的换算本身是**教材外补充**——shell 把信号退出编码为 128 加信号编号）

**验证输出**：

```text
$ ./divzero; echo "exit=$?"
exit=136
$ gdb -q -batch -ex run -ex bt -ex "print divisor" -ex "print v" ./divzero
Program received signal SIGFPE, Arithmetic exception.
0x0000555555555178 in main () at divzero.c:8
8	    int r = v / divisor;
#0  0x0000555555555178 in main () at divzero.c:8
$1 = 0        ← 除数!根因
$2 = 15       ← 被除数没毛病
```

## 0.13-B {#hw-0-13-b}

**难度 L3** · 题面见 [homework](homework#hw-0-13-b)

**思路**：`stdout` 重定向到文件后是全缓冲——数据攒在用户态缓冲区，进程被 SIGSEGV 打死时**没机会刷新**，于是那行 `printf` 人间蒸发。

1. 预测 vs 现实：大多数人预测文件里至少有一行输出，实测 **0 字节**。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)「靶子程序」一节（全缓冲 + 信号打死 = 输出丢失）
2. 修法：`printf` 后紧跟 `fflush(stdout)`（或者直接打到无缓冲的 `stderr`），文件里就能看到输出了。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)（缓冲丢失机制；`fflush` 本身是**教材外补充**，第 13 章没讲这个函数）
3. 结论：printf 调试法对崩溃类 bug 不可靠——你连「最后打到了哪一行」都会看错；GDB 直接读内存里的变量，不依赖缓冲刷新，所以可靠。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)「引言」一节

**验证输出**：

```text
$ ./buff > out.txt 2>/dev/null; echo "exit=$?"
exit=139                    ← SIGSEGV
$ wc -c out.txt
0 out.txt                   ← 预测「至少有一行」的人,输了
$ ./buff2 > out2.txt 2>/dev/null     ← 加了 fflush(stdout)
$ cat out2.txt
before crash
```

## 0.14-A {#hw-0-14-a}

**难度 L2** · 题面见 [homework](homework#hw-0-14-a)

**思路**：条件断点 `break 位置 if 条件`，只在条件成立时停，省去按 49 次 `continue`。

1. 先动笔：第 50 次循环执行 `sum += i` 之前，`i` 应等于 50；`sum` 应是 1+2+…+49 = 1225。→ 知识点：[第 14 章：GDB 进阶](/00-dev-environment/15-gdb-advanced)「条件断点」一节（前提是断点停在该行**执行前**）
2. GDB 验证：`break loop100.c:5 if i == 50` 精确停在第 50 次循环，`print i`=50、`print sum`=1225，预测命中；`continue` 跑完总结果 5050。→ 知识点：[第 14 章](/00-dev-environment/15-gdb-advanced)（条件断点语法与语义）

**验证输出**：

```text
$ gdb -q -batch -ex "break loop100.c:5 if i == 50" -ex run \
      -ex "print i" -ex "print sum" -ex continue ./loop100
Breakpoint 1, main () at loop100.c:5
5	        sum += i;
$1 = 50
$2 = 1225        ← 1+2+...+49,动笔算的结果对上
sum = 5050
[Inferior 1 (process 817) exited normally]
```

## 0.14-B {#hw-0-14-b}

**难度 L3** · 题面见 [homework](homework#hw-0-14-b)

**思路**：①watchpoint 盯住变量，一被改就停并给出 Old/New；②core dump 把崩溃现场冻结成文件，事后不重跑也能查。

1. 设 `watch sum` 后配条件 `condition 2 sum == 999`：程序只在 `sum` 变成 999 那一刻停——注意 GDB 显示的是 `corrupt.c:8`，那是**改坏那一行的下一行**（`printf` 行）：watchpoint 是「值**已被改之后**」停下，PC 已经走到下一条语句。真正写坏 `sum` 的是第 7 行，`Old value = 55 / New value = 999` 这个 Old/New 对照才是定位证据——别把「停在哪一行」当成案发现场。→ 知识点：[第 14 章](/00-dev-environment/15-gdb-advanced)「watchpoint」一节（要先 `break main` 进作用域再 watch）
2. 在 GDB 里让 `divzero` 崩掉，`generate-core-file` 生成 core；然后 `gdb ./divzero divzero.core` **不重跑程序**直接读现场——`bt` 与 `print` 与当场调试一致。→ 知识点：[第 14 章](/00-dev-environment/15-gdb-advanced)「core dump」一节（`generate-core-file` 比等系统 core 省心）

**验证输出**：

```text
$ gdb -q -batch -ex "break main" -ex run -ex "watch sum" \
      -ex "condition 2 sum == 999" -ex continue -ex "print i" ./corrupt
Hardware watchpoint 2: sum
Old value = 55
New value = 999
main () at corrupt.c:8        ← 改坏 sum 的那一行
8	    printf("sum = %d\n", sum);

$ gdb -q -batch -ex run -ex "generate-core-file divzero.core" ./divzero
Saved corefile divzero.core
$ gdb -q -batch -ex bt -ex "print divisor" -ex "print v" ./divzero divzero.core
Core was generated by `/tmp/cj-ex0-hw/divzero'.
Program terminated with signal SIGFPE, Arithmetic exception.
#0  0x0000555555555178 in main () at divzero.c:8
$1 = 0
$2 = 15
```

## 0.15-A {#hw-0-15-a}

**难度 L1** · 题面见 [homework](homework#hw-0-15-a)

**思路**：三区流动（工作区 → `add` → 暂存区 → `commit` → 版本库），分支开发再 `merge --no-ff` 合回。

1. 基础流程：`init -b main`（注意：`git init` 的默认分支名取决于你的 git 配置，本机默认是 `master`——不指定 `-b main` 的话后面 `git switch main` 会报 `invalid reference: main`，这也是个真坑）、`add`/`commit`、改文件后 `status`/`diff` 看改动、第二次 `commit`。→ 知识点：[第 15 章：Git 工作流](/00-dev-environment/16-git-workflow)「三个区」「基础流程」「看改动」三节
2. `switch -c feature` 开分支、提交 v3、`switch main` 切回、`merge --no-ff` 合回——历史图里最顶上的 `Merge branch 'feature'` 是合并提交，分叉发生在 `升级到 version 2` 处。→ 知识点：[第 15 章](/00-dev-environment/16-git-workflow)「分支与合并」一节

**验证输出**：

```text
$ git init -b main && git config user.email "demo@cj.dev" && git config user.name "CJ Demo"
$ git add main.c && git commit -m "feat: 初始版本,打印 version 1"
$ sed -i 's/version 1/version 2/' main.c
$ git status --short
 M main.c                        ← 工作区改了、还没 add
$ git add main.c && git commit -m "feat: 升级到 version 2"
$ git switch -c feature
$ sed -i 's/version 2/version 3/' main.c
$ git add main.c && git commit -m "feat: feature 分支 version 3"
$ git switch main
$ git merge --no-ff feature -m "Merge branch 'feature'"
$ git log --oneline --graph
*   866956d Merge branch 'feature'        ← 合并提交
|\
| * 1747d4a feat: feature 分支 version 3   ← feature 分支的工作
|/
* 6ab2d2c feat: 升级到 version 2          ← 分叉点在这
* d38966b feat: 初始版本,打印 version 1
```

## 0.15-B {#hw-0-15-b}

**难度 L2** · 题面见 [homework](homework#hw-0-15-b)

**思路**：约定式提交用类型词表达「改动的性质」；`.gitignore` 挡「能从源码重新生成」或「本机私有」的东西；暂存区存在是为了「挑着提交」。

1. 三条提交信息：`fix: 修复除零导致的崩溃`、`docs: 新增 README.md`、`refactor: 重写 Makefile,行为不变`。类型词分别表达「修 bug/改文档/重构」。→ 知识点：[第 15 章](/00-dev-environment/16-git-workflow)「基础流程」一节（conventional commits 类型词表）
2. `.gitignore` 示例：`*.o`（目标文件）、`build/`（CMake 构建目录）、`main` 或 `*.exe`（可执行产物）、`*.so`（动态库产物）、`.vscode/`（编辑器本地配置）——前四类能从源码重建，最后是机器私有配置。→ 知识点：[第 15 章](/00-dev-environment/16-git-workflow)「远程、协作」一节
3. 暂存区的意义：一次改动多个文件时，可以把「功能 A 的三个文件」和「修 bug 的两个文件」分两次 `add`/`commit`，让每个提交都是一个完整、可读的单元——没有暂存区就只能一股脑全提交。→ 知识点：[第 15 章](/00-dev-environment/16-git-workflow)「三个区」一节

## 0.16-A {#hw-0-16-a}

**难度 L2** · 题面见 [homework](homework#hw-0-16-a)

**思路**：workflow 骨架 = `name` + `on`（触发）+ `jobs`（并行）→ `steps`（串行）；矩阵让一个 job 复制成多份并行跑。

**参考 YAML**：

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
      fail-fast: false      # 一个格子失败不取消另一个,两边问题一次看全
      matrix:
        cc: [gcc, clang]    # 同一份代码,gcc 和 clang 各编一遍
    steps:
      - uses: actions/checkout@v4
      - name: 选择编译器
        run: echo "CC=${{ matrix.cc }}" >> $GITHUB_ENV
      - name: 编译
        run: make
```

知识点：[第 16 章：GitHub Actions](/00-dev-environment/17-github-actions)「workflow 文件长什么样」与「四个 job」两节（`on`/`runs-on`/`matrix`/`checkout`/`fail-fast` 的含义都在那）。

## 0.16-B {#hw-0-16-b}

**难度 L3** · 题面见 [homework](homework#hw-0-16-b)

**思路**：「本地过、CI 红」的排查本质是「找出本地和 CI 环境的一切差异」，按四个 job 逐个对照。

1. **编译器不同**（`build-examples` 矩阵 job）：本地 gcc 能过、CI 的 clang 格子炸（或反过来）——比如依赖了 gcc 扩展、或踩了实现定义行为。第一步：看是矩阵哪个格子红的、读它的报错。→ 知识点：[第 1 章](/00-dev-environment/01-toolchain-health-check)（本地 gcc ≠ CI 的 `$CC`）
2. **没显式钉 `-std`**：本地 gcc 16 默认 gnu23、CI 的 clang 默认 gnu17，同段代码两种待遇。第一步：在所有编译命令里显式加 `-std=c11`。→ 知识点：[第 1 章](/00-dev-environment/01-toolchain-health-check)「真正的第一大坑」
3. **警告升级成错误**（`-Werror`）：CI 编译旗标里带 `-Werror`，你本地没开——本地只有 warning 无所谓，CI 里 warning 直接变 error 红掉。第一步：本地用与 CI 相同的旗标重编。→ 知识点：[第 8 章](/00-dev-environment/09-warning-flags)「`-Werror`」一节
4. **sanitizer 门**（`sanitize` job）：你的代码有越界/UB/UAF，普通构建「碰巧能跑」，CI 的 ASan/UBSan 构建一跑就非 0 退出。第一步：本地复现 sanitize job 的环境变量（`CC=clang CFLAGS="-fsanitize=address,undefined -g"`）重跑。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)「代价、退出码」一节
5. **格式门**（`format-check` job）：`clang-format --dry-run --Werror` 在你没格式化的文件上红。第一步：本地跑同一条命令。→ 知识点：[第 17 章](/00-dev-environment/18-format-and-quality-gate)「clang-format 怎么用」一节
6. **文档门**（`docs` job）：新增文档 frontmatter 缺字段或 markdownlint 报错。第一步：`python3 scripts/validate_frontmatter.py`。→ 知识点：[第 16 章](/00-dev-environment/17-github-actions)「第三、四个 docs 和 format-check」一节

## 0.17-A {#hw-0-17-a}

**难度 L1** · 题面见 [homework](homework#hw-0-17-a)

**思路**：`--dry-run --Werror` 只检查不改动、把不合规当错误；`-i` 原地修复。

1. 检查阶段：每处不合规被标成 `error`，退出码 1。→ 知识点：[第 17 章：格式化与质量门](/00-dev-environment/18-format-and-quality-gate)「clang-format 怎么用」一节
2. `clang-format -i messy.c` 原地修：`x=42` → `x = 42`、单行堆叠的 if 按 `.clang-format`（`Attach` 大括号、短 if 强制展开、4 空格）展开成多行。再查退出码 0。→ 知识点：[第 17 章](/00-dev-environment/18-format-and-quality-gate)「`.clang-format`：格式的真相源」一节

**验证输出**：

```text
$ clang-format --dry-run --Werror messy.c
messy.c:3:10: error: code should be clang-formatted [-Wclang-format-violations]
    int x=42;
...
$ echo $?
1
$ clang-format -i messy.c && cat messy.c
#include <stdio.h>
int main(void) {
    int x = 42;
    if (x == 42) {
        printf("%d\n", x);
    }
    return 0;
}
$ clang-format --dry-run --Werror messy.c && echo "再查 exit=0,格式合规"
再查 exit=0,格式合规
```

## 0.17-B {#hw-0-17-b}

**难度 L2** · 题面见 [homework](homework#hw-0-17-b)

**思路**：这两个「反常规」设置都是为教学服务的；三层防线从近到远守住同一个真相源。

1. `SortIncludes: Never`：机器按字母重排 `#include` 会破坏「对应头 → 标准库 → 项目头」的教学顺序（这个顺序的出处是仓库根目录 `.claude/writing-style.md` §1.3 的 include 顺序约定，**不是教材第 1 章**），所以关掉。`AllowShortIfStatementsOnASingleLine: Never`：哪怕 `if (x) return;` 一行写得下，也强制展开成带大括号的多行——教学代码可读性优先。→ 知识点：[第 17 章](/00-dev-environment/18-format-and-quality-gate)「`.clang-format`」一节
2. 三层：编辑器保存时自动格式化（最省心）→ git pre-commit hook（提交前拦一次）→ CI 的 `format-check` job（`--dry-run --Werror`，最硬，挡合并）。要三层是因为前两层都能被绕过（换编辑器、`--no-verify` 跳过 hook），只有 CI 这一层是强制的。→ 知识点：[第 17 章](/00-dev-environment/18-format-and-quality-gate)「把格式化挂进日常和 CI」一节

## 0.C-1 {#hw-0-c-1}

**难度 L3** · 题面见 [homework](homework#hw-0-c-1)

**思路**：一条龙走完「构建 → 格式 → sanitizer → 修复 → 复检」。伏笔在这：普通构建下直接 `./main` 也会崩（stack smashing），这正是 UB「表现随环境漂移」的活样本。

1. Makefile 构建通过；但 `./main` 直接 `Aborted`（`*** stack smashing detected ***`）——本机 gcc 默认开栈保护，越界写踩坏了金丝雀值。注意：**这也是 UB 的一种表现**，和 sanitizer 的报告是同一个 bug 的两副面孔。→ 知识点：[第 11 章](/00-dev-environment/12-make-basics)（构建）、[第 10 章](/00-dev-environment/11-sanitizer-gate)（栈越界表现不可预测）
2. 格式检查：本项目代码本来就合规，`--dry-run --Werror` 退出码 0。注意：**把仓库根的 `.clang-format` 拷进工作目录再查**——否则 clang-format 找不到配置文件、退回默认 LLVM 风格，检查结果没有参考意义。→ 知识点：[第 17 章](/00-dev-environment/18-format-and-quality-gate)
3. sanitizer 构建：UBSan 报 `index 2 out of bounds for type 'int [2]'`、ASan 报 `stack-buffer-overflow`，定位到 `main.c:6` 的 `arr[2] = 3`。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)「ASan」「UBSan」两节
4. 修复 `arr[2] = 3` → `arr[1] = 3`；重跑输出 `4 6`（`add(1,3)=4`、`mul(2,3)=6`）、退出码 0。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)（sanitizer 全绿 = 非 0 退出码的门过了）

**验证输出**：

```text
$ make && ./main
gcc -std=c11 -Wall -Wextra -c main.c -o main.o
gcc -std=c11 -Wall -Wextra -c calc.c -o calc.o
gcc -std=c11 -Wall -Wextra -o main main.o calc.o
*** stack smashing detected ***: terminated     ← 伏笔:普通构建也崩,但报错很含糊

$ clang-format --dry-run --Werror main.c calc.c; echo "exit=$?"
exit=0                                           ← 格式合规

$ make clean && make CFLAGS="-std=c11 -Wall -Wextra -O1 -g -fsanitize=address,undefined" \
      LDFLAGS="-fsanitize=address,undefined"
$ ./main
main.c:6:8: runtime error: index 2 out of bounds for type 'int [2]'   ← UBSan
==976==ERROR: AddressSanitizer: stack-buffer-overflow ...              ← ASan
    #0 0x... in main /tmp/cj-ex0-hw/calc/main.c:6                       ← 精确定位

$ sed -i 's/arr\[2\] = 3;/arr[1] = 3;/' main.c
$ make clean && make CFLAGS="... -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
$ ./main
4 6
$ echo "exit=$?"
exit=0
```

## 0.C-2 {#hw-0-c-2}

**难度 L4** · 题面见 [homework](homework#hw-0-c-2)

**思路**：两个 `undefined reference` 成因不同：`helper` 是「存在但对链接器隐形」（static = 内部链接），`sub` 是「根本不存在」（哪都没定义）。

1. 复现报错：`helper` 和 `sub` 各报一条。→ 知识点：[第 6 章](/00-dev-environment/07-linking-and-static-libs)「复现一」一节
2. `nm math_utils.o` 分诊：`T add`、`t helper`（小写！内部链接，跨文件不可见）、`T mul`——`helper` 的报错根因和 `sub` 完全不同。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)「最值得停一下的是 helper」一节
3. 修复：`helper` 要么去 `static`、要么加公开包装（见 0.5-B）；`sub` 补实现。修好后 main 返回 `add(1,2)+mul(3,4)+sub(5,2) = 3+12+3 = 18`。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)、[第 6 章](/00-dev-environment/07-linking-and-static-libs)
4. 加分问：打包成 `libmath.a` 后把库排在对象前面，报错位置变成了 `main_fix.o` 里对 `add`/`mul` 的引用——和「漏链」一模一样的报错，这就是库顺序陷阱的迷惑性。→ 知识点：[第 6 章](/00-dev-environment/07-linking-and-static-libs)「库顺序陷阱」一节

**验证输出**：

```text
$ gcc math_utils.o main.o -o bad
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0x20): undefined reference to `helper'   ← 存在但隐形
main.c:(.text+0x31): undefined reference to `sub'      ← 压根不存在
$ nm math_utils.o
0000000000000000 T add
0000000000000014 t helper     ← 小写 t,铁证
0000000000000022 T mul
$ gcc math_utils.o main_fix.o -o fixed && ./fixed; echo "exit=$?"
exit=18                     ← 3+12+3
$ ar rcs libmath.a math_utils.o
$ gcc -L. -lmath main_fix.o -o badorder
/usr/bin/ld: main_fix.o: in function `main':
main_fix.c:(.text+0x26): undefined reference to `add'
main_fix.c:(.text+0x37): undefined reference to `mul'
$ gcc main_fix.o -L. -lmath -o goodorder && ./goodorder; echo "exit=$?"
exit=18
```

## 0.C-3 {#hw-0-c-3}

**难度 L5** · 题面见 [homework](homework#hw-0-c-3)

**思路**：gcc 一站式编译背后就是这四段流水线；徒手走完它，需要把 `ld` 缺的每个部件（启动文件、libc、动态链接器）逐个补上。改编自「手工链接」经典练习。

1. `gcc -E` 预处理、`gcc -S` 编译。**关键点**：这里加 `-fno-pie` 让代码用绝对地址风格、和 `crt1.o` 匹配，`ld` 这步走最朴素的路——老实说，这个只有局部变量的 hello 恰好没有全局地址引用，**不加** `-fno-pie` 裸 `ld` 也能过（本机实测 `ld exit=0`、照跑不误）；但你的程序一旦带全局变量、或想用 `ld -pie` 输出 PIE，就会撞上经典的 `relocation ... against ... can not be used` 报错——`-fno-pie` 是「稳妥通用」的选择，不是「不传就必炸」。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)（四阶段）、[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)（`-fPIC`/PIE 的呼应）
2. `as` 汇编出 `.o`——`file` 显示 `relocatable`，还没法执行。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「第三站」一节
3. `ld` 直调：启动文件 `crt1.o`/`crti.o`/`crtn.o`（程序真正的入口 `_start` 在 `crt1.o` 里）、`-lc` 链 libc、`-lgcc` 链 gcc 的运行时辅助、`--dynamic-linker` 指定动态链接器。它们的路径用 `gcc -print-file-name=...` 探出来，不用背。→ 知识点：[第 6 章](/00-dev-environment/07-linking-and-static-libs)（链接器要什么）、[第 7 章](/00-dev-environment/08-dynamic-libs-and-dlopen)（动态链接器在运行期的角色）
4. 产物能跑、`file` 显示 `executable`——你亲手复刻了 gcc 背后那四步。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)（`relocatable` vs `executable`）

**验证输出**：

```text
$ gcc -std=c11 -E hello.c -o hello.i && wc -l hello.i
562 hello.i                                   ← 预处理:头文件全塞进来
$ gcc -std=c11 -fno-pie -S hello.i -o hello.s && wc -l hello.s
27 hello.s                                    ← 编译:变汇编
$ as hello.s -o hello.o && file hello.o | cut -d, -f1-2
hello.o: ELF 64-bit LSB relocatable, x86-64   ← 汇编:可重定位,还不能跑
$ CRT1=$(gcc -print-file-name=crt1.o); CRTI=$(gcc -print-file-name=crti.o); CRTN=$(gcc -print-file-name=crtn.o)
$ gcc -print-file-name=crt1.o
/usr/lib/gcc/x86_64-pc-linux-gnu/16/../../../../lib/crt1.o
$ LIBGCC_DIR=$(dirname "$(gcc -print-file-name=libgcc.a)")
$ ld -o hello hello.o "$CRT1" "$CRTI" "$CRTN" \
      -lc -lgcc --dynamic-linker /lib64/ld-linux-x86-64.so.2 -L "$LIBGCC_DIR"
$ echo "ld exit=$?"
ld exit=0
$ ./hello
hello from manual ld
$ file hello | cut -d, -f1-2
hello: ELF 64-bit LSB executable, x86-64      ← 链接:填实地址,能跑了
```
