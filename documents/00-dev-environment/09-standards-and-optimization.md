---
title: "标准与优化：把 -std / -O / -g 三个旋钮拧清楚"
description: "上一章把警告旗标开到 -Werror，这一章把编译器上另外三个最常被抄来抄去的旋钮——-std（编译哪个年代的 C）、-O0..-O3（优化到什么程度）、-g（带不带调试信息）——挨个真跑。会看到 __STDC_VERSION__ 在 c89..c23 各档的值、c11 和 gnu11 的真正分水岭其实是 __STRICT_ANSI__（不是 -Wpedantic）、C23 把 bool/true 变成了关键字、-O3 比 -O2 多出来的 SIMD 自动向量化（paddd 一次加 4 个 int）、以及最要命的一条——一段靠有符号溢出回绕来『检测溢出』的代码，gcc 连 -O0 都假定不溢出直接返回 0，clang -O0 才老实回绕，这正是 UB 在不同编译器、不同 -O 下结果彻底不同的活样本，也是下一章 sanitizer 要兜的底。"
chapter: 0
order: 9
tags:
  - host
  - toolchain
difficulty: intermediate
reading_time_minutes: 17
platform: host
c_standard: [89, 99, 11, 17, 23]
prerequisites:
  - "第 1 章：工具链体检（立「显式钉 -std」纪律）"
  - "第 4 章：编译阶段看汇编（-O0/-O2、as-if 规则、字符串字面量）"
  - "第 8 章：警告旗标进阶（-Wpedantic、-Wuninitialized）"
related:
  - "第 10 章：Sanitizer 门禁（-O 让 UB 现形后的运行期兜底）"
  - "第 13 章：GDB 基础单步（-g 调试信息的消费者）"
---

# 标准与优化：把 -std / -O / -g 三个旋钮拧清楚

## 引言：三个被抄来抄去的旋钮

这一章我们要把 `-std` 各档、`-O0..-O3` 优化级别、`-g` 调试信息一起讲透，顺带解释为什么第 8 章好几个警告旗标（尤其 `-Wuninitialized`）的行为会和 `-O` 级别绑在一起。这三个旋钮几乎在每个 Makefile、每个 CMakeLists 里都会出现，但我打赌很多人是照着别人的抄来的（没事，抄是对的，做工程和完成需求，完成第一，优雅第二。当然有代码洁癖的朋友绕行~），并没有真懂——有人分不清 `-std=c11` 和 `-std=gnu11` 到底差在哪，有人以为 `-O0` 就是「完全不优化所以最安全」，有人不知道 `-g` 和 `-O` 互相牵扯。

这三个旋钮看似各管各的，实则暗中咬合：`-O` 会改变一段含未定义行为（UB）的代码的运行结果（让 bug 现形、或者干脆帮你「修」没了）；`-g` 有没有用，取决于 `-O` 把你的代码改成了什么样（开了 `-O2`，调试信息就半残）；`-Wuninitialized` 这类静态警告的灵敏度，又依赖 `-O` 喂给它的数据流分析。所以把它们放在一起拧一遍，比单独看更说得清。每一步我们都真编译、真跑、贴输出。

## `-std`：你到底在编译哪个年代的 C

### 先问一句：现在在哪个标准档

C 从 1989 年的 C89（也叫 C90）一路演进到 C99、C11、C17，再到最新的 C23。`-std=` 这个旋钮就是告诉编译器「按哪个年代的规矩来」。那怎么确认当前到底在哪个档？C 标准给了一个预定义宏 `__STDC_VERSION__`（ISO/IEC 9899 §6.10.8 预定义宏名），它的值就是当前标准版本的「年月」编码。我们写个小程序把它打出来：

```c
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);
#else
    printf("__STDC_VERSION__ undefined (classic C89/C90)\n");
#endif
    return 0;
}
```

挨个档编译跑一遍，看它到底吐什么：

```text
$ gcc -std=c89 stdcver.c -o v && ./v
__STDC_VERSION__ undefined (classic C89/C90)
$ for s in c99 c11 c17 c23; do gcc -std=$s stdcver.c -o v && printf "%-4s " $s && ./v; done
c99  __STDC_VERSION__ = 199901L
c11  __STDC_VERSION__ = 201112L
c17  __STDC_VERSION__ = 201710L
c23  __STDC_VERSION__ = 202311L
```

