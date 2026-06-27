---
title: "C 陷阱与坑：把那些「编译能过、运行就炸」的地方一个个揪出来"
description: "按词法/语法/语义/指针内存/宏/链接/溢出 UB 分门别类,把 C 里最容易踩的坑配上真实编译器告警与运行/ASan/UBSan 输出逐个拆穿,告诉你为什么炸、怎么改。"
chapter: 2
order: 1
tags:
  - host
  - syntax
  - operator
  - control-flow
  - function
  - macro
  - pointers
  - memory
  - linker
  - debug
difficulty: intermediate
reading_time_minutes: 22
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程、GDB 与库"
  - "Chapter 2：指针、内存布局与位运算"
related:
  - "Chapter 2：指针、内存布局与位运算"
  - "Chapter 4：ASan 与 UBSan —— 让内存错误和未定义行为当场现形"
  - "Chapter 4：符号与链接"
---

# C 陷阱与坑:把那些「编译能过、运行就炸」的地方一个个揪出来

## 引言

C 这门语言有个特别"反人类"的特点:**很多坑,编译器一声不吭就给你放过去了,等你跑到生产环境,某天某个输入一进来,程序悄无声息地崩成一个段错误,或者更糟——给出一个看起来对、其实错得离谱的结果**。这背后的根源,是 C 的设计哲学:相信程序员,把性能和底层控制权交给你,代价就是你得自己为每一行可能未定义的行为负责。

这一章我们不堆概念,而是按坑的来源把它们分成几大类——**词法、语法、语义、指针与内存、宏、链接、整型溢出与 UB**——每一类都给一段真实可复现的代码,然后用本机的 GCC 16.1.1 实打实地编译、跑一遍,把编译器告警、运行结果、ASan/UBSan 报告原样贴出来。我们不光告诉你"这里错了",更要说清楚**为什么错、错在哪一层、标准是怎么规定的、怎么写才对**。

> 本文所有编译/运行/ASan/UBSan 输出均在 **GCC 16.1.1、`-std=c11`** 上实测捕获,编译命令统一为
> `gcc -std=c11 -Wall -Wextra -Wparentheses demo.c -o demo`,需要查 UB 时再额外加 `-fsanitize=undefined` 或 `-fsanitize=address`。
> 不是凭记忆、不是抄书,是真跑出来的。

## 词法陷阱:编译器在切符号时已经替你埋了雷

### `=` 写成了 `==` 的反面:赋值出现在条件里

C 里赋值是个**表达式**,它有值,所以下面这种笔误完全合法,编译器只会"温柔地"提醒你一下:

```c
if (x = y)      /* 笔误:想写 == */
    printf("进了 if 分支,x=%d\n", x);
```

这里 `x = y` 会先把 `y` 赋给 `x`,然后整个赋值表达式的值就是 `x` 的新值,再交给 `if` 判断。我们真编译跑一下,看 GCC 怎么说:

```text
=== gcc -std=c11 -Wall -Wextra -Wparentheses assign_in_cond.c ===
assign_in_cond.c: In function 'main':
assign_in_cond.c:5:9: warning: suggest parentheses around assignment used as truth value [-Wparentheses]
    5 |     if (x = y)      /* 笔误:想写 == */
      |         ^
=== ./assign_in_cond ===
进了 if 分支,x=5
```

`-Wparentheses` 这个告警就是为这种笔误量身定做的——它的潜台词是:"你确定你是想在条件里**赋值**,而不是**比较**吗?" 如果你确实是想赋值并检查(比如经典的 `if ((fd = open(...)) < 0)`),那就显式加一层括号 `if ((x = y))` 把意图写清楚,告警自然消,读代码的人也不会被吓到。

### 词法分析的"贪心"原则:`x/*p` 不是除法

C 的词法分析器在切 token 时,总是**尽可能往长里读**——看到两个 `-`,它先合成 `--`(自减),而不是两个减号。这条"贪心"原则有个特别操蛋的后果:

```c
int y = x/*p;   /* 想表达 x / (*p) */
```

你以为这是 `x` 除以 `*p`,但词法分析器看到 `/*`,直接把它当成**注释的开头**,于是这一行从 `/*` 往后全被吃掉了。我们编译一下看会发生什么:

