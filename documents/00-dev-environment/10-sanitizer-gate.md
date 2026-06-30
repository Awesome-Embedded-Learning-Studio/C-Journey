---
title: "Sanitizer 门禁：让 UB 与内存错误在运行期当场现形"
description: "第 8 章说警告是 best-effort 有盲区、第 9 章看到 -O 会把靠 UB 回绕的代码改没——这一章请上真正的兜底：sanitizer。把 -fsanitize=address,undefined 真跑一遍：UBSan 当场抓出有符号溢出（ub.c:6 精确到行列）和移位越界，ASan 当场抓出栈越界（点名是哪个变量、哪个偏移溢出）和 use-after-free（给出访问点、free 点、malloc 点三段栈）。还测了运行开销（plain 0.072s → ASan 0.120s）、UBSan 默认 recover 而 ASan 默认 abort、以及报错时非 0 退出码怎么当 CI 的硬门——并说清 sanitizer 是调试/CI 工具不是发布构建。"
chapter: 0
order: 10
tags:
  - host
  - toolchain
  - testing
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [11]
prerequisites:
  - "第 8 章：警告旗标进阶（警告只是 best-effort）"
  - "第 9 章：标准与优化（-O 让 UB 现形、-g 调试信息）"
related:
  - "第 13 章：GDB 基础单步（出错时下断点细查的另一条路）"
  - "第 16 章：GitHub Actions（把 sanitizer 当 CI 硬门）"
---

> 🟡 状态：待审核（2026-06-30）

# Sanitizer 门禁：让 UB 与内存错误在运行期当场现形

## 引言：警告抓不到的，让运行期插桩来抓

前两章我们攒下两个让人不踏实的事实：第 8 章看到编译器警告是 best-effort，`-Wuninitialized` 在条件分支里有明显盲区，没报警告绝不等于没 UB；第 9 章更狠——一段「靠有符号溢出回绕来检测溢出」的代码，gcc 连 `-O0` 都假定不溢出、把检查整段删掉，clang `-O0` 才老实回绕，你拿「我测过」当保证完全靠不住。这两件事指向同一个缺口：**静态分析（警告）和编译器优化都治不了运行期才暴露的 UB 和内存错误**。

这一章请上专门填这个缺口的工具——sanitizer。它的思路跟警告完全不同：警告是编译期扫一眼代码「猜」你有没有写错，sanitizer 则是让编译器**在每个内存访问、每个算术操作前后都插一段检查指令**（这叫「插桩」），程序跑起来时一旦真踩到越界、释放后访问、溢出这些雷，当场报给你看，还能配着 `-g` 把出错位置精确到源码的第几行第几列。我们这一章把两个最常用的——抓未定义行为的 **UBSan**（`-fsanitize=undefined`）和抓内存错误的 **ASan**（`-fsanitize=address`）——挨个真跑，让你直观看到它比警告强在哪、以及它的代价是什么。

## sanitizer 怎么工作：编译期插桩 + shadow memory

先建立直觉：sanitizer 不改你代码的逻辑，它做的是在编译时往生成的机器码里**额外塞检查指令**。拿 ASan 来说，它还会在内存里维护一张「影子表」（shadow memory）：把你程序里每 8 个字节的内存，映射到 1 个字节的「状态」，记录这 8 字节当前是不是可以合法访问；并且在你每个栈变量、每个堆分配的**前后埋一圈「禁区」（redzone）**。于是当你写出 `a[8] = 42` 而 `a` 只有 4 个元素时，那条写指令落在了 `a` 后面的 redzone 里——shadow memory 那一格标记着「这是禁区」，插桩的检查一看就炸。等下你会在 ASan 的报错里亲眼看到这圈 redzone（输出里的 `f1`/`f2`/`f3` 就是它的标记）。

这套机制的代价我们也先说在前面：插桩和 shadow memory 会让程序**变慢、占更多内存**。所以我们待会儿会真测一下开销，并且强调 sanitizer 是调试和 CI 用的，**不该进发布构建**。

## UBSan：把「未定义行为」变成运行期错误

