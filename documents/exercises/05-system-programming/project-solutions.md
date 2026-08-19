---
title: "阶段 5 Project 参考实现"
description: "阶段 5 综合项目（mini shell）的完整参考实现：四层任务逐步讲解，每步标注知识点链接，含 Makefile、REPL、内置命令、管道重定向、后台作业与 SIGCHLD 收尸的真实运行输出，全部在 WSL Arch（gcc 16.1.1）验证。"
chapter: 5
order: 5
tags:
  - host
  - system-programming
  - posix
  - ipc
  - concurrency
difficulty: advanced
reading_time_minutes: 50
platform: host
c_standard: [11]
prerequisites:
  - "阶段 5 Project 题面"
related:
  - "阶段 5 各章"
---

# 阶段 5 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1）真实运行得到。参考实现只是**一种**过关方式；你的实现不一样、验收标准对得上，就都是对的。pid 数字每次运行不同，对结构（`[1] pid` 与 `jobs` 的状态迁移、退出码 127/2）而不是对数字。

## 核心任务（L2）：能跑起来的 REPL {#pj-core}

**思路**：REPL 的骨架是「读一行 → 切 argv → fork → 子 execvp → 父 waitpid 报告」；L1 热身就是先立住「读行 + 退出」的空循环，再往里填 fork/exec。

**`include/shell.h`**——契约：命令、作业的结构体与函数声明，include guard 起步。→ 知识点：[第 3 章：exec 家族与 wait](/05-system-programming/03-exec-and-wait)「引言」一节（fork+exec+wait 三位一体）

```c
#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>

#define MAX_ARGS 64
#define MAX_LINE 1024
#define MAX_JOBS 64

/* 一条解析后的命令:argv 数组 + 重定向目标 + 后台标志 */
struct command {
    char* argv[MAX_ARGS]; /* NULL 结尾 */
    char* in_file;        /* NULL = 不重定向输入 */
    char* out_file;       /* NULL = 不重定向输出 */
    int bg;               /* 1 = 后台作业 */
};

/* 一个后台作业的记账信息 */
struct job {
    pid_t pid;
    char cmdline[MAX_LINE];
    int running; /* 1 = 还在跑, 0 = 已收尸 */
    int status;  /* waitpid 给的打包状态 */
};

extern struct job jobs[MAX_JOBS];
extern int njobs;

int parse_line(char* line, struct command* cmds, int max_cmds);
int run_pipeline(struct command* cmds, int ncmds);
void reap_jobs(void);
int run_builtin(struct command* c); /* 返回 1 = 已处理(只在单命令、无重定向时被调用) */

#endif
```

**解析**——`strtok_r` 按空白切词，认 `|`、`>`、`<`、`&` 四类元字符；不支持引号是如实划界的边界。→ 知识点：[第 12 章：基础 IO](/01-c-basics/12-basic-io)（`strtok` 家族）、[第 3 章](/05-system-programming/03-exec-and-wait)「argv[0]」一节（argv 由调用者塞入）

```c
int parse_line(char* line, struct command* cmds, int max_cmds) {
    int n = 0;
    struct command* cur = &cmds[0];
    memset(cmds, 0, (size_t) max_cmds * sizeof(struct command));

    char* save = NULL;
    char* tok = strtok_r(line, " \t\r\n", &save);
    int argc = 0;
    while (tok) {
        if (strcmp(tok, "|") == 0) {
            cur = &cmds[++n]; /* 下一段 */
            argc = 0;
        } else if (strcmp(tok, ">") == 0 || strcmp(tok, "<") == 0) {
            int is_out = tok[0] == '>';
            tok = strtok_r(NULL, " \t\r\n", &save);
            if (!tok) {
                return 0;
            }
            if (is_out) {
                cur->out_file = tok;
            } else {
                cur->in_file = tok;
            }
        } else if (strcmp(tok, "&") == 0) {
            cur->bg = 1;
        } else {
            cur->argv[argc++] = tok;
        }
        tok = strtok_r(NULL, " \t\r\n", &save);
    }
    if (argc == 0 && n == 0) {
        return 0; /* 空行 */
    }
    return n + 1;
}
```

**执行单命令（核心层）**——子进程 `execvp` 失败后的两行是第 3 章的铁律：`perror + _exit(127)`，漏了 `_exit`，exec 翻车时子进程会掉头跑父进程的后半段。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)「exec 失败必须 _exit」一节、[第 2 章：进程的诞生](/05-system-programming/02-fork-cow-and-stdio-traps)（子进程用 `_exit` 的理由）

