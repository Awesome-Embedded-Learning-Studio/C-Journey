---
title: "IPC 下：共享内存、信号量与同步的正确姿势"
description: "上一章的 pipe 是『流』——字节一头进一头出;这一章讲另一种 IPC 模型:共享内存,让两个进程直接映射同一段物理地址到各自的虚拟空间,读写就像访问自己的全局变量一样快。用 shm_open 建一个共享内存对象、ftruncate 设大小、mmap 映射进进程(关键标志 MAP_SHARED,改动才对其它进程可见);真跑 shm_basic:父写 map[0]='A'、子能读到、子回写 'Z' 父也看得到——这是真共享,和第 2 章 fork 后的 COW 副本(改了互不可见)完全相反。两个高频坑真跑复现:其一,shm_open 之后必须 ftruncate 设大小,否则对象长度 0、mmap 出来看着成功、一访问就 SIGBUS 打死(真跑退出码 135);其二,共享内存段不会随进程退出自动消失,必须 shm_unlink 否则 /dev/shm 里残留泄漏。重点讲清:共享内存本身**不提供任何同步**——两个进程同时读写同一段就是裸数据竞争(数据竞争的进程版、同样 UB),所以必须配信号量;真跑 sem_sync 用 POSIX 命名信号量(sem_open/sem_wait/sem_post)让子进程等父写好数据再读。最后点 POSIX 信号量(sem_open)与 System V 信号量(semget)是两套 API、别混用;信号量名字以 / 开头、跨进程靠同名 rendezvous。全 gcc16+clang22+ASan 真跑(需 -lrt)。"
chapter: 5
order: 7
tags:
  - host
  - system-programming
  - posix
  - ipc
difficulty: advanced
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 6 章：IPC 上（pipe 的流模型,对比共享内存的共享模型）"
  - "第 2 章：进程的诞生（fork、写时复制 COW——共享内存正是它的反面）"
  - "第 1 章：文件 IO 与 fd（open/ftruncate/dup2,fd 作为资源的思路）"
related:
  - "第 8 章：IO 多路复用（select 监听多路 IPC 的就绪状态）"
  - "第 5 章：信号（共享内存的同步也可结合信号通知）"
---

# IPC 下：共享内存、信号量与同步的正确姿势

## 引言：从「流」到「共享」

上一章的 `pipe` 是一种「**流**」式的 IPC——你往这头写字节、它从那头按顺序读出来,中间经过内核缓冲搬一道。它好用,但有搬运开销、而且是单向的。这一章讲另一种更直接的 IPC:**共享内存**(shared memory)。思路是:让内核安排一段物理内存,**同时映射进两个(或多个)进程各自的虚拟地址空间**——于是对任何一个进程,读写这段地址就像访问自己的全局变量;而因为它背后是**同一段物理内存**,一个进程的写,**别的进程立刻看得见**,没有任何拷贝、没有内核中转。这是**最快的 IPC**(零拷贝),代价是:它**只管「共享」、不管「同步」**——两个进程同时读写同一段就是赤裸裸的数据竞争,得你自己上信号量来管顺序。

## shm_open + mmap:映射一段共享内存

