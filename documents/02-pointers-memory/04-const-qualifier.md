---
title: "const 限定：谁被 const 修饰，谁就只读"
description: "这一章专门讲 const(ISO §6.7.3 的类型限定符)。真跑指针的 const 三态:① const int* p(指向 const 的指针)——不能通过 *p 改对象,但 p 可以改指向别处;② int* const p(const 指针)——p 不能改指向,但 *p 可以改对象;③ const int* const ——两者都 const。读法口诀「const 修饰它左边的东西,若在最左则修饰右边的类型」(所以 const int* 和 int const* 是一回事)。真跑三态各自的合法操作(*p1 改指向得 20、*p2 改 a 得 30、双 const 只读),再用两个编译失败对照:改 const int* 的对象报 error: assignment of read-only location '*p'、改 int* const 的指向报 error: assignment of read-only variable 'p'。再真跑 const 当函数参数的「只读契约」(int sum(const int* a, int n),函数承诺不改数组)和 const 正确性——把 const int* 偷偷赋给 int* 会被 gcc -Wdiscarded-qualifiers 警告、运行是 UB(真跑局部 const 被绕过改成了 20)。末尾对比 const 与 #define。全 gcc16+clang22 真跑。"
chapter: 2
order: 4
tags:
  - host
  - pointers
  - type
difficulty: intermediate
reading_time_minutes: 12
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第1章：指针是什么（指针类型 int*）"
  - "阶段2·第3章：用指针改调用者的变量（指针参数、输入/输出之分）"
  - "第 2 章：整型家族与 sizeof（类型概念）"
related:
  - "阶段2·第3章：用指针改调用者的变量（const 标记「输入参数」）"
  - "阶段2·第11章：void* 与字节级操作（restrict 限定、字节级 const）"
---

> 🟡 状态:待审核(2026-06-30)

# const 限定：谁被 const 修饰，谁就只读

## 引言：const 是一份「只读」契约

第 3 章末尾我们留了一句「只读不改的参数用 `const int*`」——这一章就把它讲透。`const` 是 C 的**类型限定符**（type qualifier，ISO §6.7.3），它的作用一句话：**谁被 `const` 修饰、谁就「只读」、不能被修改**。它不是「常量」的同义词（后面会讲它和 `#define` 编译期常量的区别），而是一份**契约**——写代码的人向编译器和读代码的人承诺「这个东西我不改」。这份契约有两个实在的好处：一是**保护**（编译器在编译期就拦住误改，下面真跑看得到）、二是**自文档**（看到函数参数是 `const int*`，你就知道「这函数只读我的数据、不会动它」，可以放心传）。这一章聚焦最让人犯晕的「指针的三种 const 位置」，顺带把 const 参数契约和 const 正确性讲清。

## 指针的 const 三态

`const` 用在普通变量上很简单——`const int x = 10;` 之后 `x` 就是只读的、`x = 20` 会编译报错（和下一行 `*p` 的情况同理）。真正让人晕的是 const 和 `*` 混在一起，因为「const 指针」有两个截然不同的位置，含义完全相反。把三种形态一次性摆出来：

```c
#include <stdio.h>

int main(void) {
    int a = 10, b = 20;

    const int* p1 = &a; /* 指向 const int 的指针:*p1 不能改,p1 能改指向 */
    p1 = &b;            /* OK:p1 可以指向别处 */
    printf("*p1 = %d\n", *p1);

    int* const p2 = &a; /* const 指针:p2 不能改指向,*p2 能改 */
    *p2 = 30;           /* OK:可以通过 p2 改 a */
    printf("*p2 = %d, a = %d\n", *p2, a);

    const int* const p3 = &a; /* 双 const:p3 和 *p3 都不能改 */
    printf("*p3 = %d\n", *p3);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall three_forms.c -o tf && ./tf
*p1 = 20
*p2 = 30, a = 30
*p3 = 30
```

第一种 `const int* p1`（注意 `const` 在 `*` **前面**）是「**指向 const int 的指针**」：`p1` 自己能改指向（`p1 = &b` 合法，所以 `*p1` 变成读 `b` 得 `20`），但你**不能通过 `*p1` 改它指的对象**——编译器认为「`p1` 指向的东西是 const 的、只读」。第二种 `int* const p2`（`const` 在 `*` **后面**、修饰的是指针变量 `p2` 本身）是「**const 指针**」：恰恰相反，`p2` 自己不能改指向（一辈子指向初始化时的那个 `a`），但你**可以通过 `*p2` 改对象**（`*p2 = 30` 把 `a` 改成了 30，所以打印 `a = 30`）。第三种 `const int* const p3` 是前两者叠加：指针不能改指向、对象也不能通过它改，全锁死。

