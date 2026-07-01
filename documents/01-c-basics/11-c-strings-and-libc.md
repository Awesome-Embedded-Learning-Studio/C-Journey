---
title: "C 字符串与不安全 libc：一个 \\\\0 引发的所有麻烦"
description: "这一章讲 C 的字符串——它根本不是一个独立类型,而是「以 \\0 结尾的 char 数组」(ISO §7.1.1)。真跑 strlen 与 sizeof 的区别:对 char s[]=\"hi\",strlen 得 2(不算 \\0)、sizeof 得 3(算 \\0);还区分 '\\0'(空字符,码 0)和 '0'(零字符,码 48)。重点拆字符串字面量:\"abc\" 落在只读存储区、是个 char*,char* p = \"hello\" 让 p 指向它,真跑 p[0]='H' 修改字面量直接段错误(退出码 139,ASan 报 SEGV)——这是 UB(§6.4.5p7);而 char a[] = \"hello\" 是把字面量拷贝到栈上、这份拷贝可改。再过一遍 string.h 家族(strcpy/strcat/strcmp/strchr/strstr),并真跑 strcpy 缓冲区溢出(写越界,gcc 编译期 -Wstringop-overflow 警告 + ASan stack-buffer-overflow 点名变量)。最后讲安全替代的真坑:strncpy 在源串>=n 时不补 \\0(gcc -Wstringop-truncation 警告,真跑字节 65 66 67 68 无 \\0)、snprintf 带 \\0 返回值是「本该写入的长度」(截断时 n > 缓冲区)、fgets 取代 C11 已删的 gets。全 gcc16+clang22 真跑。"
chapter: 1
order: 11
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 4 章：浮点、字符、常量与隐式转换（char 是小整数，'A'=65）"
  - "第 10 章：数组（char 数组、退化、越界访问是 UB）"
related:
  - "阶段 2：指针与内存（char* 的彻底拆解、动态字符串、字符串与指针算术）"
---

# C 字符串与不安全 libc：一个 \0 引发的所有麻烦

## 引言：C 没有真正的字符串类型

很多语言有一个 `string` 类型，写 `"hello"` 就是一个字符串对象，长度随便问、拼接随便来、越界有人管。C 不是这样。**C 标准里压根没有「字符串类型」这个东西**——它的字符串定义（ISO/IEC 9899:2011 §7.1.1）说的是：「一个字符串，就是一段连续的字符，从某个位置开始、到第一个空字符 `\0` 为止」。换句话说，C 的字符串就是**一个以 `\0` 结尾的 `char` 数组**，仅此而已。没有长度字段、没有边界保护、编译器不知道你手里那段 `char*` 到底是不是一个「合法结尾」的字符串。这设计在 70 年代挺省内存，代价就是今天我们还在为它写整本整本的缓冲区溢出事故报告。

这一章我们就把这个「`\0` 结尾的 char 数组」彻底认全：先看清 `\0` 这个终止符到底怎么影响 `strlen` 和 `sizeof`、它和字符 `'0'` 有什么天壤之别；再看清字符串字面量 `"hello"` 到底住在内存哪里、为什么 `char* p = "hello"; p[0]='H';` 会段错误而 `char a[] = "hello"; a[0]='H';` 就没事；然后把 `<string.h>` 那一大家子（`strcpy`/`strcat`/`strcmp`/`strchr`/`strstr`）过一遍；最后——也是这一章最该上心的部分——讲清楚为什么这一票老函数（`strcpy`、`strcat`、`gets`……）个个都是「缓冲区溢出」的温床，以及它们的带边界版本（`strncpy`、`snprintf`、`fgets`）各自藏着什么坑。上一章数组学的「越界是 UB」在这里会反复出场，那一套 sanitizer 门禁同样全程在线。

## `\0` 终止符：strlen 和 sizeof 的分水岭