```text
=== gcc -std=c11 -Wall -Wextra greedy_comment.c ===
greedy_comment.c:6:21: warning: '/*' within comment [-Wcomment]
    6 |     int y = x/*p;   /* 想表达 x / (*p),结果 /* 被当成注释开头 */
greedy_comment.c:6:46: warning: '/*' within comment [-Wcomment]
greedy_comment.c:7:5: error: expected ',' or ';' before 'printf'
    7 |     printf("y = %d\n", y);
      |     ^~~~~~
```

看清楚了吗:`/*` 一吃,这一行就断了,`printf` 前面根本没分号,直接报 `expected ',' or ';'`。解决方法朴实无华:**要么写 `x / *p`,要么老老实实加括号 `x / (*p)`**。写除法和指针解引用挨着的时候,一定要把 `*` 和 `/` 之间隔开,或者加括号,别给词法分析器任何"贪心"的机会。

### 八进制字面值:`046` 不等于 `46`

很多人为了"对齐好看",喜欢给数字前面补 0,这在 C 里是个隐藏的雷——**以 `0` 开头的整数字面值是八进制**。我们跑一段:

```c
int a = 046;   /* 八进制:4*8+6 */
int b = 46;    /* 十进制 */
printf("046 = %d\n", a);
printf("46  = %d\n", b);
```

```text
=== ./octal_literal ===
046 = 38
46  = 46
相等吗? no
```

`046` 在十进制下其实是 `38`,跟 `46` 八竿子打不着。这种坑在给零件号、端口地址做对齐表的时候特别容易翻车——你以为两行写的是同一个数,其实一个八进制一个十进制。记住:**整数前导 0 = 八进制,前导 0x = 十六进制,想表达十进制就别加无谓的 0**。

## 语法陷阱:看起来对,其实被解析成了另一个意思

### 运算符优先级:位运算比关系运算**低**

C 的运算符优先级有十几档,真要全背下来不现实。有一条经验法则特别值钱:**位运算 `&`、`|`、`^` 的优先级,比 `==`、`!=` 这些关系运算符还要低**。所以下面这个判断位掩码的写法是个经典雷:

```c
int flags = 0x4;            /* 0100 */
int mask  = 0x6;            /* 0110 */
if (flags & mask == mask)   /* 实际被解析成 flags & (mask == mask) */
```

本意是 `(flags & mask) == mask`,结果因为 `==` 优先级更高,被解析成了 `flags & (mask == mask)`——而 `mask == mask` 恒为 `1`,于是整个判断退化成"flags 的最低位是不是 1"。GCC 的 `-Wparentheses` 又一次救了你:

```text
=== gcc -std=c11 -Wall -Wextra -Wparentheses precedence.c ===
precedence.c:7:22: warning: suggest parentheses around comparison in operand of '&' [-Wparentheses]
    7 |     if (flags & mask == mask)
      |                 ~~~~~^~~~~~~
```

> 经验法则:**只要表达式里混了位运算、移位之外的任何运算符,一律加括号**。优先级表别去死记,括号比记忆力靠谱得多。

### `switch` 的 fallthrough:忘了 `break` 就一路滑到底

C 的 `switch` 默认是"穿透"的——某个 `case` 匹配后,如果不写 `break`,会**一路执行下去**,把后面所有 `case` 的代码都跑一遍,直到遇到 `break` 或 `switch` 结束。这跟很多语言(自动 break)完全不同:

```c
switch (grade) {
case 4: printf("优秀\n");
case 3: printf("良好\n");
case 2: printf("及格\n");
case 1: printf("再接再厉\n");
default: printf("默认分支\n");
}
```

`grade = 2` 时,你以为只打印"及格",实际:

```text
=== ./switch_fallthrough ===
及格
再接再厉
默认分支
```

三个分支全跑了。GCC 有专门的 `-Wimplicit-fallthrough=` 告警族,本机 `-Wextra` 会带上:

```text
switch_fallthrough.c:7:9: warning: this statement may fall through [-Wimplicit-fallthrough=]
    7 |         printf("优秀\n");
switch_fallthrough.c:8:5: note: here
    8 |     case 3:
```