几个关键点：**C89/C90 这个最老的档位不定义 `__STDC_VERSION__`**（这个宏是 C89 修正案 AMD1 之后才有的，所以经典 C89 模式下它干脆不存在——这也是为什么上面要 `#ifdef` 兜一下，否则在 `-std=c89` 下直接用它会编译报错）；从 C99 起它才有值，`199901L` = 1999 年 1 月、`201112L` = 2011 年 12 月、`201710L` = 2017 年 10 月、`202311L` = 2023 年 11 月，年月一目了然。所以下次你接手一个工程，想知道它默认在哪个标准下编译，别靠猜——`__STDC_VERSION__` 跑一下就清楚。本课程从第 1 章起一律显式钉 `-std=c11`，就是为了把这个旋钮从「编译器默认值（gcc 16 默认是 gnu23）」的不确定性里摘出来。

### `cXX` 和 `gnuXX`：真正的分水岭不是你想的那个

第 1 章我们立过一条纪律：「显式钉 `-std=cXX`，不要依赖默认的 `-std=gnuXX`」。这里把背后的为什么讲透。很多人以为 `-std=c11` 和 `-std=gnu11` 的区别是「gnu11 放行 GNU 扩展、c11 不放行」——这说法对了一半，但容易误导。我用一个 GNU 扩展（statement expression，圆括号里塞语句块）当靶子，你以为它在 c11 下编不过、gnu11 下才行：

```c
#include <stdio.h>

int main(void) {
    /* GNU statement expression */
    int x = ({
        int tmp = 3;
        tmp + 4;
    }); // 是的，看到这个tmp + 4了嘛？猜猜他在干啥？
    printf("%d\n", x);
    return 0;
}
```

先看一个会推翻直觉的真跑结果——**两个模式 `-Wpedantic` 都报警**：

```text
$ gcc -std=c11 gnuext.c -o gnuext && ./gnuext       # 不加 -Wpedantic，c11 也能编过
7
$ gcc -std=c11   -Wpedantic gnuext.c -o gnuext 2>&1
gnuext.c:6:13: warning: ISO C forbids braced-groups within expressions [-Wpedantic]
$ gcc -std=gnu11 -Wpedantic gnuext.c -o gnuext 2>&1
gnuext.c:6:13: warning: ISO C forbids braced-groups within expressions [-Wpedantic]   ← gnu11 也警告！
```

看到了吗——**`-Wpedantic` 在 `-std=c11` 和 `-std=gnu11` 下都报了同一个警告**。这一下就纠正了一个常见误会：`-Wpedantic` 检查的是「这段代码符不符合 ISO C」，它才不管你 `-std` 是 c 还是 gnu；`-Wpedantic` 在 gnu 模式下照样会挑你扩展的毛病。而「c11 不加 `-Wpedantic` 也能编过」这件事告诉我们：现代 gcc 哪怕在 `-std=c11` 下，默认也还是**接受** GNU 扩展（只是给你留着「这玩意儿不标准」的警告口子），并不是 c11 就直接拒绝。

那 `-std=c11` 和 `-std=gnu11` 到底差在哪？真正的分水岭是一个叫 `__STRICT_ANSI__` 的宏：

```text
$ gcc -std=c11  strictansi.c -o sa && ./sa
__STRICT_ANSI__ defined
$ gcc -std=gnu11 strictansi.c -o sa && ./sa
__STRICT_ANSI__ NOT defined
```

