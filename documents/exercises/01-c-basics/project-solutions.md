---
title: "阶段 1 Project 参考实现"
description: "阶段 1 综合项目（学生成绩管理器）的完整参考实现：分层任务逐步讲解，每步标注知识点链接，含 Makefile、健壮输入、sanitizer/格式门、徒手 parse_double、排序与位图统计的真实运行输出。"
chapter: 1
order: 5
tags:
  - host
  - struct
  - bit-manipulation
difficulty: intermediate
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 1 Project 题面"
related:
  - "阶段 1 各章"
---

# 阶段 1 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1）真实运行得到。参考实现只是**一种**过关方式；你的实现不一样、验收标准对得上，就都是对的。注意本项目 `.clang-format` 是 LLVM 基底，C 风格转换后要带空格（`(double) count`），照抄时别改掉。

## 核心任务（L2）：能跑起来的成绩簿 {#pj-core}

**思路**：结构体放头文件（声明与定义分离的老规矩），`add` 用 `sscanf` 解析，`list` 用对齐格式出表格；Makefile 用变量 + 模式规则。

**`include/gradebook.h`**——头文件契约：include guard + 类型定义。→ 知识点：[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)「结构体基础」、[第 1 章](/01-c-basics/01-program-structure-and-compilation)（头文件放声明）

```c
#ifndef GRADEBOOK_H
#define GRADEBOOK_H

typedef struct {
    char name[32];
    int id;
    double score;
} Student;

#endif
```

**`src/main.c` 骨架**——命令循环：`fgets` 读一行、去换行、`sscanf` 拆出命令词、按命令分派。→ 知识点：[第 11 章](/01-c-basics/11-c-strings-and-libc)（`fgets` 会存换行符）、[第 12 章](/01-c-basics/12-basic-io)（`sscanf`）、[第 7 章](/01-c-basics/07-control-flow)（if-else 分派或 `switch`）

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gradebook.h"

#define MAX_STUDENTS 32
#define LINE_MAX 256

static Student book[MAX_STUDENTS];
static int count = 0;
```

**`do_add`（核心版 + 健壮版合体）**——`sscanf` 返回值检查（不足 3 项报用法）、成绩范围检查、`snprintf` 防名字溢出。注意 `%31s` 保证最多读 31 字符、留 1 格给 `\0`。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（`scanf` 返回值语义）、[第 11 章](/01-c-basics/11-c-strings-and-libc)（边界）、[第 10 章](/01-c-basics/10-arrays)（数组与越界）

```c
static void do_add(const char* args) {
    char name[32];
    int id;
    char score_str[32];
    if (count >= MAX_STUDENTS) {
        printf("成绩簿已满\n");
        return;
    }
    int got = sscanf(args, "%31s %d %31s", name, &id, score_str);
    if (got != 3) {
        printf("用法: add <名字> <学号> <成绩>\n");
        return;
    }
    int ok = 0;
    double score = parse_double(score_str, &ok);
    if (!ok || score < 0.0 || score > 100.0) {
        printf("成绩不合法: %s\n", score_str);
        return;
    }
    snprintf(book[count].name, sizeof(book[count].name), "%s", name);
    book[count].id = id;
    book[count].score = score;
    count++;
    printf("已添加 %s\n", name);
}
```

**`do_list`**——`%5d`/`%-12s`/`%6.2f` 对齐。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)「printf」一节（宽度/对齐/精度）

```c
static void do_list(void) {
    printf("%5s %-12s %6s\n", "ID", "姓名", "成绩");
    for (int i = 0; i < count; i++) {
        printf("%5d %-12s %6.2f\n", book[i].id, book[i].name, book[i].score);
    }
}
```

**`Makefile`**——变量 + 模式规则 + `.PHONY`。→ 知识点：[阶段 0 第 11 章](/00-dev-environment/12-make-basics)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude
LDFLAGS =

gradebook: src/main.o
	$(CC) $(CFLAGS) -o gradebook src/main.o $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f gradebook src/*.o

.PHONY: clean
```

## 进阶任务（L3）：统计命令 {#pj-avg}

**思路**：平均分的总分必须用 `double`——`total` 若是 `int`，$\frac{total}{count}$ 就是整数除法，$79.875$ 会被截成 $79$，然后才提升成 $79.000000$。这是第 4 章 $\frac{5}{2}$ 教训的放大版。

```c
static void do_avg(void) {
    if (count == 0) {
        printf("还没有学生\n");
        return;
    }
    double total = 0.0;               /* 关键:double,不是 int */
    for (int i = 0; i < count; i++) {
        total += book[i].score;
    }
    printf("平均分 = %.2f\n", total / (double) count);
}

static void do_max(void) {
    if (count == 0) {
        printf("还没有学生\n");
        return;
    }
    int best = 0;
    for (int i = 1; i < count; i++) {
        if (book[i].score > book[best].score) {
            best = i;
        }
    }
    printf("最高分 = %.2f (%s)\n", book[best].score, book[best].name);
}
```

→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)「隐式转换」一节（整数除法坑）

## 再进阶任务（L4）：把门装上 {#pj-gates}

**思路**：健壮性 = 每个输入点都假设「用户会敲歪」；`-Wconversion` 逼你显式化每个可能丢数据的转换；sanitizer 和格式门是最后两道。

1. 三个健壮性测试：缺参数 → 用法提示；`bad`/`101` → 成绩不合法；超长名字靠 `%31s` + `snprintf` 兜住。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（返回值是唯一可靠的校验手段）
2. `-Wconversion -Werror`：所有隐式窄化转换显式写 `(double)` 等，零警告才算过。→ 知识点：[阶段 0 第 8 章](/00-dev-environment/09-warning-flags)「`-Wconversion`」一节
3. sanitizer 会话零报告 + 格式门。→ 知识点：[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)、[阶段 0 第 17 章](/00-dev-environment/18-format-and-quality-gate)

