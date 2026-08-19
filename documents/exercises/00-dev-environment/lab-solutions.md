---
title: "阶段 0 Lab 实验参考"
description: "阶段 0 Lab（解剖 hello 的一生）的实验参考：六个步骤加 L5 挑战的逐步解答，每步标注知识点链接，所有输出在 WSL Arch（gcc 16 + clang 22）真实运行得到。"
chapter: 0
order: 3
tags:
  - host
  - toolchain
  - debug
difficulty: beginner
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0 Lab 题面"
related:
  - "阶段 0 各章"
---

# 阶段 0 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1 + clang 22.1.8）真实运行得到；ASan 的地址与 BuildId 每次运行因 ASLR 不同，不影响结论。建议卡住时先看「思路」逐步对照，别一次读完。

## 步骤 1：四阶段停靠 {#lab-1}

**思路**：四个停靠开关 `-E`/`-S`/`-c` 分别停在预处理、编译、汇编之后，最后一步链接出可执行文件。`.i` 行数暴涨是 `#include <stdio.h>` 被整段塞进来；`.o` 是 `relocatable`（地址没填实、不能跑），链接后才变 `pie executable`。

1. 写源码并逐站停靠。→ 知识点：[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)「核心概念：四阶段、四产物、四个停靠开关」
2. 对照 `wc -l` 行数理解预处理「只做文本替换」。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「第一站：`-E` 停在预处理后」
3. `file` 看 `.o` 与可执行的差别（`relocatable` vs `pie executable`）。→ 知识点：[第 2 章](/00-dev-environment/03-save-temps-and-four-stages)「`.o` 不能直接跑」一节

**验证输出**：

```text
$ gcc -std=c11 -E labhello.c -o labhello.i && wc -l labhello.i
563 labhello.i              ← 源码才 7 行,stdio.h 全塞进来了
$ gcc -std=c11 -S labhello.c -o labhello.s && wc -l labhello.s
28 labhello.s               ← 汇编文本
$ gcc -std=c11 -c labhello.c -o labhello.o && file labhello.o | cut -d, -f1-2
labhello.o: ELF 64-bit LSB relocatable, x86-64      ← 可重定位,还不能跑
$ gcc -std=c11 labhello.o -o labhello && file labhello | cut -d, -f1-2
labhello: ELF 64-bit LSB pie executable, x86-64     ← 链接后,能跑了
$ ./labhello
hello from lab
```

## 步骤 2：预处理侦探 {#lab-2}

**思路**：`#ifdef` 哪一支存活由命令行的 `-D` 决定——源码里看不出来，`-E` 是唯一照妖镜。

1. 两次编译运行证明「同一份源码、两种行为」。→ 知识点：[第 3 章：预处理深入](/00-dev-environment/04-preprocessor-deep-dive)「条件编译悄悄走了 `#else`」一节
2. `grep -c` 统计 `-E` 产物：带 `-D` 时 ON 支存活 1 条、OFF 支 0 条；不带 `-D` 时正好反过来——被淘汰的那一支在预处理期被**物理删除**，之后的编译阶段根本看不到它。→ 知识点：[第 3 章](/00-dev-environment/04-preprocessor-deep-dive)（`-D` 开关只存在于编译命令里）

**验证输出**：

```text
$ gcc -std=c11 cond_lab.c -o c_off && ./c_off
LAB_MODE is OFF (default)
$ gcc -std=c11 -DLAB_MODE cond_lab.c -o c_on && ./c_on
LAB_MODE is ON
$ gcc -std=c11 -DLAB_MODE -E cond_lab.c | grep -c 'LAB_MODE is ON'
1                          ← ON 支存活
$ gcc -std=c11 -DLAB_MODE -E cond_lab.c | grep -c 'LAB_MODE is OFF'
0                          ← OFF 支被物理删除
$ gcc -std=c11 -E cond_lab.c | grep -c 'LAB_MODE is ON'
0                          ← 不带 -D,反过来
$ gcc -std=c11 -E cond_lab.c | grep -c 'LAB_MODE is OFF'
1
```

## 步骤 3：符号与重定位透视 {#lab-3}

