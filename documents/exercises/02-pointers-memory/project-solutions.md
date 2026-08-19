---
title: "阶段 2 Project 参考实现"
description: "阶段 2 综合项目（动态通讯录 dybook）的完整参考实现：分层任务逐步讲解（堆上数组、tmp 模式扩容、memmove 删除、qsort 排序、转移表与原地去重），每段标注知识点链接，含真实运行输出与 sanitizer 会话。"
chapter: 2
order: 5
tags:
  - host
  - pointers
  - memory
difficulty: advanced
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 2 Project 题面"
related:
  - "阶段 2 各章"
---

# 阶段 2 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1）真实运行得到。参考实现只是**一种**过关方式；你的实现不一样、验收标准对得上，就都是对的。注意本项目 `.clang-format` 是 LLVM 基底，C 风格转换后要带空格（`(const Contact*) a`），指针靠左（`char* p`），照抄时别改掉。

## 热身（L1）：指针自检 {#pj-warm}

**思路**：四条基本功一口气验证——别名、多级指针、`&p` 与 `p` 的区别、指针大小。这 20 行是后面所有代码的「地基验收」。

```c
#include <stdio.h>

int main(void) {
    int x = 1;
    int* p = &x;
    *p = 2;      /* 别名:改 *p 就是改 x */
    int** pp = &p;
    **pp = 3;    /* 顺着 pp -> p -> x */
    printf("别名链: x = %d\n", x);
    printf("&x == p ? %d\n", (void*) &x == (void*) p);
    printf("&p != p ? %d\n", (void*) &p != (void*) p);
    printf("sizeof(int*) = %zu\n", sizeof(int*));
    return 0;
}
```

→ 知识点：[第 1 章：指针是什么](/02-pointers-memory/01-what-is-a-pointer)（别名、多级指针、指针大小三节）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra self_check.c -o self_check && ./self_check
别名链: x = 3
&x == p ? 1
&p != p ? 1
sizeof(int*) = 8
```

## 核心任务（L2）：堆上的通讯录 {#pj-core}

**思路**：通讯录本体是 `malloc` 来的动态数组（初始容量 4），`count` 记条数；命令循环 `fgets` 读行、去换行、`sscanf` 拆命令词、if-else 分派；`quit` 走 `free_book()` 归还堆内存。

**数据与全局状态**——`Contact` 是阶段 1 的 typedef 手艺，`book`/`count`/`cap` 三个全局拼成「动态数组三件套」：指针、条数、容量。→ 知识点：[第 6 章：动态内存入门](/02-pointers-memory/06-malloc-free-basics)（malloc 动态数组）、[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)（typedef 结构体）

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_CAP 4
#define NAME_MAX 32
#define PHONE_MAX 24
#define LINE_MAX 256

typedef struct {
    char name[NAME_MAX];
    char phone[PHONE_MAX];
} Contact;

typedef int (*CmdFn)(const char* args);

static Contact* book = NULL;
static int count = 0;
static int cap = 0;

static void free_book(void) {
    free(book);
    book = NULL;
    count = 0;
    cap = 0;
}
```

**`cmd_add`**——`sscanf` 拆「名字 电话」，查容量，`snprintf` 兜底拷贝。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)（malloc 后像数组一样用）、[第 11 章](/01-c-basics/11-c-strings-and-libc)（`fgets` 的边界意识；`sscanf` 是**教材外补充**——`scanf` 的字符串版，返回值纪律同第 12 章的 `scanf`）

```c
static int cmd_add(const char* args) {
    char name[NAME_MAX];
    char phone[PHONE_MAX];
    if (sscanf(args, "%31s %23s", name, phone) != 2) {
        printf("用法: add <名字> <电话>\n");
        return 0;
    }
    if (!ensure_capacity()) {
        printf("内存不足\n");
        return 0;
    }
    snprintf(book[count].name, sizeof(book[count].name), "%s", name);
    snprintf(book[count].phone, sizeof(book[count].phone), "%s", phone);
    count++;
    printf("已添加 %s\n", name);
    return 0;
}
```

**`cmd_list`**——`%4d`/`%-16s` 对齐出表格，顺手把「条数/容量」打出来，扩容有没有发生一眼可见。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（printf 宽度对齐）

