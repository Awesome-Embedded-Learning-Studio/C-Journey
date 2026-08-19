---
title: "阶段 2 课后练习（Homework）"
description: "指针与内存阶段的课后练习：12 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战（字节级轮转，改编自 CSAPP 与 LeetCode 189 的原地三反转）。难度覆盖 L1~L5，题目都做了变式处理，参考答案独立成文件、逐步解答附知识点链接。"
chapter: 2
order: 0
tags:
  - host
  - pointers
  - memory
difficulty: intermediate
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 全部章节（第 1~12 章）"
related:
  - "阶段 2 Lab：指针解剖台"
  - "阶段 2 Project：动态通讯录 dybook"
---

# 阶段 2 课后练习（Homework）

## 引言

这里的题按章组织，每章两道（基础 + 进阶），最后是两道跨章综合和一道 L5 挑战。每题标注难度档位（L1~L5，见[练习总览](/exercises/)）和涉及章节。题目都是「变式」——换场景、换推理方向，照抄教材例题抄不出答案；每道题都要真编译真跑，把输出贴下来才算完。

答案在独立的[参考答案](homework-solutions)文件里，按题号对应，每步解答带知识点链接。建议一章做完再看答案。所有代码用 `-std=c11 -Wall -Wextra` 起步（个别题目会要求别的旗标，题面会写明）。凡是碰动态内存的题，一律开 `-fsanitize=address` 跑一遍——第 7 章说过，这是它的主场。

## 2.1 指针是什么

### 2.1-A {#hw-2-1-a}

难度 **L1** · 涉及[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)

写一个「考试分数」程序：`int score = 61;`，用 `int* p = &score;` 打印 `&score` 和 `p`，验证两者相等；再通过 `*p` 先加 10 分、再扣 1 分，每次打印 `score` 和 `*p`；最后声明 `int** pp = &p`，通过 `**pp` 把分数改成 99，打印验证。回答两问：①`&p` 和 `p` 的值相等吗？为什么？②`sizeof(int*)`、`sizeof(char*)`、`sizeof(double*)` 在本机各是多少？这说明了什么？

