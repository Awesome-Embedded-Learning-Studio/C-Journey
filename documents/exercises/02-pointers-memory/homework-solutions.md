---
title: "阶段 2 课后练习参考答案（Homework）"
description: "指针与内存阶段课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出（gcc 16.1.1 / clang 22.1.8、WSL Arch 实跑，ASan 报告照实贴出）。"
chapter: 2
order: 1
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 45
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 课后练习（Homework）"
related:
  - "阶段 2 各章"
---

# 阶段 2 课后练习参考答案（Homework）

> 所有命令与输出在 WSL Arch（gcc 16.1.1，个别题 clang 22.1.8）下真实运行得到。UB 类题目的输出「只是这台机器这次的选择」，换编译器/优化级别可能不同——每道题里凡是出现这种输出，都会就地说明它属于哪一类。

## 2.1-A {#hw-2-1-a}

**难度 L1** · 题面见 [homework](homework#hw-2-1-a)

**思路**：`&score` 和 `p` 是同一个地址（`p` 里存的就是 `score` 的地址）；`&p` 是 `p` 这个指针变量自己的地址，是另一回事；四种指针 `sizeof` 全 8 说明「指针大小和指向类型无关」。

1. `int* p = &score;` 之后 `&score` 与 `p` 相等；`*p` 是 `score` 的别名，`*p` 加 10 扣 1，`score` 跟着变。→ 知识点：[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)「`&` 取地址，`*` 解引用」一节（`*p` 是别名）
2. `int** pp = &p;` 顺着走两层，`**pp = 99` 改的还是 `score`；`&p` 与 `p` 的值不同——`p` 存的是 `score` 的地址，`&p` 是 `p` 自己占的那块内存的地址。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)「指针自己也是个变量，有自己的地址」一节
3. 三个 `sizeof` 都是 8：64 位机上任何指针装的都只是一份地址，宽度由地址空间决定、与指向类型无关。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)「指针的类型与大小」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw21a.c -o hw21a && ./hw21a
&score = 0x...
p     = 0x...            ← 和 &score 同一个地址
加 10 分后: score = 71, *p = 71
扣 1 分后: score = 70, *p = 70
**pp 改后: score = 99
&p = 0x... (p 自己的地址,和 p 的值不同)
sizeof(int*)    = 8
sizeof(char*)   = 8
sizeof(double*) = 8
```

## 2.1-B {#hw-2-1-b}

**难度 L2** · 题面见 [homework](homework#hw-2-1-b)

**思路**：(a) 解引用 NULL 必崩在地址 0；(b) 野指针的值是垃圾——本机这次恰好是 0，所以「看起来」和 (a) 一样，但这纯属运气；(c) 是合法对照，全程无警告无崩溃。

1. (a) 普通构建段错误、退出码 139；ASan 报 `SEGV on unknown address 0x000000000000`——地址 0 那页被操作系统映射成不可访问。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)「两个必崩的坑」一节（NULL 解引用）
2. (b) 编译期 `-Wuninitialized` 就响了；本机普通构建这次退出码 139，因为 `q` 那个栈槽里的垃圾**恰好是 0**。但换成 ASan 的 `-O0` 构建，垃圾值又恰好落在**合法栈地址**——写入「成功」、静默退出 0，ASan 也拦不住（它管地址合不合法，而这回地址恰好合法）。这就是教材强调的：野指针可能崩、可能不崩、可能「成功」把 2 写进别人内存，哪种结果都不保证；换一次运行、换 `-O` 级别、换编译器，垃圾值都可能不同。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)（野指针的 UB 本质）
3. (c) 合法程序无警告、正常打印 3。→ 知识点：[第 1 章](/02-pointers-memory/01-what-is-a-pointer)（对照：指针指向已知对象才安全）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw21b1.c -o hw21b1 && ./hw21b1; echo "exit=$?"
Segmentation fault
exit=139
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw21b1.c -o hw21b1a && ./hw21b1a
==278==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
==278==The signal is caused by a WRITE memory access.
    #0 0x... in main /tmp/cj-ex2-hw/hw21b1.c:5
SUMMARY: AddressSanitizer: SEGV /tmp/cj-ex2-hw/hw21b1.c:5 in main
$ gcc -std=c11 -Wall -Wextra hw21b2.c -o hw21b2
hw21b2.c:5:8: warning: 'q' is used uninitialized [-Wuninitialized]
    5 |     *q = 2;
      |     ~~~^~~
$ ./hw21b2; echo "exit=$?"
Segmentation fault
exit=139                        ← 普通构建这次垃圾值恰好是 0,崩了
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw21b2.c -o hw21b2a && ./hw21b2a
q = 0x7ffdb92bc538             ← ASan 构建这次垃圾值落在合法栈地址
exit=0                          ← 写入「成功」,静默退出,ASan 也拦不住
                                ← UB 崩不崩纯看运气:换 -O1 又可能崩在地址 0
$ gcc -std=c11 -Wall -Wextra hw21b3.c -o hw21b3 && ./hw21b3
x = 3
```

## 2.2-A {#hw-2-2-a}