```c
static int cmd_list(const char* args) {
    (void) args;
    printf("%4s %-16s %s\n", "序号", "姓名", "电话");
    for (int i = 0; i < count; i++) {
        printf("%4d %-16s %s\n", i, book[i].name, book[i].phone);
    }
    printf("共 %d 条(容量 %d)\n", count, cap);
    return 0;
}
```

**命令循环（核心版）**——`fgets` 读一行、`trim_newline` 去换行、`sscanf` 拆命令词、跳过命令词算出 `args`、if-else 分派。这一版是 L2 的目标形态，L5 会把分派换成转移表。→ 知识点：[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)（fgets 存换行符）、[第 7 章：控制流](/01-c-basics/07-control-flow)（if-else 分派）

```c
static void trim_newline(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

int main(void) {
    /* L2 版分派:if-else 链(见 L5 换成转移表) */
    ...
    while (fgets(line, sizeof(line), stdin) != NULL) {
        trim_newline(line);
        char cmd[NAME_MAX] = {0};
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }
        if (strcmp(cmd, "add") == 0) {
            cmd_add(args);
        } else if (strcmp(cmd, "list") == 0) {
            cmd_list(args);
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else {
            printf("未知命令: %s\n", cmd);
        }
    }
    free_book();
    return 0;
}
```

**`Makefile`**——变量 + `clean`/`.PHONY`（阶段 0 的老规矩）。→ 知识点：[阶段 0 第 11 章](/00-dev-environment/12-make-basics)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wconversion -Werror
LDFLAGS =

dybook: dybook.c
	$(CC) $(CFLAGS) -o dybook dybook.c $(LDFLAGS)

clean:
	rm -f dybook

.PHONY: clean
```

**验证输出**（核心会话，前几行）：

```text
$ make
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -o dybook dybook.c
$ ./dybook
命令: add/list/rm/find/sort/dedupe/quit
> add Alice 13800000001
已添加 Alice
> add Bob 13900000002
已添加 Bob
> list
序号 姓名           电话
   0 Alice            13800000001
   1 Bob              13900000002
共 2 条(容量 4)
> quit
```

## 进阶任务（L3）：自动扩容与删除 {#pj-grow}

**思路**：`ensure_capacity` 把「放不下就翻倍」收进一个函数——`realloc` 失败返回 `NULL` 而原指针仍有效，所以必须 `tmp` 接、成功才赋回，直接 `book = realloc(book, ...)` 一旦失败就把唯一指向那块堆内存的指针弄丢了（泄漏 + 数据丢失）；`rm` 的前移搬移是重叠区间，用 `memmove`。

**`ensure_capacity`**——首次分配走 `INIT_CAP`，之后每次翻倍。→ 知识点：[第 6 章](/02-pointers-memory/06-malloc-free-basics)「realloc」一节（tmp 模式）

```c
static int ensure_capacity(void) {
    if (count < cap) {
        return 1;
    }
    int new_cap = (cap == 0) ? INIT_CAP : cap * 2;
    Contact* tmp = realloc(book, (size_t) new_cap * sizeof(Contact));
    if (tmp == NULL) {
        return 0;
    }
    book = tmp;
    cap = new_cap;
    return 1;
}
```

**`cmd_rm`**——找到下标后 `memmove` 把后面整体前移一位，`count--`。→ 知识点：[第 11 章：void* 与字节操作](/02-pointers-memory/11-void-ptr-and-byte-ops)（重叠换 memmove）、[第 5 章](/02-pointers-memory/05-pointer-array-string)（strcmp）

```c
static int cmd_rm(const char* args) {
    char name[NAME_MAX];
    if (sscanf(args, "%31s", name) != 1) {
        printf("用法: rm <名字>\n");
        return 0;
    }
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(book[i].name, name) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        printf("没有找到 %s\n", name);
        return 0;
    }
    memmove(book + found, book + found + 1, (size_t) (count - found - 1) * sizeof(Contact));
    count--;
    printf("已删除 %s\n", name);
    return 0;
}
```

**`cmd_find`**——`strstr` 模糊匹配；它区分大小写，`"Alice"` 里的 `A` 是大写、不匹配小写 `a`（下面真实输出里 `find a` 只命中 Carol/Dave 就是这个原因）。→ 知识点：[第 5 章](/02-pointers-memory/05-pointer-array-string)（`<string.h>` 家族都是 char* 遍历的模式匹配）

```c
static int cmd_find(const char* args) {
    char key[NAME_MAX];
    if (sscanf(args, "%31s", key) != 1) {
        printf("用法: find <子串>\n");
        return 0;
    }
    int hits = 0;
    for (int i = 0; i < count; i++) {
        if (strstr(book[i].name, key) != NULL) {
            printf("  %-16s %s\n", book[i].name, book[i].phone);
            hits++;
        }
    }
    printf("匹配 %d 条\n", hits);
    return 0;
}
```

**验证输出**（5 人加满触发扩容 + rm + find）：

```text
> add Alice 13800000001
已添加 Alice
> add Bob 13900000002
已添加 Bob
> add Carol 13700000003
已添加 Carol
> add Dave 13600000004
已添加 Dave
> add Eve 13500000005
已添加 Eve
> list
序号 姓名           电话
   0 Alice            13800000001
   1 Bob              13900000002
   2 Carol            13700000003
   3 Dave             13600000004
   4 Eve              13500000005
