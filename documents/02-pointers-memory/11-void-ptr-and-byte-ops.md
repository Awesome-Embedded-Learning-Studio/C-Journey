---
title: "void* 与字节操作：通用指针、char* 按字节走、memcpy/memset"
description: "这一章啃下 C 里最「通用」也最「不能用」的指针类型——void*。它是「通用对象指针」:任何对象指针(int*/char*/struct foo*)都能隐式转成它、再转回原类型(ISO §6.3.2.3p1/p7),这就是 C 实现「泛型」的根基——malloc 返回 void* 给你任意类型的内存、qsort 用 const void* 接收任意类型的元素。但「什么都能指」等于「不知道指的什么」,所以 void* 不能直接解引用(void 无大小、不知读几字节)、不能做算术(步长未知)——要用得先转回具体类型。真跑给你看:int*→void*→int* 地址完全一致、转换无损;void* 直接 *pv 和 pv+1 在标准里一个是 UB 一个是约束违反(gcc/clang 默认居然放行——因为把它们当 GNU 扩展,得加 -pedantic-errors 才拦下来,这是个真坑)。接着把 unsigned char* 当「字节指针」按字节遍历一个 int 的 4 字节,真跑 0x12345678 在 x86 小端机上存成 78 56 34 12(最低有效字节在最低地址)。最后讲 memcpy/memset(§7.24.2.1/§7.24.6.1):参数都是 void*、按字节搬/填,比 strcpy 更底层(不关心 \0,真跑把含 \\0 的 5 字节缓冲区用 strcpy 只搬 3 字节、用 memcpy 搬满 5 字节),restrict(§6.7.3.1)只一句带过。全 gcc16+clang22 真跑。"
chapter: 2
order: 11
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第1章：指针是什么（指针类型 int*、指针大小与指向无关）"
  - "阶段2·第2章：指针算术（p+1 步长由指向类型定）"
  - "阶段2·第6章：动态内存入门（malloc 返回 void*）"
  - "第 4 章：浮点、字符、常量与隐式转换（char 是字节、sizeof(char)==1）"
related:
  - "阶段2·第6章：动态内存（malloc 返回 void*、是泛型分配的根基）"
  - "阶段2·第9章：函数指针（qsort 的比较函数用 const void* 接收任意类型）"
  - "阶段2·第12章：内存布局与生命周期（字节级视角看内存地图）"
---

# void* 与字节操作：通用指针、char* 按字节走、memcpy/memset

## 引言：什么都能指，所以什么都不能直接干

第 6 章 `malloc` 返回 `void*`、第 9 章 `qsort` 的比较函数收 `const void*`，这个 `void*` 我们一直用着、却没正面解释过。这一章就把它讲透。`void*` 是 C 里的「**通用对象指针**」——任何指向具体对象类型的指针（`int*`、`char*`、`struct foo*`……）都能隐式转成它、再转回原类型，地址不变。这条规矩是 C 实现「泛型」的根基：`malloc` 不知道你要存 `int` 还是 `struct`，就返回一个「什么都可能」的 `void*` 让你自己转；`qsort` 不知道你排的是 `int` 数组还是字符串数组，比较函数就收两个 `const void*`、你在函数里再转回具体类型比大小。

但「什么都能指」的另一面是「不知道指的什么」——`void*` 不知道自己指向的对象是什么类型、多大，所以它**不能直接解引用**（`*pv` 不行，`void` 没有大小、编译器不知道该读几字节）、**不能做算术**（`pv + 1` 步长未知）。要用它，得先转回具体类型。这一章我们就把这三件事真跑通：`void*` 怎么转、为什么不能直接用、以及怎么用 `unsigned char*` 和 `memcpy`/`memset` 在字节层面操作任意内存。

## void* 的两条规矩

ISO §6.3.2.3 给了 `void*` 两条核心规矩。**第一条（§6.3.2.3p1）**：指向 `void` 的指针可以和任何指向对象类型的指针**互相转换**，转换后值相同（也就是同一个地址）。`int*` 能隐式转 `void*`、`void*` 也能隐式转回 `int*`，不需要强转。**第二条（§6.3.2.3p7）**：当一个 `void*` 转回它**原来的**类型时，结果和原指针比较相等（round-trip 无损）——也就是说 `int* → void* → int*` 走一圈，地址一个比特都不变。这两条合起来就是 `malloc`/`qsort` 那套「泛型」能成立的法律依据。我们真跑给你看：

