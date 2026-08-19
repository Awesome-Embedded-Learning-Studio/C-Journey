---
title: "阶段 5 课后练习（Homework）"
description: "系统编程阶段的课后练习：14 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战（共享内存乒乓基准，改编自 MIT 6.828 pingpong）。难度覆盖 L1~L5，题目都做了变式处理，参考答案独立成文件、逐步解答附知识点链接，全部输出在 WSL Arch（gcc 16.1.1）真实运行得到。"
chapter: 5
order: 0
tags:
  - host
  - system-programming
  - posix
  - ipc
  - socket
difficulty: intermediate
reading_time_minutes: 25
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 全部章节（第 1~14 章）"
related:
  - "阶段 5 Lab：一个 socket 服务的完整调试"
  - "阶段 5 Project：mini shell"
---

# 阶段 5 课后练习（Homework）

## 引言

这里的题按章组织，每章两道——一道基础、一道进阶，最后是两道跨章综合和一道 L5 挑战。每道题都标注了难度档位（L1~L5，见[练习总览](/exercises/)）和它涉及的章节；题目都是「变式」，换场景、换推理方向，照抄教材例题抄不出答案，得真动手在终端里跑。

答案在独立的[参考答案](homework-solutions)文件里，按题号一一对应，每步解答都带知识点链接。建议先把一章的题做完，再翻答案对照。本阶段所有题目都以 Linux 为前提（教材第 1 章就交代过：走出 ISO C 之后，平台就是 WSL2 + glibc），代码一律 `#define _POSIX_C_SOURCE 200809L` 起步、`-std=c11 -Wall -Wextra` 编译；涉及共享内存/信号量的题记得加 `-lrt`。

## 5.1 文件 IO 与 fd

### 5.1-A {#hw-5-1-a}

难度 **L1** · 涉及[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)

写一个程序，先**动笔预测**再真跑：①依次打开文件 A、B（`O_CREAT|O_WRONLY`），打印两个 fd；②这次**关的是 B 不是 A**（教材例子里关的是 A），再开文件 C，打印 fd；③**不关** A、C，把同一个文件连续 `open` 三次，打印三个 fd；④最后 `dup(0)`，打印它返回几。把四组预测和真实输出对齐，逐一用「最小空闲号」规则解释——尤其想清楚第 ③ 步为什么不是从 3 开始。注意：某些终端环境会给进程继承多余的 fd（比如占着 5 号的 `/dev/ptmx`），如果你的预测对不上，先用 `close(5)` 把实验环境归零再试，并说明这条规则为什么能救场。

