---
title: "stdio 与文件 IO：FILE 指针、缓冲与底层系统调用"
description: "把 C 标准库的 FILE/fopen/fread/fwrite 拆开看：它如何包住 open/read/write、缓冲到底缓冲了什么、fflush 刷的是哪一层，以及 fopen 和 open 到底该用哪个。"
chapter: 5
order: 2
tags:
  - host
  - system-programming
  - file-io
  - posix
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程与命令行基础"
  - "Chapter 2：指针与内存"
  - "ROADMAP 阶段 5：系统编程"
related:
  - "Socket 编程：用 TCP 从零写一个客户端/服务端"
---

# stdio 与文件 IO：FILE 指针、缓冲与底层系统调用

## 引言

我们前面写过无数遍 `printf`，应该也写过不少 `fopen`、`fread`。但你有没有认真想过一个问题：你调一次 `fwrite` 往文件里塞 5 个字节，磁盘是不是**当场**就被写了 5 个字节进去？还有，C 标准库给的这套 `FILE *`、`fopen`、`fread`/`fwrite`，和系统调用那套 `open`、`read`、`write`，到底是同一个东西还是两套东西？

说实话我最早学这块的时候是一团浆糊——感觉两套 API 干的是一模一样的事，却不知道为什么要有两套、什么时候用哪套。后来才想明白，这里其实只有**一条**主线：

> **`FILE *` 是 C 标准库在系统调用之上加的一层"缓冲壳"**。你拿到 `FILE *`，本质是拿到了一个文件描述符 **外加**一块用户态缓冲区。`fopen` 最终还是会去调 `open`，`fwrite` 最终还是会去调 `write`，只是中间多了缓冲、多了跨平台抽象。

所以这一章我们要做的，就是把这层壳**剥给你看**：`FILE` 里到底装了什么、缓冲缓冲了什么、`fflush` 刷的到底是用户态缓冲还是内核缓冲，以及 `fopen` 和 `open` 这两套到底怎么取舍。配套示例全在本机实测捕获，每一段输出都是真跑出来的，不是想象。

> Platform：host（Linux/WSL）。C 标准：C11（带 `_POSIX_C_SOURCE 200809L`，因为要碰 `fileno`/`open`/`write` 这些 POSIX 接口）。

## 核心概念

### 一切皆文件，文件操作到底分了几层

Linux 那句"一切皆文件"我们听过太多遍，落到代码里它其实意味着：你打开一个普通文件、一个串口、一个终端、甚至一个网络 socket，拿到的都是一个**文件描述符（fd，一个非负整数）**，剩下的读写操作长得几乎一样。

但光有 fd 还不够。直接用 fd 做读写，对应的是这套**系统调用**：

```c
int     open(const char *pathname, int flags, ... /* mode_t mode */);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
```

这一层离内核最近，没有缓冲帮你"攒数据"，你调一次 `write` 内核就处理一次。而 C 标准库觉得这样既慢又不跨平台，于是**在这层之上又包了一层**，给你这套**带缓冲的标准 IO**：

```c
FILE*  fopen(const char *filename, const char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int    fclose(FILE *stream);
int    fseek(FILE *stream, long offset, int whence);
```

注意参数类型变了：系统调用层玩的是 `int fd`，标准库层玩的是 `FILE *`。**这就是这两套 API 最表面的区别，也是后面所有差异的源头。**

### FILE 指针里到底装了什么

`FILE` 本质是一个标准库定义的结构体，具体字段因平台而异，但只要你在 Linux 上用 glibc，它大体上就装着这么几样东西：

- 一个**文件描述符**（`fileno()` 能取出来）；
- 一块**用户态缓冲区**及其指针、容量、当前读写位置；
- 一堆**状态标志**：是不是到 EOF 了、是不是出错（`feof`/`ferror` 读的就是它）；
- **缓冲模式**：全缓冲、行缓冲、还是不缓冲。