**难度 L2** · 题面见 [homework](homework#hw-2-2-a)

**思路**：指针遍历就是 `for (p = a; p < a + n; p++)`；`p - a` 是元素个数差；`long*` 加 1 跨 `sizeof(long)` = 8 字节。

1. 一个循环同时干求和、找最大值，循环结束 `p` 停在 past-the-last，`p - a` 得 6。→ 知识点：[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)「用指针遍历数组」「两个指针相减」两节
2. `lp` 与 `lp + 1` 差 8 字节：步长 = `sizeof(指向类型)`。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)「指针加减整数」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw22a.c -o hw22a && ./hw22a
sum = 54, max = 19
走过的元素数 p - a = 6
lp     = 0x...
lp + 1 = 0x...  (long* 加 1 跨 8 字节)
```

## 2.2-B {#hw-2-2-b}

**难度 L3** · 题面见 [homework](homework#hw-2-2-b)

**思路**：双指针从两头往中间走，交换、逼近；终止条件 `lo < hi` 保证奇数个元素时中间那个不被自己和自己交换（那也没错，但多此一举），更重要的是 `lo` 永远碰不到「超过 `hi`」的位置——两个指针只在同一个数组里移动，这正是指针比较 `lo < hi` 有定义的前提（§6.5.8）。

1. `int* lo = a; int* hi = a + n - 1;`——`a + n` 是合法的 past-the-last（可以「算」出来），但解引用只能到 `a + n - 1`，所以末元素指针取 `a + n - 1`。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)「越界与 past-the-last」一节
2. 每轮交换 `*lo` 与 `*hi`，`lo++`/`hi--` 各自走一步；偶数个元素两两相遇后交错（`lo > hi`）退出，奇数个元素在中间元素相遇（`lo == hi`）退出。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)（步长与比较）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw22b.c -o hw22b && ./hw22b
偶数个: 6 5 4 3 2 1
奇数个: 5 4 3 2 1
```

## 2.3-A {#hw-2-3-a}

**难度 L2** · 题面见 [homework](homework#hw-2-3-a)

**思路**：C 没有 reference，想让函数改调用者的变量只能传指针；`square_wrong` 改的是副本。

1. `square(&x)` 里 `*p` 是 `x` 的别名，改 `*p` 就是改 `x`；`square_wrong(y)` 传的是副本，`y` 纹丝不动。→ 知识点：[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)「值传递为什么改不了」「传指针」两节
2. `toggle` 里 `*flag = !*flag` 先读别名再写回，两次调用 0→1→0。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)（指针参数的输入/输出之分）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw23a.c -o hw23a && ./hw23a
square 后 x = 25
square_wrong 后 y = 6 (没变)
第一次 toggle: 1
第二次 toggle: 0
```

## 2.3-B {#hw-2-3-b}

**难度 L3** · 题面见 [homework](homework#hw-2-3-b)

**思路**：C 只能 `return` 一个值，「多返回值」的套路是一个用 `return`、其余用指针参数带出；`h`/`m`/`s` 是纯输出参数（调用者事先声明变量、传地址进去等着被写）。

1. `*h = (total_sec % 86400) / 3600;` 等三行先算各字段再写回；天数 `return total_sec / 86400;`。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)「用指针模拟『多返回值』」一节
2. 调用点声明 `int h, m, s;` 传 `&h, &m, &s`——不初始化没关系，函数会写；读代码的人看到 `int* h` 且无 `const`，就该判断它是「输出」。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)（区分输入/输出参数）、[第 4 章：const 限定](/02-pointers-memory/04-const-qualifier)（下一章用 const 标记输入）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw23b.c -o hw23b && ./hw23b
90061 秒 = 1 天 1 时 1 分 1 秒
61 秒 = 0 天 0 时 1 分 1 秒
```

## 2.4-A {#hw-2-4-a}

