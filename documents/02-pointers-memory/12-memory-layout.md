---
title: "内存布局与生命周期：一张地图把 .text/.rodata/.data/.bss/堆/栈全串起来"
description: "阶段2 收官章。把前面 11 章零散见过的「内存」用一个统一视角收口——程序加载后,虚拟内存分成几大段:代码段 .text(函数代码,Ch9 函数指针指向这)、只读段 .rodata(字符串字面量、改它段错误,Ch11)、已初始化全局段 .data、未初始化全局段 .bss(启动时清 0)、堆(malloc 来的,Ch6,向高地址增长)、栈(局部变量、函数帧,向低地址增长)。真跑一个程序同时打印栈变量、堆变量、.data 全局、.bss 全局、.rodata 字面量的地址,看它们清晰地落在不同地址段(栈 0x7ffd.. 最高、堆 0x5ac3.. 中间、.data/.bss/.rodata 在程序映像低地址块)。再用 nm 看 global_init 是 D、global_uninit 是 B、main 是 T(呼应阶段0 第5章目标文件与符号)。真跑嵌套调用证明栈向低地址增长(内层局部地址 < 外层)。最后把第 9 章的四种存储期(自动=栈/静态=.data+.bss/动态=堆/线程)在这张地图上落位,并对比栈 vs 堆(大小/增长/管理/生命周期/溢出 vs 泄漏)。全 gcc16+clang22 真跑。"
chapter: 2
order: 12
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第6章：动态内存入门(堆 malloc/free)、第7章:动态内存的坑(泄漏/UAF)、第11章:void* 与字节操作(unsigned char* 看字节)"
  - "第 9 章:作用域、存储期与 static(四种存储期)"
  - "阶段 0·第4章:编译阶段看汇编(.text/.rodata 段布局)、第5章:目标文件与符号(nm 看 T/D/B)、第10章:Sanitizer 门禁"
related:
  - "阶段 3:数据结构与算法(链表/树/哈希全靠堆 + 指针串联,本地图是底层)"
  - "阶段 0·第4章:编译看汇编(段布局首次出场)、第14章:GDB 进阶(用内存地图理解段错误、core dump)"
---

> 🟡 状态:待审核(2026-06-30)

# 内存布局与生命周期：一张地图把 .text/.rodata/.data/.bss/堆/栈全串起来

## 引言：给前 11 章画一张地图

到这一章为止,阶段 2 讲了指针(第 1 章)、算术(第 2 章)、改调用者(第 3 章)、`const`(第 4 章)、字符串遍历(第 5 章)、动态内存(第 6、7 章)、多级指针(第 8 章)、函数指针(第 9 章)、复杂声明(第 10 章)、`void*` 与字节(第 11 章)——它们全在「内存」里发生,但我们一直没把「内存到底长什么样」一次画清楚。阶段 0 第 4 章看汇编时我们瞥过 `.text`/`.rodata` 这些段,第 9 章讲过四种存储期,这一章就把这些线索全收口:程序加载到内存后,它的虚拟地址空间分成哪几大块、每块住着谁、它们各自的生命周期是什么。看懂这张地图,你就知道为什么改字符串字面量会段错误(它在只读段)、为什么栈上开大数组会爆(栈很小)、为什么 `malloc` 出来的内存不会自动归还(堆是另一块、手动管)、为什么 `static` 局部变量的值能跨调用保持(它在 `.data` 不在栈)。

## 程序的六大内存区

一个 C 程序加载运行后,操作系统给它一段虚拟地址空间,大致分成下面几块(从「程序映像」到「栈」):

- **`.text`(代码段)**:编译后的机器指令。你写的函数(`main`、`add`、`qsort` 调的 `cmp`)全在这。第 9 章函数指针指向的就是这块里某个函数的入口地址。只读、可执行。
- **`.rodata`(只读数据段)**:字符串字面量(`"hello"`)、`const` 全局变量。只读,写它就段错误(第 11 章真跑过 `char* p = "hi"; *p = 'H';` 退出码 139)。
- **`.data`(已初始化数据段)**:**已初始化**的全局变量和 `static` 变量(初始化值非 0 的)。比如 `int g = 42;` 在函数外、或 `static int s = 9;`。
- **`.bss`(未初始化数据段)**:**未初始化**的、或初始化为 0 的全局/static 变量。程序启动时这块整体清 0,所以 `int g;` 在函数外、`global_uninit` 启动后值就是 `0`(不需要在可执行文件里存一堆 0、只记大小,所以 `.bss` 不占可执行文件空间)。第 9 章的「静态存储期」变量就落在 `.data` 或 `.bss`。
- **堆(heap)**:`malloc`/`calloc`/`realloc` 出来的内存(第 6 章)。向**高地址**增长(通常),由你手动管理(忘了 `free` 就泄漏,第 7 章)。大。
- **栈(stack)**:普通局部变量、函数调用帧(参数、返回地址)。向**低地址**增长(下面真跑给你看),进函数分配、出函数自动回收(自动存储期,第 9 章)。默认不大(Linux 每进程通常 8 MB),开大数组会爆。

