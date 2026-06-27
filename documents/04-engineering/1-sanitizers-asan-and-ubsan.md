---
title: "ASan 与 UBSan:让 C 的内存错误和未定义行为当场现形"
description: "用 -fsanitize=address,undefined 把堆越界、释放后使用、有符号溢出这些 C 里最阴险的错误,变成带行号的当场报告;顺带看清 GCC16 下「返回局部变量地址」的真实结局。"
chapter: 4
order: 1
tags:
  - host
  - testing
  - memory
  - debug
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0:编译流程、GDB 与库"
  - "Chapter 2:指针、内存布局与位运算"
related:
  - "Chapter 4:CMake 与模块化工程"
---

# ASan 与 UBSan:让 C 的内存错误和未定义行为当场现形

## 引言

说实话,笔者踩过最耗时的一个坑,是在一段挺复杂的逻辑里**返回了一个栈上局部变量的地址**,然后在别处美滋滋地用它。程序时灵时不灵,排查了好久才发现这个逆天操作。C 给你直接操作内存的权力,代价就是——一次小小的指针越界、一个 `free` 之后还在用的指针、一个返回局部变量地址的函数,都可能让程序**崩得很神秘**,甚至带着安全漏洞。

几百行的程序你肉眼还能盯出来,几千上万行呢?所以我们需要一个**让错误当场现形**的工具,而且它就藏在你的编译器里,不用额外装任何东西:AddressSanitizer(ASan)和 UndefinedBehaviorSanitizer(UBSan)。本仓库的 CI([`.github/workflows/ci.yml`](../../.github/workflows/ci.yml))也已经用 `-fsanitize=address,undefined` 编译所有 examples,这一章我们就把它的输出看懂。

> 本文所有 ASan / UBSan 输出都在 **GCC 16.1.1** 上实测捕获,不是凭记忆写的。

## 一个开关就够了

ASan / UBSan 不需要装新工具,就是 gcc / clang 的编译选项:

```bash
# AddressSanitizer:抓内存错误(越界、释放后使用、重复释放……)
gcc -g -fsanitize=address -fno-omit-frame-pointer demo.c -o demo

# UndefinedBehaviorSanitizer:抓未定义行为(有符号溢出、非法位移、空指针解引用……)
gcc -g -fsanitize=undefined -fno-omit-frame-pointer demo.c -o demo

# 两个一起上(本仓库 CI 的做法)
gcc -g -fsanitize=address,undefined -fno-omit-frame-pointer demo.c -o demo
```

`-g` 让报错带行号,`-fno-omit-frame-pointer` 让栈回溯更完整。下面三个例子我们挨个跑一遍,把输出拆开看。

## 实战一:堆越界(heap-buffer-overflow)

往一个 `malloc(5)` 来的 5 字节缓冲里塞 14 字节的 `"Hello, World!"`,经典的越界写入:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char* buffer = (char*)malloc(5);
    strcpy(buffer, "Hello, World!");  /* 14 字节写进 5 字节 */
    printf("Buffer: %s\n", buffer);
    free(buffer);
    return 0;
}
```

编译运行(顺带一提,GCC 16 这种简单情况在**编译期**就能警告一句):

```text
$ gcc -g -fsanitize=address -fno-omit-frame-pointer demo.c -o demo
demo.c: In function 'main':
demo.c:7:5: warning: '__builtin_memcpy' writing 14 bytes into a region of size 5
                      overflows the destination [-Wstringop-overflow=]
$ ./demo
=================================================================
==44198==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x7bfeb77e0015
    at pc 0x7fdeb8f297b4 ...
WRITE of size 14 at 0x7bfeb77e0015 thread T0
    #0 ... in memcpy (.../libasan.so.8+0x1297b3)
    #1 0x... in main /tmp/asan_demo/demo.c:7     ← 出错位置,精确到行
    ...
0x7bfeb77e0015 is located 0 bytes after 5-byte region [0x7bfeb77e0010,0x7bfeb77e0015)
    ← 越界点正好在 5 字节合法区域的"末尾之后 0 字节"
allocated by thread T0 here:
    #1 0x... in main /tmp/asan_demo/demo.c:6     ← 这块内存在哪 malloc 的
SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/asan_demo/demo.c:7 in main
$ echo $?
1                                               ← ASan 让程序以非零码退出
```

一份 ASan 报告通常有**三段**,看懂这三段就够了:

| 报告项 | 含义 |
|---|---|
| `heap-buffer-overflow` + `WRITE of size 14` | 错误类型 + 你实际写了多少字节 |
| `#1 ... in main demo.c:7` | **出错位置**(代码里在哪踩的) |
| `located 0 bytes after 5-byte region [...]` + `allocated ... demo.c:6` | **内存位置**(这块 5 字节堆区在哪分配的) |

### 红区(Shadow Memory)机制

报告里那段 `Shadow bytes` 是 ASan 的核心机制。它给主内存配了一份"影子内存",每个影子字节标记 8 个真实字节的访问状态:

```text
=>0x7bfeb77e0000: fa fa [05] fa fa fa fa fa fa ...
                        ↑
                  05 = 这 8 字节里有 5 字节可访问
                  fa = 红区(redzone),踩到就报错
```

| Shadow 值 | 含义 |
|---|---|
| `00` | 8 字节都可访问 |
| `01`–`07` | 部分可访问(本例 `05` = 5 字节) |
| `fa` | 堆红区 |
| `fd` | 已释放的堆(下一节会看到) |
| `f9` | 全局变量红区 |

ASan 在堆分配时自动给用户数据**前后各包一圈红区**:`[左红区 16B][用户数据][右红区 16B]`。你一越界踩进红区,它立刻报警。这也是为什么 `malloc(5)` 实际占用的远不止 5 字节。

## 实战二:释放后使用(heap-use-after-free)

越界会让程序崩得很明显,而**释放之后还在用**这把指针,才是更阴险的坑——往往时灵时不灵,取决于这块内存被谁复用了:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char* buffer = (char*)malloc(5);
    strcpy(buffer, "Hi!");
    free(buffer);
    printf("buffer: %s\n", buffer);  /* use-after-free */
    return 0;
}
```

```text
$ gcc -g -fsanitize=address -fno-omit-frame-pointer demo.c -o demo
$ ./demo
=================================================================
==44207==ERROR: AddressSanitizer: heap-use-after-free on address 0x7aa8eb3e0010
READ of size 2 at 0x7aa8eb3e0010 thread T0
    #3 0x... in main /tmp/asan_demo/demo.c:9    ← 在 printf 里读了已释放的内存
0x7aa8eb3e0010 is located 0 bytes inside of 5-byte region [0x7aa8eb3e0010,0x7aa8eb3e0015)
freed by thread T0 here:
    #1 0x... in main /tmp/asan_demo/demo.c:8    ← 在哪 free 的
previously allocated by thread T0 here:
    #1 0x... in main /tmp/asan_demo/demo.c:6    ← 在哪 malloc 的
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/asan_demo/demo.c:9 in main
```

注意影子字节这次是 `[fd]`:`fd` 就是"已释放的堆"。`free` 之后这块内存被标记成 `fd`,再访问立刻被抓。报告把**分配点、释放点、非法访问点**三处栈都给你,排查时顺着这三条线走就行。

## 实战三:未定义行为(UBSan)

C 还有一大类错误**不碰内存,但同样致命**——未定义行为(UB):有符号整数溢出、移位越界、空指针解引用……这类错误编译器不报,运行时也常常"看起来正常",但标准说了算:UB 就是 UB,任何结果都合法,包括把你的程序优化没。

```c
#include <stdio.h>
int main(void) {
    int a = 0x7fffffff;   /* INT_MAX */
    int b = a + 1;        /* 有符号溢出:UB */
    int c = 1 << 31;      /* 移进符号位:UB */
    printf("%d %d\n", b, c);
    return 0;
}
```

```text
$ gcc -g -fsanitize=undefined -fno-omit-frame-pointer demo.c -o demo
$ ./demo
demo.c:4:9: runtime error: signed integer overflow: 2147483647 + 1
                    cannot be represented in type 'int'
demo.c:5:15: runtime error: left shift of 1 by 31 places
                    cannot be represented in type 'int'