**验证输出**：

```text
$ make
$ ./gradebook
命令: add/list/avg/max/rank/pass/quit
> add Eve
用法: add <名字> <学号> <成绩>
> add Eve 1005 bad
成绩不合法: bad
> add Eve 1005 101
成绩不合法: 101
> add Eve 1005 88.5
已添加 Eve
> list
   ID 姓名       成绩
 1005 Eve           88.50
$ # sanitizer 构建跑完整会话:零报告(见 L5 会话)
$ clang-format --dry-run --Werror src/*.c include/*.h; echo $?
0
```

## 终极挑战（L5）：徒手解析、排序与位图 {#pj-l5}

**思路**：①`parse_double` 逐字符解析：符号位 → 整数部分（`intpart = intpart * 10 + digit`）→ 小数部分（`frac += digit * scale; scale *= 0.1`），全程校验字符合法性；②选择排序：每轮挑剩余最大值换到前面；③位图：第 i 位标记第 i 个学生，`|= (1u << i)` 置位，循环右移数 1。

**`parse_double`**——不调用 `strtod`/`atof`。逐字符状态推进，`digits == 0` 或遇到非数字/非小数点即失败。→ 知识点：[第 4 章](/01-c-basics/04-float-char-const-cast)（`char` 是小整数，`'0'` 是 48）、[第 7 章](/01-c-basics/07-control-flow)（循环与提前返回）、[第 11 章](/01-c-basics/11-c-strings-and-libc)（字符串 = `\0` 结尾的 char 数组）

```c
/* L5: 徒手解析 "-89.5" 这类十进制小数,不调用 strtod/atof */
static double parse_double(const char* s, int* ok) {
    double sign = 1.0;
    const char* p = s;
    int digits = 0;
    *ok = 0;
    if (*p == '-') {
        sign = -1.0;
        p++;
    }
    double intpart = 0.0;
    while (*p >= '0' && *p <= '9') {
        intpart = intpart * 10.0 + (double) (*p - '0');
        p++;
        digits++;
    }
    double frac = 0.0;
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (*p >= '0' && *p <= '9') {
            frac += (double) (*p - '0') * scale;
            scale *= 0.1;
            p++;
            digits++;
        }
    }
    if (*p != '\0' || digits == 0) {
        return 0.0;
    }
    *ok = 1;
    return sign * (intpart + frac);
}
```

**`do_rank`（选择排序）**——每轮在 `i..count-1` 里挑最大元素换到 `i`。→ 知识点：排序算法阶段 3 细讲，这里是**教材外补充**：选择排序 = 「n 轮、每轮挑剩余最大」，时间复杂度 O(n²)，规模小够用

```c
static void do_rank(void) {
    for (int i = 0; i < count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++) {
            if (book[j].score > book[best].score) {
                best = j;
            }
        }
        if (best != i) {
            Student tmp = book[i];
            book[i] = book[best];
            book[best] = tmp;
        }
    }
    do_list();
}
```

**`do_pass`（位图统计）**——`bitmap |= (1u << i)` 置位，数 1 用「右移 + 掩码」循环。→ 知识点：[第 6 章](/01-c-basics/06-bitwise-and-shift)（标志位三件套的位图版）、[第 2 章](/01-c-basics/02-integer-types-and-sizeof)（`uint32_t` 定宽，最多标记 32 人）

```c
static void do_pass(void) {
    uint32_t bitmap = 0u;
    for (int i = 0; i < count; i++) {
        if (book[i].score >= 60.0) {
            bitmap |= (1u << i);
        }
    }
    unsigned n_pass = 0;
    for (unsigned b = bitmap; b != 0u; b >>= 1) {
        n_pass += b & 1u;
    }
    printf("及格 %u/%d 人,位图 = 0x%08X\n", n_pass, count, bitmap);
}
```

**验证输出**（sanitizer 构建下的完整会话，零报告）：

```text
$ make CFLAGS="-std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -O1 -g \
      -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
$ ./gradebook
命令: add/list/avg/max/rank/pass/quit
> add Alice 1001 89.5
已添加 Alice
> add Bob 1002 74
已添加 Bob
> add Carol 1003 96.5
已添加 Carol
> add Dave 1004 59.5
已添加 Dave
> list
   ID 姓名       成绩
 1001 Alice         89.50
 1002 Bob           74.00
 1003 Carol         96.50
 1004 Dave          59.50
> avg
平均分 = 79.88            ← double 总分,不是 79.00
> max
最高分 = 96.50 (Carol)
> pass
及格 3/4 人,位图 = 0x00000007    ← 0b0111:Alice/Bob/Carol
> rank
   ID 姓名       成绩
 1003 Carol         96.50
 1001 Alice         89.50
 1002 Bob           74.00
 1004 Dave          59.50
> quit

$ # L5:parse_double 的拒绝能力
$ ./gradebook
> add F1 2001 8a.5
成绩不合法: 8a.5        ← 非法字符,parse_double 拒了
> add F2 2002 -3.14
成绩不合法: -3.14       ← parse_double 能解析负号,但范围检查拒了
> add F3 2003 12
已添加 F3
> list
   ID 姓名       成绩
 2003 F3            12.00
```

到这里，「阶段 1 的知识点是一体的」就有了实物：一个成绩簿，结构体是数据、数组是容器、格式化是门面、`fgets`/`sscanf`/`snprintf` 是防御、位图是位运算的用武之地、sanitizer 和格式门是底线。下一个阶段，指针和动态内存会把这些数据从「写死的 32 人」里解放出来。
