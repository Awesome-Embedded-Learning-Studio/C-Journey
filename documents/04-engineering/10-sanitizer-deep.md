---
title: "ASan+UBSan 深入:复现本仓 CI 的 sanitizer 门"
description: "阶段 0 第 10 章第一次认识 sanitizer——UBSan 抓溢出/移位、ASan 抓栈越界/UAF、shadow memory、recover vs abort、开销。这一章把镜头从『第一次认识』拉到 CI 工程视角:逐字复现 .github/workflows/ci.yml 里 sanitize job 的真实编译方式(CC=clang、CFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g),自己拿这套 flags 编一组含错程序(栈越界/UAF/有符号溢出/use-after-scope),确认 ASan/UBSan 各抓什么、报出来长什么样;拆 ci.yml 第 44 行那条『覆盖盲区』注释——它只 sanitize 进了 CMake 子项目的代码,裸 .c 一律不进 sanitizer 门;再讲 ASAN_OPTIONS 怎么调 ASan 的脾气(halt_on_error / abort_on_error / detect_leaks)、-fsanitize-address-use-after-scope 怎么把『块作用域失效后还在用栈变量』这种阴间 bug 变成带 f8 shadow byte 的当场报告、以及 LSan(ASan 内置的 LeakSanitizer)在容器/seccomp 下的 ptrace 大坑。补 legacy 1-sanitizers 那篇没有的 CI 视角缺口。gcc 16.1.1 + clang 22.1.6 双真跑,贴真实输出 + ISO/POSIX 条款。"
chapter: 4
order: 10
tags:
  - host
  - toolchain
  - testing
  - open-source
  - debug
difficulty: advanced
reading_time_minutes: 18
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 10 章:Sanitizer 门禁(第一次认识 sanitizer——UBSan/ASan/shadow memory/recover vs abort,本章是它的 CI 视角升级)"
  - "阶段 0·第 16 章:GitHub Actions(CI 怎么把 sanitizer 变成一道门)"
  - "阶段 0·第 8 章:警告旗标进阶(警告是 best-effort,所以需要 sanitizer 兜底)"
related:
  - "第 1 章(legacy):ASan 与 UBSan 实战——堆越界/释放后使用/GCC16 shadow bytes(本章补它的 CI 工程视角)"
  - "阶段 0·第 9 章:标准与优化(-O 让 UB 现形、-g 给 sanitizer 报错提供源码映射)"
  - "第 11 章:Valgrind(sanitizer 之外另一条内存排查路线)"
---

# ASan+UBSan 深入:复现本仓 CI 的 sanitizer 门

## 前置阅读:这篇和阶段 0 的那一章是什么关系

如果你还没看过 **阶段 0 第 10 章《Sanitizer 门禁》**,先去看那一篇——它讲的是「sanitizer 是什么、怎么工作、UBSan/ASan 各管什么」,是入门的视角。这一章不再重复那些入门内容,镜头拉远一档:**站在 CI 工程的视角,看本仓库的 sanitizer 门是怎么落地的、它真实覆盖了什么、又有哪些盲区**。也就是说,阶段 0 那章解决「sanitizer 怎么用」,这一章解决「sanitizer 在一个真实仓库里**作为一个 CI job** 长什么样、怎么复现它、它的脾气怎么调、它管不到的角落在哪」。

另一篇要对照着看的是本阶段 legacy 的 **第 1 章《ASan 与 UBSan:让 C 的内存错误和未定义行为当场现形》**——那篇用的是 `gcc` 单编译器、贴了 GCC 16 的 shadow bytes(`fa`/`05`/`fd`)真跑。本章在它基础上补一个工程缺口:**真用 CI 的那套 flags(`CC=clang` + 同时跑 address/undefined + `-fno-omit-frame-pointer`)编一组程序,并把 gcc 和 clang 的输出差异摆出来**——你会发现同样一段含错代码,gcc 和 clang 报出来的东西不完全一样,这种差异在本地手敲 `gcc -fsanitize=address` 时是看不见的。

本机工具链诚实标注:**gcc 16.1.1 / clang 22.1.6**,都已装;**gdb 17.2 / gcov / clang-tidy / valgrind** 在线;**perf / cppcheck / lcov 未装**(本章用不到,但免得你以为我藏着没说)。

## 先把 CI 那扇门读一遍

要复现一道门,先得把它读对。本仓库的 sanitizer 门在 [`.github/workflows/ci.yml`](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/.github/workflows/ci.yml) 里,job 名字叫 `sanitize`。把它和编译相关的那几行原文摘出来:

