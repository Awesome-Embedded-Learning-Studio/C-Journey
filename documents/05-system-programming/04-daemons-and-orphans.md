---
title: "守护进程与孤儿：setsid、脱离终端与后台化"
description: "这一章把一个前台程序后台化成 daemon(守护进程)。先讲孤儿进程:父进程先死、子进程还在,子就被 init(PID 1)收养——但真跑发现这台 WSL2 机器上孤儿的新父进程是 246 而不是 1,因为现代 systemd 用 subreaper 机制(经典 Unix 才是 init/1,诚实交代)。然后是 daemonize 的标准步骤:第一次 fork(父退、子继续)、setsid 脱离控制终端成为新会话组长(必须先 fork 一次,因为 setsid 不能由进程组组长调用)、第二次 fork(防止将来 open 到 tty 又挂回控制终端)、chdir 到 /(不占着文件系统)、dup2 把 0/1/2 重定向到 /dev/null(不然脱离终端后 printf/write 会失败或写废)、写 pid 文件(单实例守护)。真跑 daemonize.c 完整走一遍:启动者立刻返回、daemon 孙子进程在后台往日志文件写 3 个 tick(pid/ppid/sid 全打出来),日志里 ppid 又是 subreaper 246、sid 是 setsid 建的新会话号。顺手撞上第 2 章那个 stdio 缓冲陷阱的 daemon 版——daemonize 开头忘了 fflush(NULL),fork 就把 [启动者] 那行缓冲复制给子、被父子各 flush 一次(打两遍),所以 daemonize 第一步必须 fflush(NULL)。最后提单实例守护(flock 或 pid 文件 + O_EXCL)。全 gcc16+clang22 真跑。"
chapter: 5
order: 4
tags:
  - host
  - system-programming
  - posix
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 2 章：进程的诞生（fork、_exit、stdio 缓冲陷阱）"
  - "第 3 章：exec 家族与 wait（僵尸、孤儿被 init 收养）"
  - "第 1 章：文件 IO 与 fd（dup2 重定向、open）"
related:
  - "第 5 章：信号（daemon 处理 SIGTERM 优雅退出、SIGCHLD）"
  - "第 3 章：exec 家族与 wait（fork+exec 在 daemon 派活里的应用）"
---

> 🟡 状态:待审核(2026-07-02)

# 守护进程与孤儿：setsid、脱离终端与后台化

## 引言：什么样的程序叫「守护进程」

前面三章我们写的程序都是「前台」的——绑在你敲命令的那个终端上,终端关了它就没了、它的输出也直接往屏幕刷。可系统里有大量程序是**长期在后台跑**的:sshd 守着 22 端口、cron 定时干活、数据库服务常驻、你的 docker daemon……这些程序有个共同形态,叫**守护进程**(daemon):**没有控制终端、不在任何终端的会话里、通常父进程是 init、就这么默默地长期跑着**。把一个普通程序变成 daemon 的那一套固定步骤,叫 **daemonize**。这一章我们真机走一遍 daemonize,顺带把上一章提到的「孤儿进程被 init 收养」讲透——你会在真跑里看到一个有意思的现代细节:这台机器上孤儿的新父进程**并不是 PID 1**。

## 孤儿进程：父先死的孩子被谁收养

