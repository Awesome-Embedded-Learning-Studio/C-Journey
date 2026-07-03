---
title: "exec 家族与 wait：把进程换成另一个程序并正确收尸"
description: "fork 给了我们两个进程,可它们跑的是同一份代码——真正让多进程有用的是这一章的两件大事:让子进程换一个程序去跑(exec)、让父进程正确回收跑完的子进程(wait)。先用 execlp 真跑 fork+exec 标准模式(子进程整个被 ls 替换、execlp 走 PATH 查找),讲清 exec 最反直觉的一条——成功就无返回(进程镜像整个被换掉),失败才返回 -1,所以 exec 后面必须紧跟 perror + _exit,否则子进程会掉头继续跑父进程的后半段代码(真跑这个经典 bug:漏 _exit 时『main 末尾』被打两遍)。再真跑 execv 传 argv 数组——argv[0] 想叫啥就叫啥(不是程序路径)、环境变量跨 exec 自动继承、exec 前后 pid 一模一样(进程没换、只是代码段/数据段/堆栈全换了,fd/cwd/信号屏蔽保留)。然后是 wait/waitpid + 一套状态宏:WIFEXITED/WEXITSTATUS 读正常退出码、WIFSIGNALED/WTERMSIG 读被信号打死(真跑子 exit(42) → 退出码 42、子 raise(SIGTERM) → 信号 15)。最后兑现上一章的僵尸伏笔——子先死、父不 wait 就变僵尸(Z/defunct),wait 就是收尸动作,顺带说清 SIGCHLD。全 gcc16+clang22+ASan 真跑。"
chapter: 5
order: 3
tags:
  - host
  - system-programming
  - posix
difficulty: intermediate
reading_time_minutes: 17
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 2 章：进程的诞生（fork 的两次返回、wait(NULL) 预览、僵尸进程伏笔）"
  - "第 1 章：文件 IO 与 fd（fd 的概念、_exit 与 exit）"
  - "第 12 章：基础 IO（main 的 argc/argv）"
related:
  - "第 4 章：守护进程与孤儿（fork+setsid 后台化、fork+exec 的 daemon 实战）"
  - "第 5 章：信号（SIGCHLD、子进程被信号杀死的进一步处理）"
---

# exec 家族与 wait：把进程换成另一个程序并正确收尸

## 引言：两个进程，却跑同一份代码

上一章我们用 `fork` 生出了第二个进程,可很快你会发现一个尴尬——**父子俩跑的是同一份代码**。`fork` 之后,子进程不过是把父进程的程序又从头到尾跑了一遍(`fork` 返回处分岔、各自走 `if` 的不同分支而已),它并不能让子进程去执行**另一个程序**。可真实场景里,shell 敲一个 `ls`,shell 干的事恰恰是「fork 出一个子进程、再让它**变成 ls** 去跑」——这就需要第二条腿:**`exec`**。再就是,fork 出来的子进程跑完会变成什么?如果父进程不管它,它就赖成「僵尸」(上一章的伏笔)——这就需要第三条腿:**`wait`** 收尸。`fork` + `exec` + `wait` 是 Unix 多进程编程的「三位一体」,这一章把后两位讲透。

## exec：把进程整个换成另一个程序

`exec` 不是某一个函数,而是一**家族**(头 `<unistd.h>`)。它们干的事听起来很暴力:**把当前进程的整个内存镜像(代码段、数据段、堆、栈)全部替换成另一个程序文件的内容,然后从那个程序的 `main` 开始重新执行**。注意是「替换」不是「新建」——**进程还在、pid 不变,只是它肚子里跑的程序换了一个**。`exec` 家族最常用的几个长这样,命名有规律后面专门讲,这里先用 `execlp`:

```c
int execlp(const char* file, const char* arg0, ..., NULL);
```