> 如果你**故意**要 fallthrough(某些状态机里这是合法设计),加一个 `/* fall through */` 注释(GCC 认这个)或 `__attribute__((fallthrough))`,告警就会消,读代码的人也知道你是故意的,不是漏写了 `break`。

### `if` 后面那个要命的分号

```c
if (x[i] > big);
    big = x[i];
```

第一行末尾多了一个分号,这个 `if` 的"体"就变成了一个**空语句**,缩进看着像受 `if` 控制的 `big = x[i]`,其实**每次都无条件执行**。GCC 的 `-Wempty-body` 和 `-Wmisleading-indentation` 两个告警一起把问题揪出来:

```text
=== gcc -std=c11 -Wall -Wextra stray_semicolon.c ===
stray_semicolon.c:6:24: warning: suggest braces around empty body in an 'if' statement [-Wempty-body]
    6 |         if (x[i] > big);
      |                        ^
stray_semicolon.c:6:9: warning: this 'if' clause does not guard... [-Wmisleading-indentation]
stray_semicolon.c:7:13: note: ...this statement, but the latter is misleadingly indented
```

这种坑在 `while`、`for` 后面一样会出现。**控制语句后面要么直接跟花括号,要么紧跟一条语句,绝不先来个分号**。项目里强制"非空控制体一律加花括号"能直接消灭这一整类 bug。

## 语义陷阱:类型和求值顺序里藏着的雷

### 求值顺序未规定:`a[i] = i++` 是未定义行为

C 里只有四个运算符规定了操作数的求值顺序:`&&`、`||`、`?:` 和逗号运算符 `,`。**其它所有运算符(包括赋值)的操作数求值顺序都是未规定的**,更狠的是——**在同一个表达式里,既读又写同一个变量、且中间没有序列点,直接是未定义行为(UB)**:

```c
int i = 0;
int a[5] = {0};
a[i] = i++;     /* i 被读又被写,无序列点:UB */
```

GCC 给的是 `-Wsequence-point` 告警:

```text
=== gcc -std=c11 -Wall -Wextra eval_order_ub.c ===
eval_order_ub.c:6:13: warning: operation on 'i' may be undefined [-Wsequence-point]
    6 |     a[i] = i++;     /* i 在同一表达式被读又写,且无序列点:UB */
```

这意味着编译器爱怎么算就怎么算,UBSan 在 GCC 16 上甚至不一定报(因为这种 UB 标准没规定行为,sanitizer 也不保证抓得到)。这类写法包括但不限于:`a[i] = i++`、`y[i] = x[i++]`、`c + --c`、`printf("%d %d", i++, i++)`。**拆成两条语句**就安全了:

```c
a[i] = i;
i++;
```

> 顺便澄清一个常见误解:**分隔函数参数的逗号,不是逗号运算符**。`f(x, y)` 里 `x` 和 `y` 的求值顺序未规定;但 `g((x, y))` 里那对括号才是逗号运算符,先算 `x` 丢弃,再算 `y`,`g` 只收到一个参数。这两者天差地别。

### `char` 接 `getchar()`:合法字节被当成 `EOF`

`getchar` 的返回值是 `int`(能表示 0..255 和 `EOF` 通常是 -1),但无数教程写成 `char c; while ((c = getchar()) != EOF)`。当 `c` 是 `char` 时,`0xFF` 这个合法字节会被截断,在有符号 `char` 上变成 `-1`,恰好等于 `EOF`,于是程序把一个正常字节误判成文件结束。我们直接演示:

```c
char c = (char)0xFF;          /* 0xFF 截断进 char */
int  e = EOF;                 /* EOF 通常是 -1 */
printf("(c == EOF) ? %s\n", (c == EOF) ? "yes" : "no");
```

```text
=== ./char_eof ===
c   = -1
EOF = -1
(c == EOF) ? yes
```

一个合法的 `0xFF` 字节,被误判成了 `EOF`。正确做法是用 `int` 接收,它既能装下所有字节值,又能装下 `EOF`:

```c
int c = (unsigned char)0xFF;
printf("(c == EOF) ? %s\n", (c == EOF) ? "yes" : "no");   /* no */
```

```text
=== ./char_eof_ok ===
(c == EOF) ? no
```

> 经验:**任何返回值可能是"特殊标记"(如 `EOF`、`WEOF`)的函数,一律用 `int` 接收,别图省事用 `char`**。