**难度 L2** · 题面见 [homework](homework#hw-2-4-a)

**思路**：const 三态的分工——`const` 在 `*` 前锁对象、在 `*` 后锁指针；两个失败用例各报一种「只读」。

1. ①通过：`p = &m` 改指向合法，`*p` 读的是新目标 9。②失败：`assignment of read-only location '*p'`——`const int*` 锁的是「指的对象」。→ 知识点：[第 4 章：const 限定](/02-pointers-memory/04-const-qualifier)「指针的 const 三态」一节
2. ③失败：`assignment of read-only variable 'q'`——`int* const` 锁的是指针本身（顺带一个 `-Wunused-but-set-variable` 杂音，因为 `q` 只赋值没读过）；④通过：`*q = 9` 合法，`n` 变成 9。→ 知识点：[第 4 章](/02-pointers-memory/04-const-qualifier)（两种报错措辞的分工）
3. `const int*` ≡ `int const*`：口诀「const 修饰左边紧挨的东西、在最左则修饰右边的类型」。→ 知识点：[第 4 章](/02-pointers-memory/04-const-qualifier)（读法口诀）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw24a1.c -o hw24a1 && ./hw24a1
*p = 9
$ gcc -std=c11 -Wall -Wextra hw24a2.c -o hw24a2
hw24a2.c:4:8: error: assignment of read-only location '*p'
    4 |     *p = 9;
      |        ^
$ gcc -std=c11 -Wall -Wextra hw24a3.c -o hw24a3
hw24a3.c:4:7: error: assignment of read-only variable 'q'
    4 |     q = &m;
      |       ^
$ gcc -std=c11 -Wall -Wextra hw24a4.c -o hw24a4 && ./hw24a4
n = 9
```

## 2.4-B {#hw-2-4-b}

**难度 L3** · 题面见 [homework](homework#hw-2-4-b)

**思路**：同一个「丢掉 const 再改」的动作，三个版本三种结局——这正是 UB 的教具：`-O0` 的局部 const 在栈上、恰好可写；`-O2` 下编译器按「const 对象不会被改」的假定直接常量折叠；全局 const 落在只读段，写它就段错误。

1. `-O0` 局部版打印 `x = 20`——栈上的 `x` 被绕过 const 改了。这是 UB，但本机这次「成功」了。→ 知识点：[第 4 章](/02-pointers-memory/04-const-qualifier)「const 正确性」一节（丢 const 改对象是 UB）
2. 全局版段错误、退出码 139：全局 const 住在 `.rodata`，那页只读，一写就 SIGSEGV。→ 知识点：[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)「程序的六大内存区」一节（`.rodata` 只读）
3. `-O2` 局部版打印 `x = 10`：编译器有权假定 `const int x` 永远不变，直接把 `printf` 里的 `x` 替换成常量 10——你改的栈内存它看都不看。→ 知识点：[第 4 章](/02-pointers-memory/04-const-qualifier)（UB 与优化，教材「哪种结果都不保证」的原话兑现）
4. `-Wdiscarded-qualifiers` 就是第 4 章那个警告：隐式把 `const int*` 赋给 `int*` 时提醒你「const 被丢了」；本题用显式 `(int*)` 强转压掉警告，等价于对编译器说「我知道、后果自负」——后果你三个版本都看见了。→ 知识点：[第 4 章](/02-pointers-memory/04-const-qualifier)（const 正确性：const 要一路传递）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw24b1.c -o hw24b1 && ./hw24b1
x = 20                          ← -O0:局部 const 在栈上,恰好可写
$ gcc -std=c11 -Wall -Wextra hw24b2.c -o hw24b2 && ./hw24b2; echo "exit=$?"
Segmentation fault
exit=139                        ← 全局 const:.rodata 只读,写它段错
$ gcc -std=c11 -Wall -Wextra -O2 hw24b1.c -o hw24b1o2 && ./hw24b1o2
x = 10                          ← -O2:编译器假定 const 不变,常量折叠
```

## 2.5-A {#hw-2-5-a}

**难度 L2** · 题面见 [homework](homework#hw-2-5-a)

**思路**：手写 `strchr` 就是教材那套「`char*` 遍历法」加一个提前返回；`pos - s` 是第 2 章的指针减法。

1. `while (*s)` 走到 `\0` 停，途中撞见 `c` 就 `return s`——返回的是指向 `s` 内部的指针；参数和返回值都是 `const char*`：函数只读不改，返回值也不给调用者改的口子，const 从声明一路传下去。→ 知识点：[第 5 章：指针、数组、字符串的统一视角](/02-pointers-memory/05-pointer-array-string)「用 char* 遍历字符串」一节、[第 4 章](/02-pointers-memory/04-const-qualifier)（const 正确性）
2. `'o'` 出现在下标 4，`pos - s` 得 4；`'w'` 得 6；`'z'` 找不到返回 `NULL`。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)（指针减法）、[第 5 章](/02-pointers-memory/05-pointer-array-string)（`NULL` 表示「查无结果」，第 1 章见过）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw25a.c -o hw25a && ./hw25a
找 'o': pos - s = 4, 指向 "o world"
找 'w': pos - s = 6, 指向 "world"
找 'z': NULL
```

## 2.5-B {#hw-2-5-b}

**难度 L3** · 题面见 [homework](homework#hw-2-5-b)

**思路**：`strcat` = 「走到 dst 的 `\0`」+「strcpy 的经典一行」两段；它不检查 `dst` 还剩多少空间，装不下就写越界——这正是 `strcat` 不安全的原因，ASan 报告里的 `WRITE of size 1` 会点出越界发生在哪。

1. 第一段循环 `while (*p) p++;` 走到 `\0`；第二段 `while ((*p++ = *src++) != '\0')` 把「取字符、赋字符、两指针后移、判 `\0`」压进一个表达式，拷到 `\0` 为止（`\0` 已被带过去）。→ 知识点：[第 5 章](/02-pointers-memory/05-pointer-array-string)「手写 strcpy」一节（经典一行）、「手写 strlen」一节（走到 `\0`）
2. 32 字节缓冲正常得 `pointer and memory`；16 字节缓冲装不下（"pointer" 7 字节 + " and memory" 11 字节 + `\0` 共 19 字节），ASan 报 `stack-buffer-overflow`、`WRITE of size 1`，点名 `small` 这个变量在偏移 48 处被写穿——16 字节 `small` 恰好从偏移 32 开始，写到偏移 48 就是第 17 个字节。→ 知识点：[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)（越界的 ASan 报法，栈版 `stack-buffer-overflow`）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw25b.c -o hw25b && ./hw25b
buf = pointer and memory
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw25b_small.c -o hw25bs && ./hw25bs
==385==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
WRITE of size 1 at 0x... thread T0
    #0 0x... in my_strcat /tmp/cj-ex2-hw/hw25b_small.c:8
    #1 0x... in main /tmp/cj-ex2-hw/hw25b_small.c:15
  This frame has 1 object(s):
    [32, 48) 'small' (line 14) <== Memory access at offset 48 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow /tmp/cj-ex2-hw/hw25b_small.c:8 in my_strcat
```

## 2.6-A {#hw-2-6-a}

**难度 L2** · 题面见 [homework](homework#hw-2-6-a)

**思路**：`malloc` 返回 `void*`，C 里隐式转成任意对象指针（§6.3.2.3p1），所以不写 `(double*)`；强转在 C++ 里才必需，在 C 里反而会掩盖「忘了 `#include <stdlib.h>` 导致的隐式声明」这类老 bug。

1. `double* a = malloc((size_t) n * sizeof(double));`——`(size_t)` 是第 6 章教的防溢出纪律；拿到后先查 `NULL`，然后 `a[i]` 像普通数组一样用（`p[i] ≡ *(p+i)` 对 `malloc` 来的指针同样成立）。→ 知识点：[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)「malloc：要一块堆内存」一节
2. 前 8 项倒数序列之和 2.718——和自然常数 e 的前几位（2.71828…）撞上了，**纯属巧合**：e 的真实级数是 Σ1/k!（阶乘倒数），而 1 + 1/2 + 1/3 + … 是发散的**调和级数**、与 e 毫无关系。用完 `free(a); a = NULL;`，ASan 构建零报告、退出码 0。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)「free 与内存泄漏」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw26a.c -o hw26a && ./hw26a
倒数序列: 1.000 0.500 0.333 0.250 0.200 0.167 0.143 0.125
sum = 2.718
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw26a.c -o hw26aa && ./hw26aa
倒数序列: 1.000 0.500 0.333 0.250 0.200 0.167 0.143 0.125
sum = 2.718                      ← ASan 零报告,退出码 0
```

## 2.6-B {#hw-2-6-b}

**难度 L3** · 题面见 [homework](homework#hw-2-6-b)

**思路**：动态数组的标配——「长度 `len` + 容量 `cap`」分开记，放满就 tmp 模式翻倍。`realloc` 失败时返回 `NULL` 而原指针仍有效，直接 `a = realloc(a, ...)` 一旦失败就把原地址弄丢、既泄漏又丢数据，所以必须 `tmp` 接、判成功、再赋回。

1. 容量 2→4→8→16 三次翻倍（10 个元素最终容量 16），每次扩容打印一行，全程数据保留。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)「realloc：调整一块已有内存的大小」一节（tmp 模式）
2. 失败分支 `free(a); return 1;` 是教材强调的细节：失败时原 `a` 还活着，要么继续用、要么手动释放。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)（realloc 失败处理）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw26b.c -o hw26b && ./hw26b
扩容: 容量 -> 4
扩容: 容量 -> 8
扩容: 容量 -> 16
最终容量 = 16, 元素 10 个:
3 1 4 1 5 9 2 6 5 3
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw26b.c -o hw26ba && ./hw26ba
... (同样输出,零报告)
```

## 2.7-A {#hw-2-7-a}

**难度 L2** · 题面见 [homework](homework#hw-2-7-a)

**思路**：三个程序各踩一种坑，ASan 各报一种类型——这题的关键动作是**别加 `-O1`**：本机真跑过，`-O1` 下 gcc 会把 double-free 的第二次 `free` 和越界写 `a[3] = 99` 直接优化折叠掉（UB 让编译器「有权」这么干），ASan 什么都抓不到、退出码 0。默认 `-O0` 才还原三个坑的原貌。

1. (a) 编译期 `-Wuse-after-free` 先响（`free` 和后续 `*p` 在同一个函数里编译器看得见），ASan 报 `heap-use-after-free`、`READ of size 4`，还贴心地列出「在哪 free、在哪 malloc」。→ 知识点：[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)「use-after-free」一节
2. (b) ASan 报 `attempting double-free`，并指出第一次 `free` 的位置。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「double-free」一节
3. (c) ASan 报 `heap-buffer-overflow`、`WRITE of size 4`，报告里写清「0 bytes after 12-byte region」——第 13 个字节就是 `a[3]`。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「堆越界」一节
4. `free(p); p = NULL;` 一举两得：置 NULL 后误用 `*p` 是解引用空指针、立刻崩在明处（而不是悄悄读别人数据）；再 `free(p)` 时 `free(NULL)` 是合法空操作。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「小结」一节

**验证输出**（关键行，地址/进程号每次不同）：

```text
$ gcc -std=c11 -Wall -Wextra hw27a1.c -o hw27a1
hw27a1.c:8:5: warning: pointer 'p' used after 'free' [-Wuse-after-free=]
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw27a1.c -o hw27a1a && ./hw27a1a
==429==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex2-hw/hw27a1.c:8
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj-ex2-hw/hw27a1.c:8 in main
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw27a2.c -o hw27a2a && ./hw27a2a
==435==ERROR: AddressSanitizer: attempting double-free on 0x... in thread T0:
    #1 0x... in main /tmp/cj-ex2-hw/hw27a2.c:6
SUMMARY: AddressSanitizer: double-free /tmp/cj-ex2-hw/hw27a2.c:6 in main
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw27a3.c -o hw27a3a && ./hw27a3a
==441==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
WRITE of size 4 at 0x... thread T0
0x... is located 0 bytes after 12-byte region
    #0 0x... in main /tmp/cj-ex2-hw/hw27a3.c:9
SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/cj-ex2-hw/hw27a3.c:9 in main
$ # 对照:-O1 把 UB 折叠掉了,ASan 抓不到
$ gcc -std=c11 -Wall -Wextra -O1 -g -fsanitize=address hw27a2.c -o hw27a2o1 && ./hw27a2o1; echo "exit=$?"
exit=0
$ gcc -std=c11 -Wall -Wextra -O1 -g -fsanitize=address hw27a3.c -o hw27a3o1 && ./hw27a3o1
a[3] = 99
exit=0                           ← UB 被优化折叠:这就是「可能崩可能不崩」的又一面
```

## 2.7-B {#hw-2-7-b}

**难度 L3** · 题面见 [homework](homework#hw-2-7-b)

**思路**：`make_label` 分配、调用者释放——「谁分配谁释放」必须配对，不写清楚就会两边都以为对方会 `free`、结果谁都没 `free`。

1. 泄漏版：LeakSanitizer 在程序退出时报 `192 byte(s) leaked in 3 allocation(s)`——64 × 3 正好对账。注意一个真实细节：泄漏版的 `printf` 输出在报告里**根本没出现**——ASan 的退出路径不走 stdio 缓冲刷新，`label-1/2/3` 还憋在 stdout 缓冲区里。修复版正常退出才打印出来。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)「内存泄漏」一节（LeakSanitizer 在退出时检查）
2. 修复版补上三个 `free`（并置 NULL）后零报告、退出码 0。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)「free 与内存泄漏」一节（谁 malloc 谁 free）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw27b.c -o hw27ba && ./hw27ba
==465==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 192 byte(s) in 3 object(s) allocated from:
    #1 0x... in make_label /tmp/cj-ex2-hw/hw27b.c:6
SUMMARY: AddressSanitizer: 192 byte(s) leaked in 3 allocation(s).
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw27b2.c -o hw27b2a && ./hw27b2a
label-1
label-2
label-3                        ← 修复版零报告
```

## 2.8-A {#hw-2-8-a}

**难度 L2** · 题面见 [homework](homework#hw-2-8-a)

**思路**：`argv` 是 `char**`——「指针数组 `char* argv[]` 在函数参数里退化成的指针」；`argv[argc]` 按约定是 `NULL` 哨兵，所以指针遍历版拿它当循环条件。

1. 下标版 `for (i = 0; i < argc; i++)` 与指针版 `for (char** p = argv; *p != NULL; p++)` 输出一致；`argv[argc] == NULL` 打印 1。→ 知识点：[第 8 章：多级指针与指针数组](/02-pointers-memory/08-multi-level-pointers)「命令行参数」一节
2. `argv[0]` 是程序名 `./hw28a`——`argc = 4` 表示程序名 + 3 个参数。→ 知识点：[第 8 章](/02-pointers-memory/08-multi-level-pointers)（`argv[argc]` 是 NULL 哨兵）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw28a.c -o hw28a && ./hw28a alpha beta gamma
argc = 4
--- 下标版 ---
argv[0] = ./hw28a
argv[1] = alpha
argv[2] = beta
argv[3] = gamma
--- 指针遍历版 ---
./hw28a alpha beta gamma
argv[argc] == NULL ? 1
```

## 2.8-B {#hw-2-8-b}

**难度 L3** · 题面见 [homework](homework#hw-2-8-b)

**思路**：行指针 `int (*row)[4]` 指向「4 个 `int` 的数组」，`row++` 的步长是 4 × 4 = 16 字节；`int* p = m` 的编译诊断实锤了 `m` 退化成的是 `int (*)[4]` 而不是 `int*`。

1. `(*row)[j]` 先解引用拿到一整行（一个 4 元素数组），再下标取元素；每行求完 `row++` 跨行。→ 知识点：[第 8 章](/02-pointers-memory/08-multi-level-pointers)「数组指针」一节
2. `(void*) m` 与 `(void*) (m + 1)` 差 0x10（16 字节）。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)（步长由指向类型决定）
3. `int* p = m;` 报 `initialization of 'int *' from incompatible pointer type 'int (*)[4]'`——这是约束违反，gcc/clang 默认就报 **hard error**（连 `-Werror` 都不用加）；报错本身就证明 `m` 的类型。→ 知识点：[第 10 章：复杂声明与 typedef](/02-pointers-memory/10-complex-declarations-typedef)（`[]` 优先级高于 `*`，两者区别全在括号）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw28b.c -o hw28b && ./hw28b
第 0 行和 = 10
第 1 行和 = 26
第 2 行和 = 42
总和 = 78
行指针步长: m = 0x..., m + 1 = 0x...   ← 差 0x10 = 16 字节
$ gcc -std=c11 -Wall -Wextra hw28b_bad.c -o hw28b_bad
hw28b_bad.c:3:14: error: initialization of 'int *' from incompatible pointer type 'int (*)[4]'
```

## 2.9-A {#hw-2-9-a}

**难度 L2** · 题面见 [homework](homework#hw-2-9-a)

**思路**：函数指针作参数就是「回调」——`apply1` 只负责「把一个数喂给某个函数」，喂哪个由调用方定；`neg` 与 `&neg` 同地址是「函数名退化」的实锤。

1. 三次调用得 -6/12/36，`apply1` 一行没改。→ 知识点：[第 9 章：函数指针](/02-pointers-memory/09-function-pointers)「函数指针作参数」一节
2. `(void*) neg == (void*) &neg`：函数名在表达式里自动退化成函数指针（§6.7.6.3p8，和数组名退化同理）。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)「赋值与调用」一节
3. `fp(x)` 与 `(*fp)(x)` 等价（§6.5.2.2 函数调用语义允许两种写法），工程里现在写前者更干净。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)（两种调用等价）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw29a.c -o hw29a && ./hw29a
apply1(neg, 6) = -6
apply1(dbl, 6) = 12
apply1(sq, 6)  = 36
neg = 0x..., &neg = 0x... (函数名退化)   ← 两个地址相同
```

## 2.9-B {#hw-2-9-b}

**难度 L4** · 题面见 [homework](homework#hw-2-9-b)

**思路**：`qsort` 排结构体，比较函数里 `void*` 先转回 `const struct Item*`；`double` 相减转 `int` 是双重错误——既可能丢掉小数（差小于 1 时截成 0，两个不同的价格被判「相等」）、也可能溢出，教材的「别写 `return ia - ib`」在这里变成「别写 `return (int)(a->price - b->price)`」。

1. 正确版显式 `>`/`<` 返回 ±1/0，降序得 tea 29.90 / milk 12.00 / rice 8.70 / bread 8.50 / egg 1.20。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)「标准库 qsort」一节（显式返回 -1/0/1 最稳妥）
2. 错误版（降序意图的 `(int)(b->price - a->price)`）对 `bread`(8.50) 与 `rice`(8.70) 算出 `(int)(-0.2) = 0`，比较函数声称「相等」——两个元素相对顺序保持原样，输出里 `bread 8.50` 排在 `rice 8.70` **前面**，降序被破坏。注意两点：错序能否看到，取决于**初始顺序**和 **qsort 实现**（glibc 对小数组走插入排序、通常保持原序，所以本机恰好可见；但「被判相等后谁在前」没有保证，换数据、换 libc 就可能不同）——所以错误比较器的坑是「相对顺序不可依赖」，不是「必然在这个位置排错」。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)（qsort 比较约定：0 表示相等）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra q.c -o q && ./q
正确比较函数(降序):
tea    29.90
milk   12.00
rice    8.70
bread   8.50
egg     1.20
错误比较函数:
tea    29.90
milk   12.00
bread   8.50      ← 8.50 排在 8.70 前面:差 -0.2 被截成 0,降序被破坏
rice    8.70
egg     1.20
```

## 2.10-A {#hw-2-10-a}

**难度 L2** · 题面见 [homework](homework#hw-2-10-a)

**思路**：右左法则逐个拆：`char* months[2]` 先与 `[2]` 结合（`[]` 优先级高于 `*`）是数组、元素是 `char*`；`int (*row)[3]` 括号让 `*` 先结合是指针、指向 `int[3]`；`int* first(int*)` 先与 `()` 结合是函数、返回 `int*`。

1. `sizeof(months)` = 16：2 个指针 × 8 字节——指针数组是一整块装指针的内存。→ 知识点：[第 10 章：复杂声明与 typedef](/02-pointers-memory/10-complex-declarations-typedef)「右左法则」一节
2. `(*row)[2]` 第 0 行得 3，`row++` 后第 1 行得 6。→ 知识点：[第 10 章](/02-pointers-memory/10-complex-declarations-typedef)（数组指针）、[第 8 章](/02-pointers-memory/08-multi-level-pointers)（行指针）
3. `first(a)` 返回 `a` 的首元素地址（数组退化成指针传进去），解引用得 10；它不是函数指针——函数指针必须 `(*f)(...)` 用括号把 `*` 和名字包住，`int* first(int*)` 里的 `first` 先和参数列表结合成了「函数」。→ 知识点：[第 10 章](/02-pointers-memory/10-complex-declarations-typedef)（`int *f(void)` 与 `int (*f)(void)` 风马牛不相及）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw210a.c -o hw210a && ./hw210a
char* months[2]: sizeof = 16 (2 个指针)
第 0 行 (*row)[2] = 3
第 1 行 (*row)[2] = 6
first(a) 返回首元素 = 10
```