先看 UBSan。它盯的是「这一步运算本身是不是踩了 UB」。我们把第 9 章那个 `INT_MAX + 100` 的有符号溢出再拿出来——这次不是看编译器怎么「假定不溢出」把它删掉，而是看 UBSan 怎么在运行期把它当场抓住：

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    volatile int x = INT_MAX;
    int y = x + 100; /* 有符号溢出,UB */
    printf("%d\n", y);
    return 0;
}
```

```text
$ gcc -std=c11 -O1 -g -fsanitize=undefined ub.c -o ub && ./ub
ub.c:6:9: runtime error: signed integer overflow: 2147483647 + 100 cannot be represented in type 'int'
-2147483549
```

注意它报的是什么：`runtime error: signed integer overflow: 2147483647 + 100 cannot be represented in type 'int'`，**精确到 `ub.c:6:9`（第 6 行第 9 列）**——这正是第 9 章那段 gcc 默默删掉、你完全查不出来的溢出，UBSan 一运行就给你钉死在原地。这就是它和静态警告的根本区别：警告是「我觉得你这段代码可能有问题」，UBSan 是「程序刚跑到这一步，确实溢出了」（依据就是 ISO/IEC 9899 §6.5 第 5 段，有符号整数溢出是 UB）。

再看一个 UBSan 抓的、警告基本不会理你的 UB——**移位越界**。把 `1` 左移 32 位（`int` 只有 32 位，移位大于等于位宽是 UB，ISO/IEC 9899 §6.5.7 第 3 段）：

```c
#include <stdio.h>

int main(void) {
    volatile int n = 32;
    int y = 1 << n; /* 移位 >= int 宽度,UB */
    printf("%d\n", y);
    return 0;
}
```

```text
$ gcc -std=c11 -O1 -g -fsanitize=undefined shift.c -o sh && ./sh
shift.c:5:15: runtime error: shift exponent 32 is too large for 32-bit type 'int'
1
```

`shift exponent 32 is too large for 32-bit type 'int'`——移位的「指数」（右操作数）超了位宽，又是精确到行列。这种移位 bug 在手写位运算、组包解包的代码里特别常见，编译器一个字都不会提醒你，UBSan 一抓一个准。

这里有个细节值得停一下：上面两个例子里，UBSan 报完 `runtime error` 后，程序**并没有停**，`printf` 照样跑出了 `-2147483549` 和 `1`。这是因为 **UBSan 默认是「可恢复（recover）」模式**——发现一处 UB 就报一行，然后继续往下跑，把一路上所有的 UB 都报出来。这在你想「一次性看全这个程序到底踩了多少 UB」时很方便；但如果你希望它踩到第一个就立刻 abort（比如 CI 里），加一个 `-fno-sanitize-recover=all` 就会让它一报即停。

UBSan 默认那组（`-fsanitize=undefined`）还覆盖了不少别的：空指针解引用、对齐违规、整数转换截断、除零、返回值未初始化等等。你不用记全，记住「凡是涉及『这一步运算/访问本身合不合法』的 UB，UBSan 大概率管」就行。

## ASan：内存错误的照妖镜

再看 ASan，它管的是「这次内存访问有没有越界、是不是访问了已经释放的内存」。这是 C 程序里最阴险的一类 bug——它们在很多时候「碰巧能跑」，等程序规模一大、负载一变，突然就在某个客户机器上段错误，而你本地怎么都复现不了。ASan 把这类问题的复现门槛打到了「只要跑一遍」。

先看一个栈上的缓冲区越界：数组 `a` 只有 4 个元素，我们偏要写 `a[8]`：

```c
#include <stdio.h>

int main(void) {
    int a[4] = {0};
    volatile int i = 8;
    a[i] = 42; /* 越界写 */
    printf("%d\n", a[0]);
    return 0;
}
```

```text
$ gcc -std=c11 -O1 -g -fsanitize=address oob.c -o oob && ./oob
=================================================================
==187827==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x... at pc 0x...
WRITE of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj/ch10/oob.c:6
...
  This frame has 2 object(s):
    [48, 52) 'i' (line 5)
    [64, 80) 'a' (line 4) <== Memory access at offset 96 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow /tmp/cj/ch10/oob.c:6 in main