C 字符串的「长度」靠 `\0` 这个空字符来界定——函数们顺着字符一个一个往后读，碰到 `\0` 就停。所以一个写成 `"hi"` 的字符串，实际上在内存里占 **3** 个字节：`'h'`、`'i'`、再加一个 `\0`（上一章初始化 char 数组时我们已经瞥见过它）。`\0` 是字符串的**结尾标记**，但它自己不算字符串「内容」的一部分。这件事最直接的体现，就是 `strlen` 和 `sizeof` 对同一个东西量出来的长度不一样：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char s[] = "hi";
    printf("sizeof(s) = %zu  (含末尾 \\0)\n", sizeof(s));
    printf("strlen(s) = %zu  (不算 \\0)\n", strlen(s));
    printf("'\\0' = %d, '0' = %d\n", '\0', '0');
    return 0;
}
```

```text
$ gcc -std=c11 -Wall len.c -o len && ./len
sizeof(s) = 3  (含末尾 \0)
strlen(s) = 2  (不算 \0)
'\0' = 0, '0' = 48
```

`sizeof(s)` 是 **3**——它问的是「这个数组占多少字节」，把结尾的 `\0` 也算进去（§6.5.3.4，上一章见过）。`strlen(s)` 是 **2**——它顺着字符数到 `\0` 为止、但 `\0` 不算进去，所以「内容」长度是 2。这俩数字差的那一个，就是那个看不见的 `\0`。每次你给字符串开缓冲区，都得想清楚要的是「内容长度」还是「含 `\0` 的字节数」——比如要装下 `"hi"`，数组至少得 3 格（2 内容 + 1 个 `\0`），少一格就越界。

输出最后一行专门把 `\0` 和 `'0'` 摆在一起比，因为这俩是新手最容易混的：`'\0'` 是**空字符**（null character），编码是 `0`；`'0'` 是**字符零**（就是键盘上那个 0），ASCII 编码是 `48`。它俩长得像、读音像、实际值差了 48。C 字符串靠的是编码为 `0` 的那个 `\0` 收尾，绝不是 `48` 的 `'0'`——你写 `char s[] = {'h','i','0'};`（注意是字符零），`strlen` 会一直往后读到天荒地老直到撞上一个真正的 `\0`，典型越界。

## 字符串字面量：住在只读区、改它就是段错误

代码里写死的 `"hello"` 叫**字符串字面量**（string literal，§6.4.5）。编译器对它的处理是：在程序的只读存储区（Linux 上是 `.rodata` 段）里放一个 `char` 数组、内容是 `'h','e','l','l','o','\0'`，然后这个字面量表达式的值，就是一个**指向它首字符的 `char*` 指针**。关键的一点是：这块内存在只读区，**程序运行期不许改**——标准（§6.4.5p7）明说「试图修改这种数组的程序，行为未定义」。

这条 UB 真跑起来是什么样？直接拿指针去改字面量的首字符：

```c
#include <stdio.h>