## 2.10-B {#hw-2-10-b}

**难度 L4** · 题面见 [homework](homework#hw-2-10-b)

**思路**：①`#define` 是预处理文本替换，`STR_PTR a, b;` 展开成 `char* a, b;`——`*` 只跟 `a`，`b` 落成纯 `char`；`typedef` 把「指向 `char` 的指针」封装成完整类型，对每个声明符都生效。②`int (*pipeline[2])(int)` 按右左法则：`pipeline` 是 2 元素数组、元素是指针、指针指向 `int(int)` 函数。

1. 宏版 `sizeof` 8 vs 1，typedef 版 8 vs 8。→ 知识点：[第 10 章](/02-pointers-memory/10-complex-declarations-typedef)「typedef vs #define」一节
2. `typedef int (*Unary)(int);` 之后 `Unary pipeline[2] = {dbl, neg};` 清爽如初；`pipeline[0](5)` 得 10、`pipeline[1](pipeline[0](5))` 是 `neg(dbl(5))` 得 -10——先翻倍再取负。→ 知识点：[第 10 章](/02-pointers-memory/10-complex-declarations-typedef)「typedef 拯救声明」一节、[第 9 章](/02-pointers-memory/09-function-pointers)（函数指针数组）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw210b.c -o hw210b && ./hw210b
宏版: sizeof(a) = 8, sizeof(b) = 1
typedef 版: sizeof(x) = 8, sizeof(y) = 8
pipeline[0](5) = 10 (dbl)
pipeline[1](pipeline[0](5)) = -10 (neg(dbl(5)))
```

## 2.11-A {#hw-2-11-a}

**难度 L2** · 题面见 [homework](homework#hw-2-11-a)

**思路**：`unsigned char*` 是 C 的天生字节指针（`sizeof(char) == 1` 钉死），加 1 跨 1 字节；`0x1234` 打在内存里是 `34 12` 说明本机小端（最低有效字节在最低地址）。

1. 逐字节打印 `0x34 0x12`——x86_64 小端序；字节序是实现定义的，跨平台代码不能假设。→ 知识点：[第 11 章：void* 与字节操作](/02-pointers-memory/11-void-ptr-and-byte-ops)「unsigned char*：当字节指针用」一节
2. `void*` round-trip：`&x` → `void*` → `short*` 地址一个比特不变，`*back` 读回 `0x1234`（§6.3.2.3p1/p7）。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)「void* 的两条规矩」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw211a.c -o hw211a && ./hw211a
x = 0x1234, 占 2 字节
逐字节(低地址到高地址): 0x34 0x12      ← 小端:最低有效字节在最低地址
&x = 0x..., back = 0x..., *back = 0x1234 (转换无损)
```

## 2.11-B {#hw-2-11-b}

**难度 L3** · 题面见 [homework](homework#hw-2-11-b)

**思路**：①右移一位时源和目的区间重叠，从前往后手抄会先把自己写进去的值又读出来（`1 1 1 1 1`），`memmove` 内部先处理重叠方向所以结果正确；②字节序反转是「字节视角」的经典应用——`memcpy` 进字节数组、反向、`memcpy` 回，全程不碰移位。

1. `memmove(a + 1, a, 4 * sizeof(int))` 得 `1 1 2 3 4`；手抄版 `b[i] = b[i - 1]` 把 1 一路复制过去得 `1 1 1 1 1`。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)「memcpy 与 memset」一节（重叠请换 memmove）、[第 6 章](/02-pointers-memory/06-malloc-free-basics)（重叠的根源：同一块连续内存）
2. `swap_endian(0x12345678)` = `0x78563412`，再换一次还原——两次反转回到原点。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（字节序、htonl/ntohl 处理的就是这个差异）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw211b.c -o hw211b && ./hw211b
memmove 右移: 1 1 2 3 4
手抄错误版: 1 1 1 1 1
swap_endian(0x12345678) = 0x78563412
再换一次还原 = 0x12345678
```

## 2.12-A {#hw-2-12-a}

**难度 L2** · 题面见 [homework](homework#hw-2-12-a)

**思路**：六类地址分成三层——栈最高、堆居中、程序映像（`.rodata`/`.data`/`.bss`）在低地址块；`nm` 的类型字母（`D`/`B`/`T`）与段一一对应。

1. 地址从高到低：栈 `0x7ffe...` > 堆 `0x5770a4...` > `.bss`/`.data`（`0x57707e...` 里 `.bss` 紧挨 `.data` 之后）> `.rodata`（最低）。`global_uninit = 0` 是 `.bss` 启动清 0 的招牌。→ 知识点：[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)「程序的六大内存区」一节
2. `nm`：`D global_init`（已初始化→`.data`）、`B global_uninit`（未初始化→`.bss`）、`T main`（代码→`.text`）。→ 知识点：[第 12 章](/02-pointers-memory/12-memory-layout)「用 nm 看符号落哪个段」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw212a.c -o hw212a && ./hw212a
栈      &stack_local    0x7ffe...      ← 最高
堆      heap            0x5770a4...
.data   &global_init    0x57707e...
.data   &static_local   0x57707e...
.bss    &global_uninit  0x57707e...    ← 紧挨 .data
.rodata literal         0x57707e...    ← 程序映像里最低
global_uninit = 0 (.bss 启动清 0)
$ nm hw212a | grep -E 'global_(init|uninit)| main$'
0000000000004030 D global_init
000000000000403c B global_uninit
0000000000001169 T main
```

## 2.12-B {#hw-2-12-b}

**难度 L4** · 题面见 [homework](homework#hw-2-12-b)

**思路**：①每深一层函数调用，新帧落在更低的地址——栈向低地址增长；②爆栈时进程被 SIGSEGV 直接打死，stdout 的块缓冲来不及刷，所以用 `fflush(stdout)` 逼出每五万层的记录；③程序映像内 `.rodata` 在 `.data` 之下（上题输出已实锤）。

1. `walk` 五层地址从 `0x...3454` 一路递减到 `0x...3394`，每层帧约 0x30 字节。→ 知识点：[第 12 章](/02-pointers-memory/12-memory-layout)「栈向低地址增长」一节
2. 深递归：gcc 编译期就报 `-Winfinite-recursion`（没有基线的递归它认得出来）；本机跑到约 50 万层附近爆栈（8 MB 栈 ÷ 每帧 16 字节，数量级对得上），段错误退出码 139。这个层数是这台机器这次的数——栈上限随系统配置不同、每帧大小随编译器与优化级别不同（16~48 字节都正常），重要的是「越深越往下、终有尽头」。→ 知识点：[第 12 章](/02-pointers-memory/12-memory-layout)（爆栈的机制）、[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)（段错误退出码 139）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw212b.c -o hw212b && ./hw212b
depth 0: &frame = 0x...
depth 1: &frame = 0x...
depth 2: &frame = 0x...
depth 3: &frame = 0x...
depth 4: &frame = 0x...            ← 地址一路变小
$ gcc -std=c11 -Wall -Wextra hw212b2.c -o hw212b2
hw212b2.c:5:13: warning: infinite recursion detected [-Winfinite-recursion]
$ ./hw212b2; echo "exit=$?"
depth = 50000
depth = 100000
depth = 150000
depth = 200000
depth = 250000
Segmentation fault
exit=139
```

## 2.C-1 {#hw-2-c-1}

**难度 L3** · 题面见 [homework](homework#hw-2-c-1)

**思路**：argv（第 8 章）+ malloc 动态数组（第 6 章）+ qsort 回调（第 9 章）三件套拼成一个命令行排序器；`nums` 的分配和释放都在 `main` 里配对，包括解析失败的错误分支也要先 `free` 再退出。

1. `int* nums = malloc((size_t) n * sizeof(int));` 查 `NULL`；循环里 `sscanf(argv[i + 1], "%d", &nums[i])`——`&nums[i]` 是「指针改调用者变量」的变体（`sscanf` 把解析结果写进你给的地址）。→ 知识点：[第 3 章](/02-pointers-memory/03-pointer-parameters)（scanf 家族靠指针写回）、[第 6 章](/02-pointers-memory/06-malloc-free-basics)（malloc 必查 NULL）
2. `cmp_asc` 显式返回 -1/0/1（不用 `a - b`，防有符号溢出）；`qsort(nums, n, sizeof(int), cmp_asc)` 原地排序。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)「标准库 qsort」一节
3. `free(nums); nums = NULL;` 结束——分配与释放同处 `main`，「谁分配谁释放」配对清晰；ASan 构建零报告。→ 知识点：[第 7 章](/02-pointers-memory/07-dynamic-memory-pitfalls)（谁分配谁释放）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw2c1.c -o hw2c1 && ./hw2c1 42 7 19 3 88
排序结果: 3 7 19 42 88
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw2c1.c -o hw2c1a && ./hw2c1a 42 7 19 3 88
排序结果: 3 7 19 42 88               ← 零报告
```

## 2.C-2 {#hw-2-c-2}

**难度 L4** · 题面见 [homework](homework#hw-2-c-2)

**思路**：`qsort` 同款签名 = `void*` 兜底任意类型 + 函数指针接收比较逻辑 + `size` 算偏移；`void*` 不能算术，所以一进来就 `(unsigned char*) base` 转字节指针，第 j 个元素是 `p + j * size`。冒泡排序本身是教材外补充（阶段 3 才细讲），这里只需要「相邻两两比较交换 n 轮」这个直白骨架。

1. `my_sort` 用 `cmp(left, right) > 0` 决定是否交换，交换按字节逐个搬（`unsigned char tmp`）。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（void* 转字节指针）、[第 9 章](/02-pointers-memory/09-function-pointers)（qsort 的比较约定）
2. 三个比较函数各司其职；字符串版的元素是 `char*`（8 字节一个），比较函数里先转 `const char* const*` 再解一层拿到真正的 `char*`，交给 `strcmp` 比字典序。→ 知识点：[第 8 章](/02-pointers-memory/08-multi-level-pointers)（指针数组）、[第 5 章](/02-pointers-memory/05-pointer-array-string)（strcmp）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw2c2.c -o hw2c2 && ./hw2c2
int 升序: 3 7 19 42 88
int 降序: 88 42 19 7 3
字符串字典序: apple berry fig grape pear
```

## 2.C-3 {#hw-2-c-3}

**难度 L5** · 题面见 [homework](homework#hw-2-c-3)

**思路**：三次反转是原地轮转的经典构造（改编自 LeetCode 189「Rotate Array」的 O(1) 空间解，字节级泛化到任意内存块则贴近 CSAPP 的 reverse/rotate 练习）。左移 k 字节 = 反转 `[0, k)` + 反转 `[k, n)` + 反转 `[0, n)`：前两步把两段各自掉头，最后一步整体掉头后两段就回到了正确顺序。

1. `reverse_bytes` 用字节指针走半开区间 `[lo, hi)`：`hi` 从 `lo + n` 起、先减后交换，`lo < hi` 相遇即停。→ 知识点：[第 2 章](/02-pointers-memory/02-pointer-arithmetic)（双指针相遇条件）、[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（unsigned char* 字节指针）
2. `rotate_left` 先 `k %= n`（`n == 0` 返回、`k == 0` 返回），然后三次反转。`int` 数组要左移 3 个元素，就是左移 `3 * sizeof(int)` 字节——「元素」到「字节」的换算全在调用点完成，函数本身只认字节。→ 知识点：[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)（字节视角）、[第 3 章](/02-pointers-memory/03-pointer-parameters)（void* 参数把「类型」留给调用者）
3. 对拍 4 组边界（k=0、k=1、k=n、k<n）与朴素实现逐字节一致；k>n 时 `9 % 7 = 2` 等价左移 2。gcc、clang、ASan 三种构建结果全同。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)（朴素版 malloc 临时缓冲 + free，对照用）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw2c3.c -o hw2c3 && ./hw2c3
字符串左移 2: cdefgab
int 数组左移 3 元素: 4 5 1 2 3
对拍 4 个用例,不一致 0 个
k>n (左移 9): cdefgab
$ clang -std=c11 -Wall -Wextra hw2c3.c -o hw2c3c && ./hw2c3c
(输出同上,全对)
$ gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address hw2c3.c -o hw2c3a && ./hw2c3a
(输出同上,ASan 零报告)
```