**`-std=cXX` 会定义 `__STRICT_ANSI__`，`-std=gnuXX` 不会。** 这个宏本身不是 C 标准规定的，但 glibc 这些 C 库的头文件会拿它当开关：一旦 `__STRICT_ANSI__` 被定义，glibc 就把自己那些非 ISO C 的 POSIX/GNU 扩展函数**藏起来**，只暴露标准 C 的部分。这才是 c 和 gnu 模式最常咬人的一处区别——它影响的是「你能调到哪些库函数」。拿 `strdup`（复制字符串，POSIX 函数，不是 ISO C）当例子：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char* p = strdup("hi"); /* strdup 是 POSIX 的，不是 ISO C 的 */
    if (p) {
        printf("%s\n", p);
    }
    return 0;
}
```

```text
$ gcc -std=c11  -Wall strdup.c -o dup
strdup.c: In function 'main':
strdup.c:8:15: error: implicit declaration of function 'strdup'; did you mean 'strcmp'? [-Wimplicit-function-declaration]
$ gcc -std=gnu11 -Wall strdup.c -o dup && ./dup
hi
```

`-std=c11` 下，`strdup` 的声明被 glibc 藏起来了，编译器找不到它，于是报「隐式声明」。这里有个比报错本身更值得记一笔的事：**这条隐式声明在现代 gcc 上是 `error` 不是 `warning`**——从 GCC 14 起，`-Wimplicit-function-declaration` 从警告升级成了默认错误（C23 也正式把隐式函数声明从标准里移除了），所以 `dup` 这个可执行文件压根没生成出来。而 `-std=gnu11` 下，`__STRICT_ANSI__` 没定义，glibc 把 `strdup` 的声明亮出来，干干净净编过、跑出 `hi`。

所以「c 和 gnu 的区别」最实在的一句是：**gnu 模式（也是 gcc 的默认）会把 POSIX 这类非 ISO C 的库函数默认开给你用，c 模式则把它们收起来、逼你显式开口子**。如果你在 `-std=c11` 下又确实要用 `strdup`、`strtok_r` 这些 POSIX 函数，正确做法不是退回 gnu11，而是在文件最前面（任何 `#include` 之前）显式 `#define _POSIX_C_SOURCE 200809L`——这才是「我要 POSIX，且我明确知道我要」的写法。这种隐式声明的坑不止是「编不过」这么简单：在更老的编译器（隐式声明还只是 warning 的年代）上它能编过，但隐式声明的函数默认返回 `int`，会把 64 位的指针截断成 32 位，运行时收获一个漂亮的对空指针解引用——所以现代编译器把它升成 error，是帮你把一个历史雷区直接焊死。

### C23：`bool`/`true` 终于成了关键字

顺带讲一个 C23 的实在变化，它能直接改变你写代码的方式。在 C11 及更早，`bool`、`true`、`false` 不是关键字，而是靠 `<stdbool.h>` 提供的宏（`bool` 其实是 `_Bool` 的别名）。很多人会忘了 `#include <stdbool.h>` 就用 `bool`，然后在老标准下栽跟头。C23 把它们扶正成了真正的关键字：

```c
#include <stdio.h>

int main(void) {
    const bool ready = true; /* C23: bool/true 是关键字;C11 需要 <stdbool.h> */
    if (ready) {
        printf("ready\n");
    }
    return 0;
}
```

```text
$ gcc -std=c11 c23bool.c -o c23bool
c23bool.c:6:11: error: unknown type name 'bool'
    6 |     const bool ready = true;
      |           ^~~~
c23bool.c:2:1: note: 'bool' is defined in header '<stdbool.h>'; this is probably fixable by adding '#include <stdbool.h>'
$ gcc -std=c23 c23bool.c -o c23bool && ./c23bool
ready
```

C11 下 `bool` 是个「unknown type」，必须 `#include <stdbool.h>`；C23 下它原生就是关键字，省了那个头文件。C23 类似地把 `nullptr`、`constexpr`、`static_assert`、`thread_local`、`alignof`/`alignas` 这些以前靠头文件或带下划线丑名（`_Static_assert`、`_Thread_local`）的东西，都收编成了干净的关键字，还引入了 `[[nodiscard]]`、`[[deprecated]]` 这种属性语法——这些都是「C 在补 C++ 早就有的人体工学」的方向。本课程主线用 `-std=c11`（兼容性最稳、绝大多数教程和工程都吃），但你看到 C23 特性时心里要有数：它们不是「新语法糖」，有些（比如移除隐式函数声明）会直接改变代码能不能编过。

## `-O`：优化级别不只是「更快」

第 4 章我们已经从一个具体的 `compute` 函数看过 `-O0` 和 `-O2` 的差别——`-O0` 下局部变量老老实实全在栈帧里（好调试），`-O2` 按 as-if 规则把不影响可观察行为的中间变量整个吃掉（gdb 里就是 `<optimized out>`）。那一章点到为止，这里把全档位补齐。

先看一个最直观的指标——同一个函数在不同 `-O` 下生成的汇编有多大。写个数组求和的循环：