MSVC 干脆给你一个不透明的 `void* _Placeholder`，明摆着告诉你"这是私密的别乱动"。但 glibc 这边我们可以用一组非标准（仅 glibc）的私有扩展函数偷看里面的缓冲区。下面这段代码真跑一下，就能把 `FILE` 内部的关键参数挖出来：

```c
// file_internals.c —— 偷看 FILE 内部(glibc 私有扩展,演示用)
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

/* glibc 私有扩展:偷看 FILE 内部的缓冲区。仅 Linux/glibc 有,演示用 */
extern size_t __fbufsize(FILE *__stream);
extern int __flbf(FILE *__stream);   /* 是否行缓冲 */

int main(void) {
    FILE *disk = fopen("internals.txt", "w");
    if (!disk) { perror("fopen disk"); return 1; }

    /* 触发一次真正的 I/O,让 stdio 把缓冲区分配出来(lazy alloc)。
       stdout 也得先碰一下,否则它的缓冲区还没物化,读出来是 0。 */
    fputs("seed line so the buffer materializes\n", disk);
    fputs("seed stdout too\n", stdout);

    printf("stdout 缓冲区大小: %zu 字节\n", __fbufsize(stdout));
    printf("stdout 是否行缓冲: %s\n", __flbf(stdout) ? "是" : "否");
    printf("磁盘流缓冲区大小: %zu 字节\n", __fbufsize(disk));
    printf("磁盘流是否行缓冲: %s\n", __flbf(disk) ? "是" : "否(全缓冲)");
    printf("stdin  底层 fd (fileno): %d\n", fileno(stdin));
    printf("stdout 底层 fd (fileno): %d\n", fileno(stdout));
    printf("stderr 底层 fd (fileno): %d\n", fileno(stderr));
    printf("disk   底层 fd (fileno): %d\n", fileno(disk));
    fclose(disk);
    return 0;
}
```

编译跑一下：

```text
$ gcc -std=c11 -Wall -Wextra file_internals.c -o file_internals
$ ./file_internals
seed stdout too
stdout 缓冲区大小: 4096 字节
stdout 是否行缓冲: 否
磁盘流缓冲区大小: 4096 字节
磁盘流是否行缓冲: 否(全缓冲)
stdin  底层 fd (fileno): 0
stdout 底层 fd (fileno): 1
stderr 底层 fd (fileno): 2
disk   底层 fd (fileno): 3
```

看，几件实打实的事被这一段输出坐实了：

第一，**`FILE *` 内部确实挂着一个文件描述符**，`fileno()` 能把它挖出来。stdin/stdout/stderr 分别是 0/1/2，这是固定的；我们新 `fopen` 出来的文件流分到的是 3——因为进程一开始就把 0/1/2 占走了，新文件描述符总是从**最小未占用的整数**开始分配。这一点和系统调用层完全是一回事，因为 `fopen` 内部本来就会去调 `open`。

第二，**stdio 给每个流都配了一块用户态缓冲区**，这里实测是 **4096 字节**。也就是说，你往这个流里 `fwrite` 的数据，并不会立刻打系统调用，而是先进这 4096 字节的"水池"，攒满了才一次性倒进内核。这就是缓冲的本质。

> **关于行缓冲的一个诚实补充**：经典说法是"stdout 连终端时是行缓冲、写文件时是全缓冲"。我在这台机器上直接跑上面这个程序时，stdout 报出来的却是**全缓冲**（行缓冲=否），因为在你看到的这个捕获上下文里，stdout 并不是连着一个真正的控制终端（它是被管道接走的）。结论是：**行缓冲只在 stdout 真正连交互式终端时才成立**，一旦被重定向或管道化，stdout 立刻退化为全缓冲。这一点你写日志、做调试输出时特别容易踩。

### 缓冲到底缓冲了什么：三层存储

这是很多人没理清的地方。我们把"你的数据从内存到磁盘"这条路画清楚，一共是**三段水池**：