这六块在虚拟地址空间里各占一段。我们真跑一个程序,同时打印「住在这几块里的变量」的地址,看它们怎么分层:

```c
#include <stdio.h>
#include <stdlib.h>

int global_init = 42;   /* .data:已初始化的全局变量 */
int global_uninit;      /* .bss:未初始化的全局变量(程序启动时自动清 0) */

int main(void) {
    int stack_local = 7;            /* 栈:普通局部变量(自动存储期) */
    static int static_local = 9;    /* .data:static 局部,静态存储期、但作用域还在本函数 */
    int* heap = malloc(sizeof(int)); /* 堆:动态存储期 */
    if (heap) {
        *heap = 11;
    }
    const char* literal = "hello"; /* 指向 .rodata 里的字符串字面量 */

    printf("=== 各类变量的地址(看它们落在不同内存段)===\n");
    printf("栈      &stack_local    %p\n", (void*)&stack_local);
    printf("堆      heap            %p\n", (void*)heap);
    printf(".data   &global_init    %p\n", (void*)&global_init);
    printf(".data   &static_local   %p\n", (void*)&static_local);
    printf(".bss    &global_uninit  %p\n", (void*)&global_uninit);
    printf(".rodata literal         %p\n", (void*)literal);

    printf("\n=== 值 ===\n");
    printf("global_uninit = %d  (.bss 启动时自动清 0)\n", global_uninit);

    free(heap);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall layout.c -o lay && ./lay
=== 各类变量的地址(看它们落在不同内存段)===
栈      &stack_local    0x7ffd35a75564
堆      heap            0x5ac377a3f010
.data   &global_init    0x5ac36135e038
.data   &static_local   0x5ac36135e03c
.bss    &global_uninit  0x5ac36135e044
.rodata literal         0x5ac36135c008

=== 值 ===
global_uninit = 0  (.bss 启动时自动清 0)
```

六个地址清晰地分成三组。**栈** `&stack_local` 在 `0x7ffd...`(最高,接近虚拟地址空间顶端);**堆** `heap` 在 `0x5ac377a3f...`(中间);程序映像那三个——`.data`(`global_init`、`static_local`)、`.bss`(`global_uninit`)、`.rodata`(`literal` 指向的字面量)——都在 `0x5ac36135...` 这一带(低地址,程序加载基址附近)。注意程序映像内部的顺序:`.rodata`(`...5c008`)比 `.data`(`...5e038`)地址低、`.data` 和 `.bss`(`...5e044`)紧挨着,这正符合 ELF「`.rodata` 靠 `.text`、`.data`/`.bss` 在一起」的布局习惯。最后一行 `global_uninit = 0` 是 `.bss` 的招牌特征——你从没给它赋过值,可它一启动就是 0,因为 `.bss` 整块在 `main` 跑之前就被内核清零了。(地址每次运行都不同——栈、堆、程序映像各自有独立的 ASLR 随机化,但「栈最高、堆中间、程序映像另一块」这个分层关系稳定。)

## 用 nm 看符号落哪个段

第 5 章(阶段 0)我们用过 `nm` 看目标文件的符号。现在我们对编译好的可执行文件跑 `nm`,看这几个全局符号分别落在哪个段:

```text
$ gcc -std=c11 -Wall layout.c -o lay && nm lay | grep -E "global_(init|uninit)| main$"
0000000000004038 D global_init
0000000000004044 B global_uninit
0000000000001179 T main
```