int main(void) {
    char* p = "hello"; /* p 指向字符串字面量(只读) */
    p[0] = 'H';        /* UB:试图改只读内存 */
    printf("%s\n", p);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall literal.c -o lit && ./lit; echo $?
139
```

进程跑到 `p[0] = 'H'` 那一刻就被杀掉了——没有任何输出（连 `printf` 都没来得及执行），shell 拿到的退出码是 `139`（也就是 128 + 11，11 是 `SIGSEGV` 段错误信号的编号；有些 shell 还会在终端多打印一句「段错误」/「Segmentation fault」，但那是 shell 自己加的、不是程序的输出，本机这个环境就只回退出码）。原因就是 `.rodata` 那段内存页被映射成只读，CPU 一执行写操作就触发硬件保护、内核直接发 `SIGSEGV` 把进程崩掉。开 ASan 看得更明白：

```text
$ gcc -std=c11 -Wall -fsanitize=address literal.c -o lit_asan && ./lit_asan
==117306==ERROR: AddressSanitizer: SEGV on unknown address 0x57925c54b020
SUMMARY: AddressSanitizer: SEGV (.../lit_asan+0x11d4) in main
==117306==ABORTING
```

ASan 报的是 `SEGV`（段错误信号），它甚至没法给出更多细节，因为这是 CPU 层面直接拦下的写只读页、还没轮到 ASan 的运行期检查插手。开头的 `==117306==` 是进程号、那个长地址是运行期地址，这俩每次跑都变，但 `SEGV` 这个结论是稳定的。所以铁律：**字符串字面量当成「只读」来用**，要改就别拿 `char*` 指它。

那怎么才能改？用 `char` **数组**，让字面量被**拷贝**一份到栈上：

```c
#include <stdio.h>

int main(void) {
    char a[] = "hello"; /* 数组:字面量被拷贝到栈上的 a,这份拷贝可改 */
    a[0] = 'H';
    printf("数组改首字符: a = %s\n", a);

    const char* p = "hello"; /* 指针:p 直接指向只读的字符串字面量 */
    printf("指针指向: p = %s\n", p);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall arr_vs_ptr.c -o avp && ./avp
数组改首字符: a = Hello
指针指向: p = hello
```

`char a[] = "hello";` 和 `char* p = "hello";` 长得像、但根本不是一回事：前者是「声明一个数组、把字面量的 6 个字节（含 `\0`）**拷贝**到这个栈上的数组里」，于是数组里的内容是你自己的副本、随便改（`a[0] = 'H'` 成功得到 `Hello`）；后者是「声明一个指针、让它**指向**那个只读的字面量」，你拿到的是别人地盘的地址、改就是段错误。顺带一提，因为指向字面量的指针不该被拿来写，好的写法是直接声明成 `const char*`（像上面 `p` 那样），这样万一你手滑写了 `p[0] = 'H'`，编译器在编译期就拦你，根本不用等到运行期崩。这一条「能 `const` 就 `const`」是 C 字符串里最便宜也最有效的保险。

## `<string.h>` 家族：一批以 `\0` 为界的函数

C 标准库（§7.24）给字符串配了一大家子函数，都靠 `\0` 来界定边界。下面这几个最常用，真跑一遍看看行为：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char dst[20];
    strcpy(dst, "hello");     /* 把 "hello"(含\0)拷进 dst */
    strcat(dst, " world");    /* 把 " world" 追加到 dst 末尾的 \0 处 */
    printf("dst = %s\n", dst);

    printf("strcmp(\"abc\",\"abc\") = %d\n", strcmp("abc", "abc"));
    printf("strcmp(\"abc\",\"abd\") = %d\n", strcmp("abc", "abd"));
    printf("strcmp(\"abd\",\"abc\") = %d\n", strcmp("abd", "abc"));

    char* p = strchr("hello", 'l');           /* 找第一个 'l' */
    printf("strchr(\"hello\",'l') = %s\n", p);

    char* sub = strstr("hello world", "wor"); /* 找子串 "wor" */
    printf("strstr(\"hello world\",\"wor\") = %s\n", sub);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall strfuncs.c -o sf && ./sf
dst = hello world
strcmp("abc","abc") = 0
strcmp("abc","abd") = -1
strcmp("abd","abc") = 1
strchr("hello",'l') = llo
strchr("hello world","wor") = world
```

`strcpy(dst, "hello")` 把源串（连同结尾 `\0`）拷进 `dst`；`strcat(dst, " world")` 从 `dst` 现有的 `\0` 处接着写——它先找到 `dst` 末尾的 `\0`、把它替换成 `' '`、再把 `"world"` 续上、最后补一个新的 `\0`，于是 `dst` 变成 `"hello world"`。`strcmp` 按字典序逐字符比，相等返回 `0`、左边小返回负数（这里是 `-1`）、左边大返回正数（`1`）——别记成「返回差值」，标准只保证正负号和大小关系、具体数值是实现定义的，**判断相等永远写 `strcmp(...) == 0`，别写 `== 1` 或 `< 0` 之外的依赖具体值**。`strchr` 在串里找某个字符第一次出现的位置、返回从那里开始的子串（找到 `'l'` 返回 `"llo"`、找不到返回空指针 `NULL`）；`strstr` 同理找子串（找到 `"wor"` 返回 `"world"`）。

这一家子函数有个共同特点、也是个共同的雷：**它们全都不检查你给的缓冲区够不够大**。`strcpy` 才不管 `dst` 是 20 字节还是 5 字节，它只管把源串一路拷到 `\0` 为止；`strcat` 也不管 `dst` 后面还剩多少空间。源串一旦比目标缓冲区长，写就冲出去了——这就是接下来要说的缓冲区溢出。

## 缓冲区溢出：老函数不守门，sanitizer 来兜底

把上一章数组学的越界 UB 搬到字符串上。给 `strcpy` 一个装不下的目标：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char small[5]; /* 只能装 4 字符 + \0 */
    strcpy(small, "This is way too long"); /* 源远超 5 字节,UB */
    printf("%s\n", small);
    return 0;
}
```

`small` 只有 5 字节，`"This is way too long"` 含 `\0` 是 21 字节，`strcpy` 一股脑拷过去，后面的 16 字节全写进了 `small` 之外的栈内存——典型缓冲区溢出。好笑的是新一点的 gcc 已经能在**编译期**就嗅出来（因为源是已知长度的字面量）：

```text
$ gcc -std=c11 -Wall overflow.c -o ov
overflow.c:6:5: warning: '__builtin_memcpy' writing 21 bytes into a region of size 5 overflows the destination [-Wstringop-overflow=]
    6 |     strcpy(small, "This is way too long"); /* 源远超 5 字节,UB */
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
overflow.c:5:10: note: destination object 'small' of size 5
    5 |     char small[5]; /* 只能装 4 字符 + \0 */
      |          ^
```

`-Wstringop-overflow=` 这个警告直接告诉你「往 5 字节的区域写 21 字节」、还点了名 `destination object 'small' of size 5`。但这是「源长度编译期已知」的幸运情况，要是源长度是运行期才算出来的（比如用户输入、`argv`），编译器就无能为力了，那种只能靠 sanitizer 运行期抓：

```text
$ gcc -std=c11 -Wall -fsanitize=address overflow.c -o ov_asan && ./ov_asan
==115477==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ac847cf0025
  This frame has 1 object(s):
    [32, 37) 'small' (line 5) <== Memory access at offset 37 overflows this variable
==115477==ABORTING
```

ASan 报 `stack-buffer-overflow`，点名 `'small' (line 5)`，访问偏移 37 溢出了这个 `[32, 37)` 区间的变量（32 到 37 正好是 5 字节，37 之后是栈上的红区）。退出码非 0，进程 abort。这正是上一章 Ch10 那一套 sanitizer 门禁的用武之地——缓冲区溢出是 UB、UB 该用 sanitizer 抓，一脉相承。历史上一大批著名的安全漏洞（代码执行、提权）根子都是这种「`strcpy`/`strcat`/`gets` 不检查长度」的溢出，所以下面这几个带边界的替代函数才值得认真学。

## 安全替代：strncpy 的坑、snprintf 的返回值、fgets 取代 gets

先说 `strncpy`（§7.24.2.4），它是 `strcpy` 的「带长度版本」：`strncpy(dst, src, n)` 最多往 `dst` 拷 `n` 个字节。听起来挺好，可它有个出了名的坑——**当 `src` 的长度大于等于 `n` 时，它拷满 `n` 个字节就停，不会在末尾补 `\0`**。也就是说结果可能根本不是一个合法的 C 字符串。真跑给你看：

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char dst[4];
    memset(dst, 'X', sizeof(dst));            /* 先填满 'X' 好观察 */

    strncpy(dst, "ABCDEF", sizeof(dst));      /* 源 6 字节 >= 4:只拷 4 字节,不补 \0 */
    printf("拷贝后 dst 的字节:");
    for (size_t i = 0; i < sizeof(dst); i++) printf(" %d", dst[i]);
    printf("\n");

    /* 正确用法:拷贝后强制在末尾补 \0 */
    char dst2[4];
    strncpy(dst2, "ABCDEF", sizeof(dst2));
    dst2[sizeof(dst2) - 1] = '\0';
    printf("手动补 \\0 后 dst2 = %s\n", dst2);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall strncpy.c -o sn && ./sn
strncpy.c:8:5: warning: 'strncpy' output truncated copying 4 bytes from a string of length 6 [-Wstringop-truncation]
    8 |     strncpy(dst, "ABCDEF", sizeof(dst));      /* 源 6 字节 >= 4:只拷 4 字节,不补 \0 */
      |                 ~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
拷贝后 dst 的字节: 65 66 67 68
手动补 \0 后 dst2 = ABC
```

`dst` 拷完之后四个字节是 `65 66 67 68`——也就是 `'A' 'B' 'C' 'D'`，**没有一个 `\0`**。这时候你要是拿 `printf("%s", dst)` 去打印，它因为没有终止符会一直往后读、读到别人内存里的 `\0` 才停，又一个越界。gcc 也专门给 `strncpy` 配了个 `-Wstringop-truncation` 警告，提醒你「这次拷贝结果可能没 `\0` 终止」。所以 `strncpy` 的正确姿势是**拷完之后自己手动在最后一格补 `\0`**（像 `dst2` 那样 `dst2[sizeof(dst2)-1] = '\0';`，代价是源串被截断、最后那个字符丢了，但至少换来一个合法字符串）。正因为这个「不补 `\0`」的坑，`strncpy` 其实不太招人待见，很多工程规范里它和 `strcpy` 一样被限制使用。

真正好用的是 `snprintf`（§7.21.6.5）。它是 `sprintf` 的带边界版本——`sprintf(buf, "...", ...)` 往 `buf` 里格式化写、不管 `buf` 够不够（又一个溢出大户），`snprintf(buf, n, "...", ...)` 则保证最多写 `n-1` 个字符、永远自己补 `\0`、绝不越界。它还有个特别有用的特性：**返回值是「如果没有截断、本该写入的总长度」（不含 `\0`）**，所以哪怕实际被截断了，你也能从返回值知道完整内容有多长：

```c
#include <stdio.h>

int main(void) {
    char buf[6]; /* 能装 5 字符 + \0 */
    int n = snprintf(buf, sizeof(buf), "Hello, world!"); /* 源 13 字符 */
    printf("buf = '%s'\n", buf);
    printf("返回值 n = %d\n", n);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall snprintf.c -o spf && ./spf
snprintf.c:5:47: warning: 'Hello, world!' directive output truncated writing 13 bytes into a region of size 6 [-Wformat-truncation=]
    5 |     int n = snprintf(buf, sizeof(buf), "Hello, world!"); /* 源 13 字符 */
      |                                         ~~~~~~^~~~~~~
buf = 'Hello'
返回值 n = 13
```

`buf` 只有 6 字节，`snprintf` 老老实实只写了 `'H','e','l','l','o','\0'`（内容 `Hello`），**绝不越界**——这正是它比 `sprintf` 安全的地方。可返回值 `n` 是 **13**，告诉你「如果没有 6 字节的限制、本来该写 13 个字符」，于是你立刻知道结果被截掉了 `13 - 5 = 8` 个字符。gcc 同样会在编译期用 `-Wformat-truncation=` 警告你这次发生了截断。这个返回值很实用：判断「要不要给个更大的缓冲区重写一次」就靠它（`if (n >= sizeof(buf)) { /* 缓冲区不够,扩容重来 */ }`）。需要拼接字符串时，`snprintf` 几乎总能顶替危险的 `strcat`，而且自带边界保护，是日常首选。

最后说一行读取。从标准输入读一行，老办法是 `gets(buf)`——可它根本不知道 `buf` 有多大、用户敲多少就读多少，是历史上最臭名昭著的溢出源头，臭到 **C11 标准直接把它从语言里删掉了**（C99 还在，标了「废弃」；C11 起彻底移除）。现在读一行一律用 `fgets(buf, sizeof(buf), stdin)`：它最多读 `sizeof(buf)-1` 个字符、读到换行或 EOF 就停、自己补 `\0`，带边界、安全。唯一的「小代价」是要自己处理一下——如果读到了换行符，`fgets` 会把它也存进 `buf`（`gets` 会丢掉换行），所以你常常要在拿到串之后判断末尾是不是 `\n`、是的话替换成 `\0`。这比起 `gets` 那种「随时溢出」的危险，完全是值得的。

## 小结

C 的字符串不是独立类型，而是「以 `\0` 结尾的 `char` 数组」（§7.1.1）——没有长度字段、没有边界保护，一切靠那个编码为 `0` 的空字符 `\0` 来收尾。这带来两个必须分清的量法：`strlen` 数到 `\0` 为止、不算它（内容长度），`sizeof` 把 `\0` 也算进去（整个数组的字节数），对 `char s[]="hi"` 就是 `strlen` 得 2、`sizeof` 得 3；还要分清 `'\0'`（空字符，码 0，字符串靠它终止）和 `'0'`（字符零，码 48）。字符串字面量 `"hello"` 落在只读存储区、是个 `char*`，**修改它是 UB**（§6.4.5p7），真跑 `char* p="hello"; p[0]='H';` 直接段错误（退出码 139、ASan 报 `SEGV`）；而 `char a[]="hello"` 是把字面量拷贝到栈上、那份副本可改——拿 `const char*` 指字面量、能用 `const` 就 `const`，是又便宜又好的保险。`<string.h>` 那一大家子（`strcpy`/`strcat`/`strcmp`/`strchr`/`strstr`，§7.24）都以 `\0` 为界、且**全不检查缓冲区大小**，源串一长就缓冲区溢出（`strcpy` 写越界 gcc 编译期 `-Wstringop-overflow`、运行期 ASan `stack-buffer-overflow` 点名变量，呼应第 10 章）。带边界的替代各有讲究：`strncpy(dst,src,n)` 在源 `>=n` 时**不补 `\0`**（gcc `-Wstringop-truncation`），用完得自己手动在末尾补 `\0`，所以并不讨喜；`snprintf(buf,n,...)` 保证不越界、永远自补 `\0`，返回值还是「本该写入的总长度」（截断时 `n >= sizeof(buf)`），是拼接字符串的首选，能顶掉危险的 `sprintf`/`strcat`；读一行用 `fgets`（会把换行也存进去），它取代了 C11 已彻底删除的 `gets`。一句话：C 字符串所有的麻烦，几乎都能追溯到「没有边界、靠 `\0` 收尾」这两条上，带边界版本 + sanitizer + 能 `const` 就 `const`，是应付它们的全套家当。下一章我们看基础 IO——`printf`/`scanf` 家族那些格式化字符串的坑。

## 参考资源

- ISO/IEC 9899:2011 §7.1.1（字符串的定义：连续字符到首个 `\0`）、§6.4.5（字符串字面量，p7 修改字面量是 UB）、§7.24（`<string.h>` 函数家族）、§7.21.6.5（`snprintf`）、§7.21.7（`fgets`；`gets` 在 C11 被移除）
- K. N. King《C Programming: A Modern Approach》第 13 章 Strings（字面量存储、char 数组 vs char 指针、`<string.h>` 家族、`strcpy`/`strcat` 的越界陷阱）
- Robert C. Seacord《Effective C》第 7 章 Characters and Strings（C 字符串表示、legacy 函数的缓冲区安全问题、bounds-checked 接口）
- 第 4 章：浮点、字符、常量与隐式转换（`char` 是小整数）、第 10 章：数组（char 数组、退化、越界访问是 UB）
- 阶段 0·第 10 章：Sanitizer 门禁（本章缓冲区溢出验证正是用 ASan 那一套）、阶段 2：指针与内存（`char*` 的彻底拆解、动态字符串）