```c
pid_t pid = fork();
if (pid == 0) {
    signal(SIGINT, SIG_DFL); /* 子进程还原默认信号,见 L5 */
    signal(SIGQUIT, SIG_DFL);
    execvp(cmds[i].argv[0], cmds[i].argv);
    perror(cmds[i].argv[0]);
    _exit(127);
}
```

**状态报告**——父进程 `waitpid` 后走状态宏；0 退出安静、非 0 报码、被信号杀报信号号。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)「wait 与 waitpid」一节（`WIFEXITED`/`WEXITSTATUS`/`WIFSIGNALED`/`WTERMSIG`）

```c
if (WIFSIGNALED(last_status)) {
    printf("cjsh: %s 被信号 %d 杀死\n", cmds[ncmds - 1].argv[0], WTERMSIG(last_status));
} else if (WEXITSTATUS(last_status) != 0) {
    printf("cjsh: %s 退出码 %d\n", cmds[ncmds - 1].argv[0], WEXITSTATUS(last_status));
}
```

**`Makefile`**——变量 + 模式规则 + `.PHONY`。→ 知识点：[阶段 0 第 11 章](/00-dev-environment/12-make-basics)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude
LDFLAGS =

OBJS = src/main.o src/builtin.o

cjsh: $(OBJS)
	$(CC) $(CFLAGS) -o cjsh $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f cjsh src/*.o

.PHONY: clean
```

**验证输出**（会话 1，`no-such-command-xyz: Permission denied` 是 execvp 在 PATH 含不可检索目录时的真实 errno 现象，退出码照旧走 shell 的 127 约定；`./die_sigint` 是测试靶子，`raise(SIGINT)` 后默认动作终止）：

```text
$ make
gcc -std=c11 -Wall -Wextra -Werror -Iinclude -c src/main.c -o src/main.o
gcc -std=c11 -Wall -Wextra -Werror -Iinclude -c src/builtin.c -o src/builtin.o
gcc -std=c11 -Wall -Wextra -Werror -Iinclude -o cjsh src/main.o src/builtin.o
$ printf 'echo hello-from-cjsh\nno-such-command-xyz\n./die_sigint\necho alive-after-signal\nexit\n' | ./cjsh
hello-from-cjsh
no-such-command-xyz: Permission denied
cjsh: no-such-command-xyz 退出码 127
die_sigint: 给自己发 SIGINT
cjsh: ./die_sigint 被信号 2 杀死
alive-after-signal
```

## 进阶任务（L3）：内置命令 {#pj-avg}

**思路**：`cd` 改的是进程自己的 cwd——放进子进程跑，改的是子进程的、退出就没了；所以内置命令必须在 shell 进程里执行，这是「内置 vs 外部」的本质分界。

**`builtin.c` 核心**——`cd`（`chdir`）、`pwd`（`getcwd`）、`wait`（阻塞收全部子进程）。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（exec 保留 cwd——反过来说，改 cwd 必须发生在 shell 自己进程里）、[阶段 1 第 8 章](/01-c-basics/08-functions)（`static` 函数组织）

```c
int builtin_cd(char** argv) {
    const char* dir = argv[1];
    if (dir == NULL) {
        dir = getenv("HOME");
        if (dir == NULL) {
            printf("cd: 没有 HOME 可回\n");
            return 1;
        }
    }
    if (chdir(dir) < 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int builtin_pwd(char** argv) {
    (void) argv;
    char buf[512];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", buf);
    return 0;
}
```

**分发条件**——只在「单命令、无重定向、非后台」时试内置，其余一律走 `run_pipeline`。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（execvp 只认识外部程序）

```c
if (n == 1 && cmds[0].in_file == NULL && cmds[0].out_file == NULL && !cmds[0].bg) {
    if (run_builtin(&cmds[0])) {
        continue;
    }
}
run_pipeline(cmds, n);
```

**验证输出**（会话 2）：

```text
$ printf 'pwd\ncd /tmp\npwd\ncd /\npwd\nexit\n' | ./cjsh
/tmp/cj-ex5-pj
/tmp
/
```

## 再进阶任务（L4）：管道与重定向 {#pj-gates}

**思路**：N 段流水线 = N-1 根管道 + 每段子进程按位置 dup2；父进程的管道 fd 是「多出来的写端/读端」，不关就永远等不到 EOF。

**流水线执行**——第 i 段的 stdin 接上一段的读端、stdout 接下一段的写端；子进程里把两个管道 fd 都关掉再 exec（exec 保留 fd 表，不关会漏给下游程序）。→ 知识点：[第 1 章：文件 IO 与 fd](/05-system-programming/01-file-io-and-fd)「dup 与 dup2」一节、[第 6 章：IPC 上](/05-system-programming/06-pipe-and-fifo)（fork 后各自关掉不用的端）

```c
int run_pipeline(struct command* cmds, int ncmds) {
    pid_t stage_pids[MAX_JOBS];
    int prev_rd = -1;

    for (int i = 0; i < ncmds; i++) {
        int pfd[2] = {-1, -1};
        if (i < ncmds - 1) {
            if (pipe(pfd) < 0) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(prev_rd, STDIN_FILENO);
            }
            if (i < ncmds - 1) {
                dup2(pfd[1], STDOUT_FILENO);
            }
            if (prev_rd >= 0) {
                close(prev_rd);
            }
            if (pfd[0] >= 0) {
                close(pfd[0]);
            }
            if (pfd[1] >= 0) {
                close(pfd[1]);
            }

            if (cmds[i].in_file) {
                int fd = open(cmds[i].in_file, O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmds[i].out_file) {
                int fd = open(cmds[i].out_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror(cmds[i].argv[0]);
            _exit(127);
        }

        if (prev_rd >= 0) {
            close(prev_rd);
        }
        if (i < ncmds - 1) {
            close(pfd[1]); /* 父进程必须关写端,否则下游 read 等不到 EOF */
            prev_rd = pfd[0];
        }
        stage_pids[i] = pid;
    }
    /* 前台:循环 waitpid 到 ECHILD,取最后一段的状态汇报 */
    ...
}
```

**验证输出**（会话 3，注意 `ls /nonexistent-no-dir` 的报错去了 **stderr**（照实落在日志里），`wc -l` 数出 0 行、整条管线以最后一段的退出码 0 收场——管道不检查中间段，这是 shell 的本性）：

```text
$ printf 'echo hello > /tmp/cj-ex5/pj_out.txt\ncat /tmp/cj-ex5/pj_out.txt\ncat /etc/hostname | wc -c\nls /nonexistent-no-dir | wc -l\necho end-of-pipeline\nexit\n' | ./cjsh
hello
16
ls: cannot access '/nonexistent-no-dir': No such file or directory
0
end-of-pipeline
```

## 终极挑战（L5）：后台作业与作业控制 {#pj-l5}

**思路**：作业表是「记账」，SIGCHLD 处理器是「报信」，`reap_jobs` 是「收尸」——三件拆开，处理器才能保持最小（只设标志），printf 之类的不安全调用全留在主循环里。改编自 CSAPP shell lab（tsh）的作业控制要求，省略了终端进程组控制（`tcsetpgrp` 那套是教材外内容，不要求）。

**作业表 + 后台启动**——`&` 的流水线不 wait，登记 pid、命令行、运行状态后立刻回来。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)、[第 4 章：守护进程与孤儿](/05-system-programming/04-daemons-and-orphans)（后台 = 不阻塞收尸，但必须有别的机制收）

```c
if (cmds[0].bg) {
    jobs[njobs].pid = stage_pids[ncmds - 1];
    jobs[njobs].running = 1;
    jobs[njobs].status = 0;
    build_cmdline(&cmds[0], jobs[njobs].cmdline, sizeof(jobs[njobs].cmdline));
    printf("[%d] %d\n", njobs + 1, (int) stage_pids[ncmds - 1]);
    njobs++;
    return 0;
}
```

**SIGCHLD 三件套**——处理器只设 `volatile sig_atomic_t` 标志（`printf` 不是 async-signal-safe，绝不能进处理器）；`SA_RESTART` 让阻塞在 `fgets` 里的 read 自动重启、不丢输入行；主循环「读完命令后立刻 `reap_jobs`」，用 `WNOHANG` 循环收尸、更新作业表。→ 知识点：[第 5 章：信号](/05-system-programming/05-signals)「处理器里能做什么」一节（async-signal-safe、errno 纪律）、[第 3 章](/05-system-programming/03-exec-and-wait)「僵尸进程」一节（SIGCHLD + WNOHANG 是标准收尸法）

```c
static volatile sig_atomic_t child_changed = 0;