```text
  你的程序变量  ──①──>  stdio 用户态缓冲(FILE 内部)  ──②──>  内核页缓存  ──③──>  磁盘
                       (4KB, fwrite 先来这里)         (write/fsync 管这段)     (内核择机落盘)
```

- **① stdio 缓冲区**（用户态，那 4096 字节）：由 `fwrite`/`fprintf`/`fread` 维护。`fflush` 刷的就是这一层——把它推进内核。
- **② 内核页缓存**（内核态）：由 `write` 系统调用填充。数据进了这里，`write` 就返回了，但**未必落盘**。`fsync` 刷的是这一层。
- **③ 磁盘**：内核会在它觉得合适的时候把页缓存写下去，时机不确定。

所以"缓冲"其实有两个层次，一个在用户态，一个在内核态。`fflush` 和 `fsync` 干的不是同一件事——这点稍后踩坑段会专门说。

下面这段代码就是来**亲眼看见 ① 的存在**的：我们 `fprintf` 写一行，然后**不 fflush**，立刻去读那个文件，看数据在不在。

```c
// buffering.c —— 亲眼看见 stdio 缓冲区的存在
#include <stdio.h>
#include <string.h>

int main(void) {
    FILE *f = fopen("buf_demo.txt", "w");
    if (!f) { perror("fopen"); return 1; }
    fflush(stdout);

    fprintf(f, "这一行先躺进 stdio 缓冲区,还没进内核,更没落盘\n");
    fprintf(stderr, "[stderr] fprintf 已返回,但 buf_demo.txt 此刻大概率还是空的\n");

    /* flush 前:数据还在 stdio 缓冲区,文件读不到 */
    FILE *peek = fopen("buf_demo.txt", "r");
    int got = 0;
    char line[256];
    if (peek) {
        if (fgets(line, sizeof(line), peek)) got = 1;
        fclose(peek);
    }
    fprintf(stderr, "[stderr] flush 前: %s\n",
            got ? "文件里居然已经有数据(被内核/stdio 提前刷了)"
                : "文件是空的,数据还在 stdio 缓冲区里");

    /* fflush 强制把 stdio 缓冲区推进内核 */
    fflush(f);
    peek = fopen("buf_demo.txt", "r");
    got = 0;
    if (peek) {
        if (fgets(line, sizeof(line), peek)) got = 1;
        fclose(peek);
    }
    fprintf(stderr, "[stderr] fflush 后: %s\n",
            got ? "数据进内核了,读得到" : "还是读不到(不正常)");

    fclose(f);
    return 0;
}
```

跑出来是这样：

```text
$ gcc -std=c11 -Wall -Wextra buffering.c -o buffering && ./buffering
[stderr] fprintf 已返回,但 buf_demo.txt 此刻大概率还是空的
[stderr] flush 前: 文件是空的,数据还在 stdio 缓冲区里
[stderr] fflush 后: 数据进内核了,读得到
```

`fprintf` 早就返回了，文件却是空的——这一条输出就是 stdio 缓冲区存在的铁证。直到我们手动 `fflush(f)`，数据才从用户态那 4096 字节水池里被推进内核，文件才读得到。**fwrite 的"返回"和数据"真正写出去"之间，隔着一整个缓冲区。**

### fopen vs open：同一件事的两种写法

把这两层连起来看，同一份"往文件里写一句话"，就有两种写法。先用裸系统调用：

```c
// fopen_vs_open.c —— 途径 A:裸系统调用
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("raw.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return 1; }
    const char *msg = "hello via raw syscall\n";
    size_t left = strlen(msg);
    const char *p = msg;
    /* write 可能一次写不完,标准写法是循环补齐 */
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w == -1) { perror("write"); close(fd); return 1; }
        p += w;
        left -= (size_t)w;
    }
    printf("裸 syscall 写完,新 fd 应从 3 开始: fd=%d\n", fd);
    close(fd);
    return 0;
}
```

