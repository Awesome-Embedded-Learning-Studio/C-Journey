---
title: "阶段 1 Project：学生成绩管理器"
description: "阶段 1 综合项目：做一个命令行学生成绩管理器——结构体数组、格式化表格、健壮输入、sanitizer 与格式门，最后挑战徒手解析小数（不用 strtod）、排序排名与位图统计。任务分四层，难度 L1~L5。"
chapter: 1
order: 4
tags:
  - host
  - struct
  - bit-manipulation
difficulty: intermediate
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 1 全部章节"
related:
  - "阶段 1 Homework"
  - "阶段 1 Lab"
---

# 阶段 1 Project：学生成绩管理器

## 项目定位

把阶段 1 的家当全部用进一个真实的小程序：`gradebook`——一个命令行学生成绩管理器。结构体存学生、数组存全班、`printf` 格式出对齐表格、`fgets`/`sscanf` 做健壮输入、`snprintf` 防溢出、位运算做统计位图、sanitizer 和格式门守着质量。任务分四层，一层一层往上盖；卡住了看[参考实现](project-solutions)，它按层组织，可以只读你卡住的那层。

## 任务分层

### 核心任务（L2）：能跑起来的成绩簿 {#pj-core}

**L1 热身**：先把 `gradebook.h` 的 `Student` 结构体和三个空函数骨架搭起来——不实现逻辑，只求 `gcc -c` 零警告通过。

实现命令 `add`、`list`、`quit`。数据结构：`struct Student { char name[32]; int id; double score; };`（放 `include/gradebook.h`），固定数组 32 人。`add` 用 `sscanf` 解析「名字 学号 成绩」，`list` 打印一张对齐的表格（`%5d`/`%-12s`/`%6.2f` 那套）。配一个 Makefile（变量 + 模式规则 + `clean`/`.PHONY`）。

**验收标准**：`make` 全绿；`add` 三个学生后 `list` 输出对齐表格；`quit` 正常退出。贴出 `make` 和一次会话的完整输出。

[参考实现 →](project-solutions#pj-core)

### 进阶任务（L3）：统计命令 {#pj-avg}

加两个命令：`avg`（平均分——**总分用什么类型？**想想第 4 章的整数除法坑，验证输出应是 `79.88` 而不是 `79.00` 这种）和 `max`（最高分学生）。

**验收标准**：贴出 `avg`/`max` 输出；一句话说明你的总分声明为什么能躲开整数除法坑。

[参考实现 →](project-solutions#pj-avg)

### 再进阶任务（L4）：把门装上 {#pj-gates}

三件事。①健壮性：`add` 检查 `sscanf` 返回值（不足 3 项要报用法错误）、成绩范围 0~100 之外要拒绝、名字用 `%31s` 和 `snprintf` 防溢出；分别用「缺参数」「成绩 101」「超长名字」测试。②编译用 `-Wall -Wextra -Wconversion -Werror` 做到**零警告**（这条最难，`-Wconversion` 会逼你把每个隐式转换显式化）。③质量门：`-fsanitize=address,undefined` 构建跑一遍完整会话零报告，`clang-format --dry-run --Werror` 查 `src/*.c include/*.h` 退出码 0。

**验收标准**：贴出三个健壮性测试的输出、`-Wconversion -Werror` 零警告的编译命令、sanitizer 会话零报告、格式门退出码 0。

[参考实现 →](project-solutions#pj-gates)

### 终极挑战（L5）：徒手解析、排序与位图 {#pj-l5}

三件挑战，全部用阶段 1 的知识完成（早期阶段 L5＝「用该阶段知识可解的最难问题」，档位口径见[练习总览](/exercises/)）。①**徒手 `parse_double`**：不调用 `strtod`/`atof`，自己解析 `"89.5"` 这类字符串（支持负号、整数部分、小数部分，逐字符校验，非法输入返回失败标志）——把 `add` 命令的成绩解析换成它。②**`rank` 命令**：按成绩降序重排后打印（排序算法阶段 3 才细讲，这里用选择排序或冒泡——**教材外补充**，参考实现会写清原理）。③**`pass` 命令**：用 `uint32_t` 位图标记「哪些学生及格」（≥60），`|= (1u << i)` 置位、循环数出及格人数，打印位图十六进制和统计。

**验收标准**：贴出 `add` 用 `parse_double` 接受 `89.5`、拒绝 `8a.5` 的输出；`rank` 的降序表；`pass` 的位图和统计（4 人里 3 人及格时应是 `0x00000007`、`及格 3/4 人`）。全套在 sanitizer 构建下零报告。

[参考实现 →](project-solutions#pj-l5)

## 提交物清单

项目目录（`src/`、`include/`、Makefile）+ 各层终端记录 + 200 字以内小结：说说这个项目里哪一处让你对「阶段 1 的知识点是一体的」体会最深。
