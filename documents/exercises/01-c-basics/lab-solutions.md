---
title: "阶段 1 Lab 实验参考"
description: "阶段 1 Lab（位与字节的解剖台）的实验参考：六个步骤加 L5 挑战的逐步解答，每步标注知识点链接，所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。"
chapter: 1
order: 3
tags:
  - host
  - bit-manipulation
  - type
difficulty: beginner
reading_time_minutes: 30
platform: host
c_standard: [11, 99]
prerequisites:
  - "阶段 1 Lab 题面"
related:
  - "阶段 1 各章"
---

# 阶段 1 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。建议卡住时先看「思路」逐步对照。

## 步骤 1：类型侦察 {#lab-1}

**思路**：`sizeof` + `<limits.h>` + `<stdint.h>` 三件套一次摸清本机；`long == 8` 说明本机是 LP64。

1. 打印整型家族大小与范围、定宽类型大小。→ 知识点：[第 2 章：整型家族与 sizeof](/01-c-basics/02-integer-types-and-sizeof)「整型家族与 sizeof」「`<stdint.h>`」两节
2. 关键结论：本机 `long=8` 是 LP64 的事实，但它是**实现定义**的——64 位 Windows 的 LLP64 下 `long` 是 4 字节，所以「long 是 8 字节」绝不能写进代码假设。→ 知识点：[第 2 章](/01-c-basics/02-integer-types-and-sizeof)「平台差异：LP64 vs LLP64」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall types.c -o types && ./types
char=1 short=2 int=4 long=8 longlong=8
int: -2147483648..2147483647
unsigned: 0..4294967295
int32_t=4 int64_t=8 uint8_t=1 size_t=8
```

## 步骤 2：提升与混合符号 {#lab-2}

**思路**：`sizeof(c+c)` 是 4 因为 `char` 先提升成 `int`；`sizeof('A')` 是 4 因为字符常量本身就是 `int`。两个比较为 0 是 usual arithmetic conversions 把 `-1` 转成了无符号巨数。

1. 三个 4 的来历各不相同，但都指向「算术不在窄类型里发生」。→ 知识点：[第 3 章：整型提升、溢出与回绕](/01-c-basics/03-integer-promotion-overflow)「第一座山」、[第 4 章](/01-c-basics/04-float-char-const-cast)「常量」一节
2. `-1 < 1u` 为 0：`int` 装不下 `unsigned` 的全值域，两边都转 `unsigned`，`-1` 变 $4294967295$。`-Wsign-compare` 两处都报警。→ 知识点：[第 3 章](/01-c-basics/03-integer-promotion-overflow)「三座山的交汇」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall promo.c -o promo && ./promo
sizeof(c+c)=4 sizeof('A')=4 sizeof('A'+1)=4
-1 < 1u = 0
-1 < sizeof(int) = 0
$ gcc -std=c11 -Wall -Wsign-compare promo.c
promo.c:6:47: warning: comparison of integer expressions of different signedness:
        'int' and 'unsigned int' [-Wsign-compare]
promo.c:7:42: warning: comparison of integer expressions of different signedness:
        'int' and 'long unsigned int' [-Wsign-compare]
```

## 步骤 3：位运算工具箱 {#lab-3}

**思路**：置位用 `|=`、测试用 `&`、清位用 `&= ~`；拆包 = 先右移把目标位段挪到低端再掩码；组包 = 先左移到位再或。

1. `flags |= FLAG_A | FLAG_C` 置两位，测试三标志，`flags &= ~FLAG_A` 清一位。→ 知识点：[第 6 章：位运算与移位](/01-c-basics/06-bitwise-and-shift)「应用：标志位三件套」一节
2. `0xB4`（`1011 0100`）拆包：高 4 位 `(b >> 4) & 0x0F` = `0xB`，低 4 位 `b & 0x0F` = `0x4`。拆包先移位再掩码，是为了把「目标位段」和「无关位」分开处理。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（移位 + 掩码的组合）
3. 组包 `(0x7 << 4) | 0x9` = `0x79`：左移把高半字节放到位，再或上低半字节。→ 知识点：同上

**验证输出**：

```text
$ gcc -std=c11 -Wall bitops.c -o bitops && ./bitops
置位后: A=1 B=0 C=1
清 A 后: A=0 B=0 C=1
0xB4 拆包: 高=0xB 低=0x4
组包 0x7|0x9 = 0x79
```

## 步骤 4：union 类型双关 {#lab-4}

**思路**：union 所有成员共享同一块内存——写 `int` 读 `float`，字节没动、解读方式变了。

1. `u.i = 0x3F800000` 后 `u.f` 是 1.0f（IEEE 754 的位模式）；反向 `u.f = 3.14f` 后 `u.i` 的位模式是 `0x4048F5C3`。→ 知识点：[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)「联合 union」一节（类型双关）
2. `(void*)&u.i == (void*)&u.f` 为 1 实锤同址（比较不同指针类型要先转 `void*`）。→ 知识点：同上（同址是 union 的定义性行为）