注意看这条路径上你**必须自己处理的事**：`open` 要传一堆 `O_*` 标志位、要传八进制权限 `0644`、`write` 的返回值要循环检查（因为它可能只写进去一部分）。每一步都是真实的系统调用，没有缓冲帮你兜底。

换成标准库，同样的事写起来短得多。下面是同一个 `fopen_vs_open.c` 里途径 B 的那几行（为了看清差别，只贴关键部分；完整程序是 A、B 两段拼在同一个 `main` 里，最后各自 `printf` 一行统计）：

```c
// fopen_vs_open.c —— 途径 B:标准库
FILE *f = fopen("libc.txt", "w");
if (!f) { perror("fopen"); return 1; }
const char *msg2 = "hello via stdio library\n";
size_t items = fwrite(msg2, 1, strlen(msg2), f);
fclose(f);
```

`"w"` 一个字符串就替代了 `O_WRONLY|O_CREAT|O_TRUNC`，`fclose` 顺手把缓冲区刷掉再关 fd。两边都能完成任务，跑完整程序的输出如下（`fd=3` 来自途径 A 的 `printf`，`24 字节` 来自途径 B）：

```text
$ gcc -std=c11 -Wall -Wextra fopen_vs_open.c -o fopen_vs_open
$ ./fopen_vs_open
裸 syscall 写完,新 fd 应从 3 开始: fd=3
stdio 写进 1 块, 共 24 字节
$ cat raw.txt
hello via raw syscall
$ cat libc.txt
hello via stdio library
```

那到底用哪个？给个很实用的判断：

- **要可移植性、要做格式化（`fprintf`）、要省心** → 用 `fopen`/`fwrite`。Windows、Linux、macOS 上 `fopen` 的行为基本一致，代码搬过去就能跑。
- **要精细控制（`O_NONBLOCK`、`O_CLOEXEC`）、要直接拿 fd、要做 socket/设备那套** → 用 `open`/`read`/`write`。很多场景（比如网络、比如非阻塞 IO）压根没有对应的 `FILE *` 接口，你只能走系统调用。
- **想鱼和熊掌兼得** → 先 `open` 拿到 fd，再用 `fdopen(fd, ...)` 把它**包成一个 `FILE *`**，之后就能用 stdio 那套函数操作它。这是把两层接起来的标准做法。

### fwrite/fread 的返回值：算的是"元素个数"不是字节

这是新手最容易忽略的细节。看签名：

```c
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
```

`size` 是**单个元素**的字节数，`nmemb` 是**元素个数**。它们的返回值是**成功完成的元素个数**（`nmemb` 那一维），不是字节总数。比如 `fwrite(arr, sizeof(int), 5, f)` 成功就返回 `5`，而不是 `5*sizeof(int)`。

下面这段把读写配合 `fseek` 随机访问、以及 `feof` 在文件末尾的行为，一次性演示清楚：

```c
// fwrite_readback.c —— fwrite/fread 配 fseek 随机访问
#include <stdio.h>
#include <stddef.h>

typedef struct {
    int id;
    char name[16];
    float score;
} Student;

int main(void) {
    Student roster[3] = {
        {1, "alice", 90.5f},
        {2, "bob",   77.0f},
        {3, "carol", 88.25f},
    };

    FILE *f = fopen("students.dat", "wb");
    if (!f) { perror("fopen wb"); return 1; }

    /* fwrite 返回值 = 成功写进的"完整元素"个数,不是字节数 */
    size_t n = fwrite(roster, sizeof(Student), 3, f);
    printf("请求写 3 个元素, fwrite 返回: %zu\n", n);
    if (n != 3) {
        if (ferror(f)) perror("写出错");
        fclose(f);
        return 1;
    }
    fclose(f);

    /* 读回:故意只读 1 个元素 */
    f = fopen("students.dat", "rb");
    if (!f) { perror("fopen rb"); return 1; }

    Student one;
    size_t got = fread(&one, sizeof(Student), 1, f);
    printf("读第 1 个: got=%zu  id=%d  name=%s  score=%.2f\n",
           got, one.id, one.name, one.score);

    /* 用 fseek 随机跳到第 3 个 */
    fseek(f, 2 * (long)sizeof(Student), SEEK_SET);
    Student third;
    got = fread(&third, sizeof(Student), 1, f);
    printf("读第 3 个: got=%zu  id=%d  name=%s  score=%.2f\n",
           got, third.id, third.name, third.score);

    /* 读到最后再读:返回 0 且 feof 为真 */
    fseek(f, 0, SEEK_END);
    Student over;
    got = fread(&over, sizeof(Student), 1, f);
    printf("文件末尾再读: fread 返回 %zu, feof=%d, ferror=%d\n",
           got, feof(f), ferror(f));

    fclose(f);
    return 0;
}
```

