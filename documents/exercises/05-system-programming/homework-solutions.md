---
title: "阶段 5 课后练习参考答案（Homework）"
description: "系统编程阶段课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出，代码全部在 WSL Arch（gcc 16.1.1）真实编译运行。"
chapter: 5
order: 1
tags:
  - host
  - system-programming
  - posix
  - ipc
  - socket
difficulty: intermediate
reading_time_minutes: 60
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 课后练习（Homework）"
related:
  - "阶段 5 各章"
---

# 阶段 5 课后练习参考答案（Homework）

> 所有输出在 WSL Arch（gcc 16.1.1）真实运行得到。多进程题的 pid、端口、调度顺序每次运行都不同——解答里的数字是某一次真跑的记录，你复现时对结构（谁等于谁、谁是谁的几倍）而不是对数字。凡输出里带 pid/端口处都如实贴出，可放心对照。

## 5.1-A {#hw-5-1-a}

**难度 L1** · 题面见 [homework](homework#hw-5-1-a)

**思路**：fd 是内核 per-process 表的下标，分配永远取「最小空闲号」——0/1/2 被三标准流占着，新 fd 从 3 起；关掉谁、下一次就优先复用谁。

1. ①开 A、B 依次拿 3、4；②关的是 B，最小空闲号是 4，所以 C 拿 4——变式的关键就是「关谁空谁」，和教材里关 A 空 3 是一个规则的两面。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「文件描述符：一个不起眼的小整数」一节（最小空闲号分配规则）
2. ③A 和 C 还占着 3、4，所以连开三次是 5、6、7——不是从 3 开始，是因为 3、4 没还。④`dup(0)` 在 0/1/2/3/4/5/6/7 都占着时挑 8。→ 知识点：[第 1 章](/05-system-programming/01-file-io-and-fd)「dup 与 dup2」一节（dup 也按最小空闲号）
3. 环境归零那步：有些终端环境会给进程继承多余的 fd（本机这套 WSL 会话就常见 fd 5 = `/dev/ptmx`），它会插进编号序列里让你的预测对不上——`close(5)` 把实验拉回「只有 0/1/2」的干净起点，规则依然成立，说明编号只是表的下标、环境能影响绝对数字但改不了分配规则。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0101a.c -o hw0101a && ./hw0101a
开 A: fd=3, 开 B: fd=4
关 B 后再开 C: fd=4
同一文件连开三次: 5, 6, 7
dup(0) = 8
```

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* 某些终端环境会往进程里继承多余的 fd(比如 /dev/ptmx 占着 5 号),
       先关掉它,让实验回到「只有 0/1/2」的干净起点,预测才对得上 */
    close(5);

    /* 变式 1:教材里关的是 A,这里关的是 B —— C 该拿几号? */
    int a = open("/tmp/cj-ex5/hw1a.txt", O_CREAT | O_WRONLY, 0644);
    int b = open("/tmp/cj-ex5/hw1b.txt", O_CREAT | O_WRONLY, 0644);
    printf("开 A: fd=%d, 开 B: fd=%d\n", a, b);
    close(b); /* 关 B 留 A */
    int c = open("/tmp/cj-ex5/hw1c.txt", O_CREAT | O_WRONLY, 0644);
    printf("关 B 后再开 C: fd=%d\n", c);

    /* 变式 2:同一进程连续 open 同一个文件三次,不 close */
    int x1 = open("/tmp/cj-ex5/hw1a.txt", O_WRONLY);
    int x2 = open("/tmp/cj-ex5/hw1a.txt", O_WRONLY);
    int x3 = open("/tmp/cj-ex5/hw1a.txt", O_WRONLY);
    printf("同一文件连开三次: %d, %d, %d\n", x1, x2, x3);

    /* 变式 3:dup(0) —— stdin 占着 0 号,dup 会挑最小空闲号 */
    int d = dup(0);
    printf("dup(0) = %d\n", d);
    return 0;
}
```

## 5.1-B {#hw-5-1-b}

**难度 L3** · 题面见 [homework](homework#hw-5-1-b)

**思路**：`read_all` 的循环里，短读是「返回 < 请求量继续凑」、EINTR 是「被打断重试」、返回 0 是「EOF 收工」——三种返回值三种语义，一次读循环全要判。

1. 预测：子进程 1.2 秒才写 100 字节，父进程的第一次 `read` 先被 1 秒闹钟打断（EINTR、重试），重试后拿到 100 字节——这是一次短读（要 4096 只给 100）。凑不齐时 `read_all` 继续 read，第三次 read 撞上 EOF（子进程已关写端）返回 0、`break` 带回 100。所以外层第一次调用就消费了 3 次 `read`；外层第二次调用（再读一轮）的 read 直接拿到 0，合计 **read 系统调用 4 次、短读 1 次、打断 1 次、共 100 字节**。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「短读短写：别假设一次到底」一节（read 三返回值语义）、[第 5 章：信号](/05-system-programming/05-signals)「慢系统调用被信号打断：EINTR」一节
2. 关键在循环条件：`while (done < want)` 里每次只补「剩下的」`want - done`，`n == 0` 用 `break` 而不是报错——EOF 不是错误。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)「匿名 pipe」一节（read 返回 0 = 所有写端都关了）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0101b.c -o hw0101b && ./hw0101b
合计收到 100 字节, 外层 read_all 调用 2 次(最后一次拿到 0=EOF)
read 系统调用共 4 次, 其中短读 1 次(要 4096 只拿到 100)
SIGALRM 打断次数 = 1(EINTR 被重试吞掉,数据没丢)
```

核心循环：

```c
static ssize_t read_all(int fd, char* buf, size_t want) {
    size_t done = 0;
    while (done < want) {
        ssize_t n = read(fd, buf + done, want - done);
        reads_total++;
        if (n == 0) {
            break; /* EOF */
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* 被信号打断:不算错,重试 */
            }
            return -1;
        }
        if ((size_t) n < want - done) {
            short_reads++;
        }
        done += (size_t) n;
    }
    return (ssize_t) done;
}
```

## 5.2-A {#hw-5-2-a}

**难度 L2** · 题面见 [homework](homework#hw-5-2-a)

**思路**：每次 fork 都把「当前所有进程」各复制一份，进程数按 2 的幂翻倍——第二次 fork 时已经是两个进程了，各裂一个，共 4 个。

1. 进程树：原始进程 fork 出 A，A fork 出 C；原始进程自己又 fork 出 B，B fork 出 D。共 4 行输出；两个直接子进程（A、B）的 ppid 等于原始 pid，两个孙进程（C、D）的 ppid 分别是 A、B 的 pid。→ 知识点：[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)「fork()：一次调用，两次返回」一节
2. 真跑里每一行都有同一个 `orig` pid 打头，4 行的 pid 各不相同，ppid 指向树上的父节点——排序后更清楚。→ 知识点：[第 2 章](/05-system-programming/02-fork-cow-and-stdio-traps)（子的 ppid 就是父的 pid）
3. 每个进程末尾的 `while (waitpid(-1, NULL, 0) > 0) {}` 让每个父把自己的直接子收掉——不收就变僵尸，占着 pid 不放。→ 知识点：[第 2 章](/05-system-programming/02-fork-cow-and-stdio-traps)「僵尸进程」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0102a.c -o hw0102a && ./hw0102a | sort
[orig=3272] 现在 pid=3272, ppid=3249
[orig=3272] 现在 pid=3274, ppid=3272
[orig=3272] 现在 pid=3275, ppid=3272
[orig=3272] 现在 pid=3276, ppid=3274
```

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t orig = getpid(); /* fork 之前的 pid,用来给每一行标「出身」 */
    fork();                /* 第一次 fork:现在有 2 个进程 */
    fork();                /* 第二次 fork:每个再裂一个,共 4 个 */

    printf("[orig=%d] 现在 pid=%d, ppid=%d\n", (int) orig, (int) getpid(), (int) getppid());

    /* 每个进程都把自己(可能有的)直接子进程收掉,不留僵尸 */
    while (waitpid(-1, NULL, 0) > 0) {
    }
    return 0;
}
```

## 5.2-B {#hw-5-2-b}

**难度 L3** · 题面见 [homework](homework#hw-5-2-b)

**思路**：`PRE-` 无 `\n` 就留在 stdio 缓冲里，fork 把它复制成两份——「打两遍」是坑一；子进程 `_exit` 不刷 stdio——「输出蒸发」是坑二；终端行缓冲遮住坑、重定向全缓冲放大坑。

1. mode 0 直连终端（行缓冲）：子进程 `printf("child-1\n")` 的 `\n` 把缓冲里「自己那份 `PRE-`」一起刷出去，父进程的 `POST\n` 同理——`PRE-` 出现两次。mode 1 fork 前 `fflush` 把缓冲倒空，子进程继承的是空缓冲，`PRE-` 只一次。→ 知识点：[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)「stdio 缓冲陷阱（上）：同一行被打两遍」一节
2. mode 0 重定向到文件（全缓冲）：子进程 `_exit(0)` 根本不刷 stdio，它攒的 `PRE-child-1\nchild-2\n` 整段蒸发，文件里只剩父进程的 `PRE-POST`——两个坑在这里叠加。mode 2 子进程改 `exit(0)`，`exit` 会 flush 所有流，文件里四行齐全。→ 知识点：[第 2 章](/05-system-programming/02-fork-cow-and-stdio-traps)「stdio 缓冲陷阱（下）」一节、[第 1 章](/05-system-programming/01-file-io-and-fd)「两套缓冲」一节（行缓冲 vs 全缓冲、`_exit` vs `exit`）

**验证输出**（直连终端那两场用 `script -qec` 开伪终端跑出真实行缓冲行为）：

```text
$ gcc -std=c11 -Wall -Wextra hw0102b.c -o hw0102b
$ script -qec ./hw0102b /dev/null        # mode 0 直连终端(行缓冲)
PRE-child-1
child-2
PRE-POST                                 ← PRE- 出现两次(坑一)
$ script -qec "./hw0102b 1" /dev/null    # mode 1 直连终端
PRE-child-1
child-2
POST                                     ← fflush 后只有一次
$ ./hw0102b 0 > out_mode0.txt && cat out_mode0.txt   # mode 0 重定向(全缓冲)
PRE-POST                                 ← 子进程两行被 _exit 蒸发了(坑二)
$ ./hw0102b 2 > out_mode2.txt && cat out_mode2.txt   # mode 2 重定向
PRE-child-1
child-2
POST
```

## 5.3-A {#hw-5-3-a}

**难度 L2** · 题面见 [homework](homework#hw-5-3-a)

**思路**：exec 换掉的是内存镜像（含环境变量那份新值），保留的是内核侧的进程身份（pid、fd 表、cwd 那套）——环境是「程序的数据」、fd 是「进程的资产」。

1. `execve` 自传环境后，靶子程序拿到的 HOME 是新环境里的 `/tmp/only-home`、PATH 直接是 `(null)`——环境整个被换掉，不是合并、不是继承。→ 知识点：[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)「argv[0]、环境变量与「exec 不改 pid」」一节（带 `e` 后缀的 exec 自传环境）
2. fd 5 是父进程 `dup2` 钉上的文件，exec 不碰 fd 表，靶子程序 `read(5)` 原样读到 `secret-from-parent-fd`——这就是 `FD_CLOEXEC` 要防的「fd 漏给 exec 后的程序」。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（exec 保留 fd 表）、[第 1 章](/05-system-programming/01-file-io-and-fd)「dup 与 dup2」一节（`FD_CLOEXEC` 伏笔兑现）
3. 坑就地提一句：那个文件必须 `O_RDWR` 开——只写打开的话，fd 到了靶子程序手里 `read` 会撞 EBADF，这不是 exec 丢了 fd，是 fd 本身的读写权限就不对。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0103a.c -o hw0103a
$ gcc -std=c11 -Wall -Wextra hw0103a_prog.c -o hw0103a_prog
$ ./hw0103a
[prog] pid=3305 argc=3
  argv[0] = prog-renamed
  argv[1] = one
  argv[2] = two
  HOME = /tmp/only-home
  PATH = (null)
  fd5 读到 22 字节: secret-from-parent-fd
```