```yaml
  sanitize:
    name: Sanitizer(ASan + UBSan)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装构建工具
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build clang
      - name: 用 -fsanitize 编译 examples
        env:
          CC: clang
          CFLAGS: -fsanitize=address,undefined -fno-omit-frame-pointer -g
          LDFLAGS: -fsanitize=address,undefined
        run: python3 scripts/build_examples.py
        # 注:目前仅覆盖 CMake 子项目(SC1-4);待 Phase 3 根 CMakeLists
        # 统一构建后,所有 examples(含 stage1 的位运算等)都将纳入 sanitizer。
```

这扇门值得逐行读。第一,它 `apt` 装 `clang`(不装 `gcc`),然后把 `CC=clang` 塞进环境——意思是 sanitize 这一档**只用 clang 编**,而上面那个 `build-examples` job 才走 `gcc`/`clang` 矩阵。第二,真正的门面在那三个环境变量:`CFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g`、`LDFLAGS=-fsanitize=address,undefined`。这三条就是整扇门的全部魔法:`-fsanitize=address,undefined` 同时开 ASan 和 UBSan(逗号分隔就是「两个都上」),`-fno-omit-frame-pointer` 保住栈帧基指针让栈回溯更完整(阶段 0 第 14 章 GDB 那篇讲过这个旋钮的作用),`-g` 给报错配源码行号。第三,`LDFLAGS` 那一行**也得带 `-fsanitize`**——这是个新手特别容易踩的点:sanitizer 不只是编译期插桩,它还**链接期**得把 ASan/UBSan 的运行时库(`libasan`/`libubsan`)链进来;你如果分 `gcc -c` 和 `gcc -o` 两步、只在编译那步加 `-fsanitize`,链接时会报 `undefined reference to __asan_*`。CI 用 `CFLAGS` + `LDFLAGS` 两条都带,就是为了把这个坑堵死。

第四,最底下那两行注释(第 43-45 行)是这扇门**最该被读懂的一句**,它写的是「**目前仅覆盖 CMake 子项目(SC1-4)**」。翻译成人话就是:**CI 的 sanitizer 只 sanitize 了少数几个进了 CMake 的子项目,仓库里大量裸 `.c` 根本没过 sanitizer**。这件事我待会儿在「覆盖盲区」那一节单独拆,因为它直接影响你怎么信任这道门。

## 用 CI 的那套 flags,真编一组含错程序

光读 yaml 没用,得自己拿这套 flags 编一遍,看它到底抓什么。我准备四个最小的、各代表一类的含错程序,放在 `/tmp/cj/p4ch10/` 下。第一个,栈上数组越界写——`a` 只有 4 个元素,偏要写 `a[8]`:

```c
/* oob.c — 栈越界写,复现 CI sanitize job 的第一类错误 */
#include <stdio.h>

int main(void) {
    int a[4] = {0};
    volatile int i = 8;
    a[i] = 42; /* 越界写:踩进 ASan 在 a 后面埋的 redzone */
    printf("a[0] = %d\n", a[0]);
    return 0;
}
```

第二个,经典的 use-after-free,`free` 之后还读那块内存:

```c
/* uaf.c — use-after-free,看 ASan 三段栈 */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* p = malloc(sizeof(int));
    *p = 42;
    free(p);
    printf("%d\n", *p); /* 释放后读 */
    return 0;
}
```

第三个,纯算术的 UB——有符号溢出加移位越界,UBSan 的主场:

```c
/* ovf.c — 有符号溢出 + 移位越界,看 UBSan 默认 recover */
#include <limits.h>
#include <stdio.h>

int main(void) {
    volatile int x = INT_MAX;
    int y = x + 100; /* 有符号溢出,UB(ISO §6.5 p5) */
    int z = 1 << 32; /* 移位指数 >= int 宽度,UB(ISO §6.5.7 p3) */
    printf("%d %d\n", y, z);
    return 0;
}
```

第四个,留给后面单独讲「作用域外用」的那节,先放着。注意这三个例子我都加了 `volatile`——不是为了教风格,是因为不加的话 `-O2` 一上来编译器可能把整段 UB 优化没(阶段 0 第 9 章讲过这个现象),那 sanitizer 还没来得及抓、bug 先被编译器「消失」了,演示就崩了。`volatile` 强制编译器老老实实去访问内存,sanitizer 才抓得到。

### 先用 CI 的 clang 编

我们完全照搬 CI 的 flags,先用 `clang`(CI 用的就是 clang)编这三个,一个一个跑:

```text
$ clang -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g oob.c -o oob_c
$ ./oob_c
oob.c:7:5: runtime error: index 8 out of bounds for type 'int[4]'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior oob.c:7:5
a[0] = 0
$ echo $?
0
```