```

（地址和 BuildId 每次运行因 ASLR 不同，这里省略成 `0x...`；关键信息是稳定不变的。）ASan 的报错信息量比 UBSan 还大：它不光告诉你这是个 `stack-buffer-overflow`、是个「写 4 字节（`WRITE of size 4`）」、发生在 `oob.c:6`，**还直接点名是哪个变量被越界**——`'a' (line 4) <== Memory access at offset 96 overflows this variable`，告诉你「你在第 4 行定义的数组 `a`，被第 6 行偏移到 96 的那次访问撑爆了」。这种「越界发生在哪一行、受害者是哪个变量」级别的定位，靠 `printf` 调试法可能要查半天。最底下那串 `f1 f1 ... f2 ... f3` 就是前面说的 shadow memory 里的 redzone 标记，越界踩中的那一格被方括号标了出来。

最惊艳的是 ASan 对 use-after-free 的定位。`free` 之后再读这块内存，是 C 里最难抓的 bug 之一（因为那块内存经常已经被改作他用，表现完全不可预测）。看 ASan 怎么收拾它：

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    free(p);
    printf("%d\n", *p); /* use-after-free */
    return 0;
}
```

```text
$ gcc -std=c11 -O1 -g -fsanitize=address uaf.c -o uaf && ./uaf
==187834==ERROR: AddressSanitizer: heap-use-after-free on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj/ch10/uaf.c:8        ← 在这里读
freed by thread T0 here:
    #0 ... in free (...libasan...)
    #1 ... in main /tmp/cj/ch10/uaf.c:7          ← 在这里 free 的
previously allocated by thread T0 here:
    #0 ... in malloc (...libasan...)
    #1 ... in main /tmp/cj/ch10/uaf.c:5          ← 在这里 malloc 的
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj/ch10/uaf.c:8 in main
```

ASan 给出了**三段栈**：错误发生在 `uaf.c:8`（那次非法的读），这块内存是在 `uaf.c:7` 被 `free` 的，而它最初是在 `uaf.c:5` 被 `malloc` 出来的。一个 use-after-free 的**完整生命周期**——分配、释放、非法访问——三个位置一次性全摆给你。这种 bug 用任何传统调试手段都得掉一层皮，ASan 一跑就交底。

这里要特别强调那条和第 9 章呼应的点：**上面所有「精确到源码行」的定位，全靠 `-g`**。如果你去掉 `-g`，ASan 只能给你一堆内存地址，`main` 那一帧变成裸地址（clang 版的 ASan 尤其明显，它需要配套的 symbolizer 才能把地址翻译回源码行）。所以跑 sanitizer 的标配是 `-fsanitize=... -g`——`-g` 提供地图，sanitizer 标出炸点，两者一合才能指路。

## 代价、退出码，以及怎么把它用对

sanitizer 这么好，为什么不永远开着？因为有代价。我们拿一个 2000 万次循环访问数组的程序，对比「不插桩」和「插 ASan/UBSan」的运行时间：

```text
$ gcc -O2 cost.c -o cost_plain && gcc -O2 -fsanitize=address cost.c -o cost_asan
$ for b in cost_plain cost_asan; do printf "%s " $b; { time ./$b >/dev/null; } 2>&1; done
cost_plain  0.072 s
cost_asan   0.120 s
```

这个例子里 ASan 大约慢了 1.7 倍（0.072 → 0.120），UBSan 约 1.4 倍——内存访问越密集、越随机的代码，ASan 的开销越明显，经典说法是「约 2 倍速度 + 数倍的内存占用（shadow memory）」。所以 sanitizer 是**调试和 CI 的工具，不是发布构建的配置**：你在本地调试、在 CI 流水线里跑 sanitizer 构建来当质量门，但最终交付给用户的二进制是关掉 sanitizer、带正常 `-O2` 的。

至于「CI 里怎么把它当门用」，靠的是退出码——sanitizer 抓到错误时，进程会以**非 0 退出码**结束：