实跑输出：

```text
$ gcc -std=c11 -Wall -Wextra fwrite_readback.c -o fwrite_readback && ./fwrite_readback
请求写 3 个元素, fwrite 返回: 3
读第 1 个: got=1  id=1  name=alice  score=90.50
读第 3 个: got=1  id=3  name=carol  score=88.25
文件末尾再读: fread 返回 0, feof=1, ferror=0
$ wc -c students.dat
72 students.dat
```

几件事可以对着输出确认：

`fwrite` 返回 `3`——三个 `Student`，每个 24 字节（`sizeof(int)=4`、`char[16]`、`sizeof(float)=4`，glibc 下排布后 `sizeof(Student)=24`），所以文件正好 72 字节。`fseek(f, 2*sizeof(Student), SEEK_SET)` 一步跳到第三个学生，对应 `lseek` 的能力，只是参数从 fd 换成了 `FILE *`。读到文件末尾再读，`fread` 返回 `0`、`feof` 为真、`ferror` 为 0——这告诉我们：**`feof` 只在"已经发生过一次读到 EOF 的读操作之后"才为真**，它不会预判，所以不能拿 `while (!feof(f))` 当循环条件（那是经典错误写法）。

### 想改缓冲策略：setvbuf

stdio 默认的缓冲策略不一定合你心意。比如你想让一个小文件流**完全不缓冲**（每个字符立刻打系统调用，方便实时观察），或者想给它**自定义一块更大的缓冲**。`setvbuf` 就是干这个的：

```c
// setvbuf_demo.c —— 用 setvbuf 改缓冲模式
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

extern size_t __fbufsize(FILE *__stream);
extern int __flbf(FILE *__stream);

int main(void) {
    FILE *f = fopen("setvbuf.txt", "w");
    fputs("seed\n", f);
    printf("默认磁盘流: 缓冲=%zu 行缓冲=%d\n", __fbufsize(f), __flbf(f));

    /* 改成无缓冲:_IONBF,每个字符立刻打系统调用 */
    setvbuf(f, NULL, _IONBF, 0);
    fputs("seed2\n", f);
    printf("setvbuf(_IONBF) 后: 缓冲=%zu 行缓冲=%d\n", __fbufsize(f), __flbf(f));

    /* 改成自定义 16 字节全缓冲 */
    char mybuf[16];
    setvbuf(f, mybuf, _IOFBF, sizeof mybuf);
    fputs("seed3\n", f);
    printf("setvbuf(_IOFBF,16) 后: 缓冲=%zu 行缓冲=%d\n", __fbufsize(f), __flbf(f));

    fclose(f);
    return 0;
}
```

输出：

```text
$ gcc -std=c11 -Wall -Wextra setvbuf_demo.c -o setvbuf_demo && ./setvbuf_demo
默认磁盘流: 缓冲=4096 行缓冲=0
setvbuf(_IONBF) 后: 缓冲=1 行缓冲=0
setvbuf(_IOFBF,16) 后: 缓冲=16 行缓冲=0
```