### `scanf("%d", &c)`:类型对不上,栈被悄悄改写

`scanf` 不知道你传的是什么指针,它只认格式串。你让它按 `%d`(整数)读,却塞给它一个 `char *`,它就老老实实往那个地址写一个 `int` 大小的数据——而 `char` 只占 1 字节,多出来的字节直接盖掉相邻内存:

```c
int i = 99;
char c = 'A';
int matched = scanf("%d", &c);   /* &c 是 char*,格式串要 int* */
```

```text
=== gcc -std=c11 -Wall -Wextra scanf_overwrite.c ===
scanf_overwrite.c:8:27: warning: format '%d' expects argument of type 'int *', but argument 2 has type 'char *' [-Wformat=]
    8 |     int matched = scanf("%d", &c);
      |                          ~^   ~~
      |                           |   |
      |                           |   char *
      |                           int *
      |                          %hhd
=== echo 12345 | ./scanf_overwrite ===
matched=1, i before=99, i after=48, c=57
```

看运行结果:`i` 从 99 变成了 48!就是因为 `12345` 这个整数写进 `c` 的地址时,溢出的字节覆盖了紧挨着的 `i` 的低位。`-Wformat=` 这个告警是救命的——**格式串和实参类型必须严格对应,GCC 能帮你查,前提是你开了 `-Wformat`(在 `-Wall` 里)**。改法:要么 `char` 配 `%hhd`,要么老老实实 `int` 配 `%d`。

### 复杂的函数指针声明:用 `typedef` 拆,别硬刚

作者笔记里那个"在 0 地址调用一个无参无返回函数"的写法,初看能把人吓退:

```c
(*(void(*)())0)();
```

我们验证一下它能不能编过(当然不能真调,解引用 0 地址必崩):

```c
typedef void (*FuncPtr)(void);
void (*fp)(void) = (void (*)(void))0;   /* 把 0 转成无参无返回函数指针 */
printf("fp = %p\n", (void*)fp);
```

```text
=== ./fnptr_decl ===
fp = (nil)
```

编译能过,`fp` 是 `(nil)`(空)。作者的拆解是对的:最外层是个调用 `(*STH)()`,而 `STH = (void(*)())` 是一个"无参无返回的函数指针"类型,它把 `0` 强转了。但**真去 `fp()` 必然段错误**。

处理复杂函数指针,经验法则是:**别硬刚,用 `typedef` 一层一层剥**。比如 `signal` 函数"接受一个 int 和一个信号处理函数,返回一个信号处理函数",裸写是 `void (*signal(int, void(*)(int)))(int)`,读都没法读;用 typedef 拆开就清清楚楚:

```c
typedef void (*SignalHandler)(int);
SignalHandler my_signal(int sig, SignalHandler handler);
```

## 指针与内存陷阱:C 里翻车最集中的地带

### `malloc` 忘了给 `'\0'` 留位置:越界写入

字符串在 C 里是以 `'\0'` 结尾的,`strlen` 返回的长度**不含** `'\0'`。所以 `malloc(strlen(s))` 一定少 1 字节,后面 `strcpy` 一写就越界:

```c
const char *s = "hello";          /* 长度 5,含 \0 是 6 */
char *r = malloc(strlen(s));      /* 少分配 1 字节 */
strcpy(r, s);                     /* 写入第 6 个字节越界 */
```

肉眼根本看不出问题,普通编译也没事,但 ASan 一上就当场现形:

```text
=== gcc -std=c11 -g -fsanitize=address malloc_offbyone.c ===
=== ./malloc_offbyone_asan ===
==89636==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x7b9d16de0015
WRITE of size 6 at 0x7b9d16de0015 thread T0
    #1 0x... in main /tmp/imp_pitfalls/malloc_offbyone.c:9
0x7b9d16de0015 is located 0 bytes after 5-byte region [0x7b9d16de0010,0x7b9d16de0015)
allocated by thread T0 here:
    #1 0x... in main /tmp/imp_pitfalls/malloc_offbyone.c:7
SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/imp_pitfalls/malloc_offbyone.c:9 in main
```