POSIX 共享内存的套路是三步:`shm_open`(`<sys/mman.h>`,需 `-lrt`)建一个**命名的共享内存对象**(在 Linux 上 `/dev/shm/` 里会冒出一个文件)、`ftruncate` 给它**设大小**、`mmap` 把它**映射进进程**。真跑一遍,父写一个字符、子读出来、子再回写:

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    const char* name = "/cj_shm_demo";
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return 1;
    }
    if (ftruncate(fd, 4096) < 0) { /* 必须先设大小,否则 mmap 长度 0 */
        perror("ftruncate");
        return 1;
    }
    char* map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    close(fd); /* mmap 完就可以关 fd,映射还在 */

    map[0] = 'A'; /* 父先写一个字符 */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* 子:看得到父写的 A —— 这是真共享,不是第 2 章的 COW 副本 */
        printf("[子] 看到 map[0] = '%c'(父写的,真共享)\n", map[0]);
        map[0] = 'Z'; /* 子再回写一个 */
        exit(0);
    }
    waitpid(pid, NULL, 0);
    printf("[父] 子改完后 map[0] = '%c'(子的写,父也看得见)\n", map[0]);

    munmap(map, 4096);
    shm_unlink(name); /* 用完务必 unlink,否则 /dev/shm 残留 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall shm_basic.c -o shm -lrt && ./shm
[子] 看到 map[0] = 'A'(父写的,真共享)
[父] 子改完后 map[0] = 'Z'(子的写,父也看得见)
```

`shm_open` 像开文件一样给你一个 fd(它本质上就是 `/dev/shm/` 下的一个文件);`ftruncate(fd, 4096)` 把它撑到 4096 字节;`mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)` 把这 4096 字节映射进当前进程的虚拟空间、返回首地址 `map`。**最关键的一个参数是 `MAP_SHARED`**——它告诉内核「我对这段内存的修改,要**写回共享对象**、让其它映射了同一对象的进程看见」;如果写成 `MAP_PRIVATE`,改动就只会落到一份私有的写时复制副本里、别人看不见(那就退化成了第 2 章的 COW,失去共享意义了)。

真跑结果直击「共享」的本质:父进程把 `map[0]` 写成 `'A'`,`fork` 出来的子进程**直接读到 `'A'`**(不是 COW 复制下来的快照、是同一字节);子进程把它改成 `'Z'`,父进程 `waitpid` 之后再去读,看到的就是 **`'Z'`**。回想一下第 2 章——那里 fork 后父子改全局变量**互不可见**(COW,各改各的副本);这里恰恰相反,父子改的是**真·同一块物理内存**,互相清清楚楚看得见。这就是共享内存和 COW 的根本区别:`MAP_SHARED` 让 fork 出来的映射不走 COW、走真共享。

## 漏了 ftruncate → SIGBUS;忘了 shm_unlink → 残留

共享内存有两个高频坑,都得踩一次才记得住。第一个:**`shm_open` 之后必须 `ftruncate` 设大小**。`shm_open` 建出来的对象初始长度是 **0**,你不 `ftruncate` 就直接 `mmap(NULL, 4096, ...)`,mmap 大概率**不会报错**、乐呵呵返回一个地址;可这个映射背后实际长度是 0,你一访问它(`map[0] = 'X'`),内核就给你进程发 **`SIGBUS`** 信号——默认动作是**直接打死**。真跑这个翻车路径:

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    const char* name = "/cj_sigbus_demo";
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    /* 故意漏 ftruncate:共享内存对象长度为 0 */
    char* map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    close(fd);
    printf("[主] mmap 成功了,但对象长度是 0,现在访问它...\n");
    fflush(stdout);
    map[0] = 'X'; /* 访问 0 长度映射 → SIGBUS */
    printf("[主] 到不了这里\n");
    shm_unlink(name);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall sigbus.c -o sbus -lrt