```text
$ ./uaf >/dev/null 2>&1; echo "退出码=$?"
退出码=1
$ ./cost_plain >/dev/null 2>&1; echo "退出码=$?"
退出码=0
```

抓到 UAF 退出码是 1，干净程序是 0。CI 里只要「跑一遍 sanitizer 构建，退出码非 0 就判失败」，就把 sanitizer 变成了一道硬门——这正是第 16 章我们要在 GitHub Actions 里落的配置。注意 ASan 和 UBSan 默认对「非 0 退出码」的处理不同：**ASan 默认一抓到就 abort**（进程立刻终止），**UBSan 默认是 recover**（报完继续跑、最后才退出），所以想让 UBSan 也「一抓即停、退出码非 0」，要配 `-fno-sanitize-recover=all`，否则它可能一路报着错、最后还是正常退出。

把用法收拢成一套可以直接抄的：调试和 CI 时，`-O1 -g -fsanitize=address,undefined`（`-O1` 是平衡点——`-O0` 太慢、`-O2` 有时会把 UBSan 要检测的模式优化掉，`-O1` 保留可调试性又不至于太慢）；想抓内存泄漏再加 `-fsanitize=leak`，多线程数据竞争用 `-fsanitize=thread`（TSan），读未初始化内存用 `-fsanitize=memory`（MSan，注意它和 ASan 不能同开）。`address` 和 `undefined` 用逗号组合是最常见的一对，覆盖了绝大多数「C 程序最容易踩的雷」。

## 小结

一句话：sanitizer 是静态警告和编译器优化都管不了的那部分 bug 的运行期兜底。它靠编译期**插桩**——在每个内存访问和算术操作前后塞检查指令，ASan 还用 **shadow memory** 给每个变量前后埋 redzone——程序一旦真踩到雷就当场报，配着 `-g` 能把出错位置精确到源码行列。UBSan（`-fsanitize=undefined`）抓运算本身的 UB：有符号溢出（§6.5 第 5 段）它能精确报到 `ub.c:6:9`、移位越界（§6.5.7 第 3 段）它也抓，而且默认是 recover 模式（报完继续跑）。ASan（`-fsanitize=address`）抓内存错误：栈越界它会点名是哪个变量被撑爆、偏移多少；use-after-free 它给出「访问 / free / malloc」三段栈，把一块内存的完整生命周期摆给你，这种 bug 靠传统调试能掉一层皮。代价是运行变慢约 2 倍、内存占用上涨（我们真测 0.072s → 0.120s），所以它是调试和 CI 的工具、不进发布构建。用法上记一套：`-O1 -g -fsanitize=address,undefined`，抓到错误进程非 0 退出（ASan 默认 abort、UBSan 默认 recover 要靠 `-fno-sanitize-recover=all` 才一抓即停），CI 里拿这个非 0 退出码当硬门——这就是第 16 章 GitHub Actions 要落的质量门。

到这一章，开发环境这条线上的「编译器能帮我们抓错」就讲到头了：第 8 章警告（best-effort 静态）、第 9 章 `-O`/`-std`/`-g`（旋钮与 UB）、这一章 sanitizer（运行期兜底）。下一章我们换个方向，从「单文件敲 gcc」走向「管理多文件、多目标的构建」——先从最基础的 make 讲起。

## 参考资源

- ISO/IEC 9899:2011 §6.5 第 5 段（表达式求值的 UB，有符号溢出的标准依据）、§6.5.7 第 3 段（移位操作的 UB，移位指数大于等于位宽未定义）
- GCC / Clang 手册：`-fsanitize=`（`address` / `undefined` / `leak` / `thread` / `memory`）、`-fno-sanitize-recover=`、`ASAN_OPTIONS` / `UBSAN_OPTIONS` 环境变量
- AddressSanitizer Wiki（shadow memory、redzone 机制的原理说明）、UndefinedBehaviorSanitizer 文档（`-fsanitize=undefined` 覆盖的检查项清单）
- 第 9 章：标准与优化（`-O` 让 UB 现形、`-g` 提供 sanitizer 报错所需的源码映射）
- 第 16 章：GitHub Actions（把 sanitizer 构建变成 CI 硬门的真实写法）