`nm` 输出三列:地址、类型字母、符号名。类型字母告诉你符号住哪个段(阶段 0 第 5 章见过):`D`(大写)= 已初始化数据段 `.data`、`B`(大写)= 未初始化数据段 `.bss`、`T`(大写)= 代码段 `.text`。所以 `global_init`(初始化成 42)是 `D`、`global_uninit`(没初始化)是 `B`、`main`(函数代码)是 `T`——和上一节真跑的地址分层完全对得上。注意 `nm` 给的地址(`4038`/`4044`/`1179`)是**文件里的虚拟地址偏移**(小),而上节运行时打印的 `0x5ac36135e038` 是**加载后的实际地址**(ASLR 把整个程序映像搬到 `0x5ac3...` 这个随机基址上、再加上偏移)——两者差一个加载基址,但相对关系一致(`global_uninit` 比 `global_init` 地址高、`main` 在更低的 `.text`)。

## 栈向低地址增长

栈有个反直觉的特性:它**向低地址增长**(越深的函数调用、局部变量的地址越小)。真跑给你看——外层函数取一个栈变量地址传给内层,内层再取自己的栈变量地址,比较两者:

```c
#include <stdio.h>

/* 嵌套调用:外层把一个栈变量地址传给内层,内层再取自己栈变量地址,比较两者 */
void inner(const int* outer_addr) {
    int inner_var;
    printf("外层局部 %p, 内层局部 %p\n", (void*)outer_addr, (void*)&inner_var);
    printf("内层地址 %s 外层", (void*)&inner_var < (void*)outer_addr ? "小于" : "大于等于");
    printf("(栈向低地址增长:越深调用、地址越小)\n");
}

int main(void) {
    int outer_var = 0; /* 初始化:我们只关心它的地址、不读值 */
    inner(&outer_var);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall stack_dir.c -o sd && ./sd
外层局部 0x7fffd6e38a44, 内层局部 0x7fffd6e38a24
内层地址 小于 外层(栈向低地址增长:越深调用、地址越小)
```

`inner` 是被 `main` 调用的(更深一层),它的局部变量 `inner_var` 地址 `0x7fffd6e38a24` 比 `main` 里的 `outer_var`(`0x7fffd6e38a44`)**小**——栈从高地址往低地址长,每深一层调用、新的函数帧就落在更低的地址。这正是「栈溢出」通常表现为「地址一路往下、踩进别的内存」的原因,也是为什么递归太深会爆栈(栈往下长、撞到栈底限制就 segment fault)。堆则相反——`malloc` 多次拿到的地址通常**递增**(向高地址长),不过堆的具体增长由 `malloc` 的实现(brk/mmap)决定,不像栈方向这么稳定,所以「栈方向」是真跑结论、「堆方向」了解即可。

## 四种存储期在地图上落位

第 9 章讲过 C 的四种存储期,现在把它们在这张内存地图上各自落位,你会看得特别清楚:

- **自动存储期**(普通局部变量)→ **栈**。进函数分配、出函数回收。`stack_local` 就是。
- **静态存储期**(全局变量、`static` 局部)→ **`.data`**(有非 0 初值)或 **`.bss`**(无初值或 0 初值)。程序启动时落位、一直活到程序结束。`global_init`/`static_local` 在 `.data`、`global_uninit` 在 `.bss`。
- **动态存储期**(`malloc`/`calloc`/`realloc`)→ **堆**。你决定何时分配(第 6 章)、何时归还(忘了就泄漏,第 7 章)。`heap` 就是。
- **线程存储期**(`_Thread_local`,C11)→ 每个线程一份,本章不展开。

记住第 9 章那句话——**作用域(编译期「名字在哪可见」)和存储期(运行期「内存活多久」)是两回事**:`static int static_local` 的作用域还在 `main` 里(只有 `main` 能看见它),但它的存储期是静态(落在 `.data`、活到程序结束,不是栈上),这就是它「值能跨调用保持」的真相。地图把这件事讲得最透。

## 栈 vs 堆:一张对比表收尾

栈和堆是动态内存的两个主场,把它们并排对比,前面 11 章的诸多细节就各归其位:

| 维度 | 栈 | 堆 |
|---|---|---|
| 谁分配/释放 | 编译器(进/出函数自动) | 你(`malloc`/`free`,第 6 章) |
| 存储期 | 自动(第 9 章) | 动态(第 6/7 章) |
| 增长方向 | 向低地址(真跑:内层 < 外层) | 向高地址(通常,由 malloc 实现定) |
| 大小 | 小(Linux 默认 8 MB,爆栈段错) | 大(受物理内存/虚拟地址空间限) |
| 速度 | 快(移动栈指针一条指令) | 慢(`malloc` 要查空闲链表/向 OS 要) |
| 生命周期 | 函数返回即回收 | `free` 才回收(忘 `free` 泄漏,第 7 章) |
| 典型坑 | 栈溢出(大数组/深递归)、返回局部地址悬垂 | UAF/double-free/泄漏(第 7 章 ASan 抓) |

这张表是阶段 2 动态内存部分的「索引」:每一行都对应前面某一章真跑过的东西。栈快但小、自动回收但生命周期短;堆大但慢、生命周期长但要你手动管。工程里到底用哪个,看「数据大小是否编译期可知」「生命周期是否跨函数」「是否需要动态扩容」——局部、临时、小 → 栈;跨函数、运行期才知大小、要扩容 → 堆(`malloc` + 记得 `free`)。

## 小结

程序加载后虚拟内存分六大区:**`.text`**(函数代码,第 9 章函数指针指向这)、**`.rodata`**(字符串字面量/`const` 全局,只读、写它段错,第 11 章)、**`.data`**(已初始化全局/static)、**`.bss`**(未初始化或 0 初始化全局,启动时整块清 0,`global_uninit` 真跑得 0)、**堆**(`malloc` 来的,第 6 章)、**栈**(局部变量/函数帧)。真跑同时打印栈/堆/`.data`/`.bss`/`.rodata` 变量地址,看它们清晰分层(栈 `0x7ffd..` 最高、堆 `0x5ac3..` 中间、程序映像 `.data/.bss/.rodata` 在低地址块;每次 ASLR 变但分层关系稳)。`nm` 看符号落段:`global_init` 是 `D`、`global_uninit` 是 `B`、`main` 是 `T`(呼应阶段 0 第 5 章;`nm` 给文件偏移、运行时加 ASLR 基址)。栈**向低地址增长**(真跑嵌套调用,内层局部地址 < 外层),堆通常向高地址长(malloc 实现定)。第 9 章四种存储期在这张地图上落位:自动→栈、静态→`.data`/`.bss`、动态→堆、线程→`_Thread_local`;作用域(编译期可见)和存储期(运行期存活)是两回事——`static int static_local` 作用域在 `main`、存储期在 `.data`,所以值能跨调用保持。栈 vs 堆:栈自动/小/快/向低/函数返回回收,堆手动/大/慢/向高/`free` 才回收——局部小数据用栈、跨函数或运行期才知大小或要扩容用堆(配 `free`)。这张地图是阶段 2 的总收口:指针操作的每一块内存、`malloc`/`free`、字符串、字节遍历,全在上面有定位。阶段 2 完结,下一阶段我们用指针+堆搭出更复杂的数据结构(链表、树、哈希表),它们全是对「堆上节点 + 指针串联」的组合。

## 参考资源

- ISO/IEC 9899:2011 §6.2.4(存储期:自动/静态/线程/动态)、§5.1.2(执行环境:程序启动、`.bss` 清零)、§7.22.3(内存管理:堆)
- K. N. King《C Programming: A Modern Approach》第 13 章(存储类别「Storage Classes」、`.data`/`.bss`/栈/堆布局)、第 17 章(栈 vs 堆的动态内存视角)
- Robert C. Seacord《Effective C》第 6 章(内存管理、栈 vs 堆、UAF/泄漏在这张地图上的位置)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 6 章(结构体与指针,栈/堆上分配节点)
- Peter van der Linden《Expert C Programming》第 7 章(内存布局、段、栈与堆的增长方向、活动记录)
- 阶段 0·第4章:编译看汇编(`.text`/`.rodata`/`.data`/`.bss` 段布局、`-O0/-O2`)、第5章:目标文件与符号(`nm` 的 T/D/B/R)、第10章:Sanitizer 门禁(ASan 在地图上抓栈/堆越界、LeakSanitizer 抓堆泄漏)
- 第 9 章:作用域、存储期与 `static`(四种存储期,本章在地图上落位)、阶段2·第6章:动态内存(堆)、第7章:坑(UAF/double-free/泄漏)、第11章:void* 与字节(字节级视角遍历各段)
- 阶段 3:数据结构与算法(链表/树/哈希——全靠堆节点 + 指针串联,这张地图是它们的地基)