注意这个输出——它有点反直觉,值得停一下。我们期望的是 ASan 报一个漂亮的 `stack-buffer-overflow`,但 clang 实际上**先**被 UBSan 的 bounds 检查(`-fsanitize=undefined` 里包含的 `-fsanitize=bounds`)截胡了:`index 8 out of bounds for type 'int[4]'`,精确到 `oob.c:7:5`。报完之后程序**没有停**——`a[0] = 0` 还打印了、退出码还是 `0`。这是因为 **UBSan 默认是 recover 模式**(阶段 0 第 10 章讲过):它发现一处 UB 就报一行、然后继续往下跑,不终止进程。于是我们这一跑,clang 的 UBSan 把越界这件事拦在了「数组下标检查」那一关,ASan 那套 shadow memory 的 `stack-buffer-overflow` 报告反倒是被抢了戏、根本没出场。

这就是「同时开 address 和 undefined」会遇到的第一个微妙:两个 sanitizer 谁先看到错误、谁就先报,而后到的那个可能就看不到这次的错误了。换个例子看 UAF,UAF 不在 UBSan 的管辖范围(它是内存访问越界/释放后用,ASan 才管),所以这次该是 ASan 出场了:

```text
$ clang -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g uaf.c -o uaf_c
$ ./uaf_c
=================================================================
==613901==ERROR: AddressSanitizer: heap-use-after-free on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 ... in main /tmp/cj/p4ch10/uaf.c:9
freed by thread T0 here:
    #0 ... in free
    #1 ... in main /tmp/cj/p4ch10/uaf.c:8
previously allocated by thread T0 here:
    #0 ... in malloc
    #1 ... in main /tmp/cj/p4ch10/uaf.c:6
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj/p4ch10/uaf.c:9 in main
$ echo $?
1
```

这回 ASan 全力出场,给了**三段栈**:非法读在 `uaf.c:9`、这块内存是在 `uaf.c:8` 被 `free` 的、最初是 `uaf.c:6` `malloc` 出来的——一块内存的完整生命周期(分配 → 释放 → 非法访问)三个位置一次性摆给你。而且和 UBSan 不同,ASan 抓到就**让进程非 0 退出**(退出码 `1`),它默认的脾气是「报完即停」。这正是 sanitizer 之所以能当 CI 门的关键:抓到错就非 0 退出,CI 拿这个退出码判红绿。

第三个例子留给 UBSan 尽情发挥,看它「recover 模式」到底怎么表现:

```text
$ clang -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g ovf.c -o ovf_c
ovf.c:8:15: warning: left shift count >= width of type [-Wshift-count-overflow]
    8 |     int z = 1 << 32;   /* 移位指数 >= int 宽度,UB(ISO §6.5.7 p3) */
      |               ^  ~~
1 warning generated.
ovf.c:7:15: runtime error: signed integer overflow: 2147483647 + 100 cannot be represented in type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ovf.c:7:15
ovf.c:8:15: runtime error: shift exponent 32 is too large for 32-bit type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ovf.c:8:15
-2147483549 -1817198272
$ echo $?
0
```

两个 UB 都报了——第 7 行的有符号溢出(`INT_MAX + 100`,ISO/IEC 9899:2011 §6.5 第 5 段明确「有符号整数溢出是 UB」)、第 8 行的移位越界(`1 << 32`,`int` 只有 32 位,§6.5.7 第 3 段「移位指数大于等于位宽 UB」)——而且都精确到行列。但程序依然跑完了、打印了那两个被溢出回绕出来的怪值(`-2147483549 -1817198272`),**退出码还是 `0`**。这就是 UBSan 默认 recover 的全部含义:它把 UB 当成「日志」打出来、不中断执行。这里还有个细节——第 8 行 `1 << 32` 还附带了一条**编译期警告** `warning: left shift count >= width of type`,因为 `-fsanitize=undefined` 这一组里也顺手开了相关警告;这说明这一行的移位 UB 是「编译器和运行期都看得见」的,但像 `INT_MAX + 100` 这种就纯粹是运行期才现形的,警告一句都没有,只靠 UBSan 抓。

### 换 gcc 编,看差异

CI 的 sanitize job 只用 clang,但我们本地手敲时通常更习惯 `gcc`。换成 gcc 16.1.1,同样那套 flags,看 `oob.c`:

```text
$ gcc -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g oob.c -o oob_g
$ ./oob_g
oob.c:7:6: runtime error: index 8 out of bounds for type 'int [4]'
oob.c:7:10: runtime error: store to address 0x7446cd3f0060 with insufficient space for an object of type 'int'
0x7446cd3f0060: note: pointer points here
 00 00 00 00  00 00 00 00 ...
              ^
=================================================================
==614102==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7446cd3f0060 at pc 0x... thread T0
WRITE of size 4 at 0x7446cd3f0060 thread T0
    #0 ... in main /tmp/cj/p4ch10/oob.c:7
Address 0x7446cd3f0060 is located in stack of thread T0 at offset 96 in frame
    #0 ... in main /tmp/cj/p4ch10/oob.c:4

  This frame has 2 object(s):
    [48, 52) 'i' (line 6)
    [64, 80) 'a' (line 5) <== Memory access at offset 96 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow /tmp/cj/p4ch10/oob.c:7 in main
Shadow bytes around the buggy address:
  0x7446cd3eff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x7446cd3f0000: f1 f1 f1 f1 f1 f1 04 f2 00 00 f3 f3[f3]f3 f3 f3
  0x7446cd3f0080: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  ...
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  ...
==614102==ABORTING
$ echo $?
1
```

差异一眼就看到——gcc 在同一个 `oob.c` 上**两个都报**:先是 UBSan 的 `index 8 out of bounds`(和 clang 一样),紧接着**还**触发了 ASan 的 `stack-buffer-overflow`,最后 `ABORTING`、退出码 `1`。也就是说 gcc 这套实现里,UBSan 的 bounds 检查报完**没有短路掉**那次实际的越界写,ASan 的 shadow memory 仍然逮到了那次写越进了 redzone(那段 `f1 f1 ... f2 ... f3[f3]` 就是栈的左红区 `f1`、中间红区 `f2`、右红区 `f3`,被方括号标出的 `[f3]` 就是越界写踩中的那一格)。

这两个输出摆在一起,你要带走的核心结论不是「哪个对哪个错」(都正确,UB 本来就该有多重抓法),而是 **sanitizer 的具体行为依赖编译器实现**:同样一套 `-fsanitize=address,undefined` flags,clang 和 gcc 在「越界这个 UB 上 UBSan 会不会把 ASan 的报告截掉」「UBSan 报完进程停不停」这些细节上不完全一致。CI 用 clang 有它的道理(clang 的 ASan 报告栈通常配 symbolizer 更干净、且 LLVM 这边 sanitizer 维护最活跃),但你本地用 gcc 调时看到的输出可能和 CI 不一样——**别拿「我本地 gcc -fsanitize 跑过没报」当「CI 也一定会过」**,反过来也一样。

### ASan 报告怎么读:三段式 + shadow

上面 `oob.c` 那份 gcc 输出信息量很大,我们把读法钉一下,因为它和 legacy 第 1 章那篇是一致的套路,记住一次就够。一份 ASan 报告通常有三段:第一段是**错误类型 + 出错位置**——`ERROR: AddressSanitizer: stack-buffer-overflow` 告诉你这是什么错,`WRITE of size 4 at ... thread T0` 告诉你是一次 4 字节的写,`#0 ... in main oob.c:7` 把出错位置钉到源码行。第二段是**受害者定位**——`[64, 80) 'a' (line 5) <== Memory access at offset 96 overflows this variable` 这句最值钱,它直接点名:你在第 5 行定义的数组 `a`(在栈上占 `[64,80)` 这 16 字节),被偏移到 96 的那次访问撑爆了——这种「越界发生在哪一行、受害者是哪个变量」级别的定位,靠 `printf` 调试法能查半天。第三段是 **shadow bytes**——那张把每 8 字节真实内存映射成 1 字节状态的影子表,`f1`/`f2`/`f3` 是栈的三种 redzone(左/中/右),越界踩中的 `[f3]` 被方括号标出来,这就是 ASan「给每个栈变量前后埋一圈禁区」机制的直接证据。前两个阶段 0 第 10 章和 legacy 第 1 章已经讲透,这里不重复。

## 读懂 CI 那行注释:sanitize 门到底覆盖了什么

现在回头看 ci.yml 第 43-45 行那段注释「目前仅覆盖 CMake 子项目(SC1-4)」,它说的「盲区」到底是什么。这件事我们顺着 `scripts/build_examples.py` 的逻辑一查就清楚——这个脚本是 sanitize job 真正跑的东西,它发现要构建的目标靠的是这一行:

```python
for cl in sorted((REPO / "examples").glob("**/CMakeLists.txt")):
```

也就是说,**它只 glob `examples/**/CMakeLists.txt`,只有自带 `CMakeLists.txt` 的子目录才会被构建(也才会被 sanitize)**。我们仓库里 `examples/` 下实际带 `CMakeLists.txt` 的子目录,我数了一下:

```text
$ find examples -name CMakeLists.txt
examples/stage5-tcp-socket/SC1/CMakeLists.txt
examples/stage5-tcp-socket/SC2/CMakeLists.txt
examples/stage5-tcp-socket/SC3/CMakeLists.txt
examples/stage5-tcp-socket/SC4/CMakeLists.txt
```

就这四个(`stage5-tcp-socket/SC1-4`)。可 `examples/` 下明明还有一堆 `.c`:

```text
$ find examples -name '*.c' | head -20
examples/hello.c
examples/stage1-armc-basics/Exp1/var.c
examples/stage0-compiling-and-debug/gdb_use/main.c
examples/stage0-compiling-and-debug/1/main.c
examples/stage0-compiling-and-debug/2/src.c
...
```

`examples/hello.c`、`stage1-armc-basics/Exp1/var.c`、`stage0-compiling-and-debug/*` 下那一堆——**这些裸 `.c` 全都没有被 `build_examples.py` 扫到,自然也就从来没有过 sanitizer 门**。这就是注释说的「盲区」的全部含义,而且范围比字面看到的「SC1-4」要具体得多:**整个 stage0/stage1 的示例代码,sanitize job 根本没碰过**。

这件事对读者(也对仓库维护者)的意义是:**别把「CI 过了 sanitizer」当成「整个仓库的代码都干净」**。它当下只担保 `stage5-tcp-socket/SC1-4` 这四个 CMake 子项目里没有 ASan/UBSan 抓得到的内存错和 UB;其它目录的 `.c` 要么靠 `build-examples` job(那个只编译、不 sanitize)做个「能编过」的担保,要么连编都没编进 CI。注释里那句「待 Phase 3 根 CMakeLists 统一构建后……都将纳入 sanitizer」,说的就是**修这个盲区的计划**:等仓库有个根 CMakeLists 把所有裸 `.c` 都收编成 CMake 目标,盲区才会消失。在那之前,你本地写裸 `.c` 时,**手敲一遍 `gcc -std=c11 -O1 -g -fsanitize=address,undefined your.c`** 是唯一能给自己补上这道门的办法——这正是阶段 0 第 10 章那一套用法的真正动机。

## 调 ASan 的脾气:ASAN_OPTIONS 三件套

CI 的 yaml 只设了三个环境变量(`CC`/`CFLAGS`/`LDFLAGS`),它没碰的、但你需要知道的一组旋钮是 **`ASAN_OPTIONS`** 这个环境变量——它不重新编译、只改 ASan 运行时的行为。我们拿前面那个 `uaf_g`(gcc 编的 UAF 程序)当受试者,看三个最常用的旋钮各是什么效果。

第一个,**`halt_on_error`**——它控制「ASan 报完第一个错之后要不要立刻让进程死」。它的默认值我们可以直接问 ASan 自己(`ASAN_OPTIONS=help=1` 会让 ASan 把所有 flag 及当前值列出来):

```text
$ ASAN_OPTIONS=help=1 ./uaf_g 2>&1 | grep -A1 "halt_on_error"
	halt_on_error
		- Crash the program after printing the first error report
		  (WARNING: USE AT YOUR OWN RISK!) (Current Value: true)
```

`Current Value: true`——也就是说 **ASan 默认就是「报完即崩」**,这和我们前面看到的「UAF 报完就 ABORTING、退出码 1」是一致的。所以阶段 0 那篇说「ASan 默认 abort」严格来说指的是这个 `halt_on_error=true`;你要是把它设成 `0`,ASan 就会变成「报完继续跑、把一路上所有的内存错都报出来」,适合你想一次性看全一个程序到底踩了多少雷的场景:

```text
$ ASAN_OPTIONS=halt_on_error=0 ./uaf_g 2>&1 | grep -E "ERROR:|SUMMARY:|ABORTING"
==615451==ERROR: AddressSanitizer: heap-use-after-free on address 0x... thread T0
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj/p4ch10/uaf.c:9 in main
```

(这个具体例子里只有一处错误所以看不出「报多个」的效果,但 flag 的语义就是这样。)

第二个旋钮,跟「halt」长得像、容易混的 **`abort_on_error`**——它管的是「进程死的时候,是用 `_exit()` 还是 `abort(3)`」。这两者的区别在于:`abort(3)` 会触发 `SIGABRT`、能被内核抓成 core dump,而 `_exit()` 不会。我们照样问 ASan:

```text
$ ASAN_OPTIONS=help=1 ./uaf_g 2>&1 | grep -A1 "abort_on_error"
	abort_on_error
		- If set, the tool calls abort() instead of _exit() after printing the
		  error report. (Current Value: false)
```

`Current Value: false`——这是个反直觉的点:虽然我们嘴上一直说「ASan 默认 abort」,但**严格意义上的 `abort(3)` 默认是关的**,默认走的是 `_exit()`。想拿 ASan 报错时的 core dump 进 GDB 事后分析(阶段 0 第 14 章讲过 core dump),就得显式 `ASAN_OPTIONS=abort_on_error=1`。这是个细节,但偶尔会用上,值得记一笔。