**思路**：`.o` 随身带两张表——符号表（我有什么/我缺什么）和重定位表（哪些地址等链接器填）。`nm` 字母的大小写对应 ISO C 的链接性。

1. `nm lab_lib.o`：`D counter`（已初始化全局）、`B tally`（零初始化全局）、`t helper`（**小写** = `static`、内部链接）、`T visible_fn`（全局函数）。→ 知识点：[第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)「nm 输出的字母表」一节
2. `nm lab_main.o`：`T main` 是自己定义的；`U visible_fn` 和 `U printf` 是「我没有、等链接器找」。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)「UND」一节
3. `readelf -r`：三条重定位条目分别对应 `main` 里调用 `visible_fn`、引用格式串 `.rodata`、调用 `printf` 的三处「待填地址」。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)「那『填』具体填在哪」一节

**验证输出**：

```text
$ nm lab_lib.o
0000000000000000 D counter
0000000000000000 t helper       ← 小写:static,内部链接
0000000000000000 B tally
000000000000000e T visible_fn
$ nm lab_main.o
0000000000000000 T main
                 U printf       ← 未定义,要链接器填
                 U visible_fn
$ readelf -r lab_main.o
Relocation section '.rela.text' at offset 0x1c8 contains 3 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
00000000000a  000500000004 R_X86_64_PLT32    0000000000000000 visible_fn - 4
000000000013  000300000002 R_X86_64_PC32     0000000000000000 .rodata - 4
000000000022  000600000004 R_X86_64_PLT32    0000000000000000 printf - 4
```

## 步骤 4：静态库与顺序陷阱 {#lab-4}

**思路**：链接器从左到右**单趟**扫描。库在前时，轮到它还没人声明缺符号，一个成员都不抽；等 `lab_main.o` 声明缺 `visible_fn` 时，库已经扫过去、不回头了。

1. `ar rcs` 打包、`ar t` 确认成员。→ 知识点：[第 6 章：链接与静态库](/00-dev-environment/07-linking-and-static-libs)「把 `.o` 打包成静态库」一节
2. 正确顺序 `lab_main.o -L. -llab`：先处理对象登记需求，再抽库满足。→ 知识点：[第 6 章](/00-dev-environment/07-linking-and-static-libs)「库顺序陷阱」一节
3. 错误顺序的报错与「漏链 `.o`」一模一样——排查先看命令行顺序，别急着改代码。→ 知识点：同上

**验证输出**：

```text
$ ar rcs liblab.a lab_lib.o && ar t liblab.a
lab_lib.o
$ gcc lab_main.o -L. -llab -o lab_ok && ./lab_ok
r=17                     ← visible_fn(5) = helper(5)+counter = 10+7
$ gcc -L. -llab lab_main.o -o lab_bad
/usr/bin/ld: lab_main.o: in function `main':
lab_main.c:(.text+0xa): undefined reference to `visible_fn'
collect2: error: ld returned 1 exit status
```

## 步骤 5：sanitizer 抓 UB {#lab-5}

**思路**：`vals[3]` 越界写，UBSan 先报边界违规（带 `runtime error`），ASan 再报 `stack-buffer-overflow`（shadow memory 的 redzone 被踩中）——两者都精确到 `ub_lab.c:4`。

1. 编译运行贴报告。→ 知识点：[第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)「UBSan」「ASan」两节
2. 修复并复跑：退出码 0 即门过。→ 知识点：[第 10 章](/00-dev-environment/11-sanitizer-gate)「代价、退出码」一节（sanitizer 靠非 0 退出码当门）

**验证输出**：

```text
$ gcc -std=c11 -O1 -g -fsanitize=address,undefined ub_lab.c -o ub_lab && ./ub_lab
ub_lab.c:4:9: runtime error: index 3 out of bounds for type 'int [2]'   ← UBSan
=================================================================
==331==ERROR: AddressSanitizer: stack-buffer-overflow ...               ← ASan
WRITE of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex0-lab/ub_lab.c:4                         ← 精确定位
...
$ sed -i 's/vals\[3\] = 42;/vals[1] = 42;/' ub_lab.c
$ gcc -std=c11 -O1 -g -fsanitize=address,undefined ub_lab.c -o ub_fix && ./ub_fix
vals[0]=1
$ echo $?
0
```