记这三态有个口诀：**`const` 修饰的是「它左边紧挨着的东西」；如果 `const` 在最左边，就修饰右边的整个类型**。所以 `const int*` 里 `const` 在最左、修饰 `int`，意思是「指向 const int 的指针」；`int* const` 里 `const` 修饰的是 `*`（指针本身）。从这个口诀还能推出一个等价写法：`const int*` 和 `int const*` 完全一样（`const` 修饰的都是 `int`）——两种写法都对、表达同一个意思，工程里 `const int*` 更常见。

光看「能改什么」还不够直观，把「**不能**改什么」也真跑出来——两种 const 各自拦一种修改：

```c
int main(void) {
    int a = 10;
    const int* p = &a; /* 指向 const int */
    *p = 20;           /* 编译错误:不能通过 const int* 改对象 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fail_to_obj.c -o f1
fail_to_obj.c:4:8: error: assignment of read-only location '*p'
    4 |     *p = 20;           /* 编译错误:不能通过 const int* 改对象 */
      |        ^
```

`const int* p` 试图 `*p = 20`，gcc 报 `assignment of read-only location '*p'`（「只读位置」不能赋值）——`const` 把 `*p` 标成只读，编译期就拦下。换一个方向：

```c
int main(void) {
    int a = 10, b = 20;
    int* const p = &a; /* const 指针 */
    p = &b;            /* 编译错误:const 指针不能改指向 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fail_to_ptr.c -o f2
fail_to_ptr.c:4:7: error: assignment of read-only variable 'p'
    4 |     p = &b;            /* 编译错误:const 指针不能改指向 */
      |       ^
```

`int* const p` 试图 `p = &b`，gcc 报 `assignment of read-only variable 'p'`（「只读变量 `p`」不能赋值）——这次只读的是 `p` 本身、而不是 `*p`。两个报错合起来，正好印证上面三态的分工：「`const` 在 `*` 前面 → 对象只读；`const` 在 `*` 后面 → 指针只读」。clang 的措辞更直白，分别说 `read-only variable is not assignable` 和 `cannot assign to variable 'p' with const-qualified type 'int *const'`，意思完全一致。

## const 当函数参数：一份「只读不改」的契约

const 最有用的场景是**函数参数**。第 3 章我们说「指针参数有输入（函数读）和输出（函数写）之分」，`const int*` 就是把「输入」明确标记出来——它向调用者承诺：**我只读你的数据、绝不改它**。调用者因此可以放心地把数据交出来，不必担心被偷改：

```c
#include <stdio.h>

/* const int* 是契约:函数只读数组内容、绝不改它(调用者可放心传) */
int sum(const int* a, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += a[i];
        /* a[i] = 0; // 若打开此行,编译错误:const 拦住误改 */
    }
    return total;
}

int main(void) {
    int a[3] = {1, 2, 3};
    printf("sum = %d\n", sum(a, 3));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall const_param.c -o cp && ./cp
sum = 6
```

`int sum(const int* a, int n)` 的 `const int* a` 就是「我只读数组、不改」的契约。它的价值有两层：第一，**自文档**——读这个签名的人立刻知道 `a` 是「输入」、函数不会动它，和第 3 章 `divmod` 那种「输出参数 `int* rem`」（没加 const）形成对照；第二，**防误改**——如果哪天你不小心在函数里写了 `a[i] = 0`，编译器当场报错（就是上面 `fail_to_obj` 那个 `read-only location`），把 bug 扼杀在编译期。所以工程铁律是：**凡是「函数只读不改」的指针参数，一律加 `const`**——它零成本、却换来安全性和可读性，是 const 正确性的起点。

## const 正确性：别偷偷丢掉 const

「const 正确性」（const correctness）指的是：一旦一个东西被声明为 `const`，你在传递它时就得一路保持 const，不能悄悄丢掉。最常见的违规——把一个 `const` 数据的地址，交给一个「能改」的非 const 指针：

```c
#include <stdio.h>

int main(void) {
    const int x = 10;
    int* p = &x; /* 把 const int* 赋给 int*:丢掉 const,危险 */
    *p = 20;     /* UB:绕过 const 改了「只读」对象 */
    printf("x = %d\n", x);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall warn_discard.c -o wd
warn_discard.c:5:14: warning: initialization discards 'const' qualifier from pointer target type [-Wdiscarded-qualifiers]
    5 |     int* p = &x; /* 把 const int* 赋给 int*:丢掉 const,危险 */
      |              ^
$ ./wd
x = 20
```