第三个,**`detect_leaks`**——它开关 ASan 内置的 LeakSanitizer(LSan)。LSan 在程序退出时扫一遍堆,把「`malloc` 了却没 `free`」的内存报出来。我们写一个故意泄漏的程序看效果:

```c
/* leak.c — 故意不 free,看 LSan */
#include <stdlib.h>

int main(void) {
    char* p = malloc(64);
    p[0] = 'x';        /* 用一下,避免被纯优化掉 */
    return p[0] - 'x'; /* 0 */
}
```

用 CI 那套 flags 编,然后分别跑 `detect_leaks=1`(默认)和 `detect_leaks=0`:

```text
$ gcc -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g leak.c -o leak_g
$ ASAN_OPTIONS=detect_leaks=1 ./leak_g
=================================================================
==615462==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #1 0x6146038351bb in main /tmp/cj/p4ch10/leak.c:4
SUMMARY: AddressSanitizer: 64 byte(s) leaked in 1 allocation(s).
$ echo $?
1
$ ASAN_OPTIONS=detect_leaks=0 ./leak_g >/dev/null 2>&1; echo $?
0
```

`detect_leaks=1`(也是默认)时,LSan 在退出时把那 64 字节的泄漏点出来、还给到 `leak.c:4` 那个 `malloc` 的源码位置,而且**让退出码变成 `1`**——也就是说,LSan 报泄漏一样会让 CI 判红。`detect_leaks=0` 时,同样的程序退出码是 `0`,LSan 完全不吭声。这个开关在调试期一般保持默认(`=1`,多抓一个泄漏点是一个),但在某些场景下你必须关掉它——这就是下一节要讲的坑。

## `-fsanitize-address-use-after-scope`:把「作用域失效后还用」钉死

前面 `oob.c`/`uaf.c`/`ovf.c` 三类错误是 sanitizer 的「常规主菜」。还有一种内存错比它们都阴间——**变量出了作用域、内存理论上已经失效了,可你手里还攥着指向它的指针在用**。教科书最爱拿「函数返回局部变量地址」当例子,但你在 **GCC 16** 上跑会发现它已经不按教科书演了:编译器先甩一句 `warning: function returns address of local variable [-Wreturn-local-addr]`,然后干脆把那个悬空地址**优化成空指针**,运行时直接 SEGV(读地址 0)。也就是说,现代编译器已经把这个经典坑**提前、确定性地**变成崩错了——这其实是好事,但意味着你想复现教科书里的 stack-use-after-return,得另想办法。

更贴近真实代码的「作用域外用」是这样的——**在一个块作用域里取了局部变量地址,出了这个块再用那个指针**:

```c
/* scope2.c — 真正触发 use-after-scope:块作用域里取地址,出块后通过指针读 */
#include <stdio.h>

int main(void) {
    int* p;
    {
        int local = 7; /* 这个 local 的作用域就是这对花括号 */
        p = &local;
    } /* 出块:local 失效,ASan 把它标成 f8(stack-use-after-scope) */
    printf("*p = %d\n", *p); /* 读已失效作用域里的内存 */
    return 0;
}
```

抓这类错靠的是 `-fsanitize-address-use-after-scope` 这个开关。它的机制是:ASan 在「变量进入作用域」时把它标记成可访问(`00` 或类似),在「变量离开作用域」时立刻把它**毒化**成 `f8`(shadow byte legend 里的 `Stack use after scope: f8`)。于是你出块之后再通过 `p` 去读那块内存,影子表里那一格是 `f8`、插桩的检查一眼就炸。我们用 CI 那套 flags 编一下(gcc 16.1.1):

```text
$ gcc -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g scope2.c -o s2_g
$ ./s2_g
=================================================================
==615075==ERROR: AddressSanitizer: stack-use-after-scope on address 0x6c7ecb6f0020 at pc 0x... thread T0
READ of size 4 at 0x6c7ecb6f0020 thread T0
Address 0x6c7ecb6f0020 is located in stack of thread T0 at offset 32 in frame
    #0 ... in main /tmp/cj/p4ch10/scope2.c:4

  This frame has 1 object(s):
    [32, 36) 'local' (line 7) <== Memory access at offset 32 is inside this variable
SUMMARY: AddressSanitizer: stack-use-after-scope /tmp/cj/p4ch10/scope2.c:10 in main
Shadow bytes around the buggy address:
  0x6c7ecb6eff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x6c7ecb6f0000: f1 f1 f1 f1[f8]f3 f3 f3 00 00 00 00 00 00 00 00
                                            ↑
                                  f8 = 出块后被毒化的局部变量 local
$ echo $?
1
```

`stack-use-after-scope`,影子字节 `[f8]` 现身——这就是 `-fsanitize-address-use-after-scope` 的标志,出块后 `local` 那一格从「可访问」被改成了 `f8`,再读当场被抓。`clang 22.1.6` 上行为一致(同样报 `stack-use-after-scope`,同样 `f8`)。