```c
#include <stdio.h>

int main(void) {
    int x = 0x12345678;
    int* pi = &x;     /* pi 指向 x */
    void* pv = pi;    /* 任何对象指针都能隐式转 void*(ISO §6.3.2.3p1) */
    int* pi2 = pv;    /* void* 也能隐式转回 int*(§6.3.2.3p1) */

    printf("&x  = %p\n", (void*)&x);
    printf("pi  = %p\n", (void*)pi);
    printf("pv  = %p\n", pv);
    printf("pi2 = %p\n", (void*)pi2);
    printf("*pi2 = 0x%x (读回 x 的值,转换无损)\n", *pi2);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall void_convert.c -o vc && ./vc
&x  = 0x7ffe5ab8e63c
pi  = 0x7ffe5ab8e63c
pv  = 0x7ffe5ab8e63c
pi2 = 0x7ffe5ab8e63c
*pi2 = 0x12345678 (读回 x 的值,转换无损)
```

四个地址全是 `0x7ffe5ab8e63c`——`&x`、`pi`、`pv`、`pi2` 是同一个值，印证 §6.3.2.3p1「转换后值相同」和 p7「round-trip 相等」。最关键的是最后一行：`pi2` 是从 `void*` 转回来的，`*pi2` 读到 `0x12345678`，和 `x` 一模一样——转换完全无损。注意 `pi2 = pv;` 这行**没有任何强转**，`void* → int*` 在 C 里是隐式的（C++ 里反过来、要强转，这是 C 和 C++ 的一个分歧点）。`pv` 用 `%p` 打印时甚至不用 `(void*)` 转——因为 `pv` 本身就是 `void*`，`%p` 要的正是 `void*`（§7.21.6.1p8）。地址值每次跑都不一样（ASLR，第 1 章讲过），但四个一定相等。

## void* 不能直接用：解引用与算术

「什么都能指」的代价，是 `void*` 不知道自己指的类型。ISO §6.5.3.2p4 规定，一元 `*`（解引用）的操作数若指向**不完整类型**（incomplete type），行为未定义——而 `void` 正是不完整类型（它没有大小、不知道「一个 `void`」占几字节）。所以 `*pv` 在标准里是 UB。指针算术同理：§6.5.6 定义的指针±整数，整套语义都建立在「指向数组对象的某个元素」之上，步长由「指向类型的大小」决定（第 2 章）；`void` 没有大小、`void*` 加 1 该跨几字节无从谈起，所以标准没有定义 `void*` 的算术。

这里有个真坑得就地提醒：很多人以为写上 `-std=c11` 编译器就会把这些当错误拦下来，**并非如此**。gcc 和 clang 把 `void*` 算术当成各自的 GNU 扩展（gcc 扩展规定 `void*` 按 1 字节走，相当于 `char*`），默认 `-std=c11 -Wall` 居然是**编译通过、只给 warning**。要让它真正报 error，得加 `-pedantic-errors`（严格按标准、把扩展当错误）。我们先看 `void*` 算术这一条：

```c
#include <stdio.h>

int main(void) {
    int x = 42;
    void* pv = &x;
    void* q = pv + 1;   /* 标准未定义:void* 算术(步长未知) */
    (void)q;
    return 0;
}
```

```text
$ gcc -std=c11 -Wall void_arith_fail.c -o vaf
$ echo $?
0
```

看清楚了——退出码 `0`，编译**成功**了。gcc 默认根本没拦它（`-Wall` 不开 `-Wpointer-arith`，这个坑能让你写出一堆「在 gcc 上能跑、换个严格点的编译器/平台就炸」的代码）。加上 `-pedantic-errors` 才露出真面目：

```text
$ gcc -std=c11 -Wall -pedantic-errors void_arith_fail.c -o vaf
void_arith_fail.c: In function ‘main’:
void_arith_fail.c:6:18: error: pointer of type ‘void *’ used in arithmetic [-Wpointer-arith]
    6 |     void* q = pv + 1;   /* 标准未定义:void* 算术(步长未知) */
      |                  ^
$ echo $?
1
```

这回是 `error: pointer of type 'void *' used in arithmetic`、退出码 `1`——这才是标准要的「约束违反必须诊断」。clang22 同样要 `-pedantic-errors` 才报（报的是 `arithmetic on a pointer to void is a GNU extension`），措辞不同、性质一样。教训很简单：**别因为 gcc 默认放行就以为 `void* + 1` 合法**，写正经代码请老老实实先 `(char*)pv` 转成字节指针、或转回原类型再做算术。

`void*` 解引用（`*pv`）是另一番景象，得单独说。标准对它的定性是 UB（§6.5.3.2p4，不完整类型解引用），UB 不强制诊断，所以编译器是「额外送你一个 warning」：