## 5.3-B {#hw-5-3-b}

**难度 L3** · 题面见 [homework](homework#hw-5-3-b)

**思路**：退出状态是内核打包的整数，拆包必须走宏；`waitpid(-1)` 收的是「先死先收」的顺序，和 fork 顺序无关；退出码只有低 8 位。

1. `exit(300)`：300 = 0x12C，低 8 位 0x2C = 44——`WEXITSTATUS` 取出来就是 44。→ 知识点：[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)「wait 与 waitpid」一节（退出码只有低 8 位）
2. 真跑里收到顺序是 pid 3312（码 0）、3314（码 44）、3313（码 5）、3315（信号 11）——**没有**按 fork 顺序，先死先收；多跑几次顺序还会变。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（`wait` 只收最先死的那个，收干净要循环到 `ECHILD`）
3. `status` 里退出码和信号号打包在一起，直接 `== 0` 判断是错的——必须 `WIFEXITED`/`WIFSIGNALED` 先问「怎么死的」，再各自取码。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（状态宏是拆包的唯一正确姿势）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0103b.c -o hw0103b && ./hw0103b
[父] 收到 pid=3312: 正常退出, 码=0
[父] 收到 pid=3314: 正常退出, 码=44
[父] 收到 pid=3313: 正常退出, 码=5
[父] 收到 pid=3315: 被信号杀死, 信号=11
```

## 5.4-A {#hw-5-4-a}

**难度 L2** · 题面见 [homework](homework#hw-5-4-a)

**思路**：孤儿两级传递后，收养者是「路径上会负责 wait 的进程」——经典模型说 PID 1，本机真跑是 WSL 的 init/subreaper 替身，反正不是 1 就是替身，绝不会没人管。

1. 经典 Unix 模型：孙子的 ppid 应该是 1（init 收养一切孤儿）。真跑本机是 **3245**——这台 WSL2 上 PID 1 是 systemd 的角色进程，而 3245 是会话里负责收养的 subreaper 角色进程（教材里同机测到过 246，机制一样、编号不同）。→ 知识点：[第 4 章：守护进程与孤儿](/05-system-programming/04-daemons-and-orphans)「孤儿进程：父先死的孩子被谁收养」一节（subreaper 机制）
2. 孙子不会变长期僵尸：收养它的进程（1 或 subreaper）会在它死时 `wait` 收尸——孤儿的退出状态总有人读。→ 知识点：[第 4 章](/05-system-programming/04-daemons-and-orphans)（无论收养者是谁，关键性质不变）
3. 观察时机：孙子的最后一行在程序启动约 2 秒后才出现（它 `sleep(2)` 等上面的进程都退干净），此前它是「孤儿的孤儿」，输出顺序上会和后续命令的输出交错——这是真实调度，不是 bug。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0104a.c -o hw0104a && ./hw0104a
[父] pid=3321 退出, 把中间子 pid=3322 留下
[中间子] pid=3322 退出, 把孙子 pid=3323 留下
[孙子] pid=3323, ppid=3245(经典模型该是 1,真跑看本机)
```

## 5.4-B {#hw-5-4-b}

**难度 L3** · 题面见 [homework](homework#hw-5-4-b)

**思路**：pid 文件 + `O_EXCL` 是第 1 章 open 语义和第 4 章单实例守护的合体；daemonize 之后 0/1/2 都进了 `/dev/null`，想说话只能写文件。

1. 第二个实例 `open(pidfile, O_CREAT|O_EXCL)` 撞上已存在的文件，`errno=17`（EEXIST），打印后退 1——`O_EXCL` 保证「只有第一个实例是我亲手建的」。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「open(2)」一节（`O_EXCL` 语义）、[第 4 章：守护进程与孤儿](/05-system-programming/04-daemons-and-orphans)「单实例守护」一节
2. 日志三行：daemon 的 pid=3333 和启动者 3330 不同（fork 了两次）；ppid=3245 不是 1（两次 fork 的中间父都退了，孙子被本机 subreaper 收养）；sid=3332 是第一次 fork 出来的中间进程的 pid——`setsid` 建新会话的副产物。→ 知识点：[第 4 章](/05-system-programming/04-daemons-and-orphans)「每一步的「为什么」」一节
3. daemon 里 `printf` 的下场：stdout 早被 `dup2` 进 `/dev/null`，打了也看不见；日志文件是 daemon 自己 `fopen` 的新 fd，和 0/1/2 无关，所以内容看得到——调试 daemon 只能走文件或 syslog。→ 知识点：[第 4 章](/05-system-programming/04-daemons-and-orphans)（重定向 0/1/2 到 `/dev/null` 的原因）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0104b.c -o hw0104b
$ ./hw0104b > hw4b_inst1.log 2>&1 &
$ sleep 0.6
$ ./hw0104b
[实例2] pid 文件已存在, errno=17 (File exists) —— 已有实例在跑,退出
$ echo "exit=$?"
exit=1
$ sleep 3.5
$ cat hw4b_inst1.log
[实例1] pid=3330 拿到了 pid 文件,开始 daemonize
$ cat /tmp/cj-ex5/hw4b.log
daemon pid=3333 ppid=3245 sid=3332 tick=0
daemon pid=3333 ppid=3245 sid=3332 tick=1
daemon pid=3333 ppid=3245 sid=3332 tick=2
daemon pid=3333 退出,清理 pid 文件
```

## 5.5-A {#hw-5-5-a}

**难度 L2** · 题面见 [homework](homework#hw-5-5-a)

**思路**：处理器是插进主流程的「异步小段代码」，它只许干最少的事；errno 是线程级全局，处理器有义务用完还原；SIGKILL 是内核的最后手段，压根不给你装处理器的机会。

1. 计数器是 5：`raise` 三次 + `kill(getpid(), SIGUSR1)` 两次，信号都送达了；`volatile sig_atomic_t` 保证主循环能看见处理器写的值。errno 仍是 7：处理器开头 `int saved_errno = errno`、结尾还原，中间那次失败的 `open` 没留下痕迹。→ 知识点：[第 5 章：信号](/05-system-programming/05-signals)「处理器里能做什么：async-signal-safe 与 errno」一节（存/恢复 errno）
2. 装 SIGKILL：`sigaction` 返回 -1、`errno=22`（EINVAL）——SIGKILL/SIGSTOP 不能被捕获、不能被忽略，`kill -9` 必杀的原因就在这里。→ 知识点：[第 5 章](/05-system-programming/05-signals)「几个常用信号 + 两条硬规矩」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0105a.c -o hw0105a && ./hw0105a
[主] 共收到 5 次 SIGUSR1, errno 仍是 7(处理器存恢复了)
[主] 装 SIGKILL 处理器失败: errno=22 (EINVAL)
```

## 5.5-B {#hw-5-5-b}

**难度 L3** · 题面见 [homework](homework#hw-5-5-b)

**思路**：同一个 `read`，没有 `SA_RESTART` 时被信号踢回你手里（EINTR，自己决定重试），有 `SA_RESTART` 时内核替你重新躺回等待——最终都拿到数据，差的是「谁在管重试」和「调用者什么时候醒」。

1. mode 0：`read` 在 1 秒闹钟到达时返回 `-1`、`errno=4`（EINTR），实测耗时 1000ms；随后循环重试的 `read` 在 2 秒整拿到 `late-data`——数据没丢，只是第一口被信号打断。→ 知识点：[第 5 章：信号](/05-system-programming/05-signals)「慢系统调用被信号打断：EINTR」一节
2. mode 1：`SA_RESTART` 让内核自动重启被打断的 `read`，调用者从头到尾只醒一次——2 秒后直接带着 9 字节数据返回，`alarmed=1` 说明信号确实到过、只是被内核消化了。→ 知识点：[第 5 章](/05-system-programming/05-signals)（`SA_RESTART` 的含义与边界）
3. `clock_gettime(CLOCK_MONOTONIC)` 是教材外补充的 POSIX 单调时钟，用来量「read 卡了多久」——两档的 1000ms/2000ms 时间线正好把「被打断」和「被重启」的区别量化出来。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0105b.c -o hw0105b
$ ./hw0105b 0
[主] 无重启版: alarm(1) 后阻塞 read 空 pipe...
[主] read 返回 -1, errno=4 (Interrupted system call), 耗时 1000 ms, alarmed=1
[主] 重试后读到 9 字节: late-data
$ ./hw0105b 1
[主] SA_RESTART 版: alarm(1) 后阻塞 read 空 pipe...
[主] read 直接被内核重启, 拿到 9 字节: late-data, 耗时 2000 ms, alarmed=1
```

## 5.6-A {#hw-5-6-a}

**难度 L1** · 题面见 [homework](homework#hw-5-6-a)

**思路**：EOF 的判定标准是「**还有没有任何进程握着写端**」——哪怕握着的那个进程根本不写，`read` 也会一直等，直到它松手。

1. 预测：mode 0 子进程握写端 3 秒，父的 `read` 要等 3 秒才拿到 0；mode 1 子进程 fork 后立刻关写端，父的 `read` 立刻返回 0。真跑 3.0 秒对 0.0 秒，分毫不差。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)「匿名 pipe」一节（父子各关掉不用的端是铁律）
2. mode 0 的 3 秒就是「最后一个写端被关闭」的时刻：内核看到写端计数归零才给读端发 EOF，此前 `read` 只能干等——这就是「漏关一端，全家挂死」的实物。→ 知识点：[第 6 章](/05-system-programming/06-pipe-and-fifo)（`read` 返回 0 = 所有写端都关了）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0106a.c -o hw0106a
$ ./hw0106a 0
[父] read 返回 0(0=EOF), 阻塞了 3.0 秒
$ ./hw0106a 1
[父] read 返回 0(0=EOF), 阻塞了 0.0 秒
```

## 5.6-B {#hw-5-6-b}

**难度 L3** · 题面见 [homework](homework#hw-5-6-b)

**思路**：`PIPE_BUF` 以内的写是原子块，两个写者怎么并发都只可能是「整块 A、整块 B」的排列；超过它的写可能被拆成几段，交错处就会出现被拦腰截断的半条消息。

1. 预测：8 字节消息（≤ PIPE_BUF）破整 run 应为 **0**；8192 字节消息（> PIPE_BUF）在慢速读端逼满管道的情况下，大 write 会被拆开交错，破整 run 应当**大量出现**。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)「PIPE_BUF：原子性的边界」一节
2. 真跑：8 字节档 640 字节里 66 个纯色 run、**破整 0 个**——每条消息都是完整的 8 字节块；8192 档 655360 字节里 148 个 run、**破整 140 个**——绝大多数消息都在一半的地方被另一个写者插了进来。→ 知识点：[第 6 章](/05-system-programming/06-pipe-and-fifo)（多写者要么守 PIPE_BUF、要么自己上边界协议）
3. 读端故意「每次 2048 + 歇 1ms」不是炫技：它让管道经常接近写满，写者的大 write 只能写成「有多少空位写多少」的半截，交错才会被逼出来——原子性的边界只在真实的并发压力下现形。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0106b.c -o hw0106b
$ ./hw0106b 8
消息长度 8(<= PIPE_BUF), 共读到 640 字节: 纯色 run 66 个, 破整 run 0 个
$ ./hw0106b 8192
消息长度 8192(> PIPE_BUF), 共读到 655360 字节: 纯色 run 148 个, 破整 run 140 个
```

## 5.7-A {#hw-5-7-a}

**难度 L2** · 题面见 [homework](homework#hw-5-7-a)

**思路**：`MAP_SHARED` 映射的是同一段物理内存，写就写回共享对象；`MAP_PRIVATE` 在写入的那一刻切开一份私有副本——后者正是 COW 在 mmap 上的化身。

1. 预测：`MAP_SHARED` 那轮父看到的 `done` 是 **99**（子的写可见）；`MAP_PRIVATE` 那轮父看到的还是 **0**（子改的是自己的副本）。真跑两组各就各位。→ 知识点：[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)「shm_open + mmap」一节（`MAP_SHARED` 与 `MAP_PRIVATE` 的分水岭）
2. `MAP_PRIVATE` 的写时复制和第 2 章 fork 的 COW 是同一套内核机制：谁要写就临时复制那一页，写者用副本、别人看不到——所以它「看着是共享内存，实际退化成各改各的」。→ 知识点：[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)「写时复制」一节（COW 的可观测后果）
3. 纪律一脉相承：`shm_unlink` 清对象、`munmap` 还映射——`/dev/shm` 里的对象不会随进程退出消失。→ 知识点：[第 7 章](/05-system-programming/07-shm-and-semaphores)「漏了 ftruncate → SIGBUS;忘了 shm_unlink → 残留」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0107a.c -o hw0107a -lrt && ./hw0107a
==== MAP_SHARED(真共享) ====
[子] 读到 msg=hello-shared, 把 done 改成 99
[父] 子改完后 done=99
==== MAP_PRIVATE(写时复制副本) ====
[子] 读到 msg=hello-private, 把 done 改成 99
[父] 子改完后 done=0
```

## 5.7-B {#hw-5-7-b}

**难度 L4** · 题面见 [homework](homework#hw-5-7-b)

**思路**：`empty` 数的是空槽、`full` 数的是满槽——生产者吃空槽吐满槽，消费者反过来；环只有 8 槽却能装 20 条消息，因为**消费和生产是流水线并行的**，信号量在中间当闸门。

1. 生产者写到第 9 条时环满（8 个 `full` 用光），`sem_wait(empty)` 把它挂起，直到消费者取走一条、还回一个空槽——「装得下 20 条」是时空复用，不是同时放得下。→ 知识点：[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)「共享内存不带同步：必须配信号量」一节（`sem_wait`/`sem_post` 的计数语义）
2. 真跑里消费者按 1、11、21……191 的顺序取出 20 条，不丢不乱：单生产者单消费者各占独立槽位，`sem_post` 释放信号量的那一刻同时把数据写进「先发生」的保证里（happens-before）。→ 知识点：[第 7 章](/05-system-programming/07-shm-and-semaphores)（信号量与共享内存搭配才完整）
3. SIGBUS 小问：漏 `ftruncate` 的对象长度是 0，`mmap` 不报错、第一次访问 `map[0] = 'X'` 直接被 SIGBUS 打死，退出码 **135 = 128 + 7**（SIGBUS 编号 7）。→ 知识点：[第 7 章](/05-system-programming/07-shm-and-semaphores)「漏了 ftruncate → SIGBUS」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0107b.c -o hw0107b -lrt && ./hw0107b
[消费者] 取到第  1 条: 1
[消费者] 取到第  2 条: 11
[消费者] 取到第  3 条: 21
...(中间 14 条略)...
[消费者] 取到第 19 条: 181
[消费者] 取到第 20 条: 191
$ gcc -std=c11 -Wall -Wextra hw0107b_sigbus.c -o sb -lrt
$ ./sigbus_capture.sh; echo "退出码 $?"
[主] mmap 成功(没报错!),对象长度却是 0,现在碰它...
./sigbus_capture.sh: line 2:   739 Bus error                  /tmp/cj-ex5/sb/sb
退出码 135
```

## 5.8-A {#hw-5-8-a}

**难度 L2** · 题面见 [homework](homework#hw-5-8-a)

**思路**：stdin 的数据一上来就位（`echo` 早把行喂进了管道），管道的 `from-pipe` 要等子进程 2 秒——select 按「谁先就绪先报谁」的自然顺序，两路都能被同一个循环接住。

1. 真跑里 `[stdin 就绪]` 先打、`[pipe 就绪]` 两秒后打——stdin 行在程序启动时就已经可读，管道要等子进程写。→ 知识点：[第 8 章：IO 多路复用](/05-system-programming/08-select)「select 的接口与 fd_set」一节
2. 循环铁律：`fd_set` 每轮都要 `FD_ZERO + FD_SET` 重建——select 返回时会把集合改写成「只剩就绪的 fd」，不重建的话下一轮 stdin 就从集合里丢了。→ 知识点：[第 8 章](/05-system-programming/08-select)「select 的几个坑」第一节
3. 管道 EOF 后 `close` 读端退出循环，父再 `waitpid` 收子——「多路复用 + 事件循环 + 不留僵尸」的三件套第一次合体。→ 知识点：[第 8 章](/05-system-programming/08-select)（事件循环节奏：建集合 → select → FD_ISSET 查 → 处理 → 回去再等）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0108a.c -o hw0108a
$ echo hello-from-stdin | ./hw0108a
[stdin 就绪] 读到: hello-from-stdin
[pipe 就绪] 读到: from-pipe
```

事件循环骨架：

```c
for (;;) {
    fd_set rfds;
    FD_ZERO(&rfds);                        /* 每轮重建,铁律 */
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(pfd[0], &rfds);
    int maxfd = (STDIN_FILENO > pfd[0] ? STDIN_FILENO : pfd[0]) + 1;

    int n = select(maxfd, &rfds, NULL, NULL, NULL);
    if (FD_ISSET(STDIN_FILENO, &rfds)) { /* stdin 就绪 */ }
    if (FD_ISSET(pfd[0], &rfds)) {       /* pipe 就绪,EOF 就退出循环 */ }
}
```

## 5.8-B {#hw-5-8-b}

**难度 L3** · 题面见 [homework](homework#hw-5-8-b)

**思路**：select 返回时改两个东西——`timeval` 改成剩余时间、`fd_set` 改成只剩就绪位；两个都是「原地改写入参」的坑，复用旧值就出事。

1. timeval 坑：第一轮等满 1 秒返回 0，`tv` 被改成 0；后两轮拿「0 秒超时」去 select，几乎瞬间返回——真跑三轮都返回 0（超时），但第一轮肉眼可感 1 秒、后两轮一眨眼。→ 知识点：[第 8 章：IO 多路复用](/05-system-programming/08-select)「select 的几个坑」第三节（timeout 被改写成剩余时间）
2. fd_set 坑：第一轮返回后集合里 A=0、B=1（内核清掉了没就绪的 A）；第二轮不重建、原样传入——此刻 A 的写端已经关干净（EOF 就绪），但 A 根本不在集合里，返回后仍是 A=0；第三轮老实重建，A=1 立刻回来。→ 知识点：[第 8 章](/05-system-programming/08-select)（fd_set 每次循环都要重建）
3. 子进程的「写 B 关 B、再活 2 秒才关 A」是让实验讲得清的关键：第一轮返回时 A 的写端还握着（A 真不就绪），等父 `waitpid` 收完尸、A 才处于 EOF 就绪——不重建的后果才显得出来。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0108b.c -o hw0108b
$ ./hw0108b 1
第 1 轮 select 返回 0, 返回后 tv.tv_sec=0, tv.tv_usec=0
第 2 轮 select 返回 0, 返回后 tv.tv_sec=0, tv.tv_usec=0
第 3 轮 select 返回 0, 返回后 tv.tv_sec=0, tv.tv_usec=0
$ ./hw0108b 2
第 1 轮: B 就绪, 读到 from-B
第 1 轮返回后集合里: A=0 B=1  ← 内核把没就绪的 A 清掉了
第 2 轮(不重建): A=0 B=1  ← A 明明 EOF 就绪,却不被报告
第 3 轮(重建后): A=1 B=1  ← 重建一次,A 立刻回来了
```

## 5.9-A {#hw-5-9-a}

**难度 L3** · 题面见 [homework](homework#hw-5-9-a)

**思路**：`POLLHUP` 在「所有写端关闭的那一刻」就置位，与管道里残留的数据**同轮并存**——所以第一轮 `revents` 就可能同时是 `POLLIN|POLLHUP`，而不是「读完数据、下一轮才报 HUP」。（**教材外补充**：POLLHUP 的时序语义教材第 9 章没讲，此处为补充知识，man poll 页有完整描述。）

1. 真跑：第一轮 `revents = 0x11`（`POLLIN|POLLHUP` 同时置位）——先按 POLLIN 去 `read` 拿到 `ping` 的 5 字节；下一轮 `revents` 只剩 `POLLHUP`，`read` 返回 0（EOF）收场。注意 POSIX **不保证**「先 POLLIN 轮、后 POLLHUP 轮」的先后顺序——写端全关那一刻 HUP 就绪，与残留数据同轮报告；所以事件循环必须「每轮都同时看两个位」，不能假设 HUP 一定晚一轮。→ 知识点：[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)「poll：用结构体数组摆脱 1024 上限」一节（`events` 关心什么、`revents` 报告什么）
2. `POLLHUP` 与 `read` 返回 0 的 EOF 是「写端全关」这一件事的两个观察面：`revents` 里的 HUP 是内核的通知（与残留数据并存），`read` 返回 0 是「数据已读完」的确认——两者**可能在同一轮、也可能隔一轮**，别把它们绑定成固定先后。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（EOF 语义）、[第 9 章](/05-system-programming/09-poll-and-epoll)（POLLHUP，教材外补充）
3. `revents` 每轮重置为 0 是硬规矩：它由内核**填写**，上轮留下的旧位会污染下一轮判断——`revents` 是输出参数，不是你要关心的输入。→ 知识点：[第 9 章](/05-system-programming/09-poll-and-epoll)（poll 原地标记、不聚拢就绪项）

**验证输出**：

```text
$ gcc -std=c11 -Wall poll1.c -o poll1 && ./poll1
第 1 轮: revents = 0x11 (POLLIN=Y POLLHUP=Y)   ← HUP 与最后一笔数据同轮!
第 2 轮: revents = 0x10 (POLLIN=N POLLHUP=Y)
read 返回 0(EOF)
```

## 5.9-B {#hw-5-9-b}

**难度 L4** · 题面见 [homework](homework#hw-5-9-b)

**思路**：LT 的判据是「**还有没有数据可读**」（有就持续通知），ET 的判据是「**状态有没有新变化**」（从无到有只报一次）——读一半走人，前者下次还叫你，后者再也不会叫。

1. LT：第二轮 `epoll_wait` 返回 1，继续读拿到剩余 5904 字节——数据还在，通知就不断。→ 知识点：[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)「LT vs ET：水平触发与边沿触发」一节
2. ET：第二轮 1.5 秒超时返回 0；随后手工 `read` 一把拿出 5904 字节——**数据没丢，是通知漏了**，这就是「只报一次边沿」的代价。→ 知识点：[第 9 章](/05-system-programming/09-poll-and-epoll)（ET 只在状态新变化那一次通知）
3. ET 的正确姿势必须配非阻塞 IO + 循环 `read` 到 `EAGAIN`：因为只有一次通知，必须在那一次把管道抽干，抽到「暂时没数据」为止——这也正是下一章 reactor 的读法。→ 知识点：[第 9 章](/05-system-programming/09-poll-and-epoll)、[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)（EAGAIN 是「抽干」的信号）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0109b.c -o hw0109b
$ ./hw0109b 0
[主] LT 模式: 注册读端,等第一波通知...
[主] 第一轮 epoll_wait 返回 1 个就绪
[主] 只读了一次 4096 字节(剩下的还在管道里),再回去 epoll_wait...
[主] 第二轮 epoll_wait 返回 1 个就绪 —— 有数据就持续通知,不会漏
[主] 继续读,拿到剩余 5904 字节
$ ./hw0109b 1
[主] ET 模式: 注册读端,等第一波通知...
[主] 第一轮 epoll_wait 返回 1 个就绪
[主] 只读了一次 4096 字节(剩下的还在管道里),再回去 epoll_wait...
[主] 第二轮 epoll_wait 超时返回 0 —— 剩下的数据再也没收到通知
[主] 手工去读,拿到了剩余 5904 字节(数据没丢,是通知漏了)
```

## 5.10-A {#hw-5-10-a}

**难度 L3** · 题面见 [homework](homework#hw-5-10-a)

**思路**：非阻塞的 `write` 和 `read` 一样「不卡、给多少算多少」——写不进去就 EAGAIN 踢回来，管道容量 65536 字节就是这题的答案。

1. 读端也得先设非阻塞：不然第一个「空管道读」会永远卡住——写端还开在同一进程里，EOF 永远不会来，这是本题的第一个坑。→ 知识点：[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)「非阻塞 IO：O_NONBLOCK 与 EAGAIN」一节
2. 真跑：16 次 `write(4096)` 共 65536 字节后，第 17 次撞 EAGAIN——65536 就是 Linux 默认管道容量；抽干读出同样多。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（管道容量上限 65536 属**教材外补充**——第 6 章只讲 PIPE_BUF 原子性）、[第 10 章](/05-system-programming/10-nonblock-and-reactor)（写侧 EAGAIN）
3. 意义：事件循环里「可写」事件处理到一半写不进去了，非阻塞 `write` 立刻把控制权还你——你回 epoll 继续等下一轮可写，而不是整个 reactor 卡死在一次 write 上。→ 知识点：[第 10 章](/05-system-programming/10-nonblock-and-reactor)（reactor 的铁律：没有一个调用能阻塞循环）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0110a.c -o hw0110a && ./hw0110a
空管道非阻塞读: 返回 -1, errno=11 (Resource temporarily unavailable)
第 17 次写失败: errno=11 (Resource temporarily unavailable) —— 管道满了
写满为止共写进 65536 字节(这就是默认管道容量), 用了 16 次 write
抽干读出 65536 字节(与写进的一致)
```

## 5.10-B {#hw-5-10-b}

**难度 L4** · 题面见 [homework](homework#hw-5-10-b)

**思路**：LT 模式下「就绪」的判据是 `read` 不阻塞——EOF 的 fd 上 `read` 立刻返回 0、从不阻塞，于是 epoll 认为它**永远就绪**，每轮 epoll_wait 都瞬间返回它，事件循环变成死循环空转。

1. 真跑 mode 0：1 秒内 EOF 回调被空转 **2,367,911 次**（约 240 万次/秒，实打实烧满一个核），强制停车才收场——这就是教材作者踩过的坑的量化版。→ 知识点：[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)「LT 模式下 EOF 的坑：必须 DEL」一节
2. mode 1：回调返回「移除」，循环 `EPOLL_CTL_DEL + close`，回调只被调用 2 次（1 次数据 + 1 次 EOF），事件循环干净退出。→ 知识点：[第 10 章](/05-system-programming/10-nonblock-and-reactor)（建连时 ADD、断开时 DEL+close 的生命周期铁律）
3. 计数不回显是必须的：如果每次 EOF 都 printf，日志一秒就被刷爆几十万行——真实程序里这种 bug 的观感是「CPU 100% + 日志疯涨」，而不是一条清楚的报错。

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0110b.c -o hw0110b
$ ./hw0110b 0
[回调] 读到 4 字节: data[bug 版] 1 秒内 EOF 回调被空转了 2367911 次, 手动停车(不然就是死循环烧 CPU)
$ ./hw0110b 1
[回调] 读到 4 字节: data[回调] EOF(n=0) → 要求移除
[修好版] 事件循环干净退出
```

## 5.11-A {#hw-5-11-a}

**难度 L2** · 题面见 [homework](homework#hw-5-11-a)

**思路**：客户端不 `bind`，`connect` 时内核替它分一个临时端口——所以两端看到的「对端端口」各不相同：客户端看到服务端的监听端口，服务端看到客户端的临时端口。

1. 真跑：客户端 `getsockname` 看到自己是 `127.0.0.1:44854`（临时端口，内核分的）、对端是 `127.0.0.1:45333`（服务端监听端口）；服务端这边恰好是镜像——自己 45333、对端 44854。→ 知识点：[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)「客户端两件套」一节（临时端口谁分的）
2. 服务端 `getpeername` 看到的 44854 和客户端 `getsockname` 打出来的完全一对——同一条 TCP 连接的两头，各自的「自己」就是对方的「对端」。→ 知识点：[第 11 章](/05-system-programming/11-socket-tcp)（一条连接、两端视角）
3. 双向对答的时序：服务端 `write(WELCOME)` 之后才 `read` 名字，客户端先 `read` 欢迎语再发名字——TCP 是字节流，两边的读写顺序是协议的一部分，写反了就是「各等各的」死锁。→ 知识点：[第 11 章](/05-system-programming/11-socket-tcp)（socket 上的 read/write 与普通 fd 一样）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0111a.c -o hw0111a && ./hw0111a
[server] 监听 127.0.0.1:45333
[client] 自己 127.0.0.1:44854, 对端 127.0.0.1:45333
[client] 收到欢迎语: WELCOME
[client] 收到告别语: BYE alice
[server] 自己 127.0.0.1:45333, 对端 127.0.0.1:44854
[server] 收到名字: alice
```

## 5.11-B {#hw-5-11-b}

**难度 L3** · 题面见 [homework](homework#hw-5-11-b)

**思路**：网络协议规定 sockaddr 里的多字节整数是大端，本机是小端——主机序 12345 的内存字节是 `39 30`，内核按大端读成 0x3930 = 14640，connect 去的端口从一开始就错了。

1. 第一幕字节 dump：`0x1234` 主机序内存里是 `34 12`（低字节在前，小端实锤），`htons` 后变 `12 34`（大端，网络要的排布）。→ 知识点：[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)「字节序：htonl / htons 别漏」一节
2. 第二幕：漏 htons 的 `connect` 返回 -1、`errno=111`（ECONNREFUSED）——它连去了 14640，那里没人 listen；补上 `htons` 后立刻连上，服务端 accept 到 fd=4。→ 知识点：[第 11 章](/05-system-programming/11-socket-tcp)（htons/ntohs 铁律）
3. 报错是 ECONNREFUSED 而不是「参数错」：内核层面你的 `sin_port` 是合法的端口号，只是「那个端口没人听」——字节序 bug 的报错和「填错端口号」完全同款，所以特别难查。→ 知识点：[第 11 章](/05-system-programming/11-socket-tcp)（漏 htons 的后果是连到错误目标）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0111b.c -o hw0111b && ./hw0111b
主机序 0x1234 的内存字节: 34 12
htons 后 0x1234 的内存字节: 12 34
[client-漏htons] connect 返回 -1, errno=111 (Connection refused)
[client-正确] connect 返回 0
[server] accept 到连接 fd=4
```

## 5.12-A {#hw-5-12-a}

**难度 L3** · 题面见 [homework](homework#hw-5-12-a)

**思路**：服务端视角的坑比客户端更凶——客户端死一个是一个，服务端死一个是一窝；`write` 的前几十次成功不是对端还活着，是 TCP 发送缓冲还在吞数据。

1. mode 0：**死在和 mode 1 同一处——第 28 次写**（前 27 次约 110KB 被 TCP 发送缓冲吞掉，第 28 次才撞上对端已关的真相）；差别只在处置：默认动作直接杀进程（信号 13），服务端连一行日志都没留下，父进程用 `WIFSIGNALED`/`WTERMSIG` 才查出死因。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「SIGPIPE：往死连接 write 会被默默杀掉」一节
2. mode 1：`SIG_IGN` 之后同样场景下，前 27 次 `write(4096)` 都成功——发送缓冲吞掉了约 110KB，直到第 28 次才返回 `-1`、`errno=32`（EPIPE），服务端打印错误、优雅收场。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（小 write 被发送缓冲吞掉、EPIPE 是优雅处理的接口）
3. 「一个客户端干掉整个服务端」的因果链：对端一关，你的任何一次 write 都可能成为最后一击——所以服务端 main 开头 `signal(SIGPIPE, SIG_IGN)` 是标配，不是可选项。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（防护两招：全局忽略或 `MSG_NOSIGNAL`）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0112a.c -o hw0112a
$ ./hw0112a 0
[main] 端口 45819
[server] accept 到连接, 睡 1 秒等客户端的 RST 传回来
[main] 服务端子进程被信号 13 杀死(SIGPIPE=13) —— 一个客户端把它整没了
$ ./hw0112a 1
[main] 端口 47061
[server] accept 到连接, 睡 1 秒等客户端的 RST 传回来
[server] 第 28 次 write 失败: errno=32 (Broken pipe)
[main] 服务端子进程正常退出, 码=0(防护版:处理完 EPIPE 活下来了)
```

## 5.12-B {#hw-5-12-b}

**难度 L4** · 题面见 [homework](homework#hw-5-12-b)

**思路**：长度前缀把「消息边界」从「猜 TCP 什么时候给全」变成「自己算出来」——先严格收 4 字节长度，再严格收那么多字节载荷，中间 TCP 怎么粘怎么拆都无所谓。

1. 真跑：前两条小消息连发，服务端照样按帧各拆各的；第三条 2000 字节被拆两半、隔 0.5 秒发，`read_full` 的载荷阶段**用了 2 次 read 才凑齐**——半包被严格读够的循环吸收了。（「前半 1000 字节恰好一次 read 全到」在 loopback 上稳定成立，但严格说 TCP 是字节流、不保证一次能读多少——这正是必须 `read_full` 的原因。）→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「TCP 没有消息边界」一节（长度前缀组帧）
2. `read_full` 里短读、EINTR、EOF 三判齐全，是第 1 章 `write_all` 的读侧对称版——「严格读够 N 字节」和「严格写满 N 字节」是同一套循环的两面。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「短读短写」一节
3. 预测核对：第三条的载荷确实用了 2 次 `read`（半包生效）；长度字段每次都 1 次（4 字节小请求几乎不会半途而废）——若换大文件传输，长度字段也可能被拆，所以它也得走 `read_full`。→ 知识点：[第 12 章](/05-system-programming/12-socket-advanced)（严格读够的循环两边都要）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0112b.c -o hw0112b && ./hw0112b
[server] 长度字段=5, 本次 read 用了 1 次才凑齐 4 字节
[server] 载荷 5 字节(用了 1 次 read 凑齐): hello...
[server] 长度字段=16, 本次 read 用了 1 次才凑齐 4 字节
[server] 载荷 16 字节(用了 1 次 read 凑齐): world-of-sockets...
[server] 长度字段=2000, 本次 read 用了 1 次才凑齐 4 字节
[server] 载荷 2000 字节(用了 2 次 read 凑齐): long-msg:...
```

## 5.13-A {#hw-5-13-a}

**难度 L2** · 题面见 [homework](homework#hw-5-13-a)

**思路**：UDP 一次 `sendto` 就是一个独立数据报，`recvfrom` 一次整条拿走——「有消息边界」就是不会粘包，服务端靠 `recvfrom` 带回来的对端地址回信，全程没有连接。

1. 真跑：服务端两次 `recvfrom` 各收到一条完整数据报（3 字节和 16 字节），不粘不拆；回信各走各的 `sendto`，客户端两条都收到。→ 知识点：[第 13 章：UDP 与本地域套接字](/05-system-programming/13-udp-and-unix-domain)「UDP：无连接、有消息边界的数据报」一节
2. `recvfrom` 的 `peer` 是发送方**这次临时分配的** UDP 端口（真跑里 :47436），服务端拿着它 `sendto` 回信——UDP 服务端不用 `accept`，一个 socket 就能和很多对端来回。→ 知识点：[第 13 章](/05-system-programming/13-udp-and-unix-domain)（`recvfrom` 带回发送方地址）
3. 对照 TCP：TCP 上这两条消息可能一次 read 就被粘成一条；UDP 天然按条收——有边界是用「不可靠」换来的，丢包、乱序都不保证。→ 知识点：[第 13 章](/05-system-programming/13-udp-and-unix-domain)「TCP vs UDP」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0113a.c -o hw0113a && ./hw0113a
[server] UDP 绑定 127.0.0.1:45206
[client] 收到回信: echo:one
[client] 收到回信: echo:two-long-message
[server] 第 1 个数据报 3 字节(来自 :47436): one
[server] 第 2 个数据报 16 字节(来自 :47436): two-long-message
```

## 5.13-B {#hw-5-13-b}

**难度 L3** · 题面见 [homework](homework#hw-5-13-b)

**思路**：`AF_UNIX` 的地址就是文件系统路径，`SOCK_DGRAM` 版一样有 UDP 的「按条收发」；bind 在路径上留下的 socket 文件不会随进程退出消失——和共享内存对象一个脾气。

1. 真跑：服务端 `recvfrom` 收到 11 字节 `hello-dgram` 并回信，客户端收到 `echo-dgram`——`AF_UNIX + SOCK_DGRAM` 把「同机高速」和「数据报语义」叠在一起。→ 知识点：[第 13 章：UDP 与本地域套接字](/05-system-programming/13-udp-and-unix-domain)「本地域套接字 AF_UNIX」一节（AF_UNIX 可用 SOCK_STREAM 或 SOCK_DGRAM）
2. 残留坑：对同一路径不 unlink 直接再 bind，`errno=98`（EADDRINUSE）——预测命中。socket 文件留在文件系统里，下次 bind 撞「路径已占」。→ 知识点：[第 13 章](/05-system-programming/13-udp-and-unix-domain)（socket 文件不会自动消失，要 `unlink`）
3. 纪律对照：`unlink(socket 文件)`、`shm_unlink(共享内存对象)`、`unlink(FIFO)` 是同一条「谁创建谁清理」的资源纪律在不同 IPC 上的三种名字。→ 知识点：[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)、[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（资源生命周期纪律一以贯之）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0113b.c -o hw0113b && ./hw0113b
[client] 收到回信: echo-dgram
[server] 通过 AF_UNIX DGRAM 收到 11 字节: hello-dgram
[server] 不 unlink 就再 bind: 失败, errno=98 (Address already in use) —— 残留文件占着路
```

## 5.14-A {#hw-5-14-a}

**难度 L2** · 题面见 [homework](homework#hw-5-14-a)

**思路**：`AF_UNSPEC` 让 getaddrinfo 把 localhost 能解析出的地址全给你——本机 IPv6 的 `::1` 和 IPv4 的 `127.0.0.1` 各一条；符号服务名走 `/etc/services` 查端口；错误走 `gai_strerror`。

1. 真跑：`localhost:http` 解析出两条——`::1:80` 和 `127.0.0.1:80`，端口 80 正是 `/etc/services` 里 `http` 的注册值。→ 知识点：[第 14 章：getaddrinfo](/05-system-programming/14-getaddrinfo)「getaddrinfo：填 hints、拿链表」一节（服务名参数、AF_UNSPEC）
2. 坏域名返回 `EAI_NONAME`，`gai_strerror` 打出 "Name or service not known"——`getaddrinfo` 有自己的一套错误码，不走 `errno`/`strerror`。→ 知识点：[第 14 章](/05-system-programming/14-getaddrinfo)「AI_PASSIVE」一节（`gai_strerror`）
3. 两条结果的意义：协议无关代码就是「遍历逐条试」——IPv6-only 环境里那条 `::1` 才是能用的，写死 IPv4 的老代码到那天就废了。→ 知识点：[第 14 章](/05-system-programming/14-getaddrinfo)「告别 gethostbyname」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0114a.c -o hw0114a && ./hw0114a
http → ::1:80
http → 127.0.0.1:80
坏域名 → 错误码 EAI_NONAME: Name or service not known
```

## 5.14-B {#hw-5-14-b}

**难度 L3** · 题面见 [homework](homework#hw-5-14-b)

**思路**：`AI_PASSIVE + host=NULL` 要的是「本机所有网卡」的通配地址，拿来 `bind`；客户端解析具体域名，拿来 `connect`——同一个 getaddrinfo，靠 hints 区分角色。

1. 真跑：服务端拿到的通配地址是 `0.0.0.0:0`（IPv4 的 INADDR_ANY；IPv6 环境下还可能有 `::`），bind 后 `getsockname` 查回实际端口 47179——通配地址绑「所有网卡」，具体连接来时再落到某个网卡上。→ 知识点：[第 14 章：getaddrinfo](/05-system-programming/14-getaddrinfo)「AI_PASSIVE：服务端通配绑定」一节
2. 客户端 `getaddrinfo("localhost", "47179")` 解析后 connect 成功，服务端收到 `hello-gai`——两端代码里**一个手写的 IP/端口常量都不剩**，全由解析结果驱动。→ 知识点：[第 14 章](/05-system-programming/14-getaddrinfo)（协议无关的标准写法：遍历结果逐条试、成功就用）
3. 两端都记得 `freeaddrinfo`：链表是 malloc 出来的，用完整条释放，和第 7 章 `shm_unlink` 一个纪律。→ 知识点：[第 14 章](/05-system-programming/14-getaddrinfo)（`freeaddrinfo` 别漏）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw0114b.c -o hw0114b && ./hw0114b
[server] AI_PASSIVE 给的通配地址: 0.0.0.0:0
[server] 实际绑定 0.0.0.0:47179
[server] 收到 10 字节: hello-gai
```

## 5.C-1 {#hw-5-c-1}

**难度 L4** · 题面见 [homework](homework#hw-5-c-1)

**思路**：事件循环里两种 fd 两种角色——`fd == listen_fd` 是「来新连接了」（accept + 注册），否则是「某条连接有事」（read/回显/EOF 摘除）；非阻塞 + EAGAIN 保证循环永远不被卡住。

1. 服务端骨架：监听 fd 设非阻塞，`epoll_wait` 就绪后 `accept` 到 EAGAIN 为止——非阻塞 accept 一次只接一个、接完回头再等，这是「循环 accept」的前提。→ 知识点：[第 12 章：进阶 Socket](/05-system-programming/12-socket-advanced)「epoll 并发服务端」一节、[第 10 章：非阻塞 IO 与 reactor](/05-system-programming/10-nonblock-and-reactor)（非阻塞 + EAGAIN）
2. 连接 fd 的处理：`read > 0` 用 `write_full` 回显（小消息也可能短写，循环凑齐）；`== 0` 是 EOF，`EPOLL_CTL_DEL + close`——漏了 DEL 就是第 10 章的烧 CPU 死循环。→ 知识点：[第 9 章：poll 与 epoll](/05-system-programming/09-poll-and-epoll)、[第 10 章](/05-system-programming/10-nonblock-and-reactor)「LT 模式下 EOF 的坑」一节
3. 真跑：三个客户端错峰连上，服务端 accept 三次、回显三次、EOF 关闭三次——注意连接 fd 三次都是 6，这是第 1 章「最小空闲号回收」规则在网络编程里继续生效。`signal(SIGPIPE, SIG_IGN)` 护住写路径。→ 知识点：[第 11 章：Socket TCP](/05-system-programming/11-socket-tcp)、[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)（fd 复用规则）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw01c1.c -o hw01c1 && ./hw01c1; echo "exit=$?"
[server] 监听 127.0.0.1:45879
[server] accept 新连接 fd=6(当前 1 条)
[server] fd=6 回显 20 字节
[client-0] 收到回显: hello-from-client-0
[server] fd=6 EOF, 摘除并关闭
[server] accept 新连接 fd=6(当前 1 条)
[server] fd=6 回显 20 字节
[client-1] 收到回显: hello-from-client-1
[server] fd=6 EOF, 摘除并关闭
[server] accept 新连接 fd=6(当前 1 条)
[server] fd=6 回显 20 字节
[client-2] 收到回显: hello-from-client-2
[server] fd=6 EOF, 摘除并关闭
[server] 3 条连接全部处理完毕: 共回显 3 次, 活跃连接 0
exit=0
```

服务端事件循环核心：

```c
while (closed < 3) {
    struct epoll_event events[MAX_EVENTS];
    int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        if (fd == listen_fd) {
            for (;;) {
                int cfd = accept(listen_fd, NULL, NULL);
                if (cfd < 0) {
                    break; /* EAGAIN:接完了 */
                }
                fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | O_NONBLOCK);
                struct epoll_event cev;
                cev.events = EPOLLIN;
                cev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                conns++;
            }
        } else {
            char buf[64];
            ssize_t k = read(fd, buf, sizeof(buf) - 1);
            if (k > 0) {
                write_full(fd, buf, (size_t) k); /* 原样回显 */
            } else if (k == 0) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                closed++;
            }
        }
    }
}
```

## 5.C-2 {#hw-5-c-2}

**难度 L4** · 题面见 [homework](homework#hw-5-c-2)

**思路**：管道流水线 = fork 两个子 + `dup2` 把 stdio 改道进管道两端 + 各自 exec；父进程那份 fd 是「第三个写端」和「第三个读端」，不关掉，消费者就永远等不到 EOF。

1. 子 A（生产者）：`dup2(pfd[1], STDOUT_FILENO)` 后关掉两个管道 fd（dup2 的新 fd 0/1 已指向目标，原 fd 该还就还），再 `execlp`。子 B（消费者）对称地接读端。→ 知识点：[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)「exec」一节、[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「dup 与 dup2」一节（重定向的底层原理）
2. 父进程 `close(pfd[0]); close(pfd[1]);` 是**生死线**：父不关写端，cons 的 `read` 读空数据后继续阻塞等 EOF——管线挂死。exec 后面跟的 `perror + _exit(127)` 是第 3 章的纪律：exec 失败不能掉头跑父进程的后半段。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（写端计数与 EOF）、[第 3 章](/05-system-programming/03-exec-and-wait)「exec 失败必须 _exit」一节
3. 真跑：cons 数出 100000 行、1088895 字节（`line-N\n` 的长度随位数增长，不是 7×100000），两个子进程退出码都 0——管道把 10 万行无损地从 prod 流到了 cons。→ 知识点：[第 6 章](/05-system-programming/06-pipe-and-fifo)（字节流 + EOF 语义）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw01c2_prod.c -o hw01c2_prod
$ gcc -std=c11 -Wall -Wextra hw01c2_cons.c -o hw01c2_cons
$ gcc -std=c11 -Wall -Wextra hw01c2.c -o hw01c2 && ./hw01c2
消费者: 共 1088895 字节, 100000 行
[管线] prod 退出码 0, cons 退出码 0
```

管线核心：

```c
int pfd[2];
pipe(pfd);

pid_t a = fork();
if (a == 0) {
    dup2(pfd[1], STDOUT_FILENO); /* stdout 改道进管道写端 */
    close(pfd[0]);
    close(pfd[1]);
    execlp("./hw01c2_prod", "hw01c2_prod", NULL);
    perror("exec prod");
    _exit(127);
}

pid_t b = fork();
if (b == 0) {
    dup2(pfd[0], STDIN_FILENO); /* stdin 改道成管道读端 */
    close(pfd[0]);
    close(pfd[1]);
    execlp("./hw01c2_cons", "hw01c2_cons", NULL);
    perror("exec cons");
    _exit(127);
}

close(pfd[0]); /* 父进程必须关掉自己那两份 fd,不然消费者永远等不到 EOF */
close(pfd[1]);
```

## 5.C-3 {#hw-5-c-3}

**难度 L5** · 题面见 [homework](homework#hw-5-c-3)

**思路**：乒乓的本质是「每轮一次上下文切换 + 几次内核进出」的接力；共享内存版的数据零拷贝、同步靠 futex 信号量，管道版每轮 4 次 syscall 还带内核缓冲搬运——预测前者明显快，真跑拉开 5 倍。

1. 共享内存 + 双信号量版：球就是共享内存里的一个 `uint64_t`，父 `sem_post(发球) + sem_wait(等球)`，子 `sem_wait + 计数 + sem_post`。真跑：200000 次往返 1306 ms，**每秒 15.3 万次往返**，最终 `ball=200000`——计数分毫不差就是同步正确的证明。→ 知识点：[第 7 章：IPC 下](/05-system-programming/07-shm-and-semaphores)（共享内存 + 信号量的标准搭配、`MAP_SHARED`）
2. 管道版：两个管道互传 1 字节，每轮 2 read + 2 write。真跑：同样 200000 次要 6807 ms，**每秒 2.9 万次**——约 5 倍的差距。→ 知识点：[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（pipe 的流模型有内核搬运开销）
3. 差异根源拆两半：其一，两版每轮都是 4 次 syscall（管道读写各 2 次、信号量 wait/post 各 2 次），**次数一样，干的活不同**——管道的 read/write 要走 VFS 层、要在内核缓冲区里搬字节、再经管道等待队列唤醒对端；futex 信号量只做「挂起自己/唤醒对方」的簿记，零数据搬运。其二，信号量在乒乓场景下每次 `sem_wait` 都必然阻塞（球在对方手里，计数是 0），「无竞争快路径」的优化完全失效——所以这里信号量也实打实进内核，差距纯粹来自「不搬数据 + 更轻的 syscall 路径」。→ 知识点：[第 7 章](/05-system-programming/07-shm-and-semaphores)（POSIX 信号量的阻塞语义）、[第 6 章](/05-system-programming/06-pipe-and-fifo)（PIPE_BUF 与内核缓冲）
4. 测量纪律：`clock_gettime(CLOCK_MONOTONIC)` 是教材外补充的 POSIX 单调时钟；吞吐数字每台机器不同，如实记录即可，可复现的是「shm 版快数倍」这个结构。

**验证输出**（本机 WSL Arch 一次真跑，数字如实）：

```text
$ gcc -std=c11 -Wall -Wextra hw01c3.c -o hw01c3 -lrt && ./hw01c3
shm+sem 乒乓 200000 次: 耗时 1306 ms, 每秒 153117 次往返, ball=200000
pipe 乒乓 200000 次: 耗时 6807 ms, 每秒 29380 次往返
```

乒乓核心（共享内存 + 双信号量版）：

```c
sem_t* sp = sem_open(sem_p, O_CREAT, 0600, 0); /* 初始 0:父先 sem_post 发球,子先 wait 接球——每轮都是真往返 */
sem_t* sc = sem_open(sem_c, O_CREAT, 0600, 0);

pid_t pid = fork();
if (pid == 0) {
    for (int i = 0; i < ROUNDS; i++) {
        sem_wait(sc); /* 接球 */
        (*ball)++;
        sem_post(sp); /* 扔回 */
    }
    _exit(0);
}

double t0 = now_ms();
for (int i = 0; i < ROUNDS; i++) {
    sem_post(sc); /* 发球 */
    sem_wait(sp); /* 等球回来 */
}
double dt = now_ms() - t0;
waitpid(pid, NULL, 0);
```