```c
#include <stdio.h>

__attribute__((noinline)) int sum(const int* a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += a[i];
    }
    return s;
}

int main(void) {
    int a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    printf("%d\n", sum(a, 8));
    return 0;
}
```

`sum` 上加了 `__attribute__((noinline))`，防止优化器把它内联进 `main` 后整个常量折叠掉（不然 `-O2` 会发现 `sum(a, 8)` 的结果是编译期常量 36，连循环都不生成）。现在看各档汇编的体积：

```text
$ for o in O0 O1 O2 O3 Os; do gcc -std=c11 -$o -S sumloop.c -o sum_$o.s; printf "%-3s %s 字节\n" $o "$(wc -c < sum_$o.s)"; done
O0  1437 字节
O1  1158 字节
O2  1329 字节
O3  2234 字节
Os  1085 字节
```

这个趋势比「越高越快」要丰富：从 `-O0` 到 `-O1` 体积降了（基础优化删掉了一堆啰嗦的栈帧倒腾），但 **`-O2` 比 `-O1` 还大了一点**，`-O3` 更是直接蹿到最大——因为越激进的优化（循环展开、向量化）越倾向于**用代码体积换执行速度**，它会把一段代码展开成多份、塞进 SIMD 指令。而 `-Os`（optimize for size）反其道而行，是所有档里最小的，它专门关掉那些会增大体积的优化，这在嵌入式、在乎 binary 体积的场景里是首选。

最有看头的是 `-O2` 和 `-O3` 在 `sum` 上的差别——`-O3` 默认开了 `-ftree-vectorize`（自动向量化），这是 `-O2` 默认不做的。看两者的 `sum` 函数体（AT&T 语法）：

```asm
// -O2 的 sum():标量循环,一次累加一个 int
sum:
    testl   %esi, %esi
    jle     .L4
    xorl    %eax, %eax
.L3:
    addl    (%rdi), %eax        ← 一次加一个 int
    addq    $4, %rdi
    cmpq    %rdx, %rdi
    jne     .L3
    ret

// -O3 的 sum():自动向量化,一次处理 4 个 int(SSE 的 paddd)
sum:
    ...
    pxor    %xmm0, %xmm0        ← 128 位累加器清零
.L6:
    movdqu  (%rax), %xmm2       ← 一次读进 4 个 int(16 字节)
    addq    $16, %rax
    paddd   %xmm2, %xmm0        ← packed add:4 个 int 并行相加!
    cmpq    %rdx, %rax
    jne     .L6
    ...                          ← 把 xmm0 里的 4 个 int 水平归约成一个
```

`-O2` 是老老实实一个 `addl` 加一个 `int`、循环 8 次；`-O3` 用 SSE 的 `paddd`（packed add dword）一条指令同时加 4 个 `int`，循环次数直接砍到四分之一，最后再水平归约。这就是「`-O3` 比 `-O2` 多了什么」最具体的样子：**自动向量化**。代价就是上面看到的体积膨胀（2234 vs 1329）。把几档的定位收拢一下：

| 级别 | 干什么 | 本课程什么时候用 |
|---|---|---|
| `-O0` | 不优化，变量都在栈上 | 调试（配 `-g`），默认就是它 |
| `-O1` | 基础优化（常量折叠、死代码消除），体积减小 | 想轻量优化又不太破坏调试 |
| `-O2` | 发布级优化（内联、循环优化等） | 性能基准、正式发布构建 |
| `-O3` | `-O2` + 自动向量化、循环展开等激进优化 | 算密集、且已用 profiler 验证有收益时 |
| `-Os` | 优化体积（关掉增体积的优化） | 嵌入式、在乎 binary 大小 |
| `-Og` | 调试友好的轻量优化 | 想要一点优化、又要保留可调试性 |

一句话记法：调试用 `-O0 -g`，发布默认 `-O2`，确认是算密集热点且 profiler 指路后再上 `-O3`，体积敏感用 `-Os`。`-O3` 不是「更好的 `-O2`」，它有体积和编译时间的代价，没验证过收益就盲目开 `-O3`，常常是又慢又大。

### `-O` 让未定义行为现形：一段「溢出检测」的彻底翻车