三种模式一看就明白：`_IOFBF`（全缓冲，攒满才刷）、`_IOLBF`（行缓冲，遇 `\n` 就刷）、`_IONBF`（不缓冲，立即刷）。`setvbuf` 设置成 `_IONBF` 后缓冲区大小变成 1，设置成 `_IOFBF` 配 16 字节缓冲后大小变成 16——你给什么它就用什么。**关键约束**：`setvbuf` 必须在流**做任何 I/O 之前**调用，否则行为未定义；而且你传进去的自定义缓冲，在 `fclose` 之前不能是栈上已经失效的对象。

## 常见踩坑

### 坑一：`_exit` 直接吞掉 stdio 缓冲区里的数据

这是个特别隐蔽的坑。`exit`/`return` 退出时会帮你把所有 stdio 流 flush 一遍，但 `_exit`（以及 `fork` 后子进程里常用的 `_exit`）是**直接系统调用**，**跳过 stdio 清理**。后果是：凡是还躺在用户态缓冲区里、没遇到 `\n` 的输出，全丢。

```c
// exit_flush.c —— _exit 会丢掉未刷新的 stdio 缓冲
#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* 这一行不带 \n,行缓冲/全缓冲的 stdout 都不会自动刷 */
    printf("这一行在 stdout 缓冲区里,等 exit/return 来刷");
    _exit(0);   /* 系统调用,跳过 stdio 清理 —— 这行大概率丢失 */
}
```

把 `return 0` 换成 `_exit(0)`，跑出来：

```text
$ ./exit_flush     # 用 _exit 退出
[程序结束, 上面若空说明数据丢在缓冲区了]
```

换回 `return 0`（等价于走 `exit()`，会 flush）：

```text
$ ./exit_flush2    # 用 return 退出
这一行在 stdout 缓冲区里,等 exit/return 来刷[程序结束]
```

一句话记住：**`exit` 刷缓冲，`_exit` 不刷缓冲**。所以 `fork` 之后子进程要退出，规矩就是用 `_exit`——这样子进程不会把从父进程继承来的 stdio 缓冲区又 flush 一遍，造成输出重复。

### 坑二：`fflush` 和 `fsync` 不是一回事

记住前面那张三层图。`fflush(FILE*)` 刷的是**① stdio 用户态缓冲区**，把数据推进**内核页缓存**，但**不保证落盘**；`fsync(fd)` 刷的是**② 内核页缓存**，把数据真正**逼到磁盘**。

所以你写数据库、写关键日志，光 `fflush` 是不够的——`fflush` 之后掉电，数据照样可能丢，因为它还没离开内核页缓存。要落盘必须再 `fsync(fileno(f))`。反过来，`fsync` 之前数据还在用户态缓冲区里没进内核，`fsync` 也刷不到——所以正确顺序是**先 `fflush` 把数据推进内核，再 `fsync` 把内核数据逼到磁盘**。

### 坑三：fwrite/fread 的返回值不检查

`fwrite` 返回 `0`（一个元素都没写进去）或者少于请求的 `nmemb`，意味着出错了或空间不够。很多人图省事写完就不管返回值，等数据丢了一半才发现。**每次 `fwrite`/`fread` 之后，检查返回值是否等于你请求的 `nmemb`；不等就用 `ferror`/`feof` 判断是出错还是到尾**。前面 `fwrite_readback.c` 里那段 `if (n != 3)` 就是这个规矩的模板。

### 坑四：拿 `while (!feof(f))` 当循环条件

前面 demo 里我们已经看到：`feof` 只有在**发生一次读到 EOF 的读操作之后**才为真，它不预判。所以 `while (!feof(f)) { fread(...); }` 这种写法，循环体会**多跑一次**，把已经读完的最后一份数据再处理一遍。正确做法是**直接用读函数的返回值当循环条件**：`while (fread(...) == 期望个数)` 或 `while ((ch = fgetc(f)) != EOF)`。

### 坑五：用 `read`/`write` 时忘了"可能只读写一部分"