ASan 把话说得明明白白:你分配了 5 字节,却写了 6 字节("WRITE of size 6","0 bytes after 5-byte region")。正确写法有三条同时到位:**`malloc(strlen(s) + 1)`、检查返回值是否 `NULL`、用完 `free`**。

```c
char *r = malloc(strlen(s) + 1);
if (r == NULL) { /* 处理失败 */ }
strcpy(r, s);
/* ... 用完 ... */
free(r);
```

> 这正是 [Chapter 4 的 ASan/UBSan 文档](../04-engineering/1-sanitizers-asan-and-ubsan.md)存在的意义——这类内存错误肉眼查不出来,sanitizer 能让它们当场报错带行号。

### 空指针解引用:未定义行为

```c
int *p = NULL;
*p = 42;                       /* 空指针解引用:UB */
```

UBSan 直接点名:

```text
=== gcc -std=c11 -g -fsanitize=undefined null_deref.c ===
=== ./null_deref_ubsan ===
null_deref.c:5:8: runtime error: store to null pointer of type 'int'
```

> 注意:**空指针不是空字符串**。你不能 `printf("%s", (char*)NULL)`、不能 `strcmp(p, (char*)0)`,因为标准库里这些函数会去解引用指针看内容,而对空指针解引用是 UB。除了赋值和比较,任何对 `NULL` 的使用都要打个问号。

### 野指针:未初始化就解引用

```c
int *a;        /* 未初始化:野指针 */
*a = 12;       /* UB:写入随机地址 */
```

GCC 的 `-Wuninitialized` 能在编译期给你提示:

```text
=== gcc -std=c11 -Wall -Wextra uninit_ptr.c ===
uninit_ptr.c:5:8: warning: 'a' is used uninitialized [-Wuninitialized]
    5 |     *a = 12;       /* UB:写入随机地址 */
uninit_ptr.c:4:10: note: 'a' was declared here
    4 |     int *a;        /* 未初始化:野指针 */
```

这里有个特别阴险的点:UB 意味着"标准没规定",所以程序**可能不崩,甚至看起来正常**——本机 ASan 跑这段居然打印出了 `12`、正常退出。但这不代表它对,只是**这次运气好**,换个编译选项、换个平台、换个时刻,它随时可能段错误或写坏别的内存。**永远别相信"它现在能跑",UB 没有"正确行为"可言**。养成习惯:指针声明时就初始化,没东西指就给 `NULL`。

### 数组名和指针:长得像,内存布局完全不同

```c
char arr[] = "hello";        /* 数组:6 字节,内容就地存放 */
char *ptr  = "hello";        /* 指针:存一个地址,指向只读的字符串字面量 */
```

这俩用起来都能 `printf("%s", ...)`,但底层完全是两回事。我们打印 `sizeof` 和地址看清楚:

```text
=== ./arr_vs_ptr ===
sizeof(arr) = 6        <- 整个数组的大小
sizeof(ptr) = 8        <- 指针自身的大小(64 位系统)
&arr  = 0x7ffeb544a6e2
arr   = 0x7ffeb544a6e2   <- 数组名 = 首元素地址,&arr 同址
&ptr  = 0x7ffeb544a6d8   <- 指针变量自己的地址(在栈上)
ptr   = 0x603dcdfe1004   <- 指向字符串字面量(在只读区)
```

`arr` 是 6 字节的存储,**内容就是 `'h','e','l','l','o','\0'`**;`ptr` 是 8 字节的指针变量,它自己住在栈上,里面存着一个地址,指向别处的字符串字面量。两者内存布局不同,`sizeof` 不同,可修改性也不同(`arr` 可改,`ptr` 指向的字面量改是 UB)。这个区别在下一节的链接坑里会变成真正的灾难。

## 宏陷阱:文本替换,不是函数

宏是预处理器干的事,它只做**文本替换**,没有任何类型检查、求值保护。这是 C 里最容易翻车的特性之一。

### 宏参数的副作用被求值多次

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))
int m = MAX(i++, j++);
```

`MAX(i++, j++)` 展开后,`i++`/`j++` 至少有一边会被求值两次——因为宏是文本替换,`a` 在展开式里出现了两次。我们对比宏和真正的函数:

```text
=== ./macro_sideeffect  (宏) ===
m = 3, i = 2, j = 4       <- j++ 算了两次,j 从 2 跳到 4

