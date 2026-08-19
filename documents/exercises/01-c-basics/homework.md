---
title: "阶段 1 课后练习（Homework）"
description: "C 语言基底阶段的课后练习：13 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战（浮点位级分解，改编自 CSAPP）。难度覆盖 L1~L5，题目都做了变式处理，参考答案独立成文件、逐步解答附知识点链接。"
chapter: 1
order: 0
tags:
  - host
  - syntax
  - type
difficulty: beginner
reading_time_minutes: 20
platform: host
c_standard: [11, 99, 89]
prerequisites:
  - "阶段 1 全部章节（第 1~13 章）"
related:
  - "阶段 1 Lab：位与字节的解剖台"
  - "阶段 1 Project：学生成绩管理器"
---

# 阶段 1 课后练习（Homework）

## 引言

这里的题按章组织，每章两道（基础 + 进阶），最后是两道跨章综合和一道 L5 挑战。每题标注难度档位（L1~L5，见[练习总览](/exercises/)）和涉及章节。题目都是「变式」——换场景、换推理方向，照抄教材例题抄不出答案；每道题都要真编译真跑，把输出贴下来才算完。

答案在独立的[参考答案](homework-solutions)文件里，按题号对应，每步解答带知识点链接。建议一章做完再看答案。所有代码用 `-std=c11 -Wall -Wextra` 起步（个别题目会要求别的标准或旗标，题面会写明）。

## 1.1 程序结构与编译四阶段

### 1.1-A {#hw-1-1-a}

难度 **L1** · 涉及[第 1 章：程序结构与编译四阶段](/01-c-basics/01-program-structure-and-compilation)

写一个打印一句话但不写 `return` 的 `main`，分别用 `-std=c89 -Wall` 和 `-std=c11` 编译运行，记录退出码和编译警告。解释：同一份源码、退出码为什么不一样？这条「main 的特权」是从哪个标准版本开始的？进阶自测：把打印内容改长或改短（比如 `"hello\n"` 换成 `"hi\n"`），C89 下退出码会不会跟着变？先预测再验证。