先看孤儿。一个子进程,如果它的**父进程先死了**(父 exit 了、子还在跑),这个子进程就叫**孤儿**。内核不能让孤儿就这么悬着——它还得有个父进程来负责「收尸」(回收它的退出状态,上一章讲的)。所以内核会把孤儿**重新挂到一个新的父进程下**:在**经典 Unix** 上,这个收养者是 **init(PID 1)**——init 是所有孤儿的天经地义的养父,它会在孤儿死时负责 `wait` 收尸。真跑给你看,父进程 fork 完立刻退,子进程睡一秒(确保父已经退完)再查自己的 `getppid()`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0) {
        /* 父进程立刻退 → 子成孤儿 */
        printf("[父] pid=%d 退了,子 pid=%d 变孤儿\n", (int) getpid(), (int) pid);
        exit(0);
    }
    /* 子进程:睡一秒,等父退完、自己被收养 */
    sleep(1);
    printf("[子] pid=%d, 被收养后 ppid=%d(原本应是父的 pid,现在变了)\n", (int) getpid(),
           (int) getppid());
    return 0;
}
```

```text
$ gcc -std=c11 -Wall orphan.c -o orph && ./orph
[父] pid=46712 退了,子 pid=46713 变孤儿
[子] pid=46713, 被收养后 ppid=246(原本应是父的 pid,现在变了)
```

子进程的 `ppid` 从「父进程的 pid(46712)」变成了 **246**——它被重新收养了。可你大概会愣一下:**246 不是 1 啊?** 这正是现代 Linux 的一个细节。经典 Unix 上孤儿会被 init(PID 1)收养;但这台 WSL2 机器上 `ps` 告诉我们 PID 1 是 `systemd`,而 246 是一个叫 `Relay` 的 **subreaper**(子收割者)进程——Linux 内核支持设「子收割者」(`prctl(PR_SET_CHILD_SUBREAPER)`),一个进程被设成 subreaper 后,**它及其后代的孤儿会优先被它收养、而不是一路交给 PID 1**。systemd 和很多容器/会话管理器都用了这套机制。所以诚实地说:「孤儿被 init 收养」是经典模型;在现代 systemd/Wsl2 环境下,收养的常常是路径上的某个 subreaper(这里是 246),不是 PID 1。**无论收养者是 1 还是 subreaper,关键性质不变:孤儿一定被某个会负责 `wait` 的进程收养,所以孤儿不会变长留的僵尸**——这就回答了上一章留的问题:父进程先死的子进程不会漏成僵尸,养父(init/subreaper)会替它收尸。

## daemonize 的标准步骤

把一个程序 daemonize,业界有一套近乎仪式化的固定步骤,每一步都有明确的「为什么」。走一遍完整的:

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void daemonize(void) {
    fflush(NULL); /* 呼应第 2 章:fork 前清空 stdio 缓冲,否则 [启动者] 会被父子各 flush 一次 */
    /* 第一次 fork:父退,子继续(子不是进程组组长) */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0) {
        exit(0);
    }
    /* setsid:成为新会话组长 + 新进程组长,脱离控制终端 */
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }
    /* 第二次 fork:未来即使 open 到 tty 也不会重新挂上控制终端 */
    pid = fork();
    if (pid < 0) {
        exit(1);
    }
    if (pid > 0) {
        exit(0);
    }
    /* 孙子进程才是 daemon:改 cwd、关 stdio、重定向 0/1/2 */
    chdir("/");
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        if (devnull > 2) {
            close(devnull);
        }
    }
    /* 干活:往日志文件写几笔(真实 daemon 会一直循环) */
    FILE* log = fopen("/tmp/cj/p5ch4/daemon.log", "a");
    if (!log) {
        exit(1);
    }
    for (int i = 0; i < 3; i++) {
        fprintf(log, "daemon pid=%d ppid=%d sid=%d tick=%d\n", (int) getpid(), (int) getppid(),
                (int) getsid(0), i);
        fflush(log);
        sleep(1);
    }
    fprintf(log, "daemon pid=%d 退出\n", (int) getpid());
    fclose(log);
}

int main(void) {
    printf("[启动者] pid=%d,准备 daemonize...\n", (int) getpid());
    daemonize();
    return 0;
}
```