共 5 条(容量 8)          ← 4 满后 tmp 模式翻倍到 8
> find a
  Carol            13700000003
  Dave             13600000004
匹配 2 条               ← strstr 区分大小写:Alice 的大写 A 不匹配小写 a
> rm Bob
已删除 Bob
> list
序号 姓名           电话
   0 Alice            13800000001
   1 Carol            13700000003      ← memmove 把后面整体前移
   2 Dave             13600000004
   3 Eve              13500000005
共 4 条(容量 8)
```

## 再进阶任务（L4）：排序、健壮性与质量门 {#pj-sort}

**思路**：`qsort` 排 `Contact` 数组，比较函数把 `const void*` 转回 `const Contact*` 再 `strcmp`；健壮性靠 `sscanf` 返回值 + `%31s`/`%23s` 限宽 + `snprintf` 兜底；`-Wconversion -Werror` 逼你把 `int`→`size_t` 的每次隐式转换都显式写 `(size_t)`。

**`cmd_sort` 与 `cmp_name`**——`qsort(book, count, sizeof(Contact), cmp_name)` 原地排。→ 知识点：[第 9 章：函数指针](/02-pointers-memory/09-function-pointers)「标准库 qsort」一节

```c
static int cmp_name(const void* a, const void* b) {
    const Contact* ca = (const Contact*) a;
    const Contact* cb = (const Contact*) b;
    return strcmp(ca->name, cb->name);
}

static int cmd_sort(const char* args) {
    (void) args;
    qsort(book, (size_t) count, sizeof(Contact), cmp_name);
    printf("已按姓名排序\n");
    return 0;
}
```

**健壮性**——`sscanf` 返回值不足 2 报用法；`%31s`/`%23s` 限宽；`snprintf` 兜底。→ 知识点：[第 12 章](/01-c-basics/12-basic-io)（scanf 返回值是唯一可靠的校验手段）

**验证输出**（健壮性测试）：

```text
$ ./dybook
命令: add/list/rm/find/sort/dedupe/quit
> add Alice
用法: add <名字> <电话>          ← 缺参数:返回值不足 2 被拦下
> add A123456789012345678901234567890123456789 111
已添加 A123456789012345678901234567890
> list
序号 姓名           电话
   0 A123456789012345678901234567890 123456789
共 1 条(容量 4)
```

这里有个真实细节值得注意：超长名字被 `%31s` 限宽成 31 字符，但 `scanf` 在宽度上限处停下时**并不跳到空白之后**，于是名字里剩下的 9 个字符继续喂给了下一个 `%23s`——电话字段变成了 `123456789`，你输入的 `111` 被丢在输入流里。所以限宽防住了越界，但「字段错位」还得靠输入约定（名字里不带空白、按空格分词）来兜，这正是不安全 libc 与 `scanf` 家族的固有脾气。

**质量门**——零警告构建 + sanitizer 会话零报告：

```text
$ make
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -o dybook dybook.c      ← 零警告
$ make CFLAGS="-std=c11 -Wall -Wextra -Wconversion -Werror -O0 -g \
      -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
