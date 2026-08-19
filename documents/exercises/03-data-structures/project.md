---
title: "阶段 3 Project：电话簿双引擎"
description: "阶段 3 综合项目：做一个命令行电话簿——单链表起步，升级到 BST 有序引擎，再挂上哈希 O(1) 索引组成双引擎，最后用 bench 计时与随机压力测试收口。任务分四层，难度 L1~L5（L1 为热身小步），参考实现分文件逐段讲解并附真实运行输出。"
chapter: 3
order: 4
tags:
  - host
  - data-structures
  - algorithm
difficulty: advanced
reading_time_minutes: 35
platform: host
c_standard: [11]
prerequisites:
  - "阶段 3 全部章节（第 1~12 章）"
related:
  - "阶段 3 Homework"
  - "阶段 3 Lab"
---

# 阶段 3 Project：电话簿双引擎

## 项目定位

把阶段 3 的家当全部用进一个真实的小程序：`phonebook`——一个命令行电话簿管理器。联系人 `Contact { int id; char name[32]; char phone[16]; }`，id 是键、姓名电话是卫星数据。这个项目最妙的地方在于**一个产品、三代引擎**：先拿单链表让它跑起来（第 1 章），再把存储换成 BST 换上有序能力（第 7 章），最后挂上哈希索引组成「BST 管有序、哈希管 O(1) 查找」的双引擎（第 8 章），收尾用 `clock()` 基准（第 12 章）和随机压力测试检验所有引擎。任务分四层，一层一层往上盖；卡住了看[参考实现](/exercises/03-data-structures/project-solutions)，它按层组织，可以只读你卡住的那层。

工程形态照阶段 1 Project 的老规矩：`include/` 放头文件、`src/` 放实现、Makefile 变量 + 模式规则，`-Wall -Wextra -Wconversion -Werror` 零警告起步，最后 `clang-format --dry-run --Werror` 和 sanitizer 把门。

## 任务分层

### 核心任务（L2）：能跑起来的电话簿 {#pj-core}

**第一步（L1）**：搭骨架——定义 `Contact` 结构，写一个最小程序：手动造 3 条联系人（`{{1001, "Alice", "..."}, ...}`）并打印。**第二步（L2）**：把骨架换成真正的引擎。用单链表（第 1 章 head/tail 双指针、尾插）存联系人，命令循环支持 `add`（`sscanf` 解析「id 名字 电话」三项，缺参要报用法）、`find`（线性遍历，找到打印、找不到提示）、`list`（遍历打印全部）、`quit`。配一个 Makefile（变量 + 模式规则 + `clean`/`.PHONY`）。

**验收标准**：`make` 全绿；`add` 三个联系人后 `find` 命中一个、`find` 一个不存在的 id、`list` 按**插入顺序**打印三行；`quit` 正常退出。贴出 `make` 和一次会话的完整输出。

[参考实现 →](/exercises/03-data-structures/project-solutions#pj-core)

### 进阶任务（L3）：换成 BST 引擎 {#pj-bst}

把存储引擎从单链表换成 **BST**（第 7 章，按 id 左小右大）。命令升级为 `add`（插入）、`find`（查找）、`del`（删除——**三种情况都要支持**：叶子、单子、双子；顺便想想删不存在的 id 会怎样）、`list`（中序遍历——现在它天然按 id 升序打印）、`height`（打印树高）、`quit`。用一组**乱序** id 建表（比如 `5 3 8 1 4 7 9`），演示 `list` 有序、`del` 逐个删三种情况、每删一次 `list` 验证中序仍然有序。

**验收标准**：贴出会话输出；一句话说清 `del` 里「双子节点」那一手化归（找后继 → 拷值 → 递归删后继）。

[参考实现 →](/exercises/03-data-structures/project-solutions#pj-bst)

### 再进阶任务（L4）：哈希索引 + 质量门 {#pj-gates}

三件事。①**哈希引擎**（第 8 章）：链地址 + **动态桶数组**（`calloc` 分配、负载因子超 0.75 沿质数表扩容并 rehash 复用节点），把 id 索引到联系人——`find` 从此走哈希 O(1)，`list` 仍走 BST 中序（有序这件事哈希给不了）。双引擎**必须同步**：`add` 两个都插、`del` 两个都删。②**健壮性**：`add` 缺参数报用法、重复 id 拒绝、id ≤ 0 拒绝；分别用「缺参数」「重复 id」「非法 id」测试。③**质量门**：`-Wall -Wextra -Wconversion -Werror` 零警告编译；`clang-format --dry-run --Werror` 查 `src/*.c include/*.h` 退出码 0；`-fsanitize=address,undefined` 构建跑一遍完整会话零报告。

**验收标准**：贴出三个健壮性测试的输出、零警告编译命令、`clang-format` 退出码 0、sanitizer 会话零报告（含 exit 0）。

[参考实现 →](/exercises/03-data-structures/project-solutions#pj-gates)

### 终极挑战（L5）：bench 与随机压力 {#pj-l5}

两件挑战。①**`bench` 命令**：用 `clock()`（第 12 章）把三代引擎（链表 / BST / 哈希）拉到同一批联系人上计时——n = 2000/5000/10000，各造一份数据、重复 1000 次 `find`，打印三种耗时与相对链表的倍数。为保证公平，BST 的插入顺序要打乱（**教材外补充**：用 rand 的洗牌打乱插入顺序，避免升序退化；洗牌与计时方法本身不属教材内容，数据结构均为教材内容）。②**`stress` 命令**：`srand(42)` 固定种子，对双引擎跑 1000 次随机 add/find/del，每次 find 都交叉验证「哈希命中 == BST 命中」，每 100 次自检「BST 中序有序 + 双引擎条数一致」（**教材外补充**：随机压力测试手法；数据结构均为教材内容）。全程 sanitizer 构建下零报告。

**验收标准**：贴出 `bench` 的三组计时输出；`stress` 的 10 次自检与最终统计；说明为什么链表那列随 n 增长、BST/哈希两列几乎不动。

[参考实现 →](/exercises/03-data-structures/project-solutions#pj-l5)

## 提交物清单

项目目录（`src/`、`include/`、Makefile）+ 各层终端记录 + 200 字以内小结：说说这个项目里哪一处让你对「阶段 3 的知识点是一体的」体会最深。