=== ./macro_func_cmp  (static inline 函数) ===
m = 2, i = 2, j = 3       <- 每个实参只求值一次
```

宏版本 `j` 变成了 4,函数版本 `j` 才是 3。这就是"宏不是函数"最典型的代价。**经验:宏参数绝对不要带副作用(`++`、`--`、函数调用等);如果非要用自增自减,改写成函数或 `static inline`**。

### 宏名后面多了个空格:从函数宏变成对象宏

```c
#define f (x) ((x)-1)     /* f 后多一个空格 */
```

`f` 和 `(x)` 之间那个空格,让预处理器以为这是**对象宏**(无参),`f` 就被替换成 `(x) ((x)-1)`。`-E` 看预处理结果最直观:

```text
=== gcc -std=c11 -Wall -Wextra -E macro_space.c (节选) ===
int r = (x) ((x)-1)(3);     <- f(3) 变成了这坨
=== gcc -std=c11 -Wall -Wextra macro_space.c ===
macro_space.c:2:12: error: 'x' undeclared (first use in this function)
```

`f(3)` 被替换成 `(x) ((x)-1)(3)`,`x` 未声明,直接编译失败。**规则:函数宏的名字和左括号 `(` 之间不能有空格;但宏调用时 `f (3)` 和 `f(3)` 等价,空格无影响**。定义要紧贴,调用随便。

### 多语句宏:`if/else` 的悬空陷阱

把宏写成 `{ ... }`,看起来像个语句块,但在 `if/else` 后面跟一个分号就会出事:

```c
#define BAD_SWAP(a, b) { int t = a; a = b; b = t; }
if (x < y) BAD_SWAP(x, y); else printf("no swap\n");
```

展开后是 `if (x < y) { ... }; else ...`,那个多余的分号把 `if` 截断了,`else` 找不到对应的 `if`:

```text
=== gcc macro_multistmt_bad.c ===
macro_multistmt_bad.c:6:32: error: 'else' without a previous 'if'
    6 |     if (x < y) BAD_SWAP(x, y); else printf("no swap\n");
```

正解是经典的 **`do { ... } while (0)`** 包裹,它让宏表现得像一个完整语句,分号正好落在 `while(0)` 后面:

```c
#define SWAP(a, b) do { int t = a; a = b; b = t; } while (0)
```

```text
=== ./macro_multistmt ===
x=2 y=1     <- 正常交换,if/else 安全
```

> `do { ... } while (0)` 是写"行为像语句的宏"的标准范式,本项目里所有多语句宏都该这么包。

### 断言宏的悬空 `else`

把 `assert` 简单定义成 `#define ASSERT(e) if(!e) fail(...)`,嵌进外层 `if/else` 时,宏内部的 `if` 会和外层的 `else` 错配(GCC 会报 `-Wdangling-else` 提醒你)。这也是为什么真正的 `assert` 不用 `if`,而用 `((void)((e) || (fail(...))))` 这种"看起来像表达式"的形式——它利用了 `||` 的短路求值,又天然是个表达式,不存在语句层面的悬挂问题。

## 链接陷阱:跨文件类型不一致,链接器不查,运行时炸

### 数组定义成指针:跨翻译单元类型不匹配

这是 C 里最阴险的坑之一。一个文件里 `filename` 是**数组**,另一个文件里 `extern` 声明成**指针**:

```c
/* def.c */
char filename[] = "/etc/passwd";   /* 定义:数组 */

/* use.c */
extern char *filename;             /* 声明:指针 —— 类型不一致! */
int main(void) { printf("%s\n", filename); }
```

链接器**完全接受**这个程序(它只管符号名 `filename` 对得上,不管类型),但一运行就把数组的前几个字节当指针值解引用——`"/et"` 在小端机器上是 `0x74652f` 这种地址,解引用必崩:

```text
=== gcc -std=c11 def.c use.c -o mismatch ===
(编译、链接都通过,无任何告警)
=== ./mismatch ===
Exit 139   <- SIGSEGV 段错误
```

改成声明与定义一致(数组配数组),立马正常:

```c
extern char filename[];            /* 声明:数组 —— 与定义一致 */
```

```text
=== ./ok_mismatch ===
filename = /etc/passwd
```

> 铁律:**一个外部对象在所有翻译单元里的类型必须严格一致**。最稳的做法是把这个声明放进一个头文件,所有用到它的 `.c`(包括定义它的那个)都 `#include` 同一个头文件,让编译器在每个 TU 里都能做类型检查。这正是头文件存在的核心意义之一。

### 重复定义外部变量:链接器直接拒收

两个文件都定义了带初值的外部变量 `counter`,链接器这次会管:

```c
/* a.c */ int counter = 1;
/* b.c */ int counter = 2;
```

```text
=== gcc -std=c11 a.c b.c main.c -o dupdef ===
/usr/bin/ld: ...:(.data+0x0): multiple definition of `counter'; ...: first defined here
collect2: error: ld returned 1 exit status
```

`multiple definition` —— 每个外部变量**只能定义一次**。如果一个变量要在多个文件共享,只能在一个文件里定义,其它文件用 `extern` 声明;如果一个变量只在本文件用,加 `static` 把它限制在文件作用域内,既能避免命名冲突,也不怕重复定义。

## 整型溢出与 UB:有符号溢出是未定义,无符号是环绕

### 有符号整数溢出:UB,别用 `if (a+b < 0)` 检查

两个有符号整数相加溢出,结果是**未定义行为**,不是"变成负数"。所以下面这种"检查溢出"的写法本身是错的:

```c
int a = INT_MAX, b = 1;
if (a + b < 0) complain();   /* 错:溢出后行为未定义,这个判断不可靠 */
```

实测看真相:

```text
=== ./signed_overflow ===
a = 2147483647
a + b = -2147483648          <- 本机恰好环绕成最小负数
(a + b < 0) ? yes   <- 错误的溢出检查!
正确检查:会溢出

=== ./signed_overflow_ubsan ===
signed_overflow.c:7:9: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'
```

`a + b` 在本机碰巧环绕成 `-2147483648`,所以 `< 0` 成立——但这是**实现定义的巧合**(GCC 默认像环绕,但标准不保证),换个编译器可能完全不是这样。UBSan 一针见血地指出这是 UB。正确检查方式有两种:

```c
/* 方式一:用无符号运算(无符号溢出是定义良好的) */
if ((unsigned)a + (unsigned)b > INT_MAX) complain();

/* 方式二:事先比较,根本不让溢出发生 */
if (a > INT_MAX - b) complain();    /* 加法溢出 */
if (b != 0 && a > INT_MAX / b) complain();  /* 乘法溢出 */
```

> 原则:**有符号溢出 = UB,永远不要"先溢出再判断",要在运算发生之前就拦住**。

### 无符号整数的环绕:定义良好,但会咬人

无符号整数"溢出"不是 UB,而是**模 2^N 环绕**(标准明确规定),所以 `0u - 1` 等于 `UINT_MAX`:

```text
=== ./unsigned_wrap ===
u = 0
u - 1 = 4294967295   (环绕到 UINT_MAX)
UINT_MAX = 4294967295
```

这带来的隐蔽坑是循环和比较:因为无符号数**永远非负**,所以 `for (unsigned i = n; i >= 0; i--)` 是个死循环(`i` 永远 `>= 0`),`size_t` 类型的 `if (len - offset >= 0)` 恒为真。涉及无符号减法时,务必确认被减数不小于减数,否则你会得到一个巨大的正数而不是负数。

## 常见踩坑速查

把上面这些坑浓缩成一张速查表,平时写代码扫一眼:

| 坑 | 表现 | 防御手段 |
|----|------|----------|
| `=` 写成 `==` | 赋值当条件 | `-Wparentheses`;显式加括号 `(x = y)` |
| `x/*p` | `/*` 被当注释 | 写成 `x / (*p)` |
| `046` 八进制 | 前导 0 是八进制 | 十进制别补 0 |
| 位运算优先级低 | `flags & mask == mask` 错 | 位运算一律加括号 |
| switch fallthrough | 忘 `break` 一路滑 | `-Wimplicit-fallthrough`;故意加注释 |
| `if(...);` | 多分号变空体 | `-Wempty-body`;控制体强制花括号 |
| `a[i]=i++` | 同表达式读写同一变量 = UB | 拆成两条语句 |
| `char` 接 `getchar` | 合法字节误判 EOF | 用 `int` 接收 |
| `scanf` 类型不符 | 栈被改写 | `-Wformat`;格式串与实参严格对应 |
| `malloc` 少 `+1` | `'\0'` 越界写 | `strlen(s)+1`;ASan |
| 空指针/野指针解引用 | UB,可能崩也可能"看着对" | 声明即初始化;sanitizer |
| 数组当指针跨文件声明 | 类型不一致,运行段错误 | 头文件统一声明 |
| 宏副作用 | `i++` 求值多次 | 宏参数不带副作用;改用函数 |
| 宏名后空格 | 函数宏变对象宏 | 名字紧贴 `(` |
| 多语句宏 `{...}` | `else` 找不到 `if` | `do { ... } while (0)` |
| 重复定义外部变量 | 链接器拒收 | 只定义一次;`static` 限文件域 |
| 有符号溢出 | UB,`a+b<0` 检查不可靠 | 事先比较 `a > INT_MAX - b` |
| 无符号环绕 | `0u-1` 变巨大正数 | 无符号减法前确认大小关系 |

## 小结

把这些坑串起来看,你会发现它们有一个共同的根:**C 把判断权交给了你,而编译器为了不"误报",很多地方默认不吭声**。所以我们的防御策略其实就三条:

1. **把编译器的嘴撬开**:`-Wall -Wextra -Wparentheses -Wformat` 这些告警是免费的体检,一条都不该关。本项目 CI 默认全开,本机调试也照着来。
2. **用 sanitizer 兜底 UB**:内存错误和未定义行为肉眼查不出来,`-fsanitize=address,undefined` 能让它们当场带行号报错。详见 [ASan/UBSan 那一章](../04-engineering/1-sanitizers-asan-and-ubsan.md)。
3. **写出"防呆"的代码**:控制体加花括号、位运算加括号、宏用 `do{}while(0)` 包、外部声明进头文件、指针声明即初始化、有符号运算先防溢出。这些不是啰嗦,是用纪律把坑堵在写代码的那一刻。

C 的坑不会消失,但它们都是**可识别、可防御**的。你今天把每一个告警和 sanitizer 报告都认真看一遍,明天写代码的时候,这些坑的位置就会像路灯一样,在你脑子里自动亮起来。

## 练习

1. 把 `if (flags & mask == mask)` 这行用 `-Wparentheses` 编译,看清告警,然后改成正确写法,对比运行结果。
2. 写一个 `SWAP(a, b)` 宏,先用裸 `{ ... }` 包,在 `if/else` 里调用,触发 `'else' without a previous 'if'`;再改成 `do { ... } while (0)`,确认问题消失。
3. 复现"数组定义、指针声明"的跨文件坑:写出 `def.c` / `use.c`,用 ASan 编译运行,观察它崩在哪一步;改成头文件统一声明后再跑一次。
4. 用 `-fsanitize=undefined` 编译 `INT_MAX + 1`,看 UBSan 报告;然后写出正确的溢出预检查(`a > INT_MAX - b`),确认预检查能拦住。
5. 写一段 `a[i] = i++`,用 `-Wsequence-point` 编译,理解为什么这是 UB,并改写成两条语句。

## 参考资源

- Andrew Koenig,《C 陷阱与缺陷》(C Traps and Pitfalls)——本文词法/语法/语义/链接/库函数部分的母本。
- Peter van der Linden,《C 专家编程》(Expert C Programming)——数组与指针、链接、运行时数据结构的深度讨论。
- Kenneth Reek,《C 和指针》(Pointers on C)——指针、数组、函数指针的系统性讲解。
- ISO/IEC 9899:2011(C11)标准,术语:未定义行为(undefined behavior)、未指定行为(unspecified)、实现定义行为(implementation-defined)。
- 本仓库 [Chapter 4:ASan 与 UBSan](../04-engineering/1-sanitizers-asan-and-ubsan.md)—— sanitizer 的实战用法。
- 本仓库 [Chapter 4:符号与链接](../04-engineering/2-symbols-and-linking.md)—— 链接器如何处理符号、为什么跨文件类型检查靠头文件。

---

*整理自作者笔记,按 C-Journey 写作规范重写;所有输出本机实测捕获。*