讲完「更快」，来讲 `-O` 最要命的一面——它会改变含 UB 的代码的运行结果。这是衔接第 10 章 sanitizer 的关键。下面这个写法，是很多人凭直觉写出来的「检测有符号溢出」的代码：它赌「溢出后值会变小」，所以用 `x + 100 < x` 来判断：

```c
#include <stdio.h>
#include <limits.h>

int check_overflow(int x) {
    if (x > 0 && x + 100 < x) { /* 想用「溢出后变小」来检测溢出 */
        return 1;
    }
    return 0;
}

int main(void) {
    volatile int v = INT_MAX;
    int x = v; /* 运行时读,阻止编译期折叠 */
    printf("check_overflow(INT_MAX) = %d\n", check_overflow(x));
    return 0;
}
```

这里有个我先替你踩过的坑：**千万别用编译期常量来演示 UB 的行为差异**。我最开始直接传 `check_overflow(INT_MAX)`，结果 gcc 在 `-O0/-O2/-O3` 三个档**全返回 0**——因为 `INT_MAX` 是编译期常量，编译器前端在 `-O0` 就把整个调用折叠求值了，UB 差异根本没机会现形。所以这里用一个 `volatile` 变量 `v` 喂运行时值（`volatile` 强制每次从内存读，编译器不能假定它的值），`x` 拿到的是运行时才确定的 `INT_MAX`，这样不同优化级别对 `x + 100 < x` 的处理差异才会暴露出来。

现在真跑——而且**两个编译器都跑**：

```text
$ gcc   -std=c11 -O0 uboverflow.c -o ub && ./ub
check_overflow(INT_MAX) = 0
$ gcc   -std=c11 -O2 uboverflow.c -o ub && ./ub
check_overflow(INT_MAX) = 0
$ clang -std=c11 -O0 uboverflow.c -o ub && ./ub
check_overflow(INT_MAX) = 1
$ clang -std=c11 -O2 uboverflow.c -o ub && ./ub
check_overflow(INT_MAX) = 0
```

事情一下子就有意思了。**clang 在 `-O0` 下返回 1**——它老老实实在运行时算 `INT_MAX + 100`，二补码回绕成一个负数，负数 `< INT_MAX` 成立，于是「检测到溢出」返回 1，这正是写这代码的人期望的行为。但 **gcc 在 `-O0` 和 `-O2` 下都返回 0**，连「检测」的影子都没有。而且注意：gcc 是连 `-O0` 都返回 0，不是只有 `-O2` 才翻车。

为什么会这样？根子在「有符号整数溢出是 UB」这条规矩（ISO/IEC 9899 §6.5 第 5 段：表达式求值时，若结果不在该类型可表示范围内，行为未定义）。既然是 UB，编译器就有权「假定它不会发生」——而 `x + 100 < x` 在「不溢出」的前提下，当 `x > 0` 时**永远为假**（正数加 100 只会更大）。配合第 4 章讲过的 as-if 规则（§5.1.2.3，只要可观察行为不变，编译器爱怎么改就怎么改），优化器一看这是个恒假条件，干脆把整个 `if` 体删了。我们看 gcc `-O0` 的 `check_overflow` 汇编，铁证就在这儿：

```asm
check_overflow:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    %edi, -4(%rbp)      ← 把参数 x 存一下
    movl    $0, %eax            ← 直接返回 0!整个 if 体被删光了
    popq    %rbp
    ret
```

**gcc 在 `-O0` 下就把 `if (x > 0 && x + 100 < x)` 整段删掉，直接 `return 0`。** 这条特别值得划重点：它说明「`-O0` 就一定老老实实按我写的跑」是个危险的天真想法——gcc 在 `-O0` 也会做一部分折叠，凡是要靠 UB（比如有符号溢出回绕）才能成立的代码，它都可能基于「UB 不会发生」提前帮你「优化」没。clang 的 `-O0` 相对老实（保留了运行时的加法和比较，所以回绕行为还在），但**这只说明 clang 选择给你这个行为，不说明 C 语言保证它**——一上 clang `-O2`，它也立刻和 gcc 一样删掉检查、返回 0。

