---
title: "阶段 1 Lab：位与字节的解剖台"
description: "阶段 1 动手实验：把整型家族、提升、位运算、union 双关、结构体对齐、安全字符串串成一条解剖线——六个步骤从 sizeof 侦察一路做到 ASan 抓溢出，最后附一道不用 math.h 徒手解码 float 的 L5 挑战。"
chapter: 1
order: 2
tags:
  - host
  - bit-manipulation
  - type
difficulty: beginner
reading_time_minutes: 25
platform: host
c_standard: [11, 99]
prerequisites:
  - "阶段 1 第 2~13 章"
related:
  - "阶段 1 Homework"
  - "阶段 1 Project"
---

# 阶段 1 Lab：位与字节的解剖台

## 实验目标

阶段 1 的知识点像一桌零件：整型家族、提升、溢出回绕、位运算、union、结构体对齐、字符串。这个 Lab 把它们拧成一条解剖线——你拿着 `sizeof`、`offsetof`、移位掩码、union 双关、sanitizer 这几把刀，把「一个字节里到底藏着什么」「一个 float 的 32 个比特长什么样」「一个 struct 为什么比字段之和胖」这些问题一个个解剖开。做完你会对「C 里一切都是字节、看你怎么解读」有肌肉记忆。

所有实验在 `/tmp` 下独立目录做。每步有验收标准；卡住先回题面每步标注的章节链接读教材，再不行看[实验参考](lab-solutions)。

## 步骤 1：类型侦察 {#lab-1}

难度 **L1** · 涉及[第 2 章：整型家族与 sizeof](/01-c-basics/02-integer-types-and-sizeof)

**目标**：把本机的整型家族和取值范围一次摸清，确认数据模型。

1. 写程序打印 `char/short/int/long/long long` 的 `sizeof`、`int` 与 `unsigned` 的取值范围（`<limits.h>`）、以及 `int32_t/int64_t/uint8_t/size_t` 的大小（`<stdint.h>`）。
2. 记录：`long` 是几字节？这说明本机是 LP64 还是 LLP64？

**验收标准**：贴出输出；一句话说清「`long` 是 8 字节」这个事实为什么不能写进你的代码假设里。

[实验参考 →](lab-solutions#lab-1)

## 步骤 2：提升与混合符号 {#lab-2}

难度 **L2** · 涉及[第 3 章：整型提升、溢出与回绕](/01-c-basics/03-integer-promotion-overflow)、[第 4 章：浮点、字符、常量与隐式转换](/01-c-basics/04-float-char-const-cast)（字符常量的类型是 int）

**目标**：亲手验证「小类型一运算就变 int」和「负数遇到 unsigned 会变正」。

1. 打印 `sizeof(c + c)`（`c` 是 `char`）、`sizeof('A')`、`sizeof('A' + 1)`，解释每个 4 的来历。
2. 打印 `-1 < 1u` 和 `-1 < sizeof(int)` 的结果；用 `-Wsign-compare` 编译，把警告贴下来。

**验收标准**：贴出全部输出和警告；能说清两个 `0` 背后的转换规则（usual arithmetic conversions）。

[实验参考 →](lab-solutions#lab-2)

## 步骤 3：位运算工具箱 {#lab-3}

难度 **L2** · 涉及[第 6 章：位运算与移位](/01-c-basics/06-bitwise-and-shift)

**目标**：攒一套「标志位三件套 + 拆包组包」的位运算基本功。

1. 定义三个掩码（第 0/1/2 位），完成置位、测试、清位全套，每步打印三个标志的状态。
2. 拆包：把 `uint8_t b = 0xB4;` 的高 4 位、低 4 位分别提取打印。
3. 组包：把高 4 位 `0x7` 和低 4 位 `0x9` 合成一个字节，打印十六进制。

**验收标准**：贴出输出；说出拆包为什么要「先移位再掩码」、组包为什么要「先移位再或」。

[实验参考 →](lab-solutions#lab-3)

## 步骤 4：union 类型双关 {#lab-4}

难度 **L3** · 涉及[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)

**目标**：同一块内存换着类型读——看 float 的二进制真身。

1. 写 `int`/`float` 的 `union`：写入 `0x3F800000`，当 `float` 读出来（应为 1.0f）。
2. 反向：写入 `3.14f`，当 `unsigned` 读出来，用 `%X` 打印（应为 `4048F5C3`）。
3. 用 `(void*)` 转换验证 `&u.i` 和 `&u.f` 是同一个地址。

**验收标准**：贴出输出；一句话说清「值变了但字节没变」是怎么回事。

[实验参考 →](lab-solutions#lab-4)

## 步骤 5：对齐侦探 {#lab-5}

难度 **L3** · 涉及[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)

**目标**：用 `offsetof` 抓住编译器塞的 padding。

1. 定义 `struct { char c; int i; char d; } A;` 和重排后的 `struct { int i; char c; char d; } B;`，打印两者的 `sizeof` 和每个字段的 `offsetof`。
2. 画出 `A` 的内存布局图（哪个字节是字段、哪个是 padding）。

**验收标准**：贴出输出和布局图；说出「字段顺序决定 sizeof」背后的两条对齐规则。

[实验参考 →](lab-solutions#lab-5)

## 步骤 6：安全字符串 {#lab-6}

难度 **L4** · 涉及[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)、[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

**目标**：把阶段 1 的字符串安全三件套（`snprintf` 截断检测、`fgets` 换行处理、ASan 抓溢出）一次做完。

1. 写一个 `format_line` 函数：用 `snprintf` 把一个「名字 + 分数」格式化进 20 字节缓冲，用返回值判断是否截断，截断就打印警告。
2. 用 `fgets` 读一行输入，去掉末尾换行符，打印处理后的字符串和长度。
3. 埋一颗雷：对一个 5 字节缓冲 `strcpy` 一个长串，用 ASan 构建运行，贴出报告并指出它点名了哪个变量。

**验收标准**：贴出三步的输出；能说出 `snprintf` 返回值 13 意味着什么。

[实验参考 →](lab-solutions#lab-6)

## 附加挑战（L5）：徒手解码 float {#lab-l5}

**目标**：**不用 `math.h`**，把上一个步骤的位分解手艺组装成一台「float 解码器」——这题受 CSAPP 浮点位级操作练习启发（早期阶段 L5＝「用该阶段知识可解的最难问题」，档位口径见[练习总览](/exercises/)）。

1. 写 `double decode_float(float f)`：用 union 双关拿到 `uint32_t` 位模式，拆出符号、指数域、尾数域。
2. **不用 `pow`/`ldexp`**：把公式 $value = sign × (1 + \frac{mant}{2²³}) × 2^{exp}$ 用纯算术实现——$2^{exp}$ 用循环乘 2（exp>0）或除 2（exp<0）自己算。
3. 验证三个输入 `1.0f`、`3.14f`、`-0.5f`：你解码出的 double 与原 float 的相对误差必须小于 `1e-6`（用 `fabs` 判断，这个可以用 math.h）。

**验收标准**：贴出三个输入的解码结果与误差；说清 $1 + \frac{mant}{2²³}$ 里的 `1`（隐含的前导 1）是什么。

[实验参考 →](lab-solutions#lab-l5)

## 提交物清单

一个目录装下全部源码、每步终端记录（`stepN.log`）、以及 200 字以内的小结——用你自己的话说清「同一块内存，换一种类型解读，值就变了」这件事你在哪一步看得最真切。