[参考答案 →](homework-solutions#hw-2-1-a)

### 2.1-B {#hw-2-1-b}

难度 **L2** · 涉及[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)

诊断题。给出三段程序，先动笔预测「编译有没有警告？普通构建跑起来发生什么？ASan 构建报什么？」，再全部真跑贴出真实结果：(a) `int* p = NULL; *p = 1;`（空指针）；(b) `int* q; *q = 2;`（未初始化的野指针）；(c) `int x = 0; int* r = &x; *r = 3; printf("%d\n", x);`（合法对照）。重点回答：为什么 (b) 的普通构建有时会「看起来没事」？把 ASan 对 (a)(b) 的报告各摘一段关键行。

[参考答案 →](homework-solutions#hw-2-1-b)

## 2.2 指针算术

### 2.2-A {#hw-2-2-a}

难度 **L2** · 涉及[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)

不动用下标，只用指针，对 `int a[6] = {12, 5, 8, 19, 3, 7};` 完成三件事：①用指针遍历求所有元素的和；②用指针遍历找最大值；③循环结束后用 `p - a` 打印「指针实际走过了几个元素」。再加一个步长观察：声明 `long b[3] = {100, 200, 300}; long* lp = b;`，打印 `(void*) lp` 与 `(void*) (lp + 1)`，看 `long*` 加 1 跨了几个字节。

[参考答案 →](homework-solutions#hw-2-2-a)

### 2.2-B {#hw-2-2-b}

难度 **L3** · 涉及[第 2 章：指针算术](/02-pointers-memory/02-pointer-arithmetic)

原地反转。写 `void reverse(int* a, int n)`：只允许用两个指针（一个指向首元素、一个指向末元素）加指针算术，不许用下标、不许开第二个数组，把数组头尾对调。用 `{1, 2, 3, 4, 5, 6}`（偶数个）和 `{1, 2, 3, 4, 5}`（奇数个）两个用例验证。要求说清：两个指针相遇的终止条件为什么写成 `lo < hi` 而不是 `lo <= hi`；取「末元素指针」时 `a + n - 1` 和 past-the-last 规则是什么关系。

[参考答案 →](homework-solutions#hw-2-2-b)

## 2.3 用指针改调用者的变量

### 2.3-A {#hw-2-3-a}

难度 **L2** · 涉及[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)

「平方与开关」。①写 `void square(int* p)`，通过 `*p` 把 5 平方成 25；再写一个故意写错的对照版 `void square_wrong(int n)`（改的是参数本身），调用后原变量不变——贴出两个版本的输出并说清差别根源。②写 `void toggle(int* flag)` 把 `*flag` 取反，连续调用两次，验证值回到原样。

[参考答案 →](homework-solutions#hw-2-3-a)

### 2.3-B {#hw-2-3-b}

难度 **L3** · 涉及[第 3 章：用指针改调用者的变量](/02-pointers-memory/03-pointer-parameters)

多返回值。写 `int time_of_day(int total_sec, int* h, int* m, int* s)`：把总秒数拆成「天、时、分、秒」——天数用 `return` 返回，时/分/秒用指针参数带出来。用 `time_of_day(90061, &h, &m, &s)` 验证（90061 = 1 天 1 时 1 分 1 秒），再调一次 `61` 秒的用例。要求说清：这个函数的四个参数里哪些是「输入」、哪些是「输出」，读代码的人靠什么区分。

[参考答案 →](homework-solutions#hw-2-3-b)

## 2.4 const 限定

### 2.4-A {#hw-2-4-a}

难度 **L2** · 涉及[第 4 章：const 限定](/02-pointers-memory/04-const-qualifier)

预测 + 验证。下面四段代码拆成四个小程序，先预测「编译通过还是失败、报错在哪个字眼」，再真跑把报错贴出来：①`int n = 5, m = 9; const int* p = &n; p = &m;`（改指向）；②`const int* p = &n; *p = 9;`（通过 p 改对象）；③`int* const q = &n; q = &m;`（改指针本身）；④`int* const q = &n; *q = 9;`（通过 q 改对象）。再答一问：`const int*` 和 `int const*` 是同一个类型吗？依据教材的「口诀」说。

[参考答案 →](homework-solutions#hw-2-4-a)

### 2.4-B {#hw-2-4-b}

难度 **L3** · 涉及[第 4 章：const 限定](/02-pointers-memory/04-const-qualifier)、[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)

const 被显式强转丢掉的两种下场。①局部：`const int x = 10;` 在 `main` 里，用 `int* p = (int*) &x;` 然后 `*p = 20;`，先预测再真跑（普通 `-O0` 构建），记录输出。②全局：把 `const int y = 10;` 放到 `main` 外面，同样的改法，真跑记录退出码。③把①用 `-O2` 再跑一遍，看输出会不会变。回答：为什么三个版本的结局各不同？（提示：全局 const 落在哪个段？`-O2` 下编译器凭什么敢直接认定 `x` 还是 10？）以及 `-Wdiscarded-qualifiers` 警告的用途。

[参考答案 →](homework-solutions#hw-2-4-b)

## 2.5 指针、数组、字符串的统一视角

### 2.5-A {#hw-2-5-a}

难度 **L2** · 涉及[第 5 章：指针、数组、字符串的统一视角](/02-pointers-memory/05-pointer-array-string)

手写 strchr。写 `const char* my_strchr(const char* s, char c)`：用 `char*` 遍历找到第一个等于 `c` 的字符，返回指向它的指针，找不到返回 `NULL`。用 `"hello world"` 找 `'o'`、`'w'`、`'z'` 三个用例验证；对找到的字符用指针减法 `pos - s` 打印它的下标。要求参数是 `const char*`，并说清：返回类型为什么也用 `const char*`、返回的指针指向哪块内存。

[参考答案 →](homework-solutions#hw-2-5-a)

### 2.5-B {#hw-2-5-b}

难度 **L3** · 涉及[第 5 章：指针、数组、字符串的统一视角](/02-pointers-memory/05-pointer-array-string)

手写 strcat。写 `char* my_strcat(char* dst, const char* src)`：先走到 `dst` 的 `\0`，再把 `src` 逐字符拷过去（含结尾 `\0`），返回 `dst` 的起点。用 `char buf[32] = "pointer";` 接 `my_strcat(buf, " and memory")`，验证输出。再加一问：如果 `buf` 只开 16 字节，同样的拼接用 ASan 构建跑一遍，贴出报告并解释——`strcat` 家族「不检查剩余空间」的不安全，具体体现在报告的哪一行。

[参考答案 →](homework-solutions#hw-2-5-b)

## 2.6 动态内存入门

### 2.6-A {#hw-2-6-a}

难度 **L2** · 涉及[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)

动态浮点数组。写程序：`int n = 8;`，用 `malloc((size_t) n * sizeof(double))` 要一块 `double` 数组，检查 `NULL`；填入 `a[i] = 1.0 / (i + 1)`（倒数序列），打印全部（`%.3f`）和总和；`free` 并置 `NULL`；再用 ASan 构建重跑一遍确认零报告。回答：为什么 `malloc` 的返回值在 C 里不需要强转 `(double*)`（强转反而掩盖什么）？

[参考答案 →](homework-solutions#hw-2-6-a)

### 2.6-B {#hw-2-6-b}

难度 **L3** · 涉及[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)

自动扩容的动态数组。给定 `int input[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};`（10 个元素），写代码把它拷进一个「容量从 2 起步」的动态数组：每次放不下就用 tmp 模式 `realloc` 翻倍容量，打印每次扩容后的容量，最后打印全部元素和最终容量。要求：必须用 `int* tmp = realloc(a, ...)` 接、查 `NULL`、成功才赋回 `a`，并在注释里写清为什么不能直接 `a = realloc(a, ...)`。ASan 构建零报告。

[参考答案 →](homework-solutions#hw-2-6-b)

## 2.7 动态内存的坑

### 2.7-A {#hw-2-7-a}

难度 **L2** · 涉及[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)

三个坏程序各跑一遍 ASan 贴关键报告行：(a) `malloc` 一块 `int`、`free` 之后又 `printf("*p")`；(b) 同一块 `free` 两次；(c) `malloc(3 * sizeof(int))` 后写 `a[3]`。先预测每个会报 `heap-use-after-free` / `attempting double-free` / `heap-buffer-overflow` 里的哪一个，再真跑验证（注意：ASan 构建不要加 `-O1`，默认 `-O0` 就行——为什么这点很关键，做完你就知道了）。最后回答：`free(p); p = NULL;` 为什么能同时防住 (a) 和 (b) 两个坑（结合 `free(NULL)` 合法这条规矩）？

[参考答案 →](homework-solutions#hw-2-7-a)

### 2.7-B {#hw-2-7-b}

难度 **L3** · 涉及[第 7 章：动态内存的坑](/02-pointers-memory/07-dynamic-memory-pitfalls)、[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)

谁分配谁释放。写 `char* make_label(int n)`：`malloc` 一块 64 字节，用 `snprintf` 填 `"label-<n>"`（snprintf 是[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)的老朋友），返回指针。`main` 里连调三次、存下三个指针、打印。先「忘了 free」跑 ASan——记录 LeakSanitizer 报告（泄漏多少字节、几次分配），注意观察打印输出在不在；再补上三个 `free` 重跑，确认零报告。回答：为什么「函数里分配、调用者释放」必须把「谁负责 free」写进注释或约定。

[参考答案 →](homework-solutions#hw-2-7-b)

## 2.8 多级指针与指针数组

### 2.8-A {#hw-2-8-a}

难度 **L2** · 涉及[第 8 章：多级指针与指针数组](/02-pointers-memory/08-multi-level-pointers)

命令行 echo。写程序打印 `argc` 与全部 `argv`（下标版），再用「指针遍历指针数组」版重写循环（`for (char** p = argv; *p != NULL; p++)`），两个版本输出要一致；最后打印验证 `argv[argc] == NULL`。用 `./程序名 alpha beta gamma` 运行，贴输出。回答：`argv` 的类型 `char**` 为什么等价于「指针数组退化」，`argv[0]` 是什么？

[参考答案 →](homework-solutions#hw-2-8-a)

### 2.8-B {#hw-2-8-b}

难度 **L3** · 涉及[第 8 章：多级指针与指针数组](/02-pointers-memory/08-multi-level-pointers)

行指针遍历矩阵。`int m[3][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};` 用 `int (*row)[4] = m;` 的行指针遍历（不许写 `m[i][j]` 下标），打印每行和与总和；再打印 `(void*) m` 和 `(void*) (m + 1)` 验证一步跨 16 字节。另外把 `int* p = m;` 这句写进程序编译一次，贴出编译诊断并解释：这个报错证明了 `m` 退化后是什么类型？最后用一句话说清 `int* a[4]`（指针数组）与 `int (*a)[4]`（数组指针）的含义差别。

[参考答案 →](homework-solutions#hw-2-8-b)

## 2.9 函数指针

### 2.9-A {#hw-2-9-a}

难度 **L2** · 涉及[第 9 章：函数指针](/02-pointers-memory/09-function-pointers)

一元回调。写 `int apply1(int (*op)(int), int x)`，配三个一元函数 `neg`（取负）、`dbl`（乘 2）、`sq`（平方），对 `x = 6` 依次调用打印结果。再打印 `(void*) neg` 与 `(void*) &neg` 验证「函数名退化」——两者地址应该相同。回答：`fp(x)` 与 `(*fp)(x)` 两种调用为什么等价，工程里现在写哪种更干净。

[参考答案 →](homework-solutions#hw-2-9-a)

### 2.9-B {#hw-2-9-b}

难度 **L4** · 涉及[第 9 章：函数指针](/02-pointers-memory/09-function-pointers)、[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)

qsort 排结构体。`struct Item { char name[16]; double price; };` 初始化 5 个商品（价格故意乱序，其中两个价格差小于 1，比如 8.5 和 8.7），用 `qsort` 按价格**降序**排序并打印表格。比较函数要求：先把 `const void*` 转回 `const struct Item*`；比较两个 `double` 时**不许**写 `return (int)(a->price - b->price)` 这种「相减再转」——用 `>`/`<` 显式返回 ±1/0。再写一个「错误版」比较函数相减，同一个数组再排一次，贴出两个版本的输出对照：差小于 1 的两个商品在错误版里发生了什么？

[参考答案 →](homework-solutions#hw-2-9-b)

## 2.10 复杂声明与 typedef

### 2.10-A {#hw-2-10-a}

难度 **L2** · 涉及[第 10 章：复杂声明与 typedef](/02-pointers-memory/10-complex-declarations-typedef)

读声明 + 验证。对下面三个声明，先用右左法则写出中文读法并预测结果，再真跑验证：①`char* months[2] = {"Jan", "Jun"};`，预测 `sizeof(months)`；②`int (*row)[3]` 指向 `int m3[3][3]` 的第 0 行，打印 `(*row)[2]`，`row++` 后再打印 `(*row)[2]`；③写一个「返回 `int*` 的函数」`int* first(int* a)`，用它取数组首元素，验证调用结果。最后说明：③为什么**不是**函数指针——它和 `int (*f)(...)` 差在哪。

[参考答案 →](homework-solutions#hw-2-10-a)

### 2.10-B {#hw-2-10-b}

难度 **L4** · 涉及[第 10 章：复杂声明与 typedef](/02-pointers-memory/10-complex-declarations-typedef)

两件事。①`#define` vs `typedef` 经典坑：`#define STR_PTR char*` 之后声明 `STR_PTR a, b;`，预测 `sizeof(a)` 与 `sizeof(b)` 并真跑；再用 `typedef char* StrPtr;` 对照（`StrPtr x, y;`）。②拆新声明 `int (*pipeline[2])(int)`：先用右左法则写出中文读法，再用 `typedef int (*Unary)(int);` 改写并实际使用——两个一元函数 `dbl`、`neg` 组成管道，对输入 5 依次打印 `pipeline[0](5)` 与 `pipeline[1](pipeline[0](5))`，验证「函数指针数组」成立。

[参考答案 →](homework-solutions#hw-2-10-b)

## 2.11 void* 与字节操作

### 2.11-A {#hw-2-11-a}

难度 **L2** · 涉及[第 11 章：void* 与字节操作](/02-pointers-memory/11-void-ptr-and-byte-ops)

short 的两字节。①`short x = 0x1234;`，用 `unsigned char* p = (unsigned char*) &x;` 逐字节打印（本机小端应打出什么顺序？），据此判断字节序；②把 `&x` 转成 `void*` 再隐式转回 `short*`，打印地址与解引用结果，验证转换无损。回答：`short` 是 2 字节、`int` 是 4 字节在本机成立，但为什么「在程序里写死 2/4」是坏习惯（结合字节序与实现定义说）。

[参考答案 →](homework-solutions#hw-2-11-a)

### 2.11-B {#hw-2-11-b}

难度 **L3** · 涉及[第 11 章：void* 与字节操作](/02-pointers-memory/11-void-ptr-and-byte-ops)

重叠与字节序。①`int a[5] = {1, 2, 3, 4, 5};` 用 `memmove(a + 1, a, 4 * sizeof(int))` 整体右移一位，验证输出；再写一个「从前往后手抄」的错误版循环（`b[i] = b[i - 1]`），对照两个结果并解释：重叠区间为什么必须 `memmove`。②写 `uint32_t swap_endian(uint32_t x)`：用 `memcpy` 把 4 字节读进字节数组、反向、再 `memcpy` 回去（纯字节操作，不用移位），验证 `swap_endian(0x12345678)` 的结果与「再换一次还原」。

[参考答案 →](homework-solutions#hw-2-11-b)

## 2.12 内存布局与生命周期

### 2.12-A {#hw-2-12-a}

难度 **L2** · 涉及[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)

六大区地址分层。写程序打印六类地址：栈局部变量、`malloc` 来的堆指针、已初始化全局（`.data`）、已初始化的 `static` 局部（`.data`）、未初始化全局（`.bss`）、字符串字面量（`.rodata`），再打印未初始化全局的值（验证启动清 0）。按地址从高到低排序，说出每类落在哪个段。最后对可执行文件跑 `nm`，把两个全局符号和 `main` 的类型字母（`D`/`B`/`T`）与地址分层对上。贴出全部输出。

[参考答案 →](homework-solutions#hw-2-12-a)

### 2.12-B {#hw-2-12-b}

难度 **L4** · 涉及[第 12 章：内存布局与生命周期](/02-pointers-memory/12-memory-layout)

栈向低地址 + 爆栈。①写递归函数 `walk(int depth)`：每层打印本层局部变量的地址，递归到 depth 4，观察地址一路变小。②写一个「没有基线」的递归函数 `deep()`，用一个全局计数器记深度、每 50000 层打印一次并 `fflush(stdout)`（为什么要 fflush，结合「stdout 是块缓冲、进程被信号打死时不刷」说），跑到爆栈，记录最后打印的深度和退出码。③回到 2.12-A 的输出，回答：六类地址里哪些属于「程序映像」，程序映像里 `.rodata` 和 `.data` 谁低谁高。

[参考答案 →](homework-solutions#hw-2-12-b)

## 2.C 跨章综合与挑战

### 2.C-1 {#hw-2-c-1}

难度 **L3** · 涉及[第 8 章](/02-pointers-memory/08-multi-level-pointers)、[第 6 章](/02-pointers-memory/06-malloc-free-basics)、[第 9 章](/02-pointers-memory/09-function-pointers)

命令行数字排序器。写 `sortnums`：从 `argv`（跳过 `argv[0]`）拿到一串数字字符串，用 `sscanf`（**教材外补充**：`scanf` 的字符串版，机制同[第 12 章：基础 IO](/01-c-basics/12-basic-io)的 `scanf`）解析成 `int`，`malloc` 一块 `(argc - 1) * sizeof(int)` 存下（检查 `NULL`），用 `qsort` 排序（自己写比较函数，注意别 `return a - b`），打印结果，`free` 并置 `NULL`。用 `./sortnums 42 7 19 3 88` 运行。ASan 构建零报告。要求说清「谁分配谁释放」在这道题里的配对关系。

[参考答案 →](homework-solutions#hw-2-c-1)

### 2.C-2 {#hw-2-c-2}

难度 **L4** · 涉及[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)、[第 9 章](/02-pointers-memory/09-function-pointers)、[第 5 章](/02-pointers-memory/05-pointer-array-string)

手写泛型冒泡排序。写 `void my_sort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*))`：`qsort` 同款签名，内部用冒泡排序（排序算法本身是**教材外补充**——阶段 3 才细讲，这里只要求「相邻两两比较交换 n 轮」），元素定位必须用 `unsigned char*` 字节指针算偏移（`void*` 不能算术，这一步要用上第 11 章的规矩）。写三个比较函数：`int` 升序、`int` 降序、`char*` 字符串字典序（用 `strcmp`）。三个用例各跑一遍贴输出。注意字符串版传进来的「元素」本身是指针，比较函数里要先转 `const char* const*` 再解一层。

[参考答案 →](homework-solutions#hw-2-c-2)

### 2.C-3 {#hw-2-c-3}

难度 **L5** · 涉及[第 11 章](/02-pointers-memory/11-void-ptr-and-byte-ops)、[第 2 章](/02-pointers-memory/02-pointer-arithmetic)、[第 3 章](/02-pointers-memory/03-pointer-parameters)

挑战题（改编自 CSAPP 的「reverse/rotate」练习与 LeetCode 189「Rotate Array」的原地三反转解法，两处来源如实标注）。写两个函数，都要求 **O(1) 额外空间、只允许 `unsigned char*` 字节指针操作**：

① `void reverse_bytes(void* p, size_t n)`：原地反转 n 字节（半开区间 `[lo, hi)`，注意字节指针的相遇条件）。

② `void rotate_left(void* p, size_t n, size_t k)`：把 n 字节内存块整体**循环左移** k 字节——三次反转法：先反转 `[0, k)`、再反转 `[k, n)`、最后反转 `[0, n)`；`k` 要先 `% n`（`n == 0` 直接返回）。

验证：`char s[8] = "abcdefg"` 左移 2 字节应为 `"cdefgab"`；`int a[5] = {1, 2, 3, 4, 5}` 左移 3 个元素应为 `4 5 1 2 3`（注意 `int` 版的 k 是「3 个元素 × 4 字节」）；再写一个「开临时缓冲的朴素实现」（对照用），对拍 k=0、k=1、k=n、k<n 几组边界，还要试 k>n（`9 % 7 = 2`）。说明为什么 `int` 版要把 k 乘 `sizeof(int)`。

[参考答案 →](homework-solutions#hw-2-c-3)