把这件事说破：**「我开了 `-O0` 测过，结果是对的」根本不能算任何保证**。你测到的是「这个编译器在这个优化级别下、碰巧选择给你的行为」，而 C 标准（因为这是 UB）对它**一个字都没承诺**。换编译器、换优化级别、升级一版 gcc，行为就可能翻——而且往往是等你切到 release 构建（`-O2`）时才翻车。这正是为什么第 10 章我们要上 sanitizer：不靠这种「赌回绕」的歪路子，而是让编译器在每个算术操作前后**真正插桩**，溢出当场报给你看。顺带这也回答了第 8 章那个扣子——为什么 `-Wuninitialized` 跟 `-O` 绑定：因为它本质上借用了优化器的数据流分析能力，所以老资料会说「开了 `-O` 才灵」；但第 8 章我们已经真跑证明，哪怕开了 `-O2`，条件分支里的未初始化读取它照样漏——静态分析（警告）和「假定无 UB」（优化）各有各的盲区，运行期插桩才是兜底。

## `-g`：往可执行文件里塞一份「源码地图」

最后这个旋钮 `-g` 管的是「带不带调试信息」。它本身不影响程序跑起来的逻辑（`.text` 段的机器码不变），它只是额外往可执行文件里塞一份「这条机器指令对应源码第几行、这个变量叫什么、类型是什么」的映射表，让 gdb 这类调试器能把机器层面的执行「翻译回」你写的 C 源码。这份映射表用的是一套独立的格式标准——**DWARF**（它不是 ISO C 规定的，是调试信息的事实标准），塞在 ELF 文件的 `.debug_*` 段里。我们先看它对体积的影响：

```text
$ gcc -O0      sumloop.c -o nog && gcc -O0 -g0 sumloop.c -o g0 && gcc -O0 -g sumloop.c -o g1 && gcc -O0 -g3 sumloop.c -o g3
$ for f in nog g0 g1 g3; do printf "%-4s %s 字节\n" $f "$(wc -c < $f)"; done
nog  16056 字节
g0   16056 字节
g1   17440 字节
g3   44800 字节
```

什么都不加，和显式 `-g0`（明确关掉调试信息）体积一样，都是 16056；加默认的 `-g`，多出来一千多字节；开到 `-g3`，体积直接蹿到 44800，翻了快三倍。多出来的东西落在哪？用 `readelf` 看段表：

```text
$ readelf -S g1 | grep -oE '\.debug_[a-z_]+'
.debug_abbrev
.debug_aranges
.debug_info
.debug_line
.debug_line_str
.debug_str
$ readelf --debug-dump=info g1 | grep Version
    Version: 5
```

这一堆 `.debug_*` 段就是 DWARF 调试信息的各个组成部分：`.debug_line` 存「机器指令 ↔ 源码行号」的映射（gdb 单步、设断点靠它）、`.debug_info` 存变量和类型的描述、`.debug_str`/`.debug_line_str` 存字符串。`Version: 5` 说明这台机器上 gcc 16 和 clang 22 默认都生成 **DWARF 第 5 版**（可以用 `-gdwarf-4` 退回第 4 版兼容老调试器，但一般不用动）。而 `-g3` 那暴增的体积，多出来的是一个 `.debug_macro` 段——它把所有宏定义（`#define`）的展开信息也记了下来，方便你在 gdb 里 `print` 一个宏、或查宏展开，普通 `-g` 是不带的：

```text
$ readelf -S g3 | grep -oE '\.debug_[a-z_]+'
.debug_abbrev
.debug_aranges
.debug_info
.debug_line
.debug_line_str
.debug_macro          ← -g3 独有:宏定义信息
.debug_str
```

有这份「源码地图」和没有，gdb 的表现天差地别。用 gdb 查 `sum` 函数对应源码的哪一行：

```text
$ gdb -q -batch -ex "info line sum" g1
Line 4 of "sumloop.c" starts at address 0x1149 <sum> and ends at 0x1154 <sum+11>.
$ gdb -q -batch -ex "info line sum" nog
No symbol table info available.
```

带 `-g` 的 `g1`，gdb 能精确告诉你 `sum` 从源码第 4 行开始、落在地址 `0x1149`；没 `-g` 的 `nog`，gdb 两眼一抹黑——「No symbol table info available」。这就是为什么本课程调试一律 `-O0 -g`：没有 `-g`，gdb 连「现在执行到第几行、这个变量叫什么」都答不上，单步调试根本无从谈起。