$ ./sbus; echo "退出码 $?"
[主] mmap 成功了,但对象长度是 0,现在访问它...
退出码 135
```

`mmap` 没报错(打印了「mmap 成功了」),但 `map[0] = 'X'` 那一行直接触发 **`SIGBUS`**(信号编号 7,所以进程退出码是 `128+7 = 135`),后面那句「到不了这里」**根本没机会打**。这就是漏 `ftruncate` 的代价:映射看着建好了,实际背后没有合法物理页,一碰就崩。纪律很简单——**`shm_open` 之后,`mmap` 之前,必有一句 `ftruncate(fd, 大小)`**。

第二个坑不像第一个那么炸裂,但很烦人:**共享内存对象不会随进程退出自动消失**。它是 `/dev/shm/` 里的一个真文件,进程退了它还在;你要是不 `shm_unlink(name)`,每次跑程序就残留一个,慢慢堆一堆垃圾(还会占内存)。所以上面 `shm_basic` 末尾有 `shm_unlink(name)` 做清理——这是个**资源管理纪律**,跟第 1 章「fd 用完 close」、第 2 章「malloc 配 free」一脉相承:C 把资源生命周期的责任全甩给你,开一个就得记得还一个。

## 共享内存不带同步:必须配信号量

讲到现在有个隐含前提一直没点破:**shm_basic 里父子之所以没踩数据竞争,是因为我精心安排了执行顺序**——父先写、`fork`、子读、子写、父 `waitpid` 之后再读,全程没有任何两个进程**同时**碰同一字节。可真实的共享内存程序里,父子是**并发**跑的,如果它们同时读写同一段内存(比如父在写一个结构体的前半、子同时在读后半),那就是**裸数据竞争**——和单线程里的数据竞争(后面并发章细讲)一样是未定义行为,会读出乱七八糟的撕裂值。

共享内存本身**不提供任何同步机制**,它只负责「让两个进程看见同一块内存」。要保证「你写完我再读」或「同一时刻只有一个进程能改」,必须**额外**配一把「锁」——跨进程最常用的是**信号量**(semaphore)。POSIX 给了一套命名信号量(`sem_open`/`sem_wait`/`sem_post`,头 `<semaphore.h>`,同样需 `-lrt`):它像一个计数器,`sem_wait` 把计数减 1(减到 0 就阻塞等)、`sem_post` 把计数加 1(唤醒一个等待者)。用初始值为 0 的信号量,就能做「一个进程等另一个进程通知」的同步。真跑一遍:子进程在共享内存里等父进程把数据写好:

```c
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    const char* shmname = "/cj_sem_demo";
    const char* semname = "/cj_sem_demo_sem";

    /* 清掉上次可能残留的同名信号量(命名信号量的坑:跨进程靠名字) */
    sem_unlink(semname);
    shm_unlink(shmname);

    int fd = shm_open(shmname, O_CREAT | O_RDWR, 0600);
    ftruncate(fd, 4096);
    char* map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    /* 初始值 0:子 sem_wait 会阻塞,直到父 sem_post */
    sem_t* sem = sem_open(semname, O_CREAT, 0600, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        printf("[子] 等 father 写好...\n");
        fflush(stdout);
        sem_wait(sem); /* 阻塞到父 post */
        printf("[子] 拿到信号,读到共享内存: %s\n", map);
        fflush(stdout);
        exit(0);
    }

    /* 父:写数据,再 post 通知子 */
    snprintf(map, 4096, "hello from parent");
    sleep(1); /* 故意慢一点,让你看清子真的在等 */
    printf("[父] 写好数据,post 信号量通知子\n");
    fflush(stdout);
    sem_post(sem);
    waitpid(pid, NULL, 0);

    sem_close(sem);
    sem_unlink(semname);
    munmap(map, 4096);
    shm_unlink(shmname);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall sem_sync.c -o sem -lrt && ./sem