这里要诚实补一个发现:我在编译时**故意没显式加 `-fsanitize-address-use-after-scope`**——也就是说,在 **gcc 16.1.1 和 clang 22.1.6 这两个较新版本上,这个检查是 `-fsanitize=address` 的默认行为**,你不必再额外加这个 flag,它已经开好了。这一点和几年前那批老教程(写着「要显式加这个 flag 才能抓 use-after-scope」)已经不一样了,时代变了。但你写 CMake / CI 时若想保证可移植(老一点的 gcc/clang 可能默认不开),在 `target_compile_options` 里**显式加上** `-fsanitize-address-use-after-scope` 是更稳的做法——多写一句无害,少写一句在老工具链上就漏报。本仓 ci.yml 当前的 `CFLAGS` 没写这一条,靠的是 clang 的新版本默认开;如果哪天 CI runner 上的 clang 降级,这条检查会悄悄消失。

## LSan 的容器/seccomp 坑:为什么 CI 里 detect_leaks 有时要关

`detect_leaks` 这个开关听上去很美,但它有一个特别会咬人的坑:**LSan 的实现依赖 `ptrace(2)` 系统调用来扫描进程内存**,而在某些受限环境里——主要是 **Docker 容器默认的 seccomp profile、和一些 CI runner 的沙箱**——`ptrace` 被禁掉了。这种禁用不是「悄悄漏报」,而是 LSan 直接让进程**启动时就挂**,报一句形如:

```text
==...==LeakSanitizer: Unable to scan process memory. ptrace(PTRACE_TRACEME, ...) failed.
```

然后整个 sanitizer 构建全军覆没,CI 因为一个跟「你代码有没有内存错」完全无关的原因判红。这件事在 GitHub Actions 的 `ubuntu-latest` runner 上目前**没复现**(它的 seccomp profile 给了 ptrace 足够的余地),所以本仓 ci.yml 当前不需要处理;但只要哪天你把同样的 sanitizer 构建搬进一个更严的容器(比如 Alpine 的默认 Docker、或者带 `--security-opt seccomp=...` 的自定义 profile),你就会撞上这个坑。修法就一条:**在那种受限环境里跑 sanitizer 时,显式 `ASAN_OPTIONS=detect_leaks=0`**——这是为什么很多生产级项目的 CI 里你会看到这一行,不是他们不喜欢抓泄漏,是 LSan 的 ptrace 机制在那环境里活不下来。换句话说,`detect_leaks` 是个「默认开、但在受限沙箱里必须关」的旋钮,这个权衡是 LSan 当前实现的固有限制(POSIX `ptrace(2)` 的语义决定了它没法在不破坏隔离的前提下扫别的进程内存)。本机我这边没有受限容器可复现这条 `ptrace failed`,所以这条输出是据 LSan 文档与公开 issue 描述、不贴伪造的运行截图——这是诚实标注。

## 把开销也量一下,好决定它该开在哪

sanitizer 不是免费的午餐,这件事阶段 0 第 10 章量过一次(0.072s → 0.120s 的开销)。我们这章用同样的方法、同样的本机,再量一次,好让结论对得上今天的工具链。受试程序是一个 800 万元素的数组循环写满再求和(`cost.c`,纯密集内存访问):

```text
$ gcc -std=c11 -O2 cost.c -o cost_plain
$ gcc -std=c11 -O2 -fsanitize=address,undefined -fno-omit-frame-pointer -g cost.c -o cost_asan
$ python3 -c "import subprocess,time; \
  for n in ['cost_plain','cost_asan']: \
    ts=[]; \
    [ts.append(time.perf_counter()- \
      (lambda t0:(subprocess.run(['./'+n],stdout=-3),t0)[1])(time.perf_counter())) for _ in range(5)]; \
    print(f'{n}: min={min(ts)*1000:.0f}ms median={sorted(ts)[2]*1000:.0f}ms')"
cost_plain: min=23ms median=23ms
cost_asan:  min=46ms median=49ms
```

23ms 对 46-49ms,大约 **2 倍**——这和 ASan 经典的「约 2 倍速度开销 + 数倍内存占用(shadow memory)」对得上,UBSan 的开销通常更小(它不维护 shadow、只在算术前后插检查)。这个 2 倍就是为什么 ci.yml 把 sanitizer 单独拆成一个 job、而不是塞进主 `build-examples` job:你想让「能不能编过」这道门**快**(所以 build-examples 不带 sanitizer、几秒跑完),让「有没有内存错」这道门**慢但只跑一次**(sanitize 单独一个 job、带 2 倍开销也无所谓,反正 CI 是并行的)。这背后是同一个工程取舍——**sanitizer 是调试和 CI 的工具,不进发布构建**:交付给用户的二进制是关掉 sanitizer、带正常 `-O2` 的。这一点阶段 0 第 10 章说过的铁律,在 CI 工程视角下依然成立,而且更明显——你看 ci.yml 里 `sanitize` job 编出来的产物,从来不会被打包成 release artifact,它就是为了**在那个 job 里跑一遍、用退出码判红绿、然后丢掉**。