$ ./dybook
... (完整会话见 L5,零报告)
```

→ 知识点：[阶段 0 第 8 章](/00-dev-environment/09-warning-flags)（`-Wconversion`）、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)（ASan/UBSan）

## 终极挑战（L5）：转移表与原地去重 {#pj-l5}

**思路**：①转移表把「命令名 → 函数指针」收进一张数据表，主循环只剩「查表、调用」两件事——加命令只加表项；②`dedupe` 先 `qsort` 让同名相邻，再用读写双指针原地收结果；这个读写指针法是**教材外补充**（阶段 3 才细讲）：`read` 从头扫到尾，遇到「和已收下最后一个不同的名字」就 `book[write++] = book[read]`，结构体整体赋值是阶段 1 的手艺。

**转移表**——`typedef int (*CmdFn)(const char* args);` 起别名，表项是「名字 + 函数指针」的结构体数组。→ 知识点：[第 10 章：复杂声明与 typedef](/02-pointers-memory/10-complex-declarations-typedef)（typedef 函数指针）、[第 9 章](/02-pointers-memory/09-function-pointers)「函数指针数组：转移表」一节

```c
int main(void) {
    static const struct {
        const char* name;
        CmdFn fn;
    } table[] = {
        {"add", cmd_add},     {"list", cmd_list},   {"rm", cmd_rm},       {"find", cmd_find},
        {"sort", cmd_sort},   {"dedupe", cmd_dedupe}, {"help", cmd_help}, {"quit", cmd_quit},
    };
    const size_t table_len = sizeof(table) / sizeof(table[0]);

    char line[LINE_MAX];
    printf("命令: add/list/rm/find/sort/dedupe/quit\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        trim_newline(line);
        char cmd[NAME_MAX] = {0};
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }
        const char* args = line;
        while (*args != '\0' && *args != ' ' && *args != '\t') {
            args++;
        }
        while (*args == ' ' || *args == '\t') {
            args++;
        }

        int quit = 0;
        int found = 0;
        for (size_t i = 0; i < table_len; i++) {
            if (strcmp(table[i].name, cmd) == 0) {
                quit = table[i].fn(args);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("未知命令: %s\n", cmd);
            continue;
        }
        if (quit) {
            break;
        }
    }
    free_book();
    return 0;
}
```

**`cmd_dedupe`**——先排后去，双指针收结果。→ 知识点：[第 9 章](/02-pointers-memory/09-function-pointers)（qsort）、排序去重的读写指针是教材外补充（阶段 3 细讲）、[第 13 章](/01-c-basics/13-struct-union-enum)（结构体整体赋值）

```c
static int cmd_dedupe(const char* args) {
    (void) args;
    if (count == 0) {
        printf("通讯录为空\n");
        return 0;
    }
    qsort(book, (size_t) count, sizeof(Contact), cmp_name);
    int write = 1;
    for (int read = 1; read < count; read++) {
        if (strcmp(book[read].name, book[write - 1].name) != 0) {
            if (write != read) {
                book[write] = book[read];
            }
            write++;
        }
    }
    count = write;
    printf("去重后 %d 条\n", count);
    return 0;
}
```

**验证输出**（sanitizer 构建下的完整会话，零报告）：

```text
$ make CFLAGS="-std=c11 -Wall -Wextra -Wconversion -Werror -O0 -g \
      -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
$ ./dybook
命令: add/list/rm/find/sort/dedupe/quit
> add Alice 13800000001
已添加 Alice
> add Bob 13900000002
已添加 Bob
> add Carol 13700000003
已添加 Carol
> add Dave 13600000004
已添加 Dave
> add Eve 13500000005
已添加 Eve
> rm Bob
已删除 Bob
> sort
已按姓名排序
> add Alice 99900000009
已添加 Alice                ← 两条 Alice
> dedupe
去重后 4 条                 ← 排序 + 双指针,去掉一条
> list
序号 姓名           电话
   0 Alice            13800000001
   1 Carol            13700000003
   2 Dave             13600000004
   3 Eve              13500000005
共 4 条(容量 8)
> quit
                              ← ASan/UBSan 零报告,退出码 0
```

到这里，「指针 + 堆 = 数据活起来」就有了实物：一个通讯录，`malloc` 起家、`realloc` 生长、`memmove` 腾挪、`qsort` 排布、函数指针表分发、`free` 收尾——阶段 2 的每一章都在这 300 行里干着自己的那份活。下一个阶段，数据结构会把这些「堆上节点 + 指针串联」的手艺焊成链表、树与哈希表。