## 步骤 6：GDB 崩溃定位 {#lab-6}

**思路**：段错误直接跑只有一个 139 退出码；GDB 在崩溃点停住，`bt` 看栈、`print` 读变量，根因立刻现形。直接跑时连 `printf` 的输出都可能丢——`stdout` 重定向/管道下全缓冲，进程被信号打死没机会刷新。

1. `run` 后 GDB 报 SIGSEGV 停在 `*p = x` 那行。→ 知识点：[第 13 章：GDB 基础](/00-dev-environment/14-gdb-basics)「在 GDB 里看崩溃现场」一节
2. `print p` = `(int *) 0x0`（空指针，根因）；`print x` = 24（`compute(4)` 算对了，bug 不在计算）。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)（`bt`/`print`/`info locals` 看现场）

**验证输出**：

```text
$ ./crash_lab; echo $?
139                     ← 128+11(SIGSEGV),啥线索都没有
$ gdb -q -batch -ex run -ex bt -ex "print p" -ex "print x" ./crash_lab
Program received signal SIGSEGV, Segmentation fault.
0x00005555555551a8 in main () at crash_lab.c:13
13	    *p = x;
#0  0x00005555555551a8 in main () at crash_lab.c:13
$1 = (int *) 0x0        ← 空指针,根因锁定
$2 = 24                 ← 阶乘没算错
```

## 附加挑战（L5）：strip 后的汇编级定位 {#lab-l5}

**思路**：`strip` 把符号表和调试信息全剥掉，`bt` 只剩 `??`；但你还有两件武器——`x/i $pc` 看崩在的那条**指令**，`p/x $rax` 看它写入的**寄存器值**。结合第 4 章的汇编知识，照样能定位。

1. `strip` 前后 `file` 输出都还是 `pie executable`（strip 动的是符号/调试段，不是可执行格式）。→ 知识点：[第 5 章](/00-dev-environment/06-object-files-and-symbols)（符号表）、[第 9 章](/00-dev-environment/10-standards-and-optimization)「`-g`」一节
2. GDB 里 `bt` 全是 `??`——函数名没了，只剩地址。→ 知识点：[第 13 章](/00-dev-environment/14-gdb-basics)（没 `-g` 只有地址）
3. `x/i $pc` 给出 `mov %edx,(%rax)`：这是「把 edx 的值写进 rax 指向的内存」；`p/x $rax` = `0x0`——**对空指针写**。根因当场锁定，全程没依赖一个符号名。（注意：带 `%` 前缀的是 **AT&T 语法**（GDB 默认风味）；Intel 写法应是 `mov DWORD PTR [rax], edx`——读法结论一样，语法标签别标错。）→ 知识点：[第 2 章：编译四阶段全景](/00-dev-environment/03-save-temps-and-four-stages)「AT&T 还是 Intel」一节（两种语法的操作数顺序相反）

**验证输出**：

```text
$ strip crash_strip
$ file crash_lab | cut -d, -f1-3
crash_lab: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV)
$ file crash_strip | cut -d, -f1-3
crash_strip: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV)   ← 表面没差别
$ ./crash_strip; echo $?
139
$ gdb -q -batch -ex run -ex "x/i \$pc" -ex "p/x \$rax" -ex bt ./crash_strip
Program received signal SIGSEGV, Segmentation fault.
0x00005555555551a8 in ?? ()
=> 0x5555555551a8:	mov    %edx,(%rax)     ← 崩在的指令:把 edx 写进 rax 指向处
$1 = 0x0                                    ← rax 是 0:对空指针写,根因锁定
#0  0x00005555555551a8 in ?? ()
#1  0x00007ffff7c27781 in ?? () from /usr/lib/libc.so.6
#2  0x00007ffff7c278b9 in __libc_start_main () from /usr/lib/libc.so.6
#3  0x0000555555555065 in ?? ()             ← 全剩地址,函数名没了
```

没有符号表时你凭什么还能定位？凭**机器码自己会说话**——那条 `mov %edx,(%rax)` 加一个值为 0 的 `rax`，就是把「空指针写」钉死了；符号只是路牌，不是道路本身。