## 小结

我们这一章把镜头从「sanitizer 怎么用」拉到「sanitizer 在一个真实仓库里怎么当一道 CI 门」。本仓 [`.github/workflows/ci.yml`](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/.github/workflows/ci.yml) 的 `sanitize` job 用的是 `CC=clang` + `CFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g` + 配套的 `LDFLAGS`(链接那步也得带 `-fsanitize`,否则找不到 `libasan`/`libubsan` 运行时)——我们拿这套 flags 真编了四个含错程序,看到 ASan 对 use-after-free 给出「分配/释放/非法访问」三段栈、对栈越界给出点名变量和 `f1`/`f2`/`f3` shadow bytes,UBSan 对有符号溢出(§6.5 p5)和移位越界(§6.5.7 p3)精确报到行列、且默认 recover(报完不停、退出码 0)。把 gcc 和 clang 的输出摆一起,你看到的是 **sanitizer 的具体行为依赖编译器实现**——同样 `oob.c`,clang 是 UBSan 把 ASan 的报告截胡、退出码 0;gcc 是两个都报、`ABORTING` 退出码 1。所以别拿「我本地 gcc -fsanitize 跑过没报」当「CI(clang)也一定过」,反过来也一样。ci.yml 第 43-45 行那段「仅覆盖 CMake 子项目(SC1-4)」的注释,顺着 `build_examples.py` 的 `glob("**/CMakeLists.txt")` 一查就清楚:整个 stage0/stage1 的裸 `.c` 从来没进过 sanitizer 门,**别把「CI 过了 sanitizer」当「全仓代码都干净」**。行为上,`ASAN_OPTIONS` 三个旋钮要记牢——`halt_on_error`(默认 true,报完即崩)、`abort_on_error`(默认 false,严格 `abort(3)` 关着,要 core dump 才显式开)、`detect_leaks`(默认开,让 LSan 在退出时报泄漏且退出码非 0)。`-fsanitize-address-use-after-scope` 抓「作用域失效后还用栈变量」,在新版 gcc/clang 上是 ASan 的默认行为(shadow byte `f8` 现身),但写 CMake/CI 时显式加上更稳。还有两个工程坑值得带走:一是 LSan 靠 `ptrace(2)` 扫内存,在 seccomp 严的容器里会让进程启动即挂(本机无受限容器、未贴伪造输出,据文档诚实标注),那种环境必须 `detect_leaks=0`;二是 sanitizer 有约 2 倍开销(本机 23ms→46ms 真测),所以它是调试和 CI 的工具、不进发布构建,ci.yml 把它单拆一个 job 也是为了和「快」的编译门并行。带着这套理解,下一章我们换条线看 Valgrind——它是 sanitizer 之外的另一条内存排查路线,脾气和适用场景都不一样。

## 参考资源

- **本仓 CI**:[`.github/workflows/ci.yml`](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/.github/workflows/ci.yml) 的 `sanitize` job(第 31-45 行),`build_examples.py` 的 `glob("**/CMakeLists.txt")`——覆盖盲区的根因。
- **阶段 0·第 10 章:Sanitizer 门禁**——sanitizer 入门视角(UBSan/ASan/shadow memory/recover vs abort/开销),本章是它的 CI 升级。
- **本阶段 legacy 第 1 章:ASan 与 UBSan 实战**——GCC 16 的 shadow bytes(`fa`/`05`/`fd`)真跑,本章补它的 CI 工程视角。
- **ISO/IEC 9899:2011**:§6.5 第 5 段(有符号整数溢出 UB)、§6.5.7 第 3 段(移位指数大于等于位宽 UB)。
- **POSIX**:`ptrace(2)`——LSan 扫描进程内存所依赖的系统调用,容器/seccomp 限制的根源。
- **Clang / GCC 手册**:`-fsanitize=address,undefined`、`-fsanitize-address-use-after-scope`、`-fno-sanitize-recover=`、`ASAN_OPTIONS`(`halt_on_error` / `abort_on_error` / `detect_leaks`)、AddressSanitizer Wiki(use-after-scope 的 shadow byte `f8` 语义)。
- **阶段 0·第 14 章:GDB 进阶**——`ASAN_OPTIONS=abort_on_error=1` 抓 core dump 后,进 GDB 事后分析的那条路。