```c
#include <stdio.h>

int main(void) {
    int x = 42;
    void* pv = &x;
    *pv;                /* UB:解引用 void*(void 无大小、不知读几字节) */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall void_deref_fail.c -o vdf
void_deref_fail.c: In function ‘main’:
void_deref_fail.c:6:5: warning: dereferencing ‘void *’ pointer
    6 |     *pv;                /* UB:解引用 void*(void 无大小、不知读几字节) */
      |     ^~~
```

gcc16 即使加上 `-pedantic-errors` 也只是这条 warning（不升 error），因为标准给的是 UB、不是约束违反。clang22 默认给 `ISO C does not allow indirection on operand of type 'void *'`，加 `-pedantic-errors` 后升成 error。两个编译器措辞不同，但都告诉你同一件事：**`*pv` 没意义，因为编译器不知道把那块内存当什么类型读**。所以 `void*` 想用，先转回去——`(int*)pv` 再 `*`，或者干脆一开始就用具体类型指针。`void*` 是个「中转站」，不是终点。

## unsigned char*：当字节指针用

既然 `void*` 不能直接走字节、不能直接读，那 C 里「按字节遍历任意内存」用什么？答案是 `char*` 或 `unsigned char*`——它们是 C 里**天生的字节指针**。原因有两个：一是 `sizeof(char) == 1` 是 ISO 钉死的（§6.5.3.4p4，`char` 就是一个字节），所以 `char*` 或 `unsigned char*` 加 1 正好跨 1 字节、`p[i]` 正好取第 `i` 个字节；二是标准允许通过 `char*`/`unsigned char*` 去访问**任何对象**的底层字节表示（§6.3.2.3p7 的精神 + §6.5p7 的别名规则给了 `char` 类型特殊豁免），这叫「类型双关」（type punning）的字节级视角。我们用它把一个 `int` 的 4 个字节逐个打出来，看它在内存里到底长什么样：

```c
#include <stdio.h>

int main(void) {
    int x = 0x12345678;
    /* unsigned char* 当「字节指针」:sizeof(unsigned char)==1,步长 1 字节 */
    unsigned char* p = (unsigned char*)&x;

    printf("x = 0x%08x,占用 %zu 字节\n", x, sizeof(x));
    printf("逐字节(从低地址到高地址):\n");
    for (size_t i = 0; i < sizeof(x); i++) {
        printf("  +%zu: 0x%02x\n", i, p[i]);
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall byte_walker.c -o bw && ./bw
x = 0x12345678,占用 4 字节
逐字节(从低地址到高地址):
  +0: 0x78
  +1: 0x56
  +2: 0x34
  +3: 0x12
```

注意这四行的顺序：写代码时我们写 `0x12345678`，最高位是 `12`、最低位是 `78`；可在内存里从低地址往高地址读出来，却是 `78 56 34 12`——**最低有效字节 `0x78` 放在最低地址**。这台机器（x86_64、WSL2）是**小端序**（little-endian）。如果你在大端机（比如某些老 ARM、PowerPC）上跑同一段代码，会得到 `12 34 56 78`，正好反过来。C 标准没规定字节序（它是实现定义行为），所以写跨平台代码千万别假设字节序——真要看，就用这段 `unsigned char*` 遍历法自己跑一遍（网络编程里 `htonl`/`ntohl` 那套就是专门处理这个差异的）。

`(unsigned char*)&x` 这步强转是合法的（§6.3.2.3p1，任何对象指针都能转 `void*`、也能转 `char*`——`char*` 在别名规则里同样有豁免），转完之后 `&x` 那块「存 `int` 的内存」就被当成「4 个 `unsigned char`」来读了。`p[i]` 等价于 `*(p + i)`（第 10 章的 `a[i] ≡ *(a+i)`），`p` 步长 1 字节，所以 `p[0]` 是第 0 字节、`p[1]` 是第 1 字节。`%02x` 把每个字节按两位十六进制打印（不足两位补零）。这套「字节指针遍历」是调试二进制数据、看浮点数位模式、序列化、检查字节序的通用招数，第 12 章讲内存布局时还会用到。

## memcpy 与 memset：按字节搬/填

把上面的「字节视角」推到极致，就是标准库的 `memcpy` 和 `memset`（都在 `<string.h>`，§7.24.2.1 / §7.24.6.1）。它们的参数都是 `void*`、内部就是按字节搬或填，比 `strcpy` 更底层——`strcpy` 遇 `\0` 就停（因为它认 C 字符串），`memcpy`/`memset` 只看你给的「字节数」、不管内容里有没有 `\0`。先看 `memcpy` 拷贝一个 `int`、`memset` 把数组清零：