[参考答案 →](homework-solutions#hw-5-1-a)

### 5.1-B {#hw-5-1-b}

难度 **L3** · 涉及[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)、[第 5 章：信号](/05-system-programming/05-signals)

教材给了 `write_all`，这次写它的读侧兄弟 `read_all` 并让它**同时吃下三个坑**：父子管道，子进程延迟 1.2 秒后只写 100 字节就关写端；父进程装一个**不带 `SA_RESTART`** 的 `SIGALRM` 处理器、`alarm(1)`，然后 `read_all(4096)` 读管道，读到 EOF 为止。你的 `read_all` 必须：处理短读（`read` 返回小于请求量时继续凑）、处理 `EINTR`（被信号打断就重试而不是报错）、把 `read` 返回 0 当 EOF。程序统计并打印：共收到多少字节、外层调用了几次 `read_all`、`read` 系统调用一共发生了几次、其中短读几次、被 SIGALRM 打断了几次。**先预测**这几个数字再真跑对照，尤其解释「read 系统调用次数」为什么比「外层调用次数」多——短读和 EOF 分别藏在哪里。

[参考答案 →](homework-solutions#hw-5-1-b)

## 5.2 进程的诞生（fork）

### 5.2-A {#hw-5-2-a}

难度 **L2** · 涉及[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)

教材演示的是 fork 一次分父子。变式：**连续 fork 两次**，两次之间不做任何 `if (pid == 0)` 区分。在两次 fork 之后，每个进程打印一行「fork 前的原始 pid + 自己现在的 pid + ppid」。①动笔画出进程树，数一数总共会打印**几行**、每一行的 ppid 指向谁；②真跑验证（把输出排序后更容易核对）；③解释为什么两次 fork 不是 3 个进程而是 4 个；④每个进程最后都要把自己可能的子进程 `waitpid` 收掉，说明不这么做的后果。

[参考答案 →](homework-solutions#hw-5-2-a)

### 5.2-B {#hw-5-2-b}

难度 **L3** · 涉及[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)

教材的两个 stdio 缓冲陷阱是分开演示的，这题把它们和「终端 vs 重定向」串成一场对照实验。写一个程序：fork 前 `printf("PRE-")`（无 `\n`），子进程打印两行 `child-1`/`child-2`，父进程 `wait` 后打印 `POST`。用 `mode` 参数控制三档：0 = 什么都不防；1 = fork 前 `fflush(stdout)`；2 = fork 前 `fflush` **且**子进程用 `exit(0)` 而不是 `_exit(0)`。然后跑四场：mode 0 直连终端、mode 1 直连终端、mode 0 重定向到文件、mode 2 重定向到文件（直连终端那两场可以用 `script -qec` 开个伪终端，或者真的在交互终端里跑）。对每一场，**先预测** `PRE-` 出现几次、子进程的两行还在不在，再真跑贴输出，解释四场各自的现象分别由哪一层缓冲（行缓冲/全缓冲）和哪个退出函数（`_exit`/`exit`）造成。

[参考答案 →](homework-solutions#hw-5-2-b)

## 5.3 exec 家族与 wait

### 5.3-A {#hw-5-3-a}

难度 **L2** · 涉及[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)、[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)

教材验证了 exec 后 pid 不变、argv[0] 可以乱起名。这题换两个方向：**环境变量裁剪**和 **fd 表保留**。写一个「靶子」程序，打印自己的 argv、`getenv("HOME")`、`getenv("PATH")`，再尝试从 **fd 5** 读一段文字打印。父进程这边：开一个文件（注意 `O_RDWR`，只写打开的话读会撞 EBADF），`dup2` 钉到 fd 5 上并写进一行内容、`lseek` 回开头；`setenv` 设好 HOME 和 PATH；然后 fork 子进程用 **`execve` 自传一份环境**（只有 `HOME=/tmp/only-home`，没有 PATH）。①预测靶子程序打印的 HOME、PATH 各是什么，fd 5 能不能读到内容；②真跑对照；③分别说明「环境被换掉」和「fd 表被保留」各自对应 exec 语义的哪一半。

[参考答案 →](homework-solutions#hw-5-3-a)

### 5.3-B {#hw-5-3-b}

难度 **L3** · 涉及[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)

教材演示了两个子进程各一种死法。这题 fork **四个**子进程，四种死法：`exit(0)`、`exit(5)`、`exit(300)`、`raise(SIGSEGV)`。父进程用 `waitpid(-1, ..., 0)` 循环收尸直到 `errno == ECHILD`，对每个死掉的子进程打印它的 pid 和死法（正常退出码 / 被哪个信号杀死）。①先预测 `exit(300)` 取出来的退出码是多少、为什么；②真跑观察：四个子进程被 `wait` 收到的**顺序**和 fork 顺序一致吗？如果多跑几次顺序变了，说明原因（教材讲过「先死先收」）；③为什么判断死法必须走 `WIFEXITED`/`WIFSIGNALED` 宏、不能直接读 `status` 整数值。

[参考答案 →](homework-solutions#hw-5-3-b)

## 5.4 守护进程与孤儿

### 5.4-A {#hw-5-4-a}

难度 **L2** · 涉及[第 4 章：守护进程与孤儿](/05-system-programming/04-daemons-and-orphans)

教材演示的是一级孤儿（父退子活）。这题做**两级**：主进程 fork 出「中间子」后立刻退出，中间子再 fork 出「孙子」后也立刻退出，孙子睡 2 秒后打印自己的 pid 和 ppid。①按经典 Unix 模型，孙子的 ppid 应该是几？②真跑看本机实际是多少——如果**不是 1**，用教材里讲过的 subreaper 机制解释收养者是谁；③说明为什么这个孤儿孙子**不会**变成长期僵尸（谁负责给它收尸）。注意孙子在睡醒前是孤儿的孤儿，观察它最后一行输出的时机。

[参考答案 →](homework-solutions#hw-5-4-a)

### 5.4-B {#hw-5-4-b}

难度 **L3** · 涉及[第 4 章：守护进程与孤儿](/05-system-programming/04-daemons-and-orphans)、[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)

把教材的 daemonize 和「单实例守护」合成一个程序：主进程先用 `O_CREAT|O_WRONLY|O_EXCL` 抢一个 pid 文件（抢不到就打印 errno 退出），抢到后写入自己的 pid、走完整 daemonize（fflush、两次 fork、setsid、chdir `/`、0/1/2 重定向到 `/dev/null`），然后往日志文件写 3 个 tick（pid/ppid/sid）后清理 pid 文件退出。①后台跑第一个实例，0.6 秒后跑第二个实例——第二个实例会怎样？贴出它的输出和退出码；②等第一个实例的 daemon 跑完，贴出日志文件的完整内容，逐项核对：daemon 的 pid 和启动者一样吗？ppid 是几（会是 1 吗）？sid 是多少、和谁相等？③回答：既然 0/1/2 都指向 `/dev/null` 了，为什么日志内容还能被我们看到——解释 daemon 里 `printf` 的下场和日志文件的区别。

[参考答案 →](homework-solutions#hw-5-4-b)

## 5.5 信号

### 5.5-A {#hw-5-5-a}

难度 **L2** · 涉及[第 5 章：信号](/05-system-programming/05-signals)

教材的处理器只设标志。这题把三件小事塞进一个程序：①装一个 `SIGUSR1` 处理器，里面只干两件事——给 `volatile sig_atomic_t` 计数器加一、**故意做一次失败的 `open`** 把 errno 搅脏；主流程先把 `errno` 设成 7，然后 `raise` 三次加 `kill(getpid(), SIGUSR1)` 两次，最后打印「共收到几次」和「errno 还是不是 7」。②解释计数器为什么是 5、errno 为什么没被搅脏（处理器里存/恢复 errno 的那两行是干什么的）。③试着给 **SIGKILL** 装处理器——`sigaction` 会返回什么？贴出 errno，解释为什么 SIGKILL 装不上。

[参考答案 →](homework-solutions#hw-5-5-a)

### 5.5-B {#hw-5-5-b}

难度 **L3** · 涉及[第 5 章：信号](/05-system-programming/05-signals)

教材演示了「不设 `SA_RESTART` 时 read 被 SIGALRM 打断返回 EINTR」。这题做**同场景双机对照**：同一个程序用 `mode` 参数切换 `sa_flags`（0 或 `SA_RESTART`）。父子管道，子进程 2 秒后才写 9 字节「late-data」；父进程 `alarm(1)` 后阻塞 `read`。①mode 0（无重启）：预测 read 什么时候返回、返回什么、errno 是什么，然后**循环重试**把数据补回来，证明「数据没丢、只是被打断」；②mode 1（`SA_RESTART`）：预测 read 什么时候返回、返回什么；③两档各真跑一遍，用 `clock_gettime(CLOCK_MONOTONIC)`（教材外补充：POSIX 单调时钟，用于量毫秒级耗时）打印 read 实际耗时，对比「1000ms 被踢回」和「2000ms 拿到数据」两条时间线，说明内核自动重启到底帮你省了什么。

[参考答案 →](homework-solutions#hw-5-5-b)

## 5.6 IPC 上（pipe 与 FIFO）

### 5.6-A {#hw-5-6-a}

难度 **L1** · 涉及[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)

教材讲了「父子都要关掉不用的那一端」，这题把后果**计时**给你看。写程序：父 fork 子后立刻关掉自己的写端，子进程用 `mode` 参数决定自己那份写端怎么处理——mode 0 是「bug 版」：**握着写端睡 3 秒**才关；mode 1 是「修好版」：fork 后立刻关掉写端再睡 3 秒。父进程在 `read` 前后各取一次单调时钟，打印「read 返回 0 且阻塞了多少秒」。①先预测两个 mode 的阻塞时长各是多少；②真跑对照；③解释 mode 0 里那个 3 秒是从哪来的——为什么「还有一个进程握着写端」就永远等不到 EOF。

[参考答案 →](homework-solutions#hw-5-6-a)

### 5.6-B {#hw-5-6-b}

难度 **L3** · 涉及[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)

教材只演示了 `PIPE_BUF=4096` 这个数值，这题造一个**真·多写者交错**的场景来验证原子性边界。fork 两个写者（A、B）并发往同一管道各写 40 条消息：A 写 `'A'` 填充、B 写 `'B'` 填充，每条消息之间只歇 1 毫秒；读端**故意慢慢读**（每次只拿 2048 字节还歇 1 毫秒，让管道经常接近写满）。用 `mode` 参数切两档：消息长度 8（≤ PIPE_BUF）和 8192（> PIPE_BUF）。读完全部字节后扫描整段流：统计纯色 run 的个数，以及**长度不是消息长度整数倍**的「破整 run」个数。①先预测两档各自的破整 run 数量级（0 个还是几十上百个），说明预测依据；②真跑贴输出；③如果 8 字节那档破整 run 为 0、8192 那档出现大量破整 run，解释「单次 write 的原子性边界」到底保了什么、没保什么——为什么大消息会在一半的地方被别的写者插进来。

[参考答案 →](homework-solutions#hw-5-6-b)

## 5.7 IPC 下（共享内存与信号量）

### 5.7-A {#hw-5-7-a}

难度 **L2** · 涉及[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)、[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)

教材在共享内存里传单字符。这题传**结构体**，并把 `MAP_SHARED` 和 `MAP_PRIVATE` 做成同程序内的对照：定义 `struct shared { int done; char msg[64]; }`，用 `shm_open + ftruncate + mmap` 映射。第一轮用 `MAP_SHARED`：父写 `msg`、`done=0`，fork 后子进程读 `msg` 打印、把 `done` 改成 99，父 `wait` 后打印自己看到的 `done`。第二轮换一个名字的新对象、用 `MAP_PRIVATE` 重跑同样的流程。①先预测两轮父进程看到的 `done` 各是几；②真跑对照；③解释 `MAP_PRIVATE` 为什么让共享退化成「各改各的副本」——这和第 2 章的 COW 是什么关系。别忘了 `shm_unlink` 清理。

[参考答案 →](homework-solutions#hw-5-7-a)

### 5.7-B {#hw-5-7-b}

难度 **L4** · 涉及[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)

教材的信号量只演示了「一对一通知」，这题把它推到经典的**生产者-消费者环形缓冲**：共享内存里放 8 个 `int` 槽位，两个命名信号量——`empty` 初始 8、`full` 初始 0。父进程当生产者，循环 20 次「`sem_wait(empty)` → 写入 $i \times 10+1$ → `sem_post(full)`」；子进程当消费者，循环 20 次「`sem_wait(full)` → 取出打印 → `sem_post(empty)`」。①先想清楚：为什么这个 8 槽小环能装下 20 条消息——哪个信号量在给生产者踩刹车？②真跑，贴出消费者打印的 20 条，验证顺序正确、没有丢失（单生产者单消费者各占独立槽位，信号量的 happens-before 就是同步）；③额外一道小问：单独写一个**故意漏 `ftruncate`** 的共享内存程序，贴出它 mmap 成功却在第一次访问时被 SIGBUS 打死的现场（退出码是多少、128+几？）。跑完所有 `sem_unlink`/`shm_unlink`。

[参考答案 →](homework-solutions#hw-5-7-b)

## 5.8 IO 多路复用（select）

### 5.8-A {#hw-5-8-a}

难度 **L2** · 涉及[第 8 章：IO 多路复用](/05-system-programming/08-select)

教材盯的是两个 pipe。这题盯 **stdin 加一个 pipe**，并且要写**事件循环**：子进程 2 秒后往管道写 `from-pipe` 并关写端；父进程每轮 `select` 同时盯 `STDIN_FILENO` 和管道读端，谁就绪就打印谁的名字并读出内容，管道 EOF 后退出。用 `echo hello-from-stdin | ./程序` 喂数据。①先预测两条消息谁先被打印、为什么；②真跑贴输出；③指出这题里必须做对的一件循环铁律（集合要在哪里重建、为什么），贴出你的事件循环骨架。

[参考答案 →](homework-solutions#hw-5-8-a)

### 5.8-B {#hw-5-8-b}

难度 **L3** · 涉及[第 8 章：IO 多路复用](/05-system-programming/08-select)

教材讲了 select 的四个坑，这题把其中两个**亲自踩一遍**。①timeval 改写坑：一个没人写数据的管道，循环三轮 `select(..., &tv)`，但 `tv` **只在第一轮前**设成 1 秒、后面故意不重设——打印三轮的返回值、返回后的 `tv.tv_sec/tv_usec`，以及肉眼可感的耗时差异（第一轮约 1 秒，后两轮几乎瞬间）；②fd_set 不重建坑：盯两个 pipe A、B，子进程 1 秒后向 B 写 `from-B` 并关掉 B 的写端、再活 2 秒才关 A 的写端。父进程第一轮建集合 `{A, B}`、select、读掉 B 的数据，打印返回后集合里 A、B 各剩多少；然后**不重建**原样再 select 一轮（第 2 轮 select 之前先 `waitpid` 收掉写 B 的子进程，确保那一刻 A、B 的写端都已关干净），打印这轮的 A、B 状态（此刻 A 的写端已关、EOF 就绪，为什么却不被报告）；最后重建一次再 select，A 回来没有？把三段输出贴出来对照。

[参考答案 →](homework-solutions#hw-5-8-b)

## 5.9 poll 与 epoll

### 5.9-A {#hw-5-9-a}

难度 **L3** · 涉及[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)

教材的 poll 只检查 `POLLIN`。这题把 **`POLLHUP`** 也用上，写成事件循环：子进程写 `ping\n` 后立刻关写端；父进程 poll 循环里用 `revents & POLLIN` 读数据、`revents & POLLHUP` 报「写端全关」退出循环。①先预测：**POLLHUP 会和最后一笔数据同轮出现吗**（还是数据读完之后、下一轮才报 HUP）？每轮 `revents` 里可能同时含哪些位？②真跑，把每一轮的 `revents` 贴出来；③两道小问答：为什么循环里必须把 `revents` 重置为 0；POSIX 保证「先 POLLIN 轮、后 POLLHUP 轮」的先后顺序吗？

[参考答案 →](homework-solutions#hw-5-9-a)

### 5.9-B {#hw-5-9-b}

难度 **L4** · 涉及[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)

教材讲了 ET 会漏读，但没演给你看。这题把漏读做成**可复现的对照实验**：子进程一次写 10000 字节进管道（写端再握 3 秒别关）；父进程把读端注册进 epoll（`mode` 参数控制带不带 `EPOLLET`），第一轮 `epoll_wait` 收到就绪后**故意只读一次 4096 字节**就回去等。第二轮 `epoll_wait` 设 1.5 秒超时。①LT 模式：预测第二轮返回什么；真跑后把剩下的 5904 字节读完，验证「有数据就持续通知」。②ET 模式：预测第二轮返回什么；真跑验证——如果超时返回 0，再手工去 read，证明剩下的数据**没有丢、只是再也没有通知**。③用「状态变化只报一次」解释 ET 的语义，并说明 ET 的正确用法要配什么（为什么必须循环读到 EAGAIN）。

[参考答案 →](homework-solutions#hw-5-9-b)

## 5.10 非阻塞 IO 与 reactor

### 5.10-A {#hw-5-10-a}

难度 **L3** · 涉及[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)、[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)

教材演示的是**读端**非阻塞（空 pipe 返回 EAGAIN）。这题反着来，把**写端**设非阻塞，一路写到管道满：先把读端也设非阻塞（不然第一个「空管道读」就会卡死，为什么？），看一眼空管道非阻塞读的 EAGAIN；然后循环 `write(4096)` 直到失败，打印「第几次写失败、errno、一共写进了多少字节」。①先预测共写进多少字节（提示：Linux 默认管道容量）、第几次 write 撞 EAGAIN；②真跑对照；③把管道抽干验证读出来同样多，说明「写端非阻塞 + EAGAIN」在事件循环里的意义——写不进去时不卡住整个 reactor。

[参考答案 →](homework-solutions#hw-5-10-a)

### 5.10-B {#hw-5-10-b}

难度 **L4** · 涉及[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)

教材的作者在 LT 模式 EOF 上踩过「2 分钟刷 68MB 输出」的坑，这题让你**亲手复现并量化**：写一个最小 reactor，一个管道、一个回调，子进程写 4 字节后关写端。mode 0 是「漏 DEL 版」：回调发现 EOF 只默默计数、不要求移除，事件循环跑 1 秒墙钟后强制停车，打印这一秒里 EOF 回调被**空转了多少次**（真实数字会很大，这是烧满一个核的节奏）；mode 1 是「修复版」：回调遇到 EOF 返回「移除」，事件循环 `EPOLL_CTL_DEL + close` 后干净退出，打印回调总共被调用的次数。①先预测两个 mode 回调次数的数量级；②真跑贴真实数字；③解释 LT 模式下为什么 EOF 的 fd 会无限报就绪——`read` 返回 0 不阻塞，和「内核认为它一直可读」之间的因果关系。

[参考答案 →](homework-solutions#hw-5-10-b)

## 5.11 Socket TCP

### 5.11-A {#hw-5-11-a}

难度 **L2** · 涉及[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)

教材的客户端是「发 10 字节就走」，这题做成**双向对答**：服务端 accept 后先发欢迎语 `WELCOME`，再读客户端发来的名字、回一句 `BYE <名字>`；客户端 connect 后先读欢迎语、发名字、再读告别语。另外在客户端和服务端各用 `getsockname`/`getpeername` 打印自己和对端的地址端口。①先预测客户端那一端的「自己」端口和服务端的监听端口一样吗（临时端口是谁分的）；②真跑贴输出；③说明服务端那个新连接 fd 上 `getpeername` 看到的是谁、它的端口和客户端 `getsockname` 打出来的是不是一对。

[参考答案 →](homework-solutions#hw-5-11-a)

### 5.11-B {#hw-5-11-b}

难度 **L3** · 涉及[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)

教材算了「漏 htons 会把 12345 连成 14640」这笔账，这题**真踩一次**。程序分两幕：第一幕，把 `uint16_t 0x1234` 的主机序内存字节和 `htons` 之后的字节逐个 `%02X` 打出来，说清本机是什么端序；第二幕，服务端老老实实 `htons(12345)` 绑定监听，客户端先**故意漏 htons**、把主机序 12345 直接塞进 `sin_port` 去 `connect`——①先预测 connect 返回什么、errno 是多少（会连到 14640 去）；②真跑贴输出；③再补上 `htons` 连一次成功收场。解释为什么「端口号填错」这种 bug 的报错是 ECONNREFUSED 而不是别的——服务端根本没在听 14640。

[参考答案 →](homework-solutions#hw-5-11-b)

## 5.12 进阶 Socket

### 5.12-A {#hw-5-12-a}

难度 **L3** · 涉及[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)

教材演示的是「客户端往死连接写被 SIGPIPE 杀」，这题换**服务端视角**：客户端连上服务端后立刻 `close` 走人；服务端 accept 后睡 1 秒（给 RST 回程时间），然后循环 `write` 4KB 块直到出错。服务端做成子进程、父进程旁观收尸。①mode 0 无防护：预测服务端怎么死，真跑贴出父进程用 `WIFSIGNALED`/`WTERMSIG` 查到的信号号；②mode 1 在服务端 `signal(SIGPIPE, SIG_IGN)`：预测 `write` 改成返回什么，真跑贴出「第几次 write 失败 + errno」（注意前几十次 write 为什么还能成功——TCP 发送缓冲在吞数据）；③对比两档，说清为什么「一个客户端就能干掉整个服务端」必须防。

[参考答案 →](homework-solutions#hw-5-12-a)

### 5.12-B {#hw-5-12-b}

难度 **L4** · 涉及[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)、[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)

教材给了 `read_full` 但没跑完整的组帧程序，这题把**长度前缀组帧**跑通并且逼出半包：实现 `write_full`/`read_full`，客户端连发三条长度前缀消息（`uint32_t` 网络序长度 + 载荷）——前两条小消息连着发（必然粘包），第三条 2000 字节**拆成两半发、中间停 0.5 秒**（必然半包）。服务端循环：先 `read_full` 4 字节长度字段、`ntohl`、再 `read_full` 载荷，打印每条消息的长度、内容前 20 字节、以及「读长度字段/读载荷各用了多少次 `read`」。①先预测第三条消息的载荷会用几次 `read` 凑齐；②真跑贴输出；③说明组帧之后粘包半包为什么都不再是问题——边界是接收方**算出来**的，不是猜的。

[参考答案 →](homework-solutions#hw-5-12-b)

## 5.13 UDP 与本地域套接字

### 5.13-A {#hw-5-13-a}

难度 **L2** · 涉及[第 13 章：UDP 与本地域套接字](/05-system-programming/13-udp-and-unix-domain)

教材的 UDP 服务端只收不回。这题把 `recvfrom` 的「对端地址」用起来做**回信**：客户端连发两条不等长数据报（`one` 3 字节、`two-long-message` 16 字节）；服务端每收到一条就打印「第几个数据报、多少字节、来自哪个端口」，并 `sendto` 回一条 `echo:<原文>`；客户端收两条回信打印。①先预测两条数据报会不会粘在一起（对照 TCP 的粘包）；②真跑贴输出；③说明 `recvfrom` 拿到的 `peer` 地址是什么、服务端凭什么不用 `accept` 就能回信——UDP 的「无连接」和「一个 socket 跟多个对端来回」是怎么回事。

[参考答案 →](homework-solutions#hw-5-13-a)

### 5.13-B {#hw-5-13-b}

难度 **L3** · 涉及[第 13 章：UDP 与本地域套接字](/05-system-programming/13-udp-and-unix-domain)

教材只演示了 `AF_UNIX` 的 STREAM 版，这题上**DGRAM 版**再加一个坑：①服务端 `socket(AF_UNIX, SOCK_DGRAM)` 绑定 `/tmp/...` 路径，客户端也绑定自己的路径（好收回信），`sendto` 发 `hello-dgram`、`recvfrom` 收回信，服务端打印收到的字节数并回信；②**残留坑**：不 `unlink` 旧路径就直接对同一个路径再 `bind` 一次，打印 errno——先预测是不是 EADDRINUSE、编号几；③真跑贴输出，说明为什么 socket 文件不会随进程退出自动消失、`unlink` 的纪律和共享内存的 `shm_unlink` 是不是一脉相承。

[参考答案 →](homework-solutions#hw-5-13-b)

## 5.14 getaddrinfo

### 5.14-A {#hw-5-14-a}

难度 **L2** · 涉及[第 14 章：getaddrinfo](/05-system-programming/14-getaddrinfo)

教材解析的是数字端口 `8080`，这题换**符号服务名**和**坏域名**：①用 `hints.ai_family = AF_UNSPEC` 解析 `getaddrinfo("localhost", "http", ...)`，遍历链表、用 `getnameinfo` 打出每条结果的数字地址和数字端口——先预测端口是不是 80、结果会不会不止一条（IPv4/IPv6 各一）；②再解析一个不存在的域名 `definitely-not-a-real-host.invalid`，用 `gai_strerror` 打出错误——先预测错误码是不是 `EAI_NONAME`；③真跑贴输出，回答两问：符号服务名是从哪里查出来的（`/etc/services` 是干什么的）；`getaddrinfo` 的错误为什么走 `gai_strerror` 而不是 `strerror(errno)`。

[参考答案 →](homework-solutions#hw-5-14-a)

### 5.14-B {#hw-5-14-b}

难度 **L3** · 涉及[第 14 章：getaddrinfo](/05-system-programming/14-getaddrinfo)、[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)

教材讲了 `AI_PASSIVE` 但没演示，这题跑通**服务端用 getaddrinfo 通配绑定**的全流程：①服务端 `getaddrinfo(NULL, "0", hints)`，`hints.ai_flags = AI_PASSIVE`、`ai_family = AF_UNSPEC`，遍历结果逐条 `socket + bind` 到成功，打印拿到的通配地址（先预测是 `0.0.0.0` 还是 `::`）；`listen` 后用 `getsockname` 查回内核实际分配的端口并打印；②客户端也用 `getaddrinfo("localhost", 服务名=实际端口)` 解析后 `connect`、发 `hello-gai\n`；③服务端 accept、读、打印，收尾 `freeaddrinfo`。真跑贴输出，说明：`AI_PASSIVE` 给的通配地址和客户端连的目标地址为什么不一样、各自扮演什么角色；两端都走 `getaddrinfo` 之后，代码里还剩几个手写的 IP/端口常量。

[参考答案 →](homework-solutions#hw-5-14-b)

## 5.C 跨章综合与挑战

### 5.C-1 {#hw-5-c-1}

难度 **L4** · 涉及[第 9 章](/05-system-programming/09-poll-and-epoll)、[第 10 章](/05-system-programming/10-nonblock-and-reactor)、[第 11 章](/05-system-programming/11-socket-tcp)、[第 12 章](/05-system-programming/12-socket-advanced)

综合题：**单进程 epoll 并发 echo 服务端**。要求：监听 fd 设非阻塞、注册进 epoll；事件循环里「监听 fd 就绪 → 循环 accept 到 EAGAIN → 新连接设非阻塞、注册进 epoll」「连接 fd 就绪 → read，`>0` 原样回写（用 `write_full`），`==0` 则 `EPOLL_CTL_DEL + close`，`EAGAIN` 则继续等」；main 开头 `signal(SIGPIPE, SIG_IGN)` 并 `setvbuf` 无缓冲。fork 三个客户端，各错峰 0.3 秒连上来、发一条 `hello-from-client-N`、读回显、打印、关闭。服务端统计：accept 了几条、回显了几次、EOF 关闭了几条。①先想清楚「监听 fd」和「连接 fd」在事件循环里的角色怎么区分；②真跑贴完整输出（注意 fd 编号可能复用，这是最小空闲号规则在干活）；③验证退出码 0、无僵尸，并说明哪里用了「非阻塞 + EAGAIN」、为什么 `accept` 不能只调一次。

[参考答案 →](homework-solutions#hw-5-c-1)

### 5.C-2 {#hw-5-c-2}

难度 **L4** · 涉及[第 3 章](/05-system-programming/03-exec-and-wait)、[第 6 章](/05-system-programming/06-pipe-and-fifo)、[第 1 章](/05-system-programming/01-file-io-and-fd)、[第 2 章](/05-system-programming/02-fork-cow-and-stdio-traps)

综合题：**在 C 里实现 shell 管道 `prod | cons`**。写两个小工具：`prod` 往 stdout 打 `line-1` 到 `line-100000` 共十万行，`cons` 读 stdin 数行数、字节数并打印。管线程序：`pipe` 之后 fork 子 A（`dup2` 把 stdout 改道成写端、关掉两个管道 fd、`execlp` 成 `prod`）和子 B（stdin 改道成读端、`execlp` 成 `cons`），父进程关掉自己那两份管道 fd、`waitpid` 两个子进程并打印各自退出码。①先想清楚：父进程漏关管道 fd 会怎样（谁在等一个永远不会来的 EOF）？fork 前有哪些 stdio 缓冲要防？②真跑贴输出，验证十万行、两个退出码 0；③把这条管线跟「教材第 3 章 exec 失败必须 `_exit`」串起来：子 A、子 B 的 `execlp` 后面跟了什么、为什么必须有。

[参考答案 →](homework-solutions#hw-5-c-2)

### 5.C-3 {#hw-5-c-3}

难度 **L5** · 涉及[第 7 章](/05-system-programming/07-shm-and-semaphores)、[第 6 章](/05-system-programming/06-pipe-and-fifo)、[第 2 章](/05-system-programming/02-fork-cow-and-stdio-traps)

挑战题（改编自 MIT 6.828 xv6 的 pingpong 作业与 CSAPP 并行程序测量的思路，如实标注）。**共享内存乒乓基准**：fork 两个进程，共享内存里放一个 `uint64_t` 计数器，用两个命名信号量（一个初始 1、一个初始 0）来回踢皮球——父 `sem_post(发球信号量) + sem_wait(等球回来)`，子 `sem_wait + 计数加一 + sem_post`，踢 `N = 200000` 次。用单调时钟量总耗时，打印「耗时、每秒往返次数、最终计数器」（必须等于 N，等于才是同步正确）。再做一版**管道乒乓**对照：两个管道互传 1 字节同样踢 N 次。①先推理两版各有哪些系统调用开销（每轮各几次内核进出、有没有数据搬运），预测哪版快、大概快几倍；②真跑贴两版真实吞吐（数字每台机器不同，如实记录）；③解释差异根源：信号量的 `sem_wait/sem_post` 和管道的 `read/write` 在内核里干的活有什么不同；④说明乒乓场景下为什么「每次 `sem_wait` 都必然进内核」——信号量「无竞争才省 syscall」的优化为什么在这里失效。

[参考答案 →](homework-solutions#hw-5-c-3)