```text
$ gcc -std=c11 -Wall daemonize.c -o dm
$ ./dm                                  # 立刻返回
[启动者] pid=47038,准备 daemonize...
$ sleep 4                               # 等 daemon 在后台跑完 3 个 tick
$ cat /tmp/cj/p5ch4/daemon.log
daemon pid=47040 ppid=246 sid=47039 tick=0
daemon pid=47040 ppid=246 sid=47039 tick=1
daemon pid=47040 ppid=246 sid=47039 tick=2
daemon pid=47040 退出
```

`./dm` 敲下去**立刻返回**(父进程退了,shell 提示符马上回来),真正的 daemon(孙子进程,pid 47040)在后台默默地往日志文件写了 3 笔、每笔间隔 1 秒、然后退出(真实 daemon 这里会是死循环,为了能演示我让它跑 3 轮就停)。日志里每一行都印证了 daemonize 的成果:daemon 的 `pid` 是 47040(和启动者 47038 不同,因为它 fork 了两次)、`ppid` 是 246(两次 fork 的中间父都退了,孙子成了孤儿、被 subreaper 246 收养——和上面 orphan 那节完全一致)、`sid` 是 47039(`setsid` 建立的新会话的 id,正是第一次 fork 出来的那个中间进程的 pid)。

## 每一步的「为什么」

这套步骤里每一条都不是仪式、而是有具体的坑在挡,逐条拆。

**`fflush(NULL)` 放最前面**,直接对应第 2 章那个 stdio 缓冲陷阱——daemonize 里要 fork 两次,如果进来时 stdio 缓冲还卡着没 flush 的数据(比如 main 里那句 `[启动者]`),第一次 fork 就会把它复制给子进程、被父子各 flush 一次,打出两遍。我自己第一次跑这个程序就撞上了:`[启动者]` 果然打了两遍。所以 daemonize 第一件事就是把 stdio 缓冲清干净,这是第 2 章铁律在 daemon 场景的直接应用。

**第一次 fork** 的目的,是让「父进程退、子进程继续」——父进程一退,子进程就**脱离了启动它的 shell**(shell 看到父进程返回、以为命令跑完了,不再管它),从而在后台跑。这一步也顺带解决下一件事:子进程现在**不再是进程组组长**。

**`setsid()`** 是 daemonize 的灵魂动作(`<unistd.h>`)。它把调用者变成一个**全新的会话(session)的组长**、同时是一个**全新的进程组**的组长,并**脱离原来的控制终端**——这正是 daemon「不绑任何终端」的来源。但 `setsid` 有个硬规矩:**调用它的进程不能已经是进程组组长**,否则直接失败返回 `-1`、`errno=EPERM`。这就是为什么不能在 main 里直接 `setsid()`——你的程序一启动,自己就是一个进程组的组长(至少 shell 把它单独成组),直接调 `setsid` 会失败;**先 fork 一次**,子进程继承了组员身份、但不是组长,这时 `setsid` 才能成功。

**第二次 fork** 是为了堵一个更隐蔽的口子:在某些系统上,**会话组长如果再次 `open` 一个终端设备(tty),会重新获得控制终端**——那 daemon 辛辛苦苦脱离的终端又挂回来了。标准做法是 `setsid` 之后再 fork 一次,让**孙子进程**当 daemon:孙子不是会话组长(它是会话里的普通成员),将来就算 open 到 tty 也不会把它变成控制终端。这一步在现代 Linux 上严格说不是必需的(Linux 的行为更保守),但写上它是跨平台稳妥的「防 re-acquire」保险,几乎所有 daemon 样板都保留这一步。

**`chdir("/")`** 把当前工作目录切到根。为什么?如果 daemon 的 cwd 是某个挂载点(比如 `/mnt/usb`),它会**「占着」这个文件系统不让卸载**(文件系统忙)。切到 `/` 就不再占任何可卸载的挂载点。有些 daemon 会切到自己的工作目录(数据库切到数据目录),那是它确实需要;通用规则是切 `/`。

