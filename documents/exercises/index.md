---
title: "练习与作业总览"
description: "C-Journey 配套练习体系:Homework / Lab / Project 三类作业,五档难度从计算机三级到国际竞赛挑战级,题题附详细解答与知识点链接。"
chapter: 99
order: 1
tags: [meta]
difficulty: beginner
reading_time_minutes: 8
platform: host
---

# 练习与作业总览

## 这套练习解决什么问题

教程正文每一章都在做同一件事:手把手陪你走一遍,从工具链体检一路走到 epoll 与 socket。但「看懂」和「做得出来」之间隔着一段路,这段路只能靠你自己走。这里就是这段路——按阶段组织的作业、实验与项目,让你每读完一个阶段之后,有机会把教材里的知识真正用起来。

所有题目遵循同一条原则:题面干净不剧透,参考答案单独成文件。每道题的解答一步步展开,每一步都标注它用到的知识点并链接回教材对应章节,卡住了随时可以顺着链接回原文补课。

## 三类作业:Homework / Lab / Project

**Homework(课后练习)** 按章出题,每章两道——一道基础、一道进阶,再加一两道跨章综合题。题量不大,目的是读完一章就趁热把这一章的概念转成自己的输出。题目都做「变式」处理:换场景、换推理方向,照抄教材例题是抄不出答案的。

**Lab(动手实验)** 每阶段一个,走「目标 → 步骤 → 命令 → 验收标准」的流程,通常四到六步,最后附一道挑战任务。它比 Homework 更接近真实工程:你要真开终端、真敲命令、真看输出,拿验收标准自己判断过没过。

**Project(综合项目)** 每阶段一个,是这一阶段全部知识的一次总排练。任务分层:先做核心功能让它跑起来,再做进阶扩展,最后是留给想折腾的人的终极挑战。参考实现拆成一个个文件逐段讲解,按工程的方式组织。

## 五档难度

每份作业内部都覆盖全部五档,档位标注在每道题上。三份作业的整体难度依次递进:Homework 以 L1~L3 为主,Lab 以 L2~L4 为主,Project 以 L3~L5 为主。

| 档位 | 对标 | 出题风格 |
| --- | --- | --- |
| L1 | 全国计算机等级考试三级 | 计算机通识(网络、数据库等)加 C 基础编程,考「知不知道」 |
| L2 | 全国计算机等级考试四级 | 计组、操作系统、数据结构、软件工程的综合风格题,考「串不串得起来」 |
| L3 | CS61A→B→C 作业与 lab、408 真题 | 国外经典课程作业改编与考研真题风格,考「写不写得出来」 |
| L4 | SICP 练习、CSAPP 练习 | 汇编阅读、缓存模拟、并发与信号,考「想不想得透」 |
| L5 | ICPC / IOI 等竞赛真题改编 | 挑战级,金牌难度。早期阶段的 L5 是「用该阶段知识可解的最难问题」;改编来源如实标注在题面 |

一句话总结这个梯度:从「知不知道」到「想不想得透」。L5 是给想看看山顶长什么样的人准备的。

## 怎么用这套练习

建议先自己做完再对答案,而不是边看答案边做。卡住的时候,先回到题目标注的知识点链接去读教材,读完再试;实在做不出来,再打开答案文件,从「解题思路」看起,逐步对照自己的思路错在哪一步。答案文件的每一步都带知识点链接,对到哪一步卡壳,就点哪一步的链接回去补。

做 Lab 和 Project 时,把验收标准当真正的门禁——不是「大概跑通了」,而是输出和验收标准逐条对得上。教材全篇的代码都在 gcc 16 + clang 22 下真跑过,你的环境只要跟着阶段 0 搭好,输出就应该对得上。

## 各阶段练习进度

| 阶段 | Homework | Lab | Project |
| --- | --- | --- | --- |
| 阶段 0 · 开发环境与编译 | [已上线](/exercises/00-dev-environment/homework)（37 题） | [已上线](/exercises/00-dev-environment/lab) | [已上线](/exercises/00-dev-environment/project) |
| 阶段 1 · C 语言基底 | [已上线](/exercises/01-c-basics/homework)（29 题） | [已上线](/exercises/01-c-basics/lab) | [已上线](/exercises/01-c-basics/project) |
| 阶段 2 · 指针与内存 | [已上线](/exercises/02-pointers-memory/homework)（27 题） | [已上线](/exercises/02-pointers-memory/lab) | [已上线](/exercises/02-pointers-memory/project) |
| 阶段 3 · 数据结构与算法 | [已上线](/exercises/03-data-structures/homework)（27 题） | [已上线](/exercises/03-data-structures/lab) | [已上线](/exercises/03-data-structures/project) |
| 阶段 4 · 工程化与质量门 | [已上线](/exercises/04-engineering/homework)（35 题） | [已上线](/exercises/04-engineering/lab) | [已上线](/exercises/04-engineering/project) |
| 阶段 5 · 系统编程 | [已上线](/exercises/05-system-programming/homework)（31 题） | [已上线](/exercises/05-system-programming/lab) | [已上线](/exercises/05-system-programming/project) |