static void on_sigchld(int sig) {
    (void) sig;
    child_changed = 1; /* 只设标志,收尸留给主循环 */
}

void reap_jobs(void) {
    child_changed = 0;
    for (;;) {
        int st;
        pid_t dead = waitpid(-1, &st, WNOHANG);
        if (dead <= 0) {
            break; /* 没有死孩子了 */
        }
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].pid == dead && jobs[i].running) {
                jobs[i].running = 0;
                jobs[i].status = st;
                if (WIFEXITED(st)) {
                    printf("[%d] %d 已完成(退出码 %d) %s\n", i + 1, (int) dead,
                           WEXITSTATUS(st), jobs[i].cmdline);
                } else if (WIFSIGNALED(st)) {
                    printf("[%d] %d 被信号 %d 杀死 %s\n", i + 1, (int) dead, WTERMSIG(st),
                           jobs[i].cmdline);
                }
                break;
            }
        }
    }
}

/* main 里装处理器:SA_RESTART 别漏 */
struct sigaction sa;
sa.sa_handler = on_sigchld;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART;
sigaction(SIGCHLD, &sa, NULL);

/* 主循环:读完命令后立刻收尸,jobs 才不会读到上一轮的老状态 */
if (child_changed) {
    reap_jobs();
}
```

**信号姿势**——shell 自己忽略 `SIGINT`，但子进程 fork 后立刻还原成 `SIG_DFL`：被忽略的信号会跨 fork+exec **继承**，不还原的话 `Ctrl-C` 永远杀不动前台命令（CSAPP 作业控制的经典细节）。→ 知识点：[第 5 章](/05-system-programming/05-signals)（信号处理继承规则）

```c
signal(SIGINT, SIG_IGN);  /* shell 自己不吃 Ctrl-C */
/* 子进程里 */
signal(SIGINT, SIG_DFL);  /* 但前台命令要能被打断 */
signal(SIGQUIT, SIG_DFL);
```

**`jobs`/`wait` 内置**——作业表是纯记账，`wait` 阻塞 `waitpid(-1)` 到 `ECHILD` 后刷新作业表。→ 知识点：[第 3 章](/05-system-programming/03-exec-and-wait)（循环收到 ECHILD）

```c
int builtin_wait_all(char** argv) {
    (void) argv;
    for (;;) {
        int st;
        pid_t r = waitpid(-1, &st, 0);
        if (r < 0) {
            break; /* ECHILD:没有子进程了 */
        }
    }
    reap_jobs();
    printf("wait: 所有后台作业已结束\n");
    return 0;
}
```

**验证输出**（会话 4，时间线：`[1]` 启动 → `jobs` 先「运行中」→ SIGCHLD 报信、主循环收尸打「已完成」→ `[2]` 的 `ls` 失败退出码 2 → `wait` 阻塞到全部结束 → `jobs` 终态。这条时间线是驱动脚本用 sleep 控制的真实调度）：

```text
$ { echo 'sleep 0.4 &'; sleep 0.2; echo 'jobs'; sleep 0.6; echo 'jobs'; \
    echo 'ls /definitely-not-there &'; sleep 0.6; echo 'jobs'; \
    echo 'wait'; echo 'jobs'; echo 'exit'; } | ./cjsh
[1] 686
[1] 686 运行中 sleep 0.4 &
[1] 686 已完成(退出码 0) sleep 0.4 &
[1] 686 已完成(退出码 0) sleep 0.4 &
[2] 689
ls: cannot access '/definitely-not-there': No such file or directory
[2] 689 已完成(退出码 2) ls /definitely-not-there &
[1] 686 已完成(退出码 0) sleep 0.4 &
[2] 689 已完成(退出码 2) ls /definitely-not-there &
wait: 所有后台作业已结束
[1] 686 已完成(退出码 0) sleep 0.4 &
[2] 689 已完成(退出码 2) ls /definitely-not-there &
```

三行「已完成(退出码 0)」分别来自：收尸打印、`jobs` 表、`wait` 后的 `jobs` 表——作业表里每一行状态迁移都真实可见。到这里，一个能跑外部命令、有内置命令、搭得起管道、管得住后台作业的 mini shell 就立住了；它和真 shell 的差距只剩词法（引号）、通配、环境变量语法和终端进程组控制，那些已经超出本阶段的地图。