`&x` 的类型是 `const int*`（指向 const int），你却把它赋给 `int* p`——gcc 立刻警告 `initialization discards 'const' qualifier`（「丢掉了 const 限定」）。这个警告救命：因为一旦丢了 const，后面 `*p = 20` 就绕过了 const 保护、改了原本「只读」的 `x`，是**未定义行为**。这次运行它「成功」把 `x` 改成了 20（局部 const 变量在栈上、恰好可写），但请记住这是 UB：编译器有权假定「`const` 对象不会被改」从而做出让你意外的优化（比如直接把 `printf` 里的 `x` 替换成常量 10、让你看到的还是 10），也可能换成全局 const（落在只读 `.rodata`）就当场段错误——哪种结果都不保证。所以别和编译器耍这种小聪明，`-Wdiscarded-qualifiers` 一响就得修：要么老老实实用 `const int*` 接、要么（极少数确有需要时）用显式类型转换 `(int*)` 表明「我知道我在干什么、后果自负」。const 正确性的目标是：**const 从声明点一路传递下去不丢失**，这样编译器才能全程替你把「只读」守到底。

## const 与 #define 的区别

新手常把 `const int MAX = 100;` 和 `#define MAX 100` 当一回事，其实它们很不一样。`#define` 是**预处理阶段的文本替换**（第 3 阶段，阶段 0 第 3 章见过），`MAX` 在编译前就被替换成 `100`、它根本不是个变量、没有地址、没有类型、不占存储（除了用在字符串里那种情况）。而 `const int MAX = 100;` 是一个**真正的变量**：它有明确的类型 `int`、有作用域（只在它声明的块里有效，不像宏到处泛滥）、可以被调试器看见、可以取地址 `&MAX`（虽然你不能通过它改）。需要「数组长度」这种编译期常量时，C99 之前只能用 `#define` 或枚举（`enum { MAX = 100 };`）；C99 起局部数组也接受 `const int` 当长度（变长数组的一种，第 10 章提过 VLA）。一般原则：**要类型安全、要作用域、要调试，优先 `const` 或 `enum`；要做字符串拼接、跨平台条件编译，才用 `#define`**。

## 小结

`const`（§6.7.3 类型限定符）是一份「只读」契约：**谁被 const 修饰、谁就不能被修改**，编译器在编译期拦住误改。指针的 const 三态是这一章的核心——`const int* p`（const 在 `*` 前）是「指向 const 的指针」：`p` 能改指向、但不能通过 `*p` 改对象（真跑 `p1=&b` 合法得 20，改 `*p` 报 `assignment of read-only location '*p'`）；`int* const p`（const 在 `*` 后）是「const 指针」：恰恰相反，`p` 不能改指向、但能通过 `*p` 改对象（真跑 `*p2=30` 把 `a` 改成 30，改 `p` 指向报 `assignment of read-only variable 'p'`）；`const int* const` 两者皆锁。口诀「const 修饰左边紧挨的东西、在最左则修饰右边的类型」，由此 `const int*` ≡ `int const*`。**const 当函数参数**是最实用场景：`int sum(const int* a, int n)` 标记「只读不改」契约，既是自文档（区分第 3 章的输出参数 `int*`）、又防误改（手滑写 `a[i]=0` 编译期就报错）——「只读的指针参数一律加 const」是 const 正确性的起点。**const 不能丢**：把 `const int*` 赋给 `int*` 会被 gcc `-Wdiscarded-qualifiers` 警告、绕过 const 改对象是 UB（真跑局部 const 被改成了 20，但这是 UB、编译器优化可能让它「看起来没改」），所以 const 要从声明一路传递下去不丢失。`const` 和 `#define` 不是一回事：前者是有类型、有作用域、可调试的真变量，后者是预处理文本替换——要类型安全/作用域优先 `const` 或 `enum`。下一章我们把指针、数组、字符串三者用一个统一的指针视角串起来。

## 参考资源

- ISO/IEC 9899:2011 §6.7.3（类型限定符:p1 const 的定义、p6「修改 const 限定对象是 UB」、p 几条 const 与指针组合的语义）
- K. N. King《C Programming: A Modern Approach》第 11 章（`const` 参数、指向 const 的指针）、第 18 章（const 与 `#define` 的区别、声明修饰符读法）
- Robert C. Seacord《Effective C》第 3 章（const 限定、const 正确性、`const` 作函数契约）
- 阶段2·第1章：指针是什么（指针类型 `int*`）、第3章：用指针改调用者的变量（输入/输出参数之分,引出 const）、第2章：整型与 `sizeof`（类型概念）
- 阶段 0·第3章：预处理深入（`#define` 文本替换、宏 vs const）、第8章：警告旗标（`-Wdiscarded-qualifiers`）、阶段2·第11章：void* 与 restrict 限定