**验证输出**：

```text
$ gcc -std=c11 -Wall pun.c -o pun && ./pun
写 0x3F800000 读 float = 1.000000
写 3.14f 读位模式   = 0x4048F5C3
(void*)&u.i == (void*)&u.f ? 1
```

## 步骤 5：对齐侦探 {#lab-5}

**思路**：字段要对齐到自身对齐要求的整数倍偏移，结构体总大小是最大成员对齐的倍数——`A` 的 `char/int/char` 里两个 `char` 各带 3 字节 padding。

1. `A`：`c@0`、`i@4`（跳过 3 字节 padding）、`d@8`，总大小补到 12。`B` 重排后 `i@0 c@4 d@5`，补到 8——**字段一样，顺序不同，省 4 字节**。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)「内存对齐与填充」一节（两条对齐规则）
2. 布局图：`A` 的字节 0=c、1~3=padding、4~7=i、8=d、9~11=padding。→ 知识点：同上（`offsetof` 是解剖刀）

**验证输出**：

```text
$ gcc -std=c11 -Wall align.c -o align && ./align
sizeof(A)=12: c@0 i@4 d@8
sizeof(B)=8: i@0 c@4 d@5
```

## 步骤 6：安全字符串 {#lab-6}

**思路**：`snprintf` 返回值是「本该写入的总长度」——`n >= sizeof(buf)` 就是截断判据；`fgets` 会存换行符；`strcpy` 溢出用 ASan 抓。

1. `format_line("Alice", 89.5)` 正常；超长名字触发截断，返回值 25 ≥ 20 打印警告。→ 知识点：[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)「安全替代」一节（`snprintf` 的返回值语义）
2. `fgets` 读到 `"hello world\n"`，把末尾 `\n` 换成 `\0` 后 len=11。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)（`fgets` 会存换行符）
3. `strcpy(small, "This is way too long")`：gcc 编译期 `-Warray-bounds` 警告「写 21 字节进 5 字节对象」；ASan 运行期报 `stack-buffer-overflow`、`WRITE of size 21`、点名 `boom.c:5`。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)「缓冲区溢出」一节、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)

**验证输出**：

```text
$ gcc -std=c11 -Wall safe.c -o safe
$ printf 'hello world\n' | ./safe
[Alice: 89.5]
警告:输出被截断(本该 25 字符)
[Alexander the Great]
输入一行: 处理后: 'hello world' len=11
$ gcc -std=c11 -Wall -O1 -g -fsanitize=address boom.c -o boom
boom.c:5:5: warning: '__builtin_memcpy' forming offset [5, 20] is out of the bounds [0, 5] ...
$ ./boom
==316==ERROR: AddressSanitizer: stack-buffer-overflow ...
WRITE of size 21 at 0x... thread T0
    #1 0x... in main /tmp/cj-ex1-lab/boom.c:5
```

## 附加挑战（L5）：徒手解码 float {#lab-l5}

**思路**：IEEE 754 的 `float` = 符号(1) + 指数(8, 偏置 127) + 尾数(23, 隐含前导 1)。不用 `pow`，$2^{exp}$ 用循环乘除实现。

1. union 双关拿位模式，三个位段用移位+掩码切开（第 3 步的手艺直接复用）。→ 知识点：[第 13 章](/01-c-basics/13-struct-union-enum)（union）、[第 6 章](/01-c-basics/06-bitwise-and-shift)（切位段）
2. 解码公式 $±(1 + \frac{mant}{2²³}) × 2^{exp−127}$：`1 +` 那个 1 是**隐含前导 1**——规格化数不存最高位，省下 1 比特精度。（**教材外补充**：IEEE 754 位布局与偏置 127 是补充知识——教材第 4 章只讲精度，第 13 章 union 演示只提了一句位段划分。）$2^{exp}$ 用循环乘 2/除 2 实现（exp 范围 −127..128，循环最多 128 次，一点不慢）。→ 知识点：[第 7 章](/01-c-basics/07-control-flow)（循环）、[第 13 章](/01-c-basics/13-struct-union-enum)「联合」一节（union 双关）
3. 验证：三个样本的相对误差精确为 **0**（解码结果与 `(double)(float)` 转换在双精度下逐位一致，`%.9f` 打印出的尾差是显示舍入的假象）。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)（`fabs` 容差比较）

**验证输出**：

```text
$ gcc -std=c11 -Wall decode.c -o decode -lm && ./decode
decode(1.000000) = 1.000000000  相对误差 = 0.000e+00
decode(3.140000) = 3.140000105  相对误差 = 0.000e+00
decode(-0.500000) = -0.500000000  相对误差 = 0.000e+00
```