```c
#include <stdio.h>
#include <string.h>  /* memcpy / memset 在这里(§7.24.2.1 / §7.24.6.1) */

int main(void) {
    /* ---- memcpy:按字节搬内存,不关心类型、不关心 \0 ---- */
    int src = 0x12345678;
    int dst = 0;                       /* 没初始化成 0 也行,会被整块覆盖 */
    memcpy(&dst, &src, sizeof(int));   /* 从 &src 搬 sizeof(int) 字节到 &dst */
    printf("memcpy 后:src = 0x%x, dst = 0x%x\n", src, dst);

    /* ---- memset:按字节填内存(常用来清零数组)---- */
    int a[5] = {1, 2, 3, 4, 5};
    printf("清零前:");
    for (int i = 0; i < 5; i++) printf(" %d", a[i]);
    printf("\n");

    memset(a, 0, sizeof(a));           /* 把 a 的 sizeof(a)=20 字节全填成 0 */
    printf("清零后:");
    for (int i = 0; i < 5; i++) printf(" %d", a[i]);
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall memcpy_memset.c -o mm && ./mm
memcpy 后:src = 0x12345678, dst = 0x12345678
清零前: 1 2 3 4 5
清零后: 0 0 0 0 0
```

`memcpy(&dst, &src, sizeof(int))` 的三个参数：目的地 `&dst`（`int*`，隐式转 `void*`）、来源 `&src`、字节数 `sizeof(int)` = 4。它把 `src` 那 4 字节原样搬到 `dst` 那 4 字节，结果 `dst` 也变成 `0x12345678`。注意它**不知道也不在乎**这是 `int` 还是 `float` 还是 `struct`，只认字节——这就是「泛型内存搬运」。`memset(a, 0, sizeof(a))` 同理：把 `a` 的 `sizeof(a)` = 20 字节全部填成 `0`（第二个参数 `0` 是要填的字节值，取低 8 位），于是 5 个 `int` 全变 `0`。清零数组、清零结构体、把一块缓冲区初始化成 `0xff`，`memset` 是标准做法。