`execlp` 的第一个参数是程序**名字**(不是完整路径)——它会自己去 `PATH` 环境变量列的目录里找;后面跟一串参数,**列表式**一个个写,最后必须用 `NULL` 收尾(告诉它参数到此为止)。来看看 fork+exec 的标准模式,子进程 fork 出来后立刻 `execlp("ls", ...)`,于是子进程整个变成了 `ls`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        /* execlp:列表式参数 + 走 PATH 查找 */
        execlp("ls", "ls", "lsdir", NULL);
        perror("exec ls");
        _exit(127); /* exec 失败才走到这里 */
    }
    waitpid(pid, NULL, 0);
    printf("[父] ls 跑完了\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall exec_ls.c -o els && ./els
alpha.txt
beta.txt
[父] ls 跑完了
```

`alpha.txt`、`beta.txt` 这两行是**子进程变成 `ls` 之后、由 `ls` 打出来的**(`lsdir` 是预先建好的目录)——子进程在 `execlp` 那一刻整个被 `ls` 替换,`ls` 跑完退出,父进程的 `waitpid` 才返回,最后打出「ls 跑完了」。子进程**没有**跑父进程 `main` 里 `execlp` 之后的任何代码(`perror`、`_exit`、`printf`),因为它在 `execlp` 成功的那一刻就已经不是原来的程序了——这正是 `exec` 最反直觉、也最该记牢的一条:**成功就无返回**。`exec` 只有在**失败**时才返回 `-1`(比如程序文件不存在、没权限),成功的话控制权直接交给新程序、永远不回来。

所以 `execlp` 后面那两行 `perror` 和 `_exit(127)` 看着像「正常后续」,其实是**失败兜底**——只有 `exec` 失败才会执行到它们。`127` 是约定俗成的「exec 失败退出码」(shell 见到子进程返回 127 就知道是「命令找不到」)。这两行不能省,省了就是下一节的大坑。

## exec 失败必须 _exit：一个「会跑两遍」的经典 bug

上一章我们说子进程常常用 `_exit` 退出,当时留了个「为什么」没讲——答案就在这里。设想你照着上面写了 `fork`+`execlp`,可**忘了在 exec 失败时 `_exit`**。`exec` 成功时倒没事(控制权不回来);可万一 `exec` **失败**了(比如把程序名写错),它返回 `-1`,然后呢?**子进程会顺着 `main` 继续往下跑**——可 `main` 后面是什么?是父进程的后半段代码!于是子进程鬼使神差地把父进程的活又干了一遍。这个 bug 极其常见、又极其难查。真跑一遍给你看,用 mode 参数区分「漏了 `_exit` 的 bug 版」和「修好版」:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int fixed = (argc > 1) ? atoi(argv[1]) : 0;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        execlp("totally-nonexistent-program", "totally-nonexistent-program", NULL);
        perror("exec 失败");
        if (fixed) {
            _exit(127); /* 修好版:失败立刻退 */
        }
        /* bug 版:漏了 _exit → 掉下去执行父进程的后半段! */
    } else {
        waitpid(pid, NULL, 0);
    }

    /* 这段是「父进程的后半段」——bug 版的子进程会掉到这里 */
    printf("[pid=%d] 跑到了 main 末尾\n", (int) getpid());
    return 0;
}
```

```text
$ gcc -std=c11 -Wall exec_fail.c -o ef
$ ./ef 0        # 漏了 _exit 的 bug 版
exec 失败: No such file or directory
[pid=43909] 跑到了 main 末尾
[pid=43908] 跑到了 main 末尾
$ ./ef 1        # 修好版(exec 失败即 _exit)
exec 失败: No such file or directory
[pid=43910] 跑到了 main 末尾
```

(pid 数字每次跑不同,这里盯行数就行。)bug 版(`./ef 0`)的 `exec` 失败了、没 `_exit`,子进程**掉到了 `main` 末尾的 `printf`**,于是「跑到了 main 末尾」**被打了两遍**——一遍来自父进程(pid 43908)、一遍来自那个本该变成别的程序、却因 exec 失败而「漏下来」的子进程(pid 43909)。修好版(`./ef 1`)在 `exec` 失败后立刻 `_exit(127)`,子进程干净退场,「main 末尾」**只打一遍**。

这就回答了上一章的悬念:**子进程 exec 之后必须用 `_exit` 退出失败路径**(用 `exit` 也行,但 `_exit` 更地道——`exit` 会 flush stdio 缓冲,而 exec 失败的子进程没什么好 flush 的,而且我们不想让它碰父进程复制过来的那套 stdio 状态)。纪律一句话——**`exec` 后面永远紧跟 `perror` + `_exit`**,把它当成 exec 调用的一部分、不可分割。漏了它,你的子进程就会在 exec 翻车时偷偷跑回父进程的赛道上继续狂奔。

## argv[0]、环境变量与「exec 不改 pid」

换一个角度想:`exec` 把进程换成另一个程序时,新程序的 `main(int argc, char** argv)` 拿到的 `argv` 是谁给的?答案:**全是 exec 调用者塞进去的**。`argv[0]` 并不非得是程序路径,你想叫它啥就叫它啥;`argv[1..]` 也是调用者列出来的参数。这一点跟「直接在 shell 里敲命令」完全一样——shell 也是 exec 时把命令行参数塞进 argv 的。环境变量(`environ` / `getenv`)则默认从父进程**继承**过来,exec 不动它(除非你用带 `e` 后缀的 `execve`/`execle` 主动传一份新的)。

写一个 `printer` 程序当「靶子」,让它打印自己的 argv、读一个环境变量、再报自己的 pid;然后父进程 fork、用 `execv` 把它换上来,顺手给 `argv[0]` 起个假名:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("[printer] pid=%d, argc=%d\n", (int) getpid(), argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    printf("  MY_VAR = %s\n", getenv("MY_VAR") ? getenv("MY_VAR") : "(未设置)");
    return 0;
}
```

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    setenv("MY_VAR", "from-parent", 1);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        /* execv:数组式参数;argv[0] 想叫啥就叫啥 */
        char* args[] = {"custom-argv0", "aaa", "bbb", NULL};
        execv("./printer", args);
        perror("exec printer");
        _exit(127);
    }

    waitpid(pid, NULL, 0);
    /* 子的 pid 是 fork 给的;printer 跑起来 getpid() 该和它一样 → exec 不改 pid */
    printf("[父] 子的 pid=%d,跟上面 printer 的 pid 一致 → exec 不改 pid\n", (int) pid);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall printer.c -o printer
$ gcc -std=c11 -Wall exec_argv.c -o eav && ./eav
[printer] pid=44238, argc=3
  argv[0] = custom-argv0
  argv[1] = aaa
  argv[2] = bbb
  MY_VAR = from-parent
[父] 子的 pid=44238,跟上面 printer 的 pid 一致 → exec 不改 pid
```

`printer` 跑起来拿到的 `argv[0]` 是 `custom-argv0`——**不是** `./printer`,因为我用 `execv` 时在 `args[0]` 里就这么写的(很多程序靠 `argv[0]` 知道「自己叫什么名字」,shell 的BusyBox 多功能二进制就是靠这个分流的)。`argc=3`、`argv[1]/argv[2]` 也都是我塞的 `aaa`/`bbb`。`MY_VAR = from-parent`——父进程 `setenv` 设的环境变量,**跨 exec 自动继承**(父进程的 `environ` 整个传给了新程序)。最妙的是最后一行:`printer` 报自己的 pid 是 `44238`,父进程拿到的子的 pid 也是 `44238`——**两者完全一致**,这就有力地证明了「exec 不改 pid」:进程还是那个进程(内核的进程表项、pid、父进程关系都没动),只是它运行的程序从 `exec_argv` 换成了 `printer`。

顺带把 exec 之后「**保留了什么、换了什么**」一次说清。**换掉的**:代码段、数据段(已初始化/未初始化全局)、堆、栈——整个内存镜像连同原来的变量值通通被新程序覆盖(所以 COW 复制下来的父进程变量,exec 之后一笔勾销、看不到了)。**保留的**:pid、父进程关系、文件描述符表(默认全部保留,这就是为什么上一章讲 `FD_CLOEXEC`——不希望某个 fd 漏给 exec 后的程序,就得用 `fcntl` 设上 `FD_CLOEXEC`,或用 `dup2` 的近亲 `F_DUPFD_CLOEXEC`)、当前工作目录(cwd)、信号屏蔽字、累计的 CPU 时间。所以一个程序 exec 成另一个之后,它打开的 fd 还在、cwd 还在、pid 还是原来那个,只是跑的代码全变了。

## wait 与 waitpid：正确收尸 + 解析退出状态

子进程跑完了(无论是 `ls` 跑完、还是自己 `exit`、还是被信号打死),它并不会凭空消失——它的「尸体」(退出状态信息:退出码、或被哪个信号杀的、资源使用统计)还得留一段时间,**等父进程来「收尸」(reap)**。父进程收尸的动作就是 `wait` 或 `waitpid`(`<sys/wait.h>`):

```c
pid_t wait(int* status);                            /* 等任意一个子进程死 */
pid_t waitpid(pid_t pid, int* status, int options); /* 等指定 pid(或 -1=任意) */
```

两者都**阻塞**到有子进程状态变化为止(默认行为),把状态写进 `status` 指向的 int,返回那个子进程的 pid。这个 `status` 是个打包的整数,**不能直接读**,必须用一组宏拆开:用 `WIFEXITED(status)` 判断是不是**正常退出**(`exit` 或 `return`),是的话用 `WEXITSTATUS(status)` 取出退出码(只有低 8 位,所以 `exit(42)` 取出来是 42,`exit(300)` 取出来是 44);用 `WIFSIGNALED(status)` 判断是不是**被信号杀死**,是的话用 `WTERMSIG(status)` 取出信号编号。真跑这两种死法,用 mode 参数切换:

```c
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int killed = (argc > 1) ? atoi(argv[1]) : 0;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        if (killed) {
            raise(SIGTERM); /* 子进程被信号打死 */
        }
        exit(42); /* 子进程正常退出,码 42 */
    }

    int st;
    waitpid(pid, &st, 0);
    if (WIFEXITED(st)) {
        printf("[父] 子正常退出, 退出码=%d\n", WEXITSTATUS(st));
    }
    if (WIFSIGNALED(st)) {
        printf("[父] 子被信号杀死, 信号=%d\n", WTERMSIG(st));
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall wait_status.c -o ws
$ ./ws 0       # 子 exit(42)
[父] 子正常退出, 退出码=42
$ ./ws 1       # 子 raise(SIGTERM)
[父] 子被信号杀死, 信号=15
```

`mode 0` 那次,子进程 `exit(42)`,父进程 `WIFEXITED` 为真、`WEXITSTATUS` 取出 `42`,干净利落。`mode 1` 那次,子进程 `raise(SIGTERM)`(给自己发 SIGTERM,默认动作是终止),它**不是正常退出、而是被信号杀死**,所以 `WIFEXITED` 为假(第一个 `if` 不打印)、`WIFSIGNALED` 为真、`WTERMSIG` 取出 `15`(SIGTERM 的编号)——这正是判断「子进程是不是崩了」的标准手段:检查 `WIFSIGNALED`,再看 `WTERMSIG` 知道是被哪个信号干掉的(SIGSEGV=11、SIGABRT=6、SIGKILL=9 等等),下一章讲信号时还会用到。

这里千万别手滑把 `status` 直接拿来判断——它是个打包值,直接读 `if (status == 0)` 是**错的**(不同的死法打包出来的 `status` 整数值不是你能直接解释的),必须走 `WIFEXITED`/`WEXITSTATUS` 这套宏。另外 `waitpid` 比 `wait` 强在:能等**指定 pid** 的子进程(多子进程场景按号收),还能通过 `options`(如 `WNOHANG`)非阻塞地「看一眼有没有死」。多个子进程时,`wait` 只收**最先死的那个**,要想把所有子都收干净得循环 `waitpid` 直到它返回 `-1`、`errno == ECHILD`(没有子进程可收了)为止。

## 僵尸进程：成因与收尸

现在能完整讲清上一章埋的「僵尸」了。一个子进程死了之后,内核**不能立刻把它彻底抹掉**——它的退出状态(退出码/被信号杀)还得留着等父进程读。所以子进程死的瞬间,它会进入一个**僵尸(zombie)**状态:进程已经死了(不再占 CPU、不跑代码),但进程表里还留着一条记录(pid、退出状态、一点记账信息),Linux 的 `ps` 里显示成 `Z` 状态或 `<defunct>`。**父进程调 `wait`/`waitpid` 收尸时**,这条僵尸记录才被正式清除、pid 才被回收。

僵尸之所以是坑,是因为它**占着 pid 不放**:要是父进程长期 fork 出子进程、却从不 `wait`,僵尸就会越积越多,最终撑爆进程表(进程号耗尽,系统再也 fork 不动新进程)。这就是为什么本章和上一章所有例子,父进程都规规矩矩地 `waitpid`——既是为了稳定输出顺序,更是为了**不留僵尸**。本章上面 `exec_ls`、`exec_argv`、`wait_status` 里父进程都 `waitpid` 了,所以它们不会留僵尸;`exec_fail` 的 bug 版子进程是 `return 0`(正常退出),父进程在 else 分支里也 `waitpid` 了,同样干净。

那父进程**一直不 wait** 会怎样?如果父进程比子进程先死,子进程变成**孤儿进程**,被 init(PID 1,现代 Linux 是 systemd)收养,init 会负责收尸,所以孤儿不会变长留的僵尸。真正麻烦的是**父进程活得比子进程久、却又不 wait**——这种父进程的子进程死后会一直挂着僵尸,直到父进程某次 `wait` 或父进程自己退出(那时子进程被 init 收养、init 收尸)。一个绕开「父进程必须阻塞 wait」的常用招是处理 **SIGCHLD** 信号——子进程状态变化时内核会给父进程发 SIGCHLD,父进程可以在信号处理器里非阻塞地 `waitpid(..., WNOHANG)` 收尸;这是「不阻塞主循环又能及时收尸」的标准做法,完整的信号机制(怎么装处理器、`sigaction` 为什么替代 `signal`)是下一章的主题。

## exec 家族的命名规律

最后把 exec 一家的名字理清楚,看着多、其实有规律。后缀字母代表两件事:**参数怎么传**、**去哪找程序**、**环境怎么给**。`l`(list)表示参数**列表式**一个个列(`execl(path, arg0, arg1, ..., NULL)`);`v`(vector)表示参数**数组式**传一个 `char* argv[]`(`execv(path, argv)`)。`p` 表示**走 PATH 查找**(第一个参数给的是程序名、不是路径,`execlp`/`execvp`);没 `p` 的(`execl`/`execv`)第一个参数必须是**完整的路径**(写错就静默失败,这是「路径写错查无此程序」的高频坑)。`e` 表示**自己传一份环境变量**(最后一个参数是 `char* envp[]`,`execve`/`execle`);没 `e` 的默认继承父进程的 `environ`(上面 `exec_argv` 演示的就是继承)。

六个常用成员两两组合:`execl`、`execv`(指定路径,继承环境)、`execlp`、`execvp`(按名字查 PATH,继承环境)、`execle`、`execve`(指定路径,自传环境)。其中**`execve` 才是真正的系统调用**(POSIX 规定的底层入口),其余五个都是 glibc 在用户态对它的封装(把列表式参数拼成数组、把 PATH 查找展开成完整路径、把继承的 `environ` 填进去,最后调 `execve`)。日常用 `execlp`(命令行参数少、想走 PATH)或 `execvp`(参数已构成数组、想走 PATH)最多——shell 执行命令时走的就是这一路。

## 小结

`exec` 把当前进程**整个换成另一个程序**:成功无返回(控制权交给新程序的 `main`),失败才返回 `-1`,所以 `exec` 后面**永远紧跟 `perror` + `_exit`**——漏了 `_exit`,exec 翻车时子进程会掉头跑父进程的后半段代码(真跑「main 末尾」被打两遍的 bug)。exec 时新程序的 `argv` 全由调用者塞入(`argv[0]` 想叫啥叫啥)、环境变量默认继承父进程、而**pid、fd 表、cwd 都保留**——所以「进程还是那个进程,只是跑的程序换了」。子进程跑完后不会自动消失,得父进程用 `wait`/`waitpid` **收尸**:`status` 用 `WIFEXITED`/`WEXITSTATUS`(正常退出码)和 `WIFSIGNALED`/`WTERMSIG`(被信号杀)这组宏拆开,直接读 `status` 是错的。父进程不收尸,死掉的子进程就变成**僵尸**(占 pid 不放),`wait` 就是清掉它的动作;父进程想「不阻塞又能及时收」就走 SIGCHLD 信号这条路(下一章)。exec 家族按后缀记:`l`/`v` 是参数形式、`p` 是 PATH 查找、`e` 是自传环境,底层都是 `execve` 这一个 syscall。

到这一章,Unix 多进程编程的「三位一体」——`fork`(造进程)、`exec`(换程序)、`wait`(收尸)——就齐了。一个 shell 敲下命令、一个服务器派活给子进程,骨架都是「fork → 子 exec → 父 wait」。下一章我们把这套骨架推向后台化:fork + setsid 把一个程序变成脱离终端的守护进程,届时 fork+exec+wait 会第一次组合进一个真实场景里。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 8 章「进程控制」讲 exec 族 + wait、第 9 章讲进程关系(僵尸、孤儿、init 收养),退出状态宏的拆解最细。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 25 章 fork+exec 标准模式、第 26 章 wait/waitpid 与僵尸,exec 六个成员的命名对照表清晰。
- **man 页**：`execve(2)`(底层 syscall)、`exec(3)`(家族封装)、`wait(2)`、`waitpid(2)`、`raise(3)`、`setenv(3)`——本章每条行为对齐 man 页;`signal(7)` 列信号编号(SIGTERM=15 等)。
- **ISO C**：`exit` 是 §7.22.4.4、`raise` 是 §7.14.2.1、`getenv`/`setenv` 是 §7.22.4.6;`fork`/`exec*`/`wait*`/`pid_t` 都不在 ISO C 里,它们是 POSIX(IEEE Std 1003.1-2008)。