[参考答案 →](homework-solutions#hw-1-1-a)

### 1.1-B {#hw-1-1-b}

难度 **L2** · 涉及[第 1 章：程序结构与编译四阶段](/01-c-basics/01-program-structure-and-compilation)

把 `int global_counter = 5;`（注意：是**定义**）放进头文件，让两个 `.c` 都 `#include` 它，链接时会发生什么？贴出真实报错，用 `nm` 说明两个 `.o` 里各自有什么。然后修复：头文件改成 `extern int global_counter;`、定义挪进其中一个 `.c`，重新链接，贴出修复后 `nm` 的关键符号（注意字母大小写和 `U`）。

[参考答案 →](homework-solutions#hw-1-1-b)

## 1.2 整型家族与 sizeof

### 1.2-A {#hw-1-2-a}

难度 **L1** · 涉及[第 2 章：整型家族与 sizeof](/01-c-basics/02-integer-types-and-sizeof)

写程序打印 `char/short/int/long/long long` 各占多少字节、`int` 和 `unsigned` 的完整取值范围。再回答两问：①为什么 `sizeof(char)` 永远是 1，而 `int` 是 4 只是「本机实现」？②判断题：「64 位机器上 `long` 一定是 8 字节」——对还是错？举出反例模型并说明它会导致什么工程问题。

[参考答案 →](homework-solutions#hw-1-2-a)

### 1.2-B {#hw-1-2-b}

难度 **L2** · 涉及[第 2 章：整型家族与 sizeof](/01-c-basics/02-integer-types-and-sizeof)

用 `sizeof` 验证 `int32_t`、`int64_t`、`uint8_t`、`size_t` 各是多少字节。再写一段注释：一个网络协议规定「长度字段是 32 位无符号整数」，你解析时该用 `long`、`int` 还是 `uint32_t`？为什么另外两个是错的（结合 LP64/LLP64 说）？

[参考答案 →](homework-solutions#hw-1-2-b)

## 1.3 整型提升、溢出与回绕

### 1.3-A {#hw-1-3-a}

难度 **L2** · 涉及[第 3 章：整型提升、溢出与回绕](/01-c-basics/03-integer-promotion-overflow)、[第 4 章：浮点、字符、常量与隐式转换](/01-c-basics/04-float-char-const-cast)

**先动笔预测**，再真跑验证：`sizeof(c + c)`（`c` 是 `char`）、`sizeof('A')`（字符常量！）、`sizeof('A' + 1)` 各是多少？后两个的结果如果出乎意料，结合「字符常量的类型是 int」解释。

[参考答案 →](homework-solutions#hw-1-3-a)

### 1.3-B {#hw-1-3-b}

难度 **L3** · 涉及[第 3 章：整型提升、溢出与回绕](/01-c-basics/03-integer-promotion-overflow)

两道实验题。①**先预测**再真跑：`-1 < 1u` 和 `-1 < sizeof(int)` 的真假各是什么？用 `-Wsign-compare` 编译观察警告，解释「负数怎么在比较里变成了正数」，并给出安全的比较写法。②写一个 `unsigned` 倒计数循环（从 3 减到「小于等于 0」），但**加一道保险**：循环体最多执行 8 次就强制退出，观察它打印出的序列，解释这个序列为什么会这样、以及为什么没有保险这个循环就是死循环。

[参考答案 →](homework-solutions#hw-1-3-b)

## 1.4 浮点、字符、常量与隐式转换

### 1.4-A {#hw-1-4-a}

难度 **L1** · 涉及[第 4 章：浮点、字符、常量与隐式转换](/01-c-basics/04-float-char-const-cast)、[第 5 章：运算符基础](/01-c-basics/05-operators-basics)（向零取整）

两道预测题，都要真跑。①`0.7 + 0.1 == 0.8` 是真还是假？再用「差的绝对值小于容差」的写法验证正确的比较姿势（需要 `<math.h>` 的 `fabs`；数学库在 libm，必要时补 `-lm`——`-l` 的规则见阶段 0 第 6 章的库顺序；本机 gcc 16 对 `fabs` 有内建展开、不链也能过，但带上更稳）。②打印 $\frac{7}{2}$、$\frac{-7}{2}$、`7 % 2`、`-7 % 2` 的值，解释 C 的整数除法和取余「向零取整」是什么意思。

[参考答案 →](homework-solutions#hw-1-4-a)

### 1.4-B {#hw-1-4-b}

难度 **L2** · 涉及[第 4 章：浮点、字符、常量与隐式转换](/01-c-basics/04-float-char-const-cast)

三道小题，都要真跑。①**先预测** `010 + 0x10` 的值再验证——如果和你直觉不一样，说清那个 `0` 开头的常量是什么进制。②不查库函数，手写一个把字符串里小写字母转大写的函数（只处理 `'a'..'z'` 范围，用 $±32$ 的规律），测试 `"Hello, C-Journey 2026!"` 的结果。③预测并验证 `double avg = 5 / 2;` 和 `double avg2 = 5.0 / 2;` 的值，解释小数是在哪一步丢掉的、$5.0$ 这个写法为什么能救它。

[参考答案 →](homework-solutions#hw-1-4-b)

## 1.5 运算符基础

### 1.5-A {#hw-1-5-a}

难度 **L2** · 涉及[第 5 章：运算符基础](/01-c-basics/05-operators-basics)

两道题。①写一个带副作用的函数（比如打印一行并返回 1），验证 `0 && 副作用()` 和 `1 || 副作用()` 里副作用各被调用几次，解释短路求值；再说明 `if (p != NULL && p->value > 0)` 为什么靠短路才安全。②**先预测**再真跑：`int x = 0; int r = (1 || x++);` 之后 `x` 的值是多少？如果预测错了，说明你的误区在哪。

[参考答案 →](homework-solutions#hw-1-5-a)

### 1.5-B {#hw-1-5-b}

难度 **L3** · 涉及[第 5 章：运算符基础](/01-c-basics/05-operators-basics)

写 `int i = 1; int j = (i++) + (++i);` 这样的「一行里多次修改同一个变量」的代码，用 `-Wall` 编译，观察警告内容；再把它分别在 `-O0` 和 `-O2` 下跑，两个结果一样吗？解释为什么这是 UB（结合序列点的概念），以及工程里该怎么规避。最后给出一个**合法**的例子：用逗号运算符或拆行写出「先加再赋值」的等价正确写法。

[参考答案 →](homework-solutions#hw-1-5-b)

## 1.6 位运算与移位

### 1.6-A {#hw-1-6-a}

难度 **L2** · 涉及[第 6 章：位运算与移位](/01-c-basics/06-bitwise-and-shift)

自拟一个三标志场景（比如文件的读/写/执行权限，用第 0/1/2 位），完成：定义三个掩码、置位两个、测试全部三个（打印每个标志的开/关）、清掉一个再测试。再用 `uint8_t b = 0x3C;` 做一次「拆包」：用移位和掩码分别提取它的高 4 位和低 4 位，打印十六进制。把输出全部贴出来。

[参考答案 →](homework-solutions#hw-1-6-a)

### 1.6-B {#hw-1-6-b}

难度 **L3** · 涉及[第 6 章：位运算与移位](/01-c-basics/06-bitwise-and-shift)、[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

写一个 `int shift_left(int n) { return 1 << n; }`，用 UBSan（`-fsanitize=undefined`）分别以 `n = 31` 和 `n = 32` 调用，贴出两份报告（一份是「有符号左移溢出」、一份是「移位位数越界」——两个 UB 不一样！）。再写一个「安全版」：调用前检查 `n` 的范围，范围外的返回一个约定值，让 sanitizer 全绿。

[参考答案 →](homework-solutions#hw-1-6-b)

## 1.7 控制流

### 1.7-A {#hw-1-7-a}

难度 **L1** · 涉及[第 7 章：控制流](/01-c-basics/07-control-flow)

写一个循环：`i` 从 1 到 20，遇到 3 的倍数就 `continue`，`i > 15` 就 `break`，其余累加。**先动笔算**最终 `sum` 是多少，再真跑对答案，并说清 `break` 和 `continue` 在这一题里各自起了什么作用。

[参考答案 →](homework-solutions#hw-1-7-a)

### 1.7-B {#hw-1-7-b}

难度 **L2** · 涉及[第 7 章：控制流](/01-c-basics/07-control-flow)

写一个「分数转等级」的 `switch`（比如 90+ = A、80+ = B……），先故意**漏掉一个 `break`**，运行观察贯穿现象并贴输出；然后修好它。再用 `-Wimplicit-fallthrough` 编译故意贯穿的版本，观察 gcc 怎么提醒你；最后补上 `/* fall through */` 注释验证警告消失。

[参考答案 →](homework-solutions#hw-1-7-b)

## 1.8 函数

### 1.8-A {#hw-1-8-a}

难度 **L1** · 涉及[第 8 章：函数](/01-c-basics/08-functions)

两道题。①写 `void swap(int a, int b)`，在 `main` 里调用后打印两个变量——它们交换了吗？解释为什么没有（值传递的副本）。②写递归的 `int power(int base, int exp)`（幂运算），必须带基线条件，验证 `power(2, 10) == 1024`；再调用一次 `power(2, -1)` 看会发生什么（如果没保护），说明基线条件不完善会怎样。

[参考答案 →](homework-solutions#hw-1-8-a)

### 1.8-B {#hw-1-8-b}

难度 **L2** · 涉及[第 8 章：函数](/01-c-basics/08-functions)

写一个函数 `next_id(void)`：每次调用返回 1001、1002、1003……递增的编号，用 `static` 局部变量实现，连调三次验证。再加一个对比函数 `reset_count(void)`（普通局部变量版，每次进去都归零），连调三次——对比两个函数的输出，说清 `static` 局部变量「只初始化一次」到底是什么意思（它在内存里住在哪、和普通局部变量有什么不同）。

[参考答案 →](homework-solutions#hw-1-8-b)

## 1.9 作用域、存储期与 static

### 1.9-A {#hw-1-9-a}

难度 **L2** · 涉及[第 9 章：作用域、存储期与 static](/01-c-basics/09-scope-storage-static)

写一个**三层嵌套块**的程序：外层 `int x = 1;`，中层块里 `int x = 2;`，内层块里 `int x = 3;`，每层打印自己看到的 `x`，出块后再打印。贴出输出，说清每个 `x` 在哪个作用域生效、为什么内层能「藏住」外层。最后补一句：这种同名嵌套为什么是坏习惯、该怎么避免。

[参考答案 →](homework-solutions#hw-1-9-a)

### 1.9-B {#hw-1-9-b}

难度 **L3** · 涉及[第 9 章：作用域、存储期与 static](/01-c-basics/09-scope-storage-static)、[第 1 章](/01-c-basics/01-program-structure-and-compilation)

两个翻译单元的实验。`a.c` 定义 `static int hidden = 10;` 和 `int shared = 20;`；`b.c` 里尝试「引用」`hidden` 和 `shared`。①先预测哪个会链接失败，真跑验证并贴报错；②`nm a.o` 看两个符号的字母，解释大小写的含义；③给出让 `b.c` 合法使用 `shared` 的写法（用 `extern` 声明），验证修改后两个文件看到同一个值。

[参考答案 →](homework-solutions#hw-1-9-b)

## 1.10 数组

### 1.10-A {#hw-1-10-a}

难度 **L2** · 涉及[第 10 章：数组](/01-c-basics/10-arrays)

三道题。①写 `void print_arr(int a[10])`，在函数内外分别打印 `sizeof(a)`，解释差别的根源（退化）和 `-Wsizeof-array-argument` 警告；写出「函数要处理数组，长度必须另传」的正确签名。②真跑验证 `1[a]` 这种写法能取到值，并解释为什么它合法（下标即指针加法的皮）。③对 `int m[3][5]` 用 `sizeof` 套出行数和列数，打印整个矩阵。

[参考答案 →](homework-solutions#hw-1-10-a)

### 1.10-B {#hw-1-10-b}

难度 **L3** · 涉及[第 10 章：数组](/01-c-basics/10-arrays)、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)

两道题。①用指定初始化器 `{[2] = 9, [4] = 7}` 初始化一个 5 元素数组，打印全部元素；再解释「初值列表比数组短，剩下的自动补 0」这条规则为什么让 `{0}` 能清零整个数组。②写一个栈数组越界**写**（下标越 1 格），分别用普通构建和 ASan 构建跑，贴出两种「报法」的差别，说清为什么普通构建下它可能「看起来没事」——这正是 UB 最阴险的地方。

[参考答案 →](homework-solutions#hw-1-10-b)

## 1.11 C 字符串与不安全 libc

### 1.11-A {#hw-1-11-a}

难度 **L2** · 涉及[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)

三道题。①对 `char s[] = "hello";` **先预测** `strlen(s)` 和 `sizeof(s)` 各是多少，真跑验证，解释差的那个字节是什么。②写两个版本的程序：`char* p = "hello"; p[0] = 'H';` 和 `char a[] = "hello"; a[0] = 'H';`，分别运行，贴出结果（一个 139 退出码），解释同样「改首字符」为什么一个死一个活。③`'\0'` 和 `'0'` 各是什么？用 `printf("%d")` 打印它们的值。

[参考答案 →](homework-solutions#hw-1-11-a)

### 1.11-B {#hw-1-11-b}

难度 **L3** · 涉及[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)

三道题。①用 `strncpy(dst, "ABCDEF", 4)` 拷贝到 4 字节缓冲，打印四个字节的值——有没有 `\0`？用 ASan 观察 `printf("%s", dst)` 会发生什么；写出「拷完手动补 `\0`」的正确姿势。②`snprintf(buf, 6, "Hello, world!")` 的返回值是多少？用返回值写出「判断是否被截断」的代码。③`fgets` 读一行时换行符会不会被存进缓冲？真实验证，并写出「读到就把它替换成 `\0`」的处理代码。

[参考答案 →](homework-solutions#hw-1-11-b)

## 1.12 基础 IO

### 1.12-A {#hw-1-12-a}

难度 **L2** · 涉及[第 12 章：基础 IO](/01-c-basics/12-basic-io)

两道题。①写一段「健壮读整数」的代码：`scanf("%d", &n)` 的返回值不等于 1 就打印错误并退出码 1，分别用合法输入和 `abc` 输入测试，贴出两次运行结果。②**先预测**再真跑：`scanf("%d", &n)` 之后立刻 `scanf("%c", &c)`，输入 `42` 换行再 `X`——那个 `%c` 读到的是什么？用「格式串里加一个空格」修好它，再测一次。

[参考答案 →](homework-solutions#hw-1-12-a)

### 1.12-B {#hw-1-12-b}

难度 **L2** · 涉及[第 12 章：基础 IO](/01-c-basics/12-basic-io)、[阶段 0 第 8 章](/00-dev-environment/09-warning-flags)

三道题。①用 `%5d`、`%-5d`、`%05d`、`%.2f` 打印一张对齐的小表格（三行数据），贴输出并逐条解释宽度/对齐/补零/精度的作用。②写 `printf("%d\n", 3.14);` 这类类型不匹配的代码，用 `-Wall` 编译观察 `-Wformat=` 警告，运行观察垃圾输出——解释为什么编译器在**调用点**发现不了这种错（变参函数的 `...`）。③问答：为什么 `printf(user_input)` 危险、`printf("%s", user_input)` 安全？说出攻击者可以在输入里塞什么。

[参考答案 →](homework-solutions#hw-1-12-b)

## 1.13 结构体、联合、枚举与内存对齐

### 1.13-A {#hw-1-13-a}

难度 **L2** · 涉及[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)

两道题。①**先预测** `struct { char c; double z; char d; }` 的 `sizeof` 是多少，真跑并用 `offsetof` 解剖每个字段的偏移，画出 padding 的位置；再重排字段（`double` 放最前），验证新的大小，总结「字段排序省内存」的规律。②写一个 `int`/`float` 的 `union`，往里写 `0x3F800000`，当 `float` 读出来是多少？（IEEE 754 里这就是 1.0f 的位模式）再验证 `&u.i == &u.f`。

[参考答案 →](homework-solutions#hw-1-13-a)

### 1.13-B {#hw-1-13-b}

难度 **L3** · 涉及[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)

三道题。①写一个 tagged union：`enum` 标签（记「当前存的是 int 还是 double」）+ `union` + `switch` 打印函数——这是 C 做「一个变量多种类型」的经典手法。②写一个位域结构（`unsigned char` 基类型的 `a:3, b:5`），验证 `sizeof == 1`；再给它赋一个 3 位装不下的值，贴出 `-Woverflow` 警告和被截断的结果。③写一个带柔性数组成员（`int len; char data[];`）的结构，一次 `malloc` 分配「结构体 + 5 字节尾巴」，填数据、打印、一次 `free`，贴出 `sizeof`（注意它不含尾巴）。

[参考答案 →](homework-solutions#hw-1-13-b)

## 1.C 跨章综合与挑战

### 1.C-1 {#hw-1-c-1}

难度 **L3** · 涉及[第 13 章](/01-c-basics/13-struct-union-enum)、[第 12 章](/01-c-basics/12-basic-io)、[第 10 章](/01-c-basics/10-arrays)、[第 4 章](/01-c-basics/04-float-char-const-cast)

综合题：写一个「学生成绩统计」程序。`struct Student { char name[32]; int id; double score; };`，用指定初始化器初始化 4 个学生（分数故意带小数），计算并打印：平均分（**小心整数除法坑**——总分怎么声明才能算出 89.5 而不是 89？）、最高分的那个学生、以及一张用 `%5d`/`%-12s`/`%6.2f` 对齐的表格。要求：`-Wall -Wextra -Wconversion` 编译**零警告**，贴出编译命令、运行输出和你的关键设计决策（总分类型、对齐格式）。

[参考答案 →](homework-solutions#hw-1-c-1)

### 1.C-2 {#hw-1-c-2}

难度 **L4** · 涉及[第 6 章](/01-c-basics/06-bitwise-and-shift)、[第 2 章](/01-c-basics/02-integer-types-and-sizeof)、[第 3 章](/01-c-basics/03-integer-promotion-overflow)、[第 13 章](/01-c-basics/13-struct-union-enum)

二进制协议解析（计组/网络综合，408 真题风格的加强版）。给定 8 字节报文 `uint8_t pkt[8] = {0x05, 0x02, 0x01, 0x2C, 0x00, 0x00, 0x27, 0x10};`，格式：字节 0 = 标志位（第 0 位「紧急」、第 2 位「重传」）、字节 1 = 类型、字节 2~3 = 长度（**大端** 16 位）、字节 4~7 = 值（**大端** 32 位）。用位运算和定宽类型解析出全部字段：打印标志位各位、类型、长度（应是多少？）、值（应是多少？）。要求全部用 `uint8_t/uint16_t/uint32_t` 和位运算完成，全程无符号运算，`-Wall -Wextra` 零警告，UBSan 零报告。

[参考答案 →](homework-solutions#hw-1-c-2)

### 1.C-3 {#hw-1-c-3}

难度 **L5** · 涉及[第 13 章](/01-c-basics/13-struct-union-enum)、[第 6 章](/01-c-basics/06-bitwise-and-shift)、[第 2 章](/01-c-basics/02-integer-types-and-sizeof)

挑战题（受 CSAPP「浮点位级操作」练习启发、按竞赛挑战强化。早期阶段 L5＝「用该阶段知识可解的最难问题」，档位口径见[练习总览](/exercises/)）。写一个 `void print_float_bits(float f)`：用 `union` 类型双关把 `float` 的 32 个比特原样拿到（`uint32_t`），然后**纯位运算**分解打印三部分——符号位（1 位）、指数（8 位，打印**真值**即减去偏置 127）、尾数（23 位，打印十六进制）。用 `1.0f`、`3.14f`、`-0.5f`、`0.0f` 四个输入验证，贴出每个的分解结果并对照你手算的 IEEE 754 编码。加分探索：正浮点数之间，直接用它们的位模式（当 `uint32_t`）比较大小，结果和按 `float` 比较一致吗？先推理、再验证，解释为什么（或为什么不）。

[参考答案 →](homework-solutions#hw-1-c-3)