那 `memcpy` 和 `strcpy` 到底差在哪？我们故意造一个含 `\0` 的缓冲区，让两者对比着搬，差别一目了然：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    /* 源缓冲区里有个 \0,后面还藏着 'X' 'Y' */
    char src[5] = {'A', 'B', '\0', 'X', 'Y'};
    char dst_s[5] = {0};
    char dst_m[5] = {0};

    strcpy(dst_s, src);                 /* 遇 \0 就停,\0 后的 X Y 不搬 */
    memcpy(dst_m, src, sizeof(src));    /* 按字节搬 5 个,\0 也只是一字节 */

    printf("strcpy 结果(遇 \\0 停):");
    for (int i = 0; i < 5; i++) printf(" 0x%02x", (unsigned char)dst_s[i]);
    printf("\n");
    printf("memcpy 结果(搬满 5 字节):");
    for (int i = 0; i < 5; i++) printf(" 0x%02x", (unsigned char)dst_m[i]);
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall memcpy_vs_strcpy.c -o mvs && ./mvs
strcpy 结果(遇 \0 停): 0x41 0x42 0x00 0x00 0x00
memcpy 结果(搬满 5 字节): 0x41 0x42 0x00 0x58 0x59
```

`src` 5 字节是 `{'A'=0x41, 'B'=0x42, '\0'=0x00, 'X'=0x58, 'Y'=0x59}`。`strcpy` 遇到第三个字节的 `\0` 就停了，连 `\0` 一起搬过去就收工，后面藏的 `X`、`Y` 完全没动（`dst_s` 后两位还是初始的 `0x00`）。`memcpy` 则老老实实搬满指定的 5 字节，`0x00` 后面的 `0x58 0x59` 也搬了过去。这就是本质差别：`strcpy` 是「字符串语义」（认 `\0` 结尾），`memcpy` 是「字节语义」（只认长度）。所以**搬非字符串数据（结构体、`int` 数组、含 `\0` 的二进制数据）一律用 `memcpy`，别用 `strcpy`**——`strcpy` 会把第一个 `\0` 之后的数据全丢掉。

顺带提一句 `restrict`。`memcpy` 的原型是 `void *memcpy(void * restrict dest, const void * restrict src, size_t n);`（§7.24.2.1），那两个 `restrict`（§6.7.3.1）是给编译器的优化提示，意思是「我保证 `dest` 和 `src` 指向的内存不重叠」，编译器据此可以放心地一边读一边写、不用先拷一份。代价是：**你要是让它们重叠了，就是 UB**（`memcpy` 不管重叠区，重叠请用 `memmove`，它会先拷到临时区再写、慢一点但安全）。`restrict` 本章只点到这，不展开——记住「`memcpy` 的源和目的不能重叠，重叠换 `memmove`」就够了。

## 小结

`void*` 是 C 的「通用对象指针」：任何对象指针（`int*`、`char*`、`struct foo*`）都能隐式转成它、它也能隐式转回原类型，转换后值相同、round-trip 无损（ISO §6.3.2.3p1/p7，真跑 `int*→void*→int*` 四个地址全是 `0x7ffe5ab8e63c`、`*pi2` 读回 `0x12345678`）——这是 `malloc` 返回 `void*`、`qsort` 收 `const void*` 实现「泛型」的法律根基。代价是 `void*` 不知道自己指的什么类型，所以**不能解引用**（§6.5.3.2p4，`void` 是不完整类型、不知读几字节，标准定为 UB）、**不能算术**（§6.5.6 指针±整数建立在「指向完整类型」上，`void` 无大小）；要用得先转回具体类型。这里有个真坑：gcc/clang 默认 `-std=c11 -Wall` 居然放行 `void* + 1`（把它当 GNU 扩展、按 1 字节走），要加 `-pedantic-errors` 才报 `error: pointer of type 'void *' used in arithmetic`（退出码 1）——别因为 gcc 没拦就以为合法，正经代码请转回具体类型或 `char*` 再算术。「按字节走任意内存」的标准工具是 `unsigned char*`：`sizeof(char) == 1` 钉死（§6.5.3.4p4），步长正好 1 字节，还能合法访问任何对象的底层字节表示；真跑 `unsigned char* p = (unsigned char*)&x;` 遍历 `int x = 0x12345678` 的 4 字节，在 x86 小端机上得 `78 56 34 12`（最低有效字节在最低地址），大端机则相反——字节序是实现定义、别假设，要看就自己跑。把字节视角推到极致是 `memcpy`/`memset`（§7.24.2.1/§7.24.6.1）：参数都是 `void*`、按字节搬/填、不管内容有没有 `\0`，真跑 `memcpy(&dst,&src,sizeof(int))` 让 `dst == src == 0x12345678`、`memset(a,0,sizeof(a))` 把数组清成 `0 0 0 0 0`；和 `strcpy` 对照（真跑含 `\0` 的 5 字节缓冲区，`strcpy` 遇 `\0` 停只搬 3 字节、`memcpy` 搬满 5 字节），可知搬非字符串/含 `\0` 的数据一律用 `memcpy`。`memcpy` 原型里的 `restrict`（§6.7.3.1）是「源、目的不重叠」的优化承诺，重叠了是 UB——重叠请换 `memmove`。下一章我们站到更高的视角，用一张内存地图把 `.text`/`.rodata`/`.data`/`.bss`/堆/栈全串起来，看这一章的字节操作在第 12 章的内存布局里落在哪儿。

## 参考资源

- ISO/IEC 9899:2011 §6.3.2.3p1（指向 void 的指针可与任何对象指针互转、值相同）、§6.3.2.3p7（`void*` 转回原类型、与原指针比较相等）、§6.5.3.2p4（解引用指向不完整类型的指针是 UB）、§6.5.6（指针算术只对指向完整对象类型的指针有定义）、§6.5.3.4p4（`sizeof(char)==1`）、§7.24.2.1（`memcpy`）、§7.24.6.1（`memset`）、§6.7.3.1（`restrict` 限定符）
- K. N. King《C Programming: A Modern Approach》第 20 章·Low-Level Programming（20.1 `memcpy`/`memmove`/`memset`/`memcmp` 字节级操作、`void*` 的用法与限制）
- Robert C. Seacord《Effective C》第 5 章·Pointers and Arrays（`void*` 通用指针、`memcpy` 与 `restrict` 的契约、`memmove` 处理重叠）
- 第 4 章:浮点、字符、常量与隐式转换（`char` 是字节、`sizeof(char)==1`）、阶段2·第1章:指针是什么（指针类型与大小）、第2章:指针算术（步长由指向类型定）、第6章:动态内存（`malloc` 返回 `void*`）、第9章:函数指针（`qsort` 比较函数收 `const void*`）
- 阶段2·第12章:内存布局与生命周期（字节级视角看 `.text`/`.rodata`/`.data`/`.bss`/堆/栈地图）
