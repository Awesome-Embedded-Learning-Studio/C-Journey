---
title: "阶段 5 Project：mini shell"
description: "阶段 5 综合项目：从零写一个命令行 mini shell（cjsh）——fork/exec/wait 骨架、内置命令、管道与重定向、后台作业与 SIGCHLD 收尸，四层任务对应 L2~L5（改编自 CSAPP shell lab 的作业控制简化版，如实标注），逐步盖上本阶段的进程、fd、信号与文件 IO 全部家当。"
chapter: 5
order: 4
tags:
  - host
  - system-programming
  - posix
  - ipc
  - concurrency
difficulty: advanced
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 第 1~6 章"
related:
  - "阶段 5 Homework"
  - "阶段 5 Lab"
---

# 阶段 5 Project：mini shell

## 项目定位

把阶段 5 的家当全部用进一个真实的小程序：`cjsh`——一个命令行 mini shell。`fork + execvp + waitpid` 执行外部命令、`chdir`/`getcwd` 做内置命令、`pipe + dup2` 搭管道与重定向、`SIGCHLD + WNOHANG` 收后台作业的尸。任务分四层，一层一层往上盖；卡住了看[参考实现](project-solutions)，它按层组织，可以只读你卡住的那层。

项目结构按工程老规矩来：`include/shell.h`（契约）、`src/main.c`（REPL + 解析 + 执行 + 信号）、`src/builtin.c`（内置命令）、`Makefile`（变量 + 模式规则 + `clean`/`.PHONY`）。词法做最简版：按空白切词，认 `|`、`>`、`<`、`&` 四类元字符，**不支持引号**（引号处理需要完整词法状态机，教材没讲，这里如实划界）。验收会话用 `printf '...' | ./cjsh` 喂命令，非交互时不打提示符（`isatty` 判断），日志干净。

## 任务分层

### 核心任务（L2）：能跑起来的 REPL {#pj-core}

实现：读一行 → 按空白切 argv → `fork` → 子进程 `execvp` → 父进程 `waitpid` 后报告状态。具体要求：①exec 失败的子进程必须 `perror + _exit(127)`；②父进程用 `WIFEXITED`/`WIFSIGNALED` 解析退出状态——正常退出码非 0 时打印「退出码 N」、被信号杀死时打印「被信号 N 杀死」、正常 0 退出则安静；③`exit` 内置命令退出 shell；空行忽略。配好 Makefile，`-Wall -Wextra -Werror` 零警告。

本层第一小步是 L1 热身：空 REPL 循环能编译、能读行、能退出——先把骨架立住，再往里填 fork/exec。

**验收标准**：贴出 `make` 和一次会话：`echo hello` 出字、`no-such-command-xyz` 报错并打印退出码 127、一个自杀程序被报告「被信号 N 杀死」、`exit` 正常退出，shell 退出码 0。

[参考实现 →](project-solutions#pj-core)

### 进阶任务（L3）：内置命令 {#pj-avg}

加三个内置命令：`cd`（无参回 `HOME`，`chdir` 失败 `perror`）、`pwd`（`getcwd`）、`wait`（阻塞 `waitpid(-1)` 到 `ECHILD` 为止）。内置命令**只在单命令、无重定向、非后台时生效**，且在 **shell 进程里执行**——`cd` 放子进程里跑，退出后 shell 的 cwd 纹丝不动，这正是内置命令和外部命令的本质区别。

**验收标准**：贴出会话：`pwd` → `cd /tmp` → `pwd` 变了 → `cd /` → `pwd` 变回来；一句话说清「为什么 `cd` 必须内置」。

[参考实现 →](project-solutions#pj-avg)

### 再进阶任务（L4）：管道与重定向 {#pj-gates}

实现 N 段管道流水线 `A | B | C` 和单段重定向 `> file`、`< file`：按 `|` 切段，相邻段之间 `pipe`，每段的子进程按位置 `dup2` 接好 stdin/stdout；父进程关掉自己那两份管道 fd（**漏关 = 流水线挂死等 EOF**）；重定向在子进程里 `open + dup2`。验收三个组合：`cat /etc/hostname | wc -c`（管道）、`echo hello > f; cat f`（重定向写读）、`ls /nonexistent | wc -l`（失败命令在管道里的真实行为——stderr 去哪了、退出码以哪段为准）。

**验收标准**：贴出三个组合的会话输出，逐条解释；说明为什么父进程的管道 fd 非关不可、为什么报告退出码要看最后一段。

[参考实现 →](project-solutions#pj-gates)

### 终极挑战（L5）：后台作业与作业控制（改编自 CSAPP shell lab，如实标注） {#pj-l5}

改编自 CSAPP 第八章 shell lab（tsh）的作业控制要求，简化为本阶段知识可达的范围——**不做**终端进程组控制（`tcsetpgrp`/前台进程组是教材外内容，参考实现里只注释说明概念）。要求：①命令行末尾 `&` 把整条流水线抛后台：登记进作业表（pid + 命令行 + 运行状态），打印 `[作业号] pid` 后立刻回来读下一条命令；②`jobs` 内置命令列出作业表（运行中 / 已完成(退出码) / 被信号杀死）；③**SIGCHLD 收尸**：shell 装 `SIGCHLD` 处理器（`SA_RESTART`），处理器只设 `volatile sig_atomic_t` 标志（`printf` 不是 async-signal-safe 的），主循环看到标志后用 `waitpid(-1, &st, WNOHANG)` 循环收尸、更新作业表——shell 一边等输入一边把死掉的后台作业捞回来，不留僵尸；④shell 自己忽略 `SIGINT`，但**子进程 fork 后要还原成默认**（否则子进程继承「忽略」、`Ctrl-C` 就杀不动它了）。

**验收标准**：贴出会话：`sleep 0.4 &` 立刻出 `[1] pid`；`jobs` 先见「运行中」再见「已完成(退出码 0)」；`ls /definitely-not-there &` 见「已完成(退出码 2)」；`wait` 阻塞到全部结束；全程退出码 0、无僵尸。说明作业表和 SIGCHLD 处理器各自负责什么、为什么要分开。

[参考实现 →](project-solutions#pj-l5)

## 提交物清单

项目目录（`src/`、`include/`、`Makefile`）+ 各层终端记录 + 200 字以内小结：说说这个项目里哪一处让你对「阶段 5 的知识点是一体的」体会最深。
