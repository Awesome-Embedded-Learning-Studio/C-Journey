---
title: "阶段 2 Project：动态通讯录 dybook"
description: "指针与内存阶段的综合项目：做一个命令行动态通讯录 dybook——数据全在堆上、realloc 自动扩容、memmove 删除搬移、qsort 排序、函数指针转移表分发、全程 sanitizer 零报告。任务分热身（L1）加四层（L2~L5），参考实现独立成文件、逐段讲解。"
chapter: 2
order: 4
tags:
  - host
  - pointers
  - memory
difficulty: advanced
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 全部章节"
related:
  - "阶段 2 Homework"
  - "阶段 2 Lab"
---

# 阶段 2 Project：动态通讯录 dybook

## 项目定位

Homework 是点、Lab 是线，Project 是面：阶段 1 的成绩簿把数据写死在栈上的固定数组里，这一次我们做 `dybook`——一个命令行动态通讯录，把阶段 2 的全部家当用进去：指针别名、指针参数、`const`、字符串指针、`malloc`/`realloc`/`free`、`memmove`、函数指针与 `qsort`，最后配上 sanitizer 质量门。它和阶段 1 项目最本质的区别一句话：**数据全在堆上、容量用完了会自动长**——这是指针与动态内存真正开工的地方。

任务分「热身 + 四层」，每层都有验收标准，建议一层做完、验收对上了再做下一层。卡住了看[参考实现](project-solutions)——它是按任务分层组织的，可以只读你卡住的那一层。

### 热身（L1）：指针自检 {#pj-warm}

项目开工前，写一个 20 行的 `self_check.c` 验证四条基本功：①`*p` 是别名（改 `*p` 就是改 `x`）；②`int** pp = &p;` 之后 `**pp` 能顺着两层改到 `x`；③`&x == p` 而 `&p != p`；④`sizeof(int*)` 是 8（本机）。

**验收标准**：贴出输出；`x` 最终是 3、两个比较都打印 1。

[参考实现 →](project-solutions#pj-warm)

### 核心任务（L2）：堆上的通讯录 {#pj-core}

做一个项目目录 `dybook/`，单文件 `dybook.c` + `Makefile`（变量 + `clean`/`.PHONY`）。数据结构 `struct Contact { char name[32]; char phone[24]; };`（typedef 成 `Contact`，阶段 1 的老手艺），通讯录本体是**堆上**的动态数组：`Contact* book = malloc(4 * sizeof(Contact))`（初始容量 4，查 `NULL`），`count` 记条数。命令 `add`（`sscanf` 解析「名字 电话」）、`list`（对齐表格）、`quit`（退出前 `free` 并置 `NULL`）。命令循环用 `fgets` 读一行、去换行、`sscanf` 拆命令词、if-else 分派。

**验收标准**：`make` 全绿；`add` 三个联系人后 `list` 输出对齐表格；`quit` 正常退出（退出前 `free`）。贴出 `make` 和一次会话的完整输出。

[参考实现 →](project-solutions#pj-core)

### 进阶任务（L3）：自动扩容与删除 {#pj-grow}

两件事。①把 `add` 的容量检查换成 `ensure_capacity()`：放满就 tmp 模式 `realloc` 翻倍容量（`int* tmp` 接、查 `NULL`、成功才赋回——想清楚为什么不能直接 `book = realloc(book, ...)`），加满 5 个人验证容量 4→8。②加 `rm <名字>` 命令：找到后把后面的元素整体前移一位——这一步源和目的**重叠**，必须用 `memmove`，然后 `count--`。③加 `find <子串>` 命令：用 `strstr` 模糊查找名字（注意区分大小写，这是 strstr 的语义）。

**验收标准**：贴出 5 人加满后 `list` 的「共 5 条(容量 8)」、`rm` 之后的表格、`find` 的输出。

[参考实现 →](project-solutions#pj-grow)

### 再进阶任务（L4）：排序、健壮性与质量门 {#pj-sort}

三件事。①`sort` 命令：`qsort` 按姓名字典序排序（比较函数先把 `const void*` 转回 `const Contact*`，再 `strcmp`）。②健壮性：`add` 检查 `sscanf` 返回值（不足 2 项报用法）、`%31s`/`%23s` 限宽防越界、`snprintf` 兜底拷贝；分别用「缺参数」「超长名字」测试。③质量门：`-Wall -Wextra -Wconversion -Werror` 做到**零警告**（`-Wconversion` 会逼你把每个 `int`→`size_t` 的隐式转换显式化，比如 `memmove` 的字节数、`qsort` 的元素个数）；再用 `-fsanitize=address,undefined` 构建跑一遍完整会话，零报告。

**验收标准**：贴出两个健壮性测试的输出、零警告的编译命令、sanitizer 会话零报告。

[参考实现 →](project-solutions#pj-sort)

### 终极挑战（L5）：转移表与原地去重 {#pj-l5}

三件事。①把命令分发的 if-else 链换成**函数指针转移表**：`typedef int (*CmdFn)(const char* args);`，一张「命令名 → 函数指针」的表（`struct { const char* name; CmdFn fn; }` 数组），`strcmp` 查表、命中就调——加命令只加表项、不动主循环（第 9/10 章的「数据驱动」）。②`dedupe` 命令：先排序让同名相邻，再用**双指针原地去重**（一个「读指针」扫全表、一个「写指针」收结果，`book[write] = book[read]` 用的是结构体整体赋值——阶段 1 的手艺；这个读写指针法是**教材外补充**，阶段 3 才细讲，这里直接照思路写）。③全套 sanitizer 构建下跑完整会话（含 `rm`、`sort`、重复 `add`、`dedupe`），退出码 0、零报告。

**验收标准**：贴出转移表版会话（含 `dedupe` 前「两条 Alice」→ 去重后一条）、sanitizer 零报告。

[参考实现 →](project-solutions#pj-l5)

## 提交物清单

项目目录（`dybook.c`、`Makefile`、`self_check.c`）+ 各层终端记录 + 200 字以内小结：说说这个项目里哪一处让你对「指针 + 堆 = 数据活起来」体会最深。