[子] 等 father 写好...
[父] 写好数据,post 信号量通知子
[子] 拿到信号,读到共享内存: hello from parent
```

`sem_open(semname, O_CREAT, 0600, 0)` 建一个**初始值为 0** 的命名信号量。子进程一跑就 `sem_wait`——计数是 0、它**阻塞**,把 CPU 让给父进程;父进程慢悠悠(`sleep(1)`,故意让你看清子在等)把数据写进共享内存,然后 `sem_post` 把计数加到 1、唤醒子进程;子进程 `sem_wait` 返回、这时去读共享内存,数据已经稳稳写好了——**「父写完、子才读」的同步就这么靠一个信号量保证**。这就是共享内存程序的标准姿势:**共享内存负责传数据(快)、信号量负责管顺序(不竞争)**,两者搭配才完整。命名信号量跟共享内存对象一样有个名字(以 `/` 开头)、跨进程靠**同名** rendezvous,用完同样要 `sem_unlink` 清理(我开头还特意先 `unlink` 一次,清掉上次可能残留的同名信号量,免得初始值不对)。代码里那些 `fflush(stdout)` 又是第 2 章那个老坑——父子各自的 printf 缓冲不同步、不刷就会出现「子已经读到了、父的『写好数据』还没显示」的乱序,IPC 程序调试时这是高频困扰。

## POSIX 还是 System V:别混用

最后认一个历史包袱:Unix 信号量有两套完全不同的 API。一套是本章用的 **POSIX**(`sem_open`/`sem_wait`/`sem_post`,接口干净、像文件),另一套是更老的 **System V**(`semget`/`semop`/`semctl`,基于「信号量集合」、接口繁琐)。两者**互不相通**——POSIX 信号量和 System V 信号量是两个独立的内核子系统,你不能用 `sem_post` 去操作一个 `semget` 建的信号量。共享内存也照样分 POSIX(`shm_open`/`mmap`)和 System V(`shmget`/`shmat`)两套。新代码**选一套用到底就行**(推荐 POSIX,接口现代、和 `mmap` 体系统一),最忌讳的是在同一项目里**混用**两套——那是给自己找别扭。两者的语义本质一样(都是计数器/共享段),只是接口长得完全不同。

## 小结

共享内存让多进程映射同一段物理内存、读写像自己的全局变量,是**最快的 IPC**(零拷贝);关键是 `mmap` 时给 `MAP_SHARED`——它让改动写回共享对象、对其它进程可见(和第 2 章 `MAP_PRIVATE`/COW 的「各改各的副本」相反)。套路三步:`shm_open` 建对象 → `ftruncate` 设大小(漏了就 `mmap` 成功但访问触发 `SIGBUS`、退出码 135)→ `mmap` 映射;用完 `munmap` + `shm_unlink`(不 unlink 会在 `/dev/shm` 残留)。共享内存**只管共享不管同步**——并发读写同一段是数据竞争 UB,必须配信号量:POSIX 命名信号量 `sem_open`(初始值控制) + `sem_wait`/`sem_post` 做通知与互斥,跨进程靠同名 rendezvous。POSIX(`sem_open`/`shm_open`)和 System V(`semget`/`shmget`)是两套独立 API、别混用,新代码选 POSIX。

到此我们有了 pipe(流)、共享内存(共享)两套 IPC。下一章换个角度——**IO 多路复用**(`select`):让一个进程能**同时盯住多个 fd**(好几根 pipe、好几个 socket),谁先有数据就先处理谁,不用为每个 fd 开一个线程死等。那是事件驱动编程的起点。

## 参考资源

- **APUE**：《Advanced Programming in the UNIX Environment》(W. Richard Stevens / Stephen A. Rago),第 15 章「进程间通信」的共享内存与信号量两节,POSIX 与 System V 两套都讲了。
- **TLPI**：《The Linux Programming Interface》(Michael Kerrisk),第 48 章 mmap、第 53–54 章 POSIX 信号量、第 63–64 章共享内存,`MAP_SHARED`、`ftruncate`+SIGBUS、命名信号量的细节最全。
- **man 页**：`shm_open(3)`、`mmap(2)`、`ftruncate(2)`、`sem_open(3)`、`sem_wait(3)`、`sem_post(3)`——本章每条行为对齐 man 页;`signal(7)`(SIGBUS=7,退出码 135 的来历)。
- **POSIX**：`shm_open`/`shm_unlink`/`sem_open`/`sem_wait`/`sem_post` 属 POSIX 实时扩展(IEEE Std 1003.1-2008 realtime option),Linux 上链接需 `-lrt`;`mmap`/`ftruncate` 是 POSIX 基础部分。