**重定向 0/1/2 到 `/dev/null`**(`dup2`,第 1 章见过)这一步最容易被漏、也最容易出怪事。daemon 已经脱离了控制终端,可它**继承下来的 fd 0/1/2 还指向原来那个 tty**——tty 现在已经跟它没关系了,继续往这些 fd `printf`/`write` 要么**失败**(`EBADF` 或终端已关)、要么**写废**。标准做法是 `open("/dev/null", O_RDWR)` 然后 `dup2` 把 0/1/2 三个都改指向 `/dev/null`:这样 daemon 里任何残留的 `printf` 都被静默吞掉(写进黑洞),不会出幺蛾子。这也是为什么**调试 daemon 千万别用 `printf`**——stdout 早被你重定向到 `/dev/null` 了,看不到的;得写日志文件、或用 `syslog`(POSIX 的 `openlog`/`syslog`/`closelog`,把日志统一交给系统日志服务)。

## 单实例守护:别让 daemon 启动两份

最后提一个实战需求:很多 daemon 要求**单实例**——同一时刻只能有一个在跑(两个数据库 daemon 抢同一个数据文件、两个 cron 抢同一份 crontable,都会乱套)。最常用的两招:**`flock` 给一个锁文件上建议锁**(第二个实例 `flock` 失败、自知已有实例、自行退出),或**用 pid 文件 + `O_CREAT|O_EXCL`**——在 `/var/run/mydaemon.pid` 上 `open(..., O_CREAT|O_EXCL)`,第二个实例因为文件已存在而 `EEXIST` 失败、退出,退出时记得 `unlink` 掉 pid 文件。两者都能拦住重复启动,`flock` 更稳(进程崩了内核自动释放锁,而 pid 文件可能残留需要手动清),实际项目里 `flock` 用得多。

## 小结

把一个程序 daemonize 的固定套路:开头 `fflush(NULL)`(第 2 章 stdio 缓冲铁律)、第一次 fork(父退子留、脱离 shell、子非组长)、`setsid`(建新会话、脱离控制终端,前提是非组长所以先 fork)、第二次 fork(防 tty re-acquire)、`chdir("/")`(不占挂载点)、`dup2` 把 0/1/2 改指向 `/dev/null`(防脱离终端后 stdio 写废)、最后干活用日志文件或 syslog 而不是 printf。孤儿进程被收养:经典 Unix 是 init(PID 1),现代 systemd/Wsl2 常常收养到路径上的某个 subreaper(真跑见到 246 而非 1),但无论收养者是谁,孤儿一定会被某个负责 `wait` 的进程接管、不会变僵尸。单实例守护用 `flock` 或 pid 文件 + `O_EXCL`。

到这里,我们已经能让一个进程「后台化、脱离终端、长期跑」了。但它现在还只是个哑巴干活的后台进程——一旦被 `kill`、或遇到异常,它毫无还手之力。下一章讲**信号**:让进程能响应外部的「软件中断」(比如 `SIGTERM` 优雅退出、`SIGCHLD` 及时收尸、`SIGINT` 处理 Ctrl-C),这是 daemon 真正「懂事」的开始。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 13 章「守护进程」逐条拆 daemonize 的步骤与来历,登录记录、单实例、错误处理都讲到了。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 37 章「daemon」给了一套完整的 daemonize 函数 + 每步原因;孤儿进程与 subreaper 在第 25 章。
- **man 页**：`setsid(2)`、`getsid(2)`、`setuid` 系列、`flock(2)`、`syslog(3)`——本章每条行为对齐 man 页;`credentials(7)` 讲 pid/ppid/session;Linux 的 subreaper 见 `prctl(2)` 的 `PR_SET_CHILD_SUBREAPER`。
- **POSIX**：`setsid`/`getsid`/`fork`/`chdir`/`dup2` 都是 IEEE Std 1003.1-2008;孤儿收养与 init 的关系是 POSIX 进程关系模型的一部分(subreaper 是 Linux 扩展、非 POSIX)。