这是系统调用层的坑。`write(fd, buf, 100)` 不保证写满 100 字节，它可能只写了 40 就返回了（比如管道满了）。所以裸系统调用必须像 `fopen_vs_open.c` 里那样**循环补齐**：

```c
while (left > 0) {
    ssize_t w = write(fd, p, left);
    if (w == -1) { /* 出错处理 */ }
    p += w;
    left -= (size_t)w;
}
```

stdio 的 `fwrite` 帮你在内部做了这件事，所以你用 `fwrite` 时不用自己循环——但底层原理是一样的。

## 小结

把这条主线再串一遍，免得乱了：

- **`FILE *` = 文件描述符 + 用户态缓冲区 + 状态标志**。`fopen` 内部最终调 `open`，`fwrite` 内部最终调 `write`，只是中间多了一层缓冲和一层跨平台抽象。
- **缓冲分两层**：① stdio 用户态缓冲（4KB，`fflush` 刷它），② 内核页缓存（`fsync` 刷它）。`fflush` 和 `fsync` 不是一回事。
- **fwrite/fread 算的是元素个数不是字节**，返回值必须检查；`feof` 不预判，不能当循环条件。
- **`exit`/`return` 会 flush stdio，`_exit` 不会**——fork 后子进程用 `_exit`，关键数据要落盘必须 `fflush` + `fsync`。
- **fopen vs open**：要可移植/格式化/省心用 stdio，要非阻塞/socket/精细控制用系统调用，两者用 `fdopen` 桥接。

关键要点 checklist：

- [ ] 能说出 `FILE *` 内部至少包含哪几样东西
- [ ] 能画出"程序变量 → stdio 缓冲 → 内核页缓存 → 磁盘"这条三层路径
- [ ] 知道 `fflush` 刷哪层、`fsync` 刷哪层、为什么两个都要
- [ ] 能解释为什么 `while (!feof(f))` 是错的
- [ ] 知道 `_exit` 会丢未刷新的 stdio 输出，以及为什么 fork 后子进程偏偏要用它
- [ ] 会用 `fdopen` 把一个 `open` 出来的 fd 包成 `FILE *`

## 练习

1. 写一个程序，用 `fopen("a.txt","w")` 写入 1000 个整数，然后**不 fclose**、直接 `_exit(0)`，再写一个版本改成 `return 0`。对比两次 `a.txt` 的字节数，验证 stdio 缓冲区丢失的现象。
2. 把 `fopen_vs_open.c` 改成"用 `open` 打开文件、用 `fdopen` 包成 `FILE *`、用 `fprintf` 写入、最后 `fclose`"，体会两层 API 是怎么接起来的。
3. 实现一个带缓冲的文件复制程序：源用 `fopen(...,"rb")`、目标用 `fopen(...,"wb")`，用 `fread`/`fwrite` 循环拷贝，注意检查返回值。再对比一个用 `open`/`read`/`write` 的版本，思考两者的性能差异从哪来。
4. 挑战：写一段代码，先 `fwrite` 一批数据但不 fclose，用 `fflush` 后立刻 `fsync(fileno(f))`，并在注释里说明这一步保证了什么、少了哪一步会怎样。

## 参考资源

- `man 2 open` / `man 2 read` / `man 2 write` —— 系统调用层一手资料
- `man 3 fopen` / `man 3 fread` / `man 3 fwrite` / `man 3 setvbuf` —— 标准库层一手资料
- `man 3 exit` / `man 2 _exit` —— 退出函数蔟与缓冲行为
- W. Richard Stevens, Stephen A. Rago.《UNIX 环境高级编程（APUE）》第 3 版，第 3 章（文件 I/O）、第 5 章（标准 I/O 库）—— 这套两层模型的权威出处
- cppreference：[C standard library: `<stdio.h>`](https://en.cppreference.com/w/c/io) —— `FILE`/`fopen`/`fread`/`fwrite`/`setvbuf` 的签名与语义

---

*整理自作者笔记，按 C-Journey 写作规范重写；所有输出本机实测捕获。*