-2147483648 -2147483648
```

> ⚠️ **一个容易误解的点**:UBSan 默认**只报告、不终止**——你看上面程序照样打印了结果、退出码还是 0。想让它在遇到 UB 时直接 abort(像 ASan 那样),加 `-fno-sanitize-recover=undefined`。而 ASan 一旦检测到内存错误,**默认就以非零码退出**。两者脾气不同,别搞混。

## 常见踩坑

- **ASan 不是万能的**:它抓越界、释放后使用、重复释放这类**spatial / temporal** 错误一把好手,但抓不了**逻辑错误**(你 `free` 了正确的指针、却忘了置空、逻辑上还在判断它——这不违法,ASan 管不着)。它还会漏报某些很窄的越界(比如恰好没踩到红区)。**ASan 通过 ≠ 程序正确**,它只是把一大类低级错误自动化了。
- **「返回局部变量地址」在现代 GCC 上的真实结局**:教科书爱拿这个演示 stack-use-after-return,但你在 **GCC 16** 上跑会发现——编译器先甩一个 `warning: function returns address of local variable [-Wreturn-local-addr]`,然后干脆**把那个悬空地址优化成空指针**,运行时直接 `SEGV`(读地址 0)。也就是说,现代编译器已经把这个经典坑**提前、确定性地**变成崩错了,反而是好事。要复现教科书里的 stack-use-after-return,得设 `ASAN_OPTIONS=detect_stack_use_after_return=1` 并设法绕过那个优化。
- **有性能开销**:ASan 会让程序慢 2 倍左右、内存多 3 倍左右。**别在 release 构建里开**,只在调试 / 测试 / CI 里开。本仓库 CI 的 sanitize job 就是这么用的。
- **链接也要带 `-fsanitize`**:如果你分编译和链接两步(`gcc -c` 再 `gcc -o`),**链接那一步也得带 `-fsanitize=address`**,否则找不到 ASan 的运行时库。用 CMake 的话,加到 `target_compile_options` 和 `target_link_options` 两处。

## ASan / UBSan 能检测什么(速查)

| 检测类型 | 工具 | 典型错误 |
|---|---|---|
| 堆越界 | ASan | `malloc(5)` 写 14 字节 |
| 释放后使用 | ASan | `free(p)` 后仍读 `p` |
| 重复释放 | ASan | 同一指针 `free` 两次 |
| 栈/全局越界 | ASan | 局部数组、静态数组越界 |
| 内存泄漏 | LSan(`-fsanitize=leak`) | `malloc` 后没 `free` |
| 有符号溢出 / 非法位移 | UBSan | `INT_MAX + 1`、`1 << 31` |
| 数据竞争 | TSan(`-fsanitize=thread`) | 多线程无锁读写同一变量 |

## 更多组合

```bash
# 内存 + 未定义行为,一把梭(本仓库 CI 默认)
gcc -fsanitize=address,undefined -O1 -g main.c -o main

# 内存泄漏(LeakSanitizer,通常 ASan 已自带)
gcc -fsanitize=leak -O1 -g main.c -o leak_test

# 多线程数据竞争(ThreadSanitizer,注意不能和 ASan 同时用)
gcc -fsanitize=thread -O1 -g thread_test.c -o thread_test
```

## 小结

一句话:**写 C,把 `-fsanitize=address,undefined` 当成调试期的默认开关**。三段式读报告(出错位置 / 内存位置 / 分配释放点)看懂了,绝大多数内存错误和 UB 都能当场抓出来,不用再靠"时灵时不灵"猜。记住两条脾气:ASan 检测到就**非零退出**,UBSan 默认**只报告不终止**——要终止加 `-fno-sanitize-recover`。

## 练习

1. 把实战一的 `strcpy` 改成 `strncpy(buffer, "Hello, World!", 5)`,再用 ASan 编译运行,观察还会不会报错、为什么(提示:`strncpy` 不保证 `\0` 结尾)。
2. 写一个**双重释放**的程序(`free(p); free(p);`),用 ASan 跑,看报告和释放后使用有什么不同。
3. 写一个会**内存泄漏**的程序(`malloc` 后不 `free`),用 `-fsanitize=leak` 或 ASan 跑,看退出时的 leak 报告。
4. 把实战三的 UBSan 例子加上 `-fno-sanitize-recover=undefined`,观察程序是否还会打印结果。

## 参考资源

- [GCC Manual — Options for Instrumentation( sanitize 系列)](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
- [AddressSanitizer Wiki(Google)](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [Clang — Sanitizer Documentation](https://clang.llvm.org/docs/UsersManual.html#controlling-code-generation)
- 本仓库 CI 的 sanitize job:[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)

---
*整理自作者笔记,按 C-Journey 写作规范重写;所有 ASan / UBSan 输出在 GCC 16.1.1 实测捕获。*