最后把 `-g` 和 `-O` 的关系点一句。`-g` 是「把源码映射记下来」，但记下来能不能用，还得看 `-O` 有没有把你的代码改得面目全非。第 4 章我们已经用 `compute` 的例子看过：`-O2` 把局部变量 `a`、`b` 优化没了，哪怕你加了 `-g`，gdb 里 `print a` 收获的也是一句 `<optimized out>`，断点停的行号还会乱跳——因为代码已经被重排、合并，源码行和机器指令的对应关系被打乱了。所以结论很硬：**要老老实实单步调试、看每个变量，就得 `-O0 -g` 一起上**；`-O2 -g` 的 `-g` 只能帮你定位到「大概在这个函数」，看不了变量细节。如果你又想要一点优化、又想保留可调试性，gcc 提供了 `-Og`（见上面的表）作为折中。

## 小结

到这，编译器上这三个最常被混用的旋钮就被我们挨个拧过一遍了。`-std` 决定你编译的是哪个年代的 C，靠 `__STDC_VERSION__` 这个预定义宏（§6.10.8，C89 不定义、C99 起是 `199901L`、一路到 C23 的 `202311L`）能确认当前档位；`-std=cXX` 和 `-std=gnuXX` 真正的分水岭不是 `-Wpedantic`（它在两种模式下都警告扩展），而是 `__STRICT_ANSI__` 这个宏——c 模式定义它、gnu 模式不定义，glibc 拿它当开关决定要不要把 `strdup` 这类 POSIX 函数亮给你，所以在 c11 下用 POSIX 函数会撞上「隐式声明」（GCC 14 起它已是 error，C23 也移除了隐式函数声明）；C23 则把 `bool`/`true`/`nullptr`/`constexpr` 这些收编成了原生关键字，省掉了 `<stdbool.h>` 之类的前置。`-O` 这边，体积趋势不是单调递减——`-O1` 比 `-O0` 小、`-O2` 反而略大、`-O3` 最大（因为它默认开自动向量化，用 `paddd` 一条指令加 4 个 int，拿体积换速度），`-Os` 最小；定位上一句话是调试 `-O0 -g`、发布 `-O2`、确认算密集热点后再 `-O3`。而 `-O` 最该记牢的是它和 UB 的关系：有符号溢出是 UB（§6.5 第 5 段），编译器有权假定它不发生，于是那段「靠溢出回绕检测溢出」的代码，gcc 连 `-O0` 都把检查整个删掉返回 0、clang `-O0` 才老实回绕、上了 `-O2` 也照样删——所以「我 `-O0` 测过」不作数，换编译器换优化级别就可能翻，运行期插桩（第 10 章 sanitizer）才是兜底。`-g` 则是往 ELF 里塞一份 DWARF 格式的「源码地图」（`.debug_line` 是行号映射、`.debug_info` 是类型/变量描述，gcc16/clang22 默认 DWARF 5），没它 gdb 连「现在第几行」都答不出，所以调试必须 `-g`，且要配 `-O0` 才能真看得到变量（`-O2` 下再带 `-g` 也只是 `<optimized out>`），`-g3` 会额外记下宏定义信息、体积翻倍。

下一章我们正式把 sanitizer 这个「兜底」请上来——看 `-fsanitize=address,undefined` 怎么在编译期给每个内存访问、每个算术操作插桩，把这一章里那种「靠 UB 回绕」检测不出来的溢出、还有越界访问、use-after-free，统统在运行期当场抓获。

## 参考资源

- ISO/IEC 9899:2011 §6.10.8（预定义宏名，`__STDC_VERSION__` 的出处，及 C89/C90 不定义它）、§6.5 第 5 段（表达式求值的 UB，有符号溢出的标准依据）、§5.1.2.3（程序执行 / 可观察行为，as-if 规则）
- GCC 手册：`-std=` 各档、`-O0..-O3`/`-Os`/`-Og`、`-ftree-vectorize`、`-g`/`-g0`/`-g3`、`-gdwarf-version`、GCC 14 release notes（`-Wimplicit-function-declaration` 升级为默认 error）
- DWARF Debugging Information Standard（`.debug_*` 段的格式定义，独立于 ISO C 的调试信息标准）
- 第 4 章：编译阶段看汇编（`-O0`/`-O2` 的 as-if 演示、`<optimized out>` 的出处）
- 第 10 章：Sanitizer 门禁（本章「`-O` 让 UB 现形」的运行期兜底）
