---
title: "阶段 1 课后练习参考答案（Homework）"
description: "阶段 1（C 语言基底）课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出（gcc 16 / clang 22 / WSL Arch 实跑）。"
chapter: 1
order: 1
tags:
  - host
  - syntax
  - type
difficulty: beginner
reading_time_minutes: 45
platform: host
c_standard: [11, 99, 89]
prerequisites:
  - "阶段 1 课后练习（Homework）"
related:
  - "阶段 1 各章"
---

# 阶段 1 课后练习参考答案（Homework）

> 所有命令与输出在 WSL Arch（gcc 16.1.1）下真实运行得到。UB 类题目的输出「只是这台机器这次的选择」，换编译器/优化级别可能不同——这正是每道题要你体会的东西。

## 1.1-A {#hw-1-1-a}

**难度 L1** · 题面见 [homework](homework#hw-1-1-a)

**思路**：`main` 不写 `return` 的行为取决于编译标准——C99 起标准（§5.1.2.2.3）给了 `main` 特权：走到 `}` 就当 `return 0`；C89 没有这条，函数会带着「最后一个表达式留在寄存器里的值」返回。

1. `-std=c11`：静默编译，退出码 0。→ 知识点：[第 1 章：程序结构与编译四阶段](/01-c-basics/01-program-structure-and-compilation)「`main`：程序的唯一入口」一节
2. `-std=c89`：gcc 用 `-Wreturn-type` 警告「非 void 函数走到结尾没 return」，退出码是本机实测的 **6**——它并不是随机垃圾，而是**最后一个表达式的返回值残留**：`printf("hello\n")` 返回打印的字符数 6、落在 `%eax` 里，函数没 `return` 就带着它出了门。把打印内容换成 `"hi\n"`，退出码就跟着变成 3；而 clang 的 `-std=c89` 这次给 0（它的代码生成恰好把寄存器清掉了）。两个编译器给不同值，正是 C89 下这行为无保证的活证。→ 知识点：同上（C99 之前没有隐式 return 0 的规定）

**验证输出**：

```text
$ gcc -std=c11 -Wall mr.c -o mr11 && ./mr11; echo "c11 exit=$?"
hello
c11 exit=0
$ gcc -std=c89 -Wall mr.c -o mr_gcc
mr.c:4:1: warning: control reaches end of non-void function [-Wreturn-type]
$ ./mr_gcc; echo "gcc c89 exit=$?"
hello
gcc c89 exit=6        ← %eax 残留了 printf 的返回值(= 6 个字符)
$ gcc -std=c89 -Wall mr2.c -o mr2_gcc && ./mr2_gcc; echo "gcc c89(hi) exit=$?"
hi
gcc c89(hi) exit=3     ← 换成 "hi\n" 就变 3,机制实锤
$ clang -std=c89 -Wall mr.c -o mr_clang && ./mr_clang; echo "clang c89 exit=$?"
hello
clang c89 exit=0       ← clang 的代码生成恰好清了寄存器
```

要点：`main` 隐式返回 0 是 **C99 起**的待遇；C89 下退出码是「最后一个表达式残留的寄存器值」（本机 gcc 恰好等于 `printf` 的返回值），换编译器、换打印内容都会变——所以老老实实写 `return 0`，别赌。

## 1.1-B {#hw-1-1-b}

**难度 L2** · 题面见 [homework](homework#hw-1-1-b)

**思路**：头文件被两个翻译单元 `#include` 后，`int global_counter = 5;` 这个**定义**在每个 `.o` 里各生成一份，链接器撞上同名强定义。

1. 链接报 `multiple definition of 'global_counter'`，还指明两个来源（`b.o` 与 `a.o` 的 `.data` 段）。→ 知识点：[第 1 章](/01-c-basics/01-program-structure-and-compilation)「声明 vs 定义」一节（头文件只放声明的原因）
2. 修复：头文件改成 `extern int global_counter;`（只声明不分配），定义放进 `a.c`。链接后 `a.o` 里是 `D global_counter`（大写 = external 链接的定义），`b.o` 里是 `U global_counter`（引用、等链接器填）。→ 知识点：[第 1 章](/01-c-basics/01-program-structure-and-compilation)「链接：external / internal / none」一节（`nm` 大小写与 `U`）

**验证输出**：

```text
$ gcc a.o b.o -o bad
/usr/bin/ld: b.o:(.data+0x0): multiple definition of `global_counter'; a.o:(.data+0x0): first defined here
collect2: error: ld returned 1 exit status
$ # 修复后
$ gcc a.o b.o -o ok && ./ok
10
$ nm a.o | grep -E 'global_counter|get_a'
0000000000000000 T get_a
0000000000000000 D global_counter      ← 定义在这,大写 D
$ nm b.o | grep -E 'global_counter|get_a'
                 U get_a
                 U global_counter      ← 引用,等链接器填
```

## 1.2-A {#hw-1-2-a}

**难度 L1** · 题面见 [homework](homework#hw-1-2-a)

**思路**：`sizeof` 打印家族大小、`<limits.h>` 打印范围；两问都是「标准规定 vs 实现定义」的辨析。

1. 只有 `sizeof(char) == 1` 是标准钉死的（§6.5.3.4），其它整型大小是实现定义的——标准只规定相对关系和最小位宽。→ 知识点：[第 2 章：整型家族与 sizeof](/01-c-basics/02-integer-types-and-sizeof)「整型家族与 sizeof」一节
2. 「64 位机器上 `long` 一定是 8 字节」是**错**的：64 位 Windows 用 LLP64（`long` 仍是 4 字节），64 位 Linux/macOS 用 LP64（8 字节）。反例带来的工程问题：同一段用 `long` 的代码、同样的二进制数据，跨平台行为不同——协议/存档/二进制布局必须用定宽类型。→ 知识点：[第 2 章](/01-c-basics/02-integer-types-and-sizeof)「平台差异：LP64 vs LLP64」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall sizes.c -o sizes && ./sizes
char=1 short=2 int=4 long=8 longlong=8
int 范围: -2147483648..2147483647
unsigned 范围: 0..4294967295
```

本机是 LP64：`long` 和 `long long` 都是 8 字节。

## 1.2-B {#hw-1-2-b}

**难度 L2** · 题面见 [homework](homework#hw-1-2-b)

**思路**：定宽类型把「要几个字节」从平台的实现定义里摘出来。

1. `int32_t`=4、`int64_t`=8、`uint8_t`=1、`size_t`=8（本机 64 位）。→ 知识点：[第 2 章](/01-c-basics/02-integer-types-and-sizeof)「`<stdint.h>`：要可移植，就用定宽类型」一节
2. 协议长度字段必须用 `uint32_t`：`long` 在 LLP64 上只有 4 字节装不下 32 位无符号全长（且符号性不对），`int` 的大小和符号都是实现定义的、也没有「恰好 32 位无符号」的保证。→ 知识点：[第 2 章](/01-c-basics/02-integer-types-and-sizeof)（定宽类型是跨平台首选）

**验证输出**：

```text
$ gcc -std=c11 -Wall fixedw.c -o fixedw && ./fixedw
int32_t=4 int64_t=8 uint8_t=1 size_t=8
```

## 1.3-A {#hw-1-3-a}

**难度 L2** · 题面见 [homework](homework#hw-1-3-a)

**思路**：两个「反直觉」的 4 都来自同一条规则——小类型一运算就提升成 `int`，而字符常量本身就是 `int`。

1. `sizeof(c + c) = 4`：`char` 参与算术先整型提升成 `int`。→ 知识点：[第 3 章：整型提升、溢出与回绕](/01-c-basics/03-integer-promotion-overflow)「第一座山：整型提升」一节
2. `sizeof('A') = 4`：C 里**字符常量的类型是 `int`**，不是 `char`（又一个反直觉点）；`'A' + 1` 提升后还是 `int`，所以也是 4。→ 知识点：[第 4 章：浮点、字符、常量与隐式转换](/01-c-basics/04-float-char-const-cast)「常量」一节（字符常量的类型）

**验证输出**：

```text
$ gcc -std=c11 -Wall promo2.c -o promo2 && ./promo2
sizeof(c)      = 1
sizeof(c + c)  = 4
sizeof('A')    = 4   <- 字符常量是 int!
sizeof('A'+1)  = 4
```

## 1.3-B {#hw-1-3-b}

**难度 L3** · 题面见 [homework](homework#hw-1-3-b)

**思路**：①混合符号比较时 `int` 装不下 `unsigned` 的全部值，两边都被转成 `unsigned`——`-1` 变成巨大的正数，所以「-1 小于正数」为假。②`unsigned` 永不为负，`i >= 0` 恒真，减到 0 再减就回绕。

1. `-1 < 1u` 为 0、`-1 < sizeof(int)` 也为 0（`sizeof` 返回 `size_t`，无符号）；`-Wsign-compare` 把两处都揪出来；安全写法是把两边转换到装得下的带符号类型（`(long)`）。→ 知识点：[第 3 章](/01-c-basics/03-integer-promotion-overflow)「三座山的交汇」一节（usual arithmetic conversions）
2. `unsigned` 倒计数：打印 `3 2 1 0 4294967295 4294967294 ...`——到 0 再 `i--` 就回绕成 `UINT_MAX`，条件 `i >= 0` 永远为真，没保险就是死循环。→ 知识点：[第 3 章](/01-c-basics/03-integer-promotion-overflow)「第三座山」一节（无符号回绕是 well-defined，但「永不为负」是代价）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wsign-compare mixed.c -o mixed
mixed.c:3:43: warning: comparison of integer expressions of different signedness:
        'int' and 'unsigned int' [-Wsign-compare]
mixed.c:4:43: warning: comparison of integer expressions of different signedness:
        'int' and 'long unsigned int' [-Wsign-compare]
$ ./mixed
-1 < 1u           = 0     ← -1 被转成 4294967295,不小于 1
-1 < sizeof(int)  = 0     ← 同理,size_t 是无符号
-1 < (long)1u     = 1     ← 显式转换到装得下的类型,比较才正常
i = 3
i = 2
i = 1
i = 0
i = 4294967295            ← 0 再减,回绕!
i = 4294967294
i = 4294967293
i = 4294967292
```

## 1.4-A {#hw-1-4-a}

**难度 L1** · 题面见 [homework](homework#hw-1-4-a)

**思路**：①二进制浮点存不准 $0.7$/$0.1$/$0.8$，`==` 比较碰运气，容差比较才是正解。②C 的 `/` 和 `%` 向零取整——商和余数朝 0 方向截断。

1. $0.7 + 0.1$ 实为 $0.79999999999999993$，与 $0.8$ 的表示不等 → `==` 为 0；`fabs(a-b) < 1e-9` 为 1。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「浮点：$0.1 + 0.2$ 的名场面」一节
2. $\frac{7}{2}=3$、$\frac{-7}{2}=-3$（向零取整，不是向下取整）、`7%2=1`、`-7%2=-1`（余数跟着被除数符号走）。→ 知识点：[第 5 章：运算符基础](/01-c-basics/05-operators-basics)「算术与关系运算符」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall fpdiv.c -o fpdiv -lm && ./fpdiv
0.7 + 0.1 == 0.8 ? 0
0.7 + 0.1      = 0.79999999999999993
fabs 容差比较  = 1
7/2=3 -7/2=-3 7%2=1 -7%2=-1
```

## 1.4-B {#hw-1-4-b}

**难度 L3** · 题面见 [homework](homework#hw-1-4-b)

**思路**：①`0` 开头是八进制常量——`010` 是 8 不是 10。②大小写字母 ASCII 差 32。③$\frac{5}{2}$ 是整数除法，小数在赋给 `double` 之前就丢了。

1. `010 + 0x10 = 8 + 16 = 24`。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「常量」一节（八进制前缀坑）
2. 遍历字符串，`'a' <= c <= 'z'` 范围内的字符减 32 转大写；`char` 是整数，所以能直接和字符常量比大小、做加减。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「`char`：它就是个小整数」一节
3. `avg = 5 / 2` 得 $2.0$（先整数除法再提升），$\frac{5.0}{2}$ 得 $2.5$（$5.0$ 是 `double`，整个表达式升为浮点除法）。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「隐式转换」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall octo.c -o octo && ./octo
010 + 0x10 = 24
大写化: HELLO, C-JOURNEY 2026!
avg = 2.000000, avg2 = 2.500000
```

## 1.5-A {#hw-1-5-a}

**难度 L2** · 题面见 [homework](homework#hw-1-5-a)

**思路**：短路求值——左边能定结果，右边整个不执行（副作用不发生）。

1. `0 && side("A")`：左边为假 → 整体必假 → `side` 不被调用（`calls` 仍是 0）；`1 || side("B")` 同理。`p != NULL && p->value > 0` 靠它：`p` 为空时右边不执行，空指针解引用被跳过。→ 知识点：[第 5 章](/01-c-basics/05-operators-basics)「逻辑运算符与『短路求值』」一节
2. `(1 || x++)`：`||` 左边为真 → `x++` 被短路跳过 → `x` 保持 0。误区：以为「这一行执行了 `x++` 就必然发生」。→ 知识点：同上（别把必须发生的副作用放在可能被短路的位置）

**验证输出**：

```text
$ gcc -std=c11 -Wall shortc2.c -o shortc2 && ./shortc2
0 && side: r1=0 calls=0
1 || side: r2=1 calls=0
(1 || x++): r3=1 x=0
```

## 1.5-B {#hw-1-5-b}

**难度 L3** · 题面见 [homework](homework#hw-1-5-b)

**思路**：同一表达式里对 `i` 两次修改、中间没有序列点 → UB（§6.5¶2）。`-Wsequence-point` 编译期就能提醒；运行结果纯属编译器心情。

1. `gcc -Wall` 报 `operation on 'i' may be undefined [-Wsequence-point]`。→ 知识点：[第 5 章](/01-c-basics/05-operators-basics)「优先级、结合性，以及求值顺序的 UB」一节
2. 本机 `-O0` 和 `-O2` 都跑出 `j=4, i=3`——**这次恰好一致，不代表有保证**；换编译器、换版本就可能变。这正是 UB 的定义：标准撒手。→ 知识点：[阶段 0 第 9 章](/00-dev-environment/10-standards-and-optimization)（「我测过」不作数）
3. 规避：拆成两行（`int a = i++; int b = ++i;`），每行各自有序列点，行为确定。→ 知识点：[第 5 章](/01-c-basics/05-operators-basics)（拆行最稳；逗号运算符、`&&`/`||`、`?:` 是少数有序列点的例外）

**验证输出**：

```text
$ gcc -std=c11 -Wall sequb.c
sequb.c:4:22: warning: operation on 'i' may be undefined [-Wsequence-point]
$ gcc -std=c11 -O0 sequb.c -o sequb0 && ./sequb0
j = 4, i = 3
$ gcc -std=c11 -O2 sequb.c -o sequb2 && ./sequb2
j = 4, i = 3
$ # 合法写法
$ gcc -std=c11 -Wall seqok.c -o seqok && ./seqok
a=1 b=3 i=3
```

## 1.6-A {#hw-1-6-a}

**难度 L2** · 题面见 [homework](homework#hw-1-6-a)

**思路**：标志位三件套（`|=` 置位、`&` 测试、`&= ~` 清位）；拆包 = 移位把目标位段挪到低端再 `&` 掩码。

1. 定义 `1u << n` 掩码；`perm |= PERM_READ | PERM_EXEC` 后测试三标志（`!!` 把非零规范成 0/1）；`perm &= ~PERM_READ` 清位。→ 知识点：[第 6 章：位运算与移位](/01-c-basics/06-bitwise-and-shift)「应用：标志位三件套」一节
2. `0x3C` 拆包：高 4 位 `(b >> 4) & 0x0F` = `0x3`，低 4 位 `b & 0x0F` = `0xC`。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（移位 + 掩码的组合拳）

**验证输出**：

```text
$ gcc -std=c11 -Wall flags2.c -o flags2 && ./flags2
置位后: read=1 write=0 exec=1
清 READ 后: read=0 write=0 exec=1
0x3C 高 4 位 = 0x3, 低 4 位 = 0xC
```

## 1.6-B {#hw-1-6-b}

**难度 L3** · 题面见 [homework](homework#hw-1-6-b)

**思路**：`1 << 31` 和 `1 << 32` 是两个**不同的** UB：前者是「有符号左移溢出」（§6.5.7¶4：有符号左移结果不可表示即 UB），后者是「移位位数越界」（§6.5.7¶3）。UBSan 都能抓，但报的词不一样。

1. 两个函数分开写（同一行会被 UBSan 去重只报一次），UBSan 各报一条：`left shift of 1 by 31 places cannot be represented in type 'int'`（结果越界）与 `shift exponent 32 is too large for 32-bit type 'int'`（指数越界）。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)「移位的两个 UB 坑」一节、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)（UBSan 精确到行列）
2. 安全版：调用前检查 `0 <= n <= 30`（对 `int` 而言），范围外返回约定值 `-1`，sanitizer 全绿。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（位数是变量时先检查范围；移位默认用 `unsigned` 最稳）

**验证输出**：

```text
$ gcc -std=c11 -Wall -O1 -g -fsanitize=undefined shift2.c -o shift2 && ./shift2
shift2.c:3:14: runtime error: left shift of 1 by 31 places cannot be represented in type 'int'
shift2.c:6:14: runtime error: shift exponent 32 is too large for 32-bit type 'int'
shift_31(31) = -2147483648
shift_32(32) = 1
$ gcc -std=c11 -Wall -O1 -g -fsanitize=undefined shiftsafe.c -o shiftsafe && ./shiftsafe
safe(10) = 1024, safe(31) = -1, safe(32) = -1
```

## 1.7-A {#hw-1-7-a}

**难度 L1** · 题面见 [homework](homework#hw-1-7-a)

**思路**：`continue` 跳过本轮剩余语句、`break` 跳出整个循环。动笔算：1 到 15 之间去掉 3 的倍数，是 1+2+4+5+7+8+10+11+13+14 = 75。

1. 循环里 `i % 3 == 0` 先 `continue`（3、6、9、12、15 都被跳过），`i > 15` 再 `break`（16 起不再进循环体）。→ 知识点：[第 7 章：控制流](/01-c-basics/07-control-flow)「循环」一节（`break` vs `continue`）

**验证输出**：

```text
$ gcc -std=c11 -Wall loopv.c -o loopv && ./loopv
sum = 75
```

## 1.7-B {#hw-1-7-b}

**难度 L2** · 题面见 [homework](homework#hw-1-7-b)

**思路**：`switch` 命中 `case` 后一路往下执行，没 `break` 就贯穿；`-Wimplicit-fallthrough` 区分「不小心」和「故意的」。

1. `score = 87` → `score/10 = 8` → 命中 `case 8` 打印 `B`，没 `break` → 贯穿进 `case 7` 又打印 `C`。→ 知识点：[第 7 章](/01-c-basics/07-control-flow)「`switch` 与 fall-through」一节
2. `-Wimplicit-fallthrough` 在贯穿处警告；加 `/* fall through */` 注释声明「故意的」，警告消失。→ 知识点：同上（编译器怎么帮你区分两种贯穿）

**验证输出**：

```text
$ gcc -std=c11 -Wall grade.c -o grade && ./grade
B
C                       ← 贯穿!87 分不该有 C
$ gcc -std=c11 -Wall -Wimplicit-fallthrough grade.c
grade.c:10:9: warning: this statement may fall through [-Wimplicit-fallthrough=]
$ gcc -std=c11 -Wall -Wimplicit-fallthrough grade2.c   ← 加了 /* fall through */
$ ./grade2
B
C                       ← 静默,编译器知道你是故意的
```

## 1.8-A {#hw-1-8-a}

**难度 L1** · 题面见 [homework](homework#hw-1-8-a)

**思路**：①C 的参数是值传递——函数改的是副本。②递归必须有基线；基线不覆盖所有输入（比如负数），就无限递归直到栈爆。

1. `swap(3, 7)` 后 `x=3 y=7` 纹丝不动。→ 知识点：[第 8 章：函数](/01-c-basics/08-functions)「参数是『值传递』」一节（想让函数改调用者的变量要靠指针，阶段 2）
2. `power(2, 10) = 1024` 正常；`power(2, -1)` 时 `exp == 0` 基线永远够不着、一路减到栈爆 → SIGSEGV（退出码 139）。注意它的 `printf` 输出连影都没有——进程被信号打死时 stdout 缓冲没机会刷，这个坑阶段 0 第 13 章讲过，这里又撞一次。→ 知识点：[第 8 章](/01-c-basics/08-functions)「递归」一节（基线必须完备、每次朝基线靠近）、[阶段 0 第 13 章](/00-dev-environment/14-gdb-basics)（缓冲丢失）

**验证输出**：

```text
$ gcc -std=c11 -Wall swappow.c -o swappow && ./swappow
swap 后 x=3 y=7 (没换!)
power(2,10) = 1024
$ gcc -std=c11 -Wall pn.c -o pn && ./pn; echo "exit=$?"
Segmentation fault
exit=139                 ← power(2,-1):无限递归,栈爆(输出经管道捕获,缓冲没机会刷,见 1.13-B)
```

## 1.8-B {#hw-1-8-b}

**难度 L2** · 题面见 [homework](homework#hw-1-8-b)

**思路**：`static` 局部变量只初始化一次、值跨调用保持；它住在静态存储期（`.data`/`.bss`），不像普通局部变量住栈上、进出函数就重建。

1. `next_id()` 连调得 `1001/1002/1003`。注意打印顺序是 `1003 1002 1001`——因为 gcc 对 `printf` 的参数**从右往左求值**；但函数实参求值顺序是**未指定**的（unspecified，§6.5.2.2¶10），本机 clang 就恰好从左往右打印 `1001 1002 1003`——所以别拿输出对答案、要拿原理对答案。→ 知识点：[第 8 章](/01-c-basics/08-functions)「`static` 局部变量」一节、[第 5 章](/01-c-basics/05-operators-basics)（求值顺序「未指定」，与「未定义」是两回事）
2. `reset_count()` 每次进函数都重建，连调三次全是 `1`——对照出 `static` 的「只初始化一次」。→ 知识点：[第 9 章：作用域、存储期与 static](/01-c-basics/09-scope-storage-static)「存储期」一节（自动 vs 静态）

**验证输出**：

```text
$ gcc -std=c11 -Wall staticid.c -o staticid && ./staticid
next_id: 1003 1002 1001      ← 值按参数位置摆放;求值从右往左
reset_count: 1 1 1
```

## 1.9-A {#hw-1-9-a}

**难度 L2** · 题面见 [homework](homework#hw-1-9-a)

**思路**：块作用域——内层块声明的同名变量**隐藏**外层，出块后内层变量消失、外层重新可见。

1. 三层嵌套各打印自己的 `x`（3/2/1），每层的 `x` 只在所属花括号里生效。→ 知识点：[第 9 章](/01-c-basics/09-scope-storage-static)「作用域：名字的可见范围」一节
2. 同名嵌套是坏习惯：读代码的人分不清「这个 `x` 是哪个」；避免方式是起有区分度的名字。→ 知识点：同上（隐藏合法但别滥用）

**验证输出**：

```text
$ gcc -std=c11 -Wall nest.c -o nest && ./nest
最内层 x = 3
中层 x = 2
外层 x = 1
```

## 1.9-B {#hw-1-9-b}

**难度 L3** · 题面见 [homework](homework#hw-1-9-b)

**思路**：`static` 全局变量是 internal 链接（`nm` 小写 `d`），别的翻译单元看不见；`extern` 声明的普通全局是 external（大写 `D`），可以跨文件引用。

1. `nm a.o`：`d hidden`（小写！内部链接）、`D shared`（大写，外部链接）。`b.c` 里引用 `hidden` → 链接报 `undefined reference to 'hidden'`。→ 知识点：[第 9 章](/01-c-basics/09-scope-storage-static)「`static` 的三重含义」一节（全局改「谁看得见」）、[第 1 章](/01-c-basics/01-program-structure-and-compilation)（`nm` 大小写）
2. 去掉 `hidden` 引用、给 `shared` 加 `extern int shared;` 声明，`b.c` 里 `shared += 1` 后 `a.c` 的 `get_shared()` 读到 21——两个翻译单元看到同一个对象。→ 知识点：[第 9 章](/01-c-basics/09-scope-storage-static)「`extern`」一节

**验证输出**：

```text
$ nm a.o
0000000000000000 T get_shared
0000000000000000 d hidden      ← 小写:内部链接,跨文件不可见
0000000000000004 D shared      ← 大写:外部链接
$ gcc a.o b.o -o bad
/usr/bin/ld: b.o: in function `main':
b.c:(.text+0x1a): undefined reference to `hidden'
$ # 修复后
$ gcc a.o b.o -o ok && ./ok
shared=21
```

## 1.10-A {#hw-1-10-a}

**难度 L2** · 题面见 [homework](homework#hw-1-10-a)

**思路**：①数组传参退化成指针，`sizeof` 在函数里量的是指针。②`a[i]` 就是 `*(a + i)`，加法可交换所以 `1[a]` 也成立。③「数组的数组」让行列都能用 `sizeof` 套出来。

1. `main` 里 40、函数里 8；gcc 用 `-Wsizeof-array-argument` 提醒；正确签名是「指针 + 显式长度参数」。→ 知识点：[第 10 章：数组](/01-c-basics/10-arrays)「退化」一节
2. `a[1]` 与 `1[a]` 都取到 1。→ 知识点：[第 10 章](/01-c-basics/10-arrays)「声明与下标」一节（下标即指针加法的皮）
3. `sizeof(m)/sizeof(m[0])` = 行数 3，`sizeof(m[0])/sizeof(m[0][0])` = 列数 5。→ 知识点：[第 10 章](/01-c-basics/10-arrays)「多维数组」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall decay2.c
decay2.c:3:58: warning: 'sizeof' on array function parameter 'a' will return size of 'int *'
$ ./decay2
main 里 sizeof(a) = 40
  函数内 sizeof(a) = 8 (是指针!)
a[1]=1, 1[a]=1
行数=3 列数=5
 1  2  3  4  5
 6  7  8  9 10
11 12 13 14 15
```

## 1.10-B {#hw-1-10-b}

**难度 L3** · 题面见 [homework](homework#hw-1-10-b)

**思路**：①指定初始化器只点零星位置，其余自动补 0。②越界写是 UB，普通构建下「看起来没事」正是它最阴险的地方。

1. `int c[5] = {[2] = 9, [4] = 7};` 打印 `0 0 9 0 7`——没点的位置自动 0，这就是 `{0}` 能清零整个数组的原理。→ 知识点：[第 10 章](/01-c-basics/10-arrays)「初始化」一节（部分初始化补 0、C99 指派初始化器）
2. `arr[3] = 99` 越界写：普通构建**静默通过**、退出码 0；ASan 构建报 `stack-buffer-overflow` 精确到 `desig.c:10`。→ 知识点：[第 10 章](/01-c-basics/10-arrays)「越界访问」一节、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)（ASan 点名变量）

**验证输出**：

```text
$ gcc -std=c11 -Wall desig.c -o desig && ./desig; echo "exit=$?"
c = 0 0 9 0 7
越界写后 arr[0]=1
exit=0                     ← 普通构建:静默,像没事一样
$ gcc -std=c11 -Wall -O1 -g -fsanitize=address desig.c -o desig_asan && ./desig_asan
==507==ERROR: AddressSanitizer: stack-buffer-overflow ...
WRITE of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj-ex1-hw/desig.c:10
```

## 1.11-A {#hw-1-11-a}

**难度 L2** · 题面见 [homework](homework#hw-1-11-a)

**思路**：①`strlen` 数到 `\0` 为止不算它，`sizeof` 把 `\0` 也算进去。②字符串字面量住只读段，指针指它一写就段错误；数组是把字面量拷贝到栈上、可改。③`'\0'` 是空字符（码 0），`'0'` 是字符零（码 48）。

1. `strlen(s)=5`、`sizeof(s)=6`，差的那个字节就是结尾 `\0`。→ 知识点：[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)「`\0` 终止符」一节
2. `char* p = "hello"; p[0] = 'H';` 退出码 139（SIGSEGV，写只读 `.rodata`）；`char a[] = "hello"; a[0] = 'H';` 正常得 `Hello`。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)「字符串字面量」一节（§6.4.5p7 修改字面量是 UB）
3. `'\0'`=0、`'0'`=48。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)（两个「零」差 48）

**验证输出**：

```text
$ gcc -std=c11 -Wall strlit.c -o strlit && ./strlit
strlen=5 sizeof=6
'\0'=0 '0'=48
数组改首字符: Hello
$ gcc -std=c11 -Wall litcrash.c -o litcrash && ./litcrash; echo "字面量版 exit=$?"
字面量版 exit=139
```

## 1.11-B {#hw-1-11-b}

**难度 L3** · 题面见 [homework](homework#hw-1-11-b)

**思路**：①`strncpy` 源长于 n 时**不补 `\0`**；②`snprintf` 返回值是「本该写入的总长度」；③`fgets` 会存换行符。

1. `strncpy(dst, "ABCDEF", 4)` 后字节是 `65 66 67 68`——没有 `\0`；`printf("%s", dst)` 越界读，ASan 报 `stack-buffer-overflow`（READ）；正确姿势是拷完手动 `dst[sizeof(dst)-1] = '\0'`。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)「安全替代」一节（`strncpy` 的坑、`-Wstringop-truncation`）
2. `snprintf(buf, 6, "Hello, world!")` 返回 13——判断截断写 `if (n >= (int)sizeof(buf))`。→ 知识点：同上（`snprintf` 返回值是「本该写入的长度」）
3. `fgets` 把换行符也存进缓冲；处理代码：`len > 0 && line[len-1] == '\n'` 就把它替换成 `'\0'`。→ 知识点：同上（`fgets` 取代已删的 `gets`）

**验证输出**：

```text
$ gcc -std=c11 -Wall strsafe.c
strsafe.c:6:5: warning: 'strncpy' output truncated copying 4 bytes ... [-Wstringop-truncation]
strsafe.c:13:47: warning: 'Hello, world!' directive output truncated ... [-Wformat-truncation=]
$ printf 'hello\n' | ./strsafe
strncpy 后字节: 65 66 67 68        ← 没有 \0
snprintf: buf='Hello' 返回值=13    ← 截断了 13-5=8 个字符
fgets 测试(输入 hello 换行):
处理后: 'hello' (len=5)            ← 换行符被换成了 \0
$ gcc -std=c11 -O1 -g -fsanitize=address strncpybad.c -o strncpybad && ./strncpybad
==534==ERROR: AddressSanitizer: stack-buffer-overflow ...
READ of size 5 at 0x... thread T0
    #1 0x... in main /tmp/cj-ex1-hw/strncpybad.c:6
```

## 1.12-A {#hw-1-12-a}

**难度 L2** · 题面见 [homework](homework#hw-1-12-a)

**思路**：①`scanf` 返回成功匹配的项数——这是唯一可靠的输入校验手段。②`%c` 不跳前导空白，`%d` 留下的换行会喂给它。

1. 合法输入 `42`：返回 1，读到 42；非法输入 `abc`：返回 0，程序报错、退出码 1。→ 知识点：[第 12 章：基础 IO](/01-c-basics/12-basic-io)「scanf」一节（返回值语义）
2. `scanf("%d")` 后接 `scanf("%c")`，输入 `42\nX`：`%c` 读到的是**换行符**（码 10），不是 `X`；把格式串改成 `" %c"`（前导空格 = 先跳任意空白）后读到 `X`（码 88）。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（`%c` 不跳前导空白的坑）

**验证输出**：

```text
$ printf '42\n' | ./scanrob
输入整数: 读到 n=42
$ printf 'abc\n' | ./scanrob; echo "exit=$?"
输入整数: 错误:输入不是合法整数
exit=1
$ printf '42\nX' | ./scanf_c
n=42 c 的码=10       ← %c 吃到了残留的换行
$ printf '42\nX' | ./scanf_c2
n=42 c 的码=88       ← " %c" 跳空白,读到 'X'
```

## 1.12-B {#hw-1-12-b}

**难度 L3** · 题面见 [homework](homework#hw-1-12-b)

**思路**：①`[标志][宽度][.精度]` 控制对齐；②`printf` 是变参函数，编译器在调用点拿不到 `...` 的类型信息，只能靠 `-Wformat=` 这种「理解格式串语义」的特殊警告兜底；③用户输入当格式串 = 格式化字符串漏洞。

1. `%5d` 右对齐宽 5、`%-5d` 左对齐、`%05d` 补零、`%.2f` 两位小数——三行数据对齐成表格。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)「printf」一节
2. `printf("%d\n", 3.14)`：`-Wformat=` 编译期警告，运行打出一个**每次运行都不同**的垃圾值（本机那次是 `1083993272`——x86-64 SysV 下 `double` 走 XMM 寄存器、`%d` 却去读通用寄存器，读到的纯属寄存器残留；别拿你的垃圾值对答案，每次都不一样才是正常的）。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)「格式串与参数类型不匹配」一节、[阶段 0 第 8 章](/00-dev-environment/09-warning-flags)（`-Wformat=`）
3. `printf(user_input)` 里塞 `%x` 能读栈上内容、`%n` 甚至能写内存；正确写法 `printf("%s", user_input)` 把输入当数据而不是格式。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（别让外部数据控制格式串）

**验证输出**：

```text
$ gcc -std=c11 -Wall fmttab.c -o fmttab && ./fmttab
[   42] [42   ] [00042] [3.14]
[    7] [7    ] [00007] [2.72]
[ 1000] [1000 ] [01000] [1.62]
$ gcc -std=c11 -Wall mism.c
mism.c:4:14: warning: format '%d' expects argument of type 'int', but argument 2 has type 'double' [-Wformat=]
$ ./mism
1083993272             ← 本机那次的值;每次运行都变,别对答案
```

## 1.13-A {#hw-1-13-a}

**难度 L2** · 题面见 [homework](homework#hw-1-13-a)

**思路**：①每个字段要对齐到自身对齐要求的整数倍偏移，结构体总大小是最大成员对齐的倍数——`char/double/char` 被 padding 撑到 24。②`union` 所有成员同址，`0x3F800000` 就是 1.0f 的 IEEE 754 位模式。

1. `A`（`char c; double z; char d;`）预测 1+7pad+8+1+7pad = 24；`offsetof` 实锤 `c@0 z@8 d@16`；重排成 `double; char; char` 后 16，省了 8 字节。→ 知识点：[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)「内存对齐与填充」一节
2. `u.i = 0x3F800000` 后 `u.f` = 1.000000（类型双关）；`(void*)&u.i == (void*)&u.f` 为 1——注意比较不同指针类型需要先转 `void*`，否则 gcc 报警告。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「联合 union」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall align2.c -o align2 && ./align2
sizeof(A)=24  c@0 z@8 d@16     ← 预测命中:1+7pad+8+1+7pad
sizeof(B)=16  z@0 c@8 d@9      ← 重排后省 8 字节
写 0x3F800000 读 float = 1.000000
&u.i == &u.f ? 1
```

## 1.13-B {#hw-1-13-b}

**难度 L3** · 题面见 [homework](homework#hw-1-13-b)

**思路**：①tagged union = `enum` 标签 + `union` + `switch`，是 C 的多态手法；②位域按位分配但受基类型约束、装不下会截断；③FAM 一次 `malloc` 连结构体带尾巴。

1. `Tagged` 两种取值都正确打印——`switch (t->kind)` 按标签决定怎么读 `union`。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「联合」与「枚举」两节（tagged union 惯用法）
2. `unsigned char a:3` 装 9（`0b1001`）被截断成 1，gcc `-Woverflow` 警告；`sizeof(BitsChar)=1`（`unsigned char` 基正好 1 字节）。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「位域」一节
3. `sizeof(Buf)=4`（`data[]` 不占 sizeof），`malloc(sizeof(Buf)+5)` 后 `b->data` 就是那段尾巴，一次 `free` 全释放。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「柔性数组成员 FAM」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall tagged.c -o tagged && ./tagged
int: 42
double: 3.140000
$ gcc -std=c11 -Wall bitfam.c
bitfam.c:13:11: warning: unsigned conversion from 'int' to 'unsigned char:3' changes value from '9' to '1' [-Woverflow]
$ ./bitfam
sizeof(BitsChar)=1 x.a=1 x.b=20
sizeof(Buf)=4 len=5 data=ABCDE
```

## 1.C-1 {#hw-1-c-1}

**难度 L3** · 题面见 [homework](homework#hw-1-c-1)

**思路**：平均分用 `double total` 累计（整数除法坑）；表格用 `%5d`/`%-12s`/`%6.2f` 对齐；`-Wconversion` 零警告要求所有转换都是显式且无损的。

1. 关键决策：`total` 必须是 `double`（或除法时写 $4.0$），否则 $\frac{total}{4}$ 是整数除法、85.25 会被截成 85。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「隐式转换」一节（$\frac{5}{2}$ 教训的放大版）
2. 指定初始化器 + 遍历求总/求最高分。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)（结构体与指定初始化器）、[第 10 章](/01-c-basics/10-arrays)（结构体数组）
3. `printf` 宽度/对齐格式出表格；`-Wall -Wextra -Wconversion` 零警告。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)「printf」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -Wconversion students.c -o students && ./students
平均分 = 85.2
最高分 = 96.5 (Carol)
   ID 姓名       成绩
 1001 Alice         89.50
 1002 Bob           74.00
 1003 Carol         96.50
 1004 Dave          81.00
```

## 1.C-2 {#hw-1-c-2}

**难度 L4** · 题面见 [homework](homework#hw-1-c-2)

**思路**：大端字段拼装 = 高字节左移更多位后按位或；全部用无符号定宽类型，规避整型提升和符号扩展的雷。

1. 标志位：`0x05 = 0b101`，第 0 位（紧急）和第 2 位（重传）都是 1——用 `!!(flags & BIT)` 规范成 0/1。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（标志位三件套的测试位）
2. 长度：`(pkt[2] << 8) | pkt[3]` = `0x012C` = **300**；值：四字节拼接 = `0x00002710` = **10000**。关键细节：先转 `uint16_t/uint32_t` 再移位，否则 `uint8_t` 会被提升成 `int` 参与运算、左移 24 位就踩了有符号溢出的雷。→ 知识点：[第 3 章](/01-c-basics/03-integer-promotion-overflow)（整型提升：`uint8_t` 运算前先变 `int`）、[第 2 章](/01-c-basics/02-integer-types-and-sizeof)（定宽类型）、[第 6 章](/01-c-basics/06-bitwise-and-shift)（移位拼装）
3. UBSan 全程零报告、`-Wall -Wextra` 零警告。→ 知识点：[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -O1 -g -fsanitize=undefined packet.c -o packet && ./packet
紧急=1 重传=1
类型=2 长度=300 值=10000
```

## 1.C-3 {#hw-1-c-3}

**难度 L5** · 题面见 [homework](homework#hw-1-c-3)

**思路**：union 类型双关把 `float` 的 32 位原样拿出来，三个位段用「移位到低端 + 掩码」切分；正浮点数的位模式按无符号整数序排列（IEEE 754 布局使然），负数正好相反。

1. `Bits b; b.f = f;` 拿到位模式（union 双关是 C99 允许的类型双关）。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「联合 union」一节
2. 切分：符号 = `bits >> 31`；指数域 = `(bits >> 23) & 0xFF`，真值 = 域值 − 127（偏置）；尾数 = `bits & 0x7FFFFF`。（**教材外补充**：IEEE 754 的位布局——教材第 4 章只讲了精度与 $0.1+0.2$，第 13 章 union 演示只提了一句「符号 1 位+指数 8 位+尾数 23 位」，本题所需的偏置 127、隐含前导 1、0 的特殊编码均属补充知识。）验证：`1.0f` → 符号 0、真值指数 0（域 127）、尾数 0；`3.14f` → 指数 1（域 128）、尾数 `0x48F5C3`；`-0.5f` → 符号 1、指数 −1（域 126）、尾数 0；`0.0f` → 全零（指数域 0，真值 −127，这是「非规格化/零」的特殊编码，说明 0 的指数域不按常规解读）。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（移位+掩码切位段）、[第 13 章](/01-c-basics/13-struct-union-enum)「联合」一节（union 双关）
3. 排序探索：`1.5f < 2.5f` 按 float 与按位模式一致（IEEE 754 正数布局：指数在低位段高位，直接当无符号整数比大小就是浮点序）；`-1.5f < 1.5f` 按 float 真、按位模式假——负数的符号位是 1，位模式变成巨大整数，顺序整体反转。→ 知识点：IEEE 754 布局（符号-指数-尾数，正数可当无符号整数排序——这是基数排序等技巧的理论基础）

**验证输出**：

```text
$ gcc -std=c11 -Wall floatbits.c -o floatbits && ./floatbits
1.000000  -> 符号=0 指数=0(真值) 尾数=0x0
3.140000  -> 符号=0 指数=1(真值) 尾数=0x48F5C3
-0.500000 -> 符号=1 指数=-1(真值) 尾数=0x0
0.000000  -> 符号=0 指数=-127(真值) 尾数=0x0
1.5f < 2.5f 按 float = 1, 按位模式 = 1
-1.5f < 1.5f 按 float = 1, 按位模式 = 0
```
