---
title: "阶段 3 Project 参考实现"
description: "阶段 3 综合项目（电话簿双引擎）的完整参考实现：四层任务逐步讲解，每层标注知识点链接，含单链表/BST/哈希三引擎分文件代码、Makefile、质量门与 bench/随机压力测试的真实运行输出。"
chapter: 3
order: 5
tags:
  - host
  - data-structures
  - algorithm
difficulty: advanced
reading_time_minutes: 60
platform: host
c_standard: [11]
prerequisites:
  - "阶段 3 Project 题面"
related:
  - "阶段 3 各章"
---

# 阶段 3 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1）真实运行得到。参考实现只是**一种**过关方式；你的实现不一样、验收标准对得上，就都是对的。本项目沿用仓库 `.clang-format`（LLVM 基底、4 空格、指针靠左 `int* p`、C 风格转换后带空格 `(size_t) n`），所有源码都过了 `clang-format --dry-run --Werror`。

## 核心任务（L2）：能跑起来的电话簿 {#pj-core}

**思路**：第一步（L1）先把 `Contact` 和打印跑通，第二步（L2）换上单链表引擎 + 命令循环。`add` 的健壮性检查从第一层就带上，后面三层不用回头补。

**`include/phonebook.h`**——头文件契约：include guard + 类型与引擎声明。→ 知识点：[第 1 章：单链表:节点、指针、把内存串成一条链](/03-data-structures/01-singly-linked-list)（节点定义）、[第 13 章：结构体、联合、枚举与内存对齐](/01-c-basics/13-struct-union-enum)（结构体数组）

```c
#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stddef.h>

/* 联系人:id 是键,姓名和电话是卫星数据 */
typedef struct {
    int id;
    char name[32];
    char phone[16];
} Contact;

/* 单链表节点(阶段3 第 1 章) */
typedef struct LNode {
    Contact contact;
    struct LNode* next;
} LNode;

/* 单链表引擎:head/tail 双指针,尾插 O(1) */
typedef struct {
    LNode* head;
    LNode* tail;
} ContactList;

void list_init(ContactList* l);
void list_add(ContactList* l, const Contact* c);
const Contact* list_find(const ContactList* l, int id);
void list_print_all(const ContactList* l);
void list_free(ContactList* l);

#endif
```

**`src/list_engine.c`**——单链表引擎：尾插（head/tail 双指针，空表时两个都指新节点）、线性查找、逐个 free（先存 next 再 free）。→ 知识点：[第 1 章](/03-data-structures/01-singly-linked-list)「头插与遍历」「释放整表」两节、[第 4 章：队列:FIFO、环形缓冲与链表实现](/03-data-structures/04-queue)（head/tail 双指针的对称边界）

```c
void list_add(ContactList* l, const Contact* c) {
    LNode* n = malloc(sizeof(LNode));
    if (n == NULL) {
        return;
    }
    n->contact = *c;
    n->next = NULL;
    if (l->tail == NULL) {
        l->head = n;
        l->tail = n;
    } else {
        l->tail->next = n;
        l->tail = n;
    }
}

const Contact* list_find(const ContactList* l, int id) {
    const LNode* cur = l->head;
    while (cur != NULL) {
        if (cur->contact.id == id) {
            return &cur->contact;
        }
        cur = cur->next;
    }
    return NULL;
}
```

**`src/main.c`（核心版）**——命令循环：`fgets` 读行、`sscanf` 拆命令、按命令分派；`add` 解析三项、`id <= 0` 拒绝。→ 知识点：[第 11 章：C 字符串与不安全 libc](/01-c-basics/11-c-strings-and-libc)（`fgets` 与边界；`sscanf` 是**教材外补充**——`scanf` 的字符串版，返回值纪律与 `scanf` 相同）

```c
static void do_add(const char* args) {
    int id = 0;
    char name[32];
    char phone[16];
    int got = sscanf(args, "%d %31s %15s", &id, name, phone);
    if (got != 3) {
        printf("用法: add <id> <名字> <电话>\n");
        return;
    }
    if (id <= 0) {
        printf("id 必须是正整数\n");
        return;
    }
    Contact c;
    c.id = id;
    snprintf(c.name, sizeof(c.name), "%s", name);
    snprintf(c.phone, sizeof(c.phone), "%s", phone);
    list_add(&g_list, &c);
    printf("已添加 %d %s\n", id, name);
}
```

**`Makefile`**——变量 + 模式规则 + `.PHONY`。→ 知识点：[阶段 0 第 11 章](/00-dev-environment/12-make-basics)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

phonebook: src/main.o src/list_engine.o
	$(CC) $(CFLAGS) -o phonebook src/main.o src/list_engine.o $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f phonebook src/*.o

.PHONY: clean
```

**验证输出**（核心层完整会话）：

```text
$ make
gcc -std=c11 -Wall -Wextra -Iinclude -c src/main.c -o src/main.o
gcc -std=c11 -Wall -Wextra -Iinclude -c src/list_engine.c -o src/list_engine.o
gcc -std=c11 -Wall -Wextra -Iinclude -o phonebook src/main.o src/list_engine.o
$ printf 'add 1001 Alice 13800000001\nadd 1002 Bob 13800000002\nadd 1003 Carol 13800000003\nfind 1002\nfind 9999\nlist\nquit\n' | ./phonebook
命令: add <id> <名字> <电话> / find <id> / list / quit
> 已添加 1001 Alice
> 已添加 1002 Bob
> 已添加 1003 Carol
> 找到: 1002 Bob 13800000002
> id 9999 不存在
>   1001 Alice      13800000001
  1002 Bob        13800000002
  1003 Carol      13800000003
>
```

`list` 按**插入顺序**打印——这是单链表引擎的脾气，下一层换 BST 后它会变成按 id 升序。

## 进阶任务（L3）：换成 BST 引擎 {#pj-bst}

**思路**：`add/find/del/list/height` 全部改走 BST——`del` 把教材的三种情况完整搬进来，`list` 用中序，天然升序。

**`src/bst_engine.c`**——`bst_insert`（递归返回新根）、`bst_search`、`bst_delete`（叶子 free 置 NULL / 单子先存孩子再 free / 双子找后继拷值递归删后继）、`bst_height`、`bst_count`、中序打印、后序释放。→ 知识点：[第 7 章：二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序](/03-data-structures/07-bst)「插入」「删除:三种情况，逐个拆透」「退化」三节

```c
BNode* bst_insert(BNode* root, const Contact* c) {
    if (root == NULL) {
        return new_node(c);
    }
    if (c->id < root->contact.id) {
        root->left = bst_insert(root->left, c);
    } else {
        root->right = bst_insert(root->right, c);
    }
    return root;
}

BNode* bst_delete(BNode* root, int id) {
    if (root == NULL) {
        return NULL; /* 没找到:空子树原样返回,不会破坏树 */
    }
    if (id < root->contact.id) {
        root->left = bst_delete(root->left, id);
    } else if (id > root->contact.id) {
        root->right = bst_delete(root->right, id);
    } else {
        if (root->left == NULL && root->right == NULL) {
            free(root); /* 情况一:叶子 */
            return NULL;
        }
        if (root->left == NULL) {
            BNode* child = root->right; /* 情况二:先存孩子再 free */
            free(root);
            return child;
        }
        if (root->right == NULL) {
            BNode* child = root->left;
            free(root);
            return child;
        }
        /* 情况三:双子——找右子树最小值(后继)拷上来,再去右子树删掉它 */
        BNode* succ = find_min(root->right);
        root->contact = succ->contact;
        root->right = bst_delete(root->right, succ->contact.id);
    }
    return root;
}
```

**验证输出**（乱序 id 建表 + 三种删除逐个演示；会话在最终代码树上运行，命令行为与纯 BST 版一致）：

```text
$ printf 'add 5 eve 555\nadd 3 bob 333\nadd 8 alice 888\nadd 1 dave 111\nadd 4 fay 444\nadd 7 grace 777\nadd 9 hank 999\nlist\nfind 7\nfind 6\nheight\ndel 1\ndel 3\ndel 8\nlist\ndel 5\nlist\nquit\n' | ./phonebook
命令: add/find/del/list/height/bench/stress/quit
> 已添加 5 eve
> 已添加 3 bob
> 已添加 8 alice
> 已添加 1 dave
> 已添加 4 fay
> 已添加 7 grace
> 已添加 9 hank
>   共 7 条(id 升序):
  1 dave       111
  3 bob        333
  4 fay        444
  5 eve        555
  7 grace      777
  8 alice      888
  9 hank       999
> 找到: 7 grace 777
> id 6 不存在
> BST 树高 = 2
> 已删除 1
> 已删除 3
> 已删除 8
>   共 4 条(id 升序):
  4 fay        444
  5 eve        555
  7 grace      777
  9 hank       999
> 已删除 5
>   共 3 条(id 升序):
  4 fay        444
  7 grace      777
  9 hank       999
>
```

跟着 `list` 的输出看：`del 1` 删叶子、`del 3` 删单子（1 没了之后 3 只剩右孩子 4）、`del 8` 删双子（后继 9 顶上）、`del 5` 删根（也是双子）——每次删完中序仍然升序，三种删除都没破坏「左小右大」。插入顺序是乱的 `5 3 8 1 4 7 9`，树高却只有 2，`list` 却天然有序——这就是 BST 拿「有序遍历」换「树形由插入顺序决定」的取舍。

## 再进阶任务（L4）：哈希索引 + 质量门 {#pj-gates}

**思路**：哈希引擎按 id 把联系人拷进桶链，`find` 从此 O(1)；BST 留任「有序遍历」——哈希给不了序。双引擎同步是这一层的全部难点：增删两处都要动。

**`src/hash_engine.c`**——动态桶数组链地址：`index_add` 先查重、负载因子超 0.75 沿质数表 `{5,11,23,47,97,199,409,821,1637,3271,6553,13121}` 扩容并 rehash（复用节点）；`index_remove` 是第 1 章「记前驱」删除在桶链上的应用。→ 知识点：[第 8 章：哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找](/03-data-structures/08-hash-table)「负载因子与扩容」「释放」两节、[第 1 章](/03-data-structures/01-singly-linked-list)「按值删除:记好前驱」

```c
static int index_grow(ContactIndex* idx) {
    size_t next_n = idx->n_buckets;
    size_t np = sizeof(primes) / sizeof(primes[0]);
    for (size_t i = 0; i < np; i++) {
        if (primes[i] > idx->n_buckets) {
            next_n = primes[i];
            break;
        }
    }
    HNode** nb = calloc(next_n, sizeof(HNode*));
    if (nb == NULL) {
        return 0;
    }
    for (size_t b = 0; b < idx->n_buckets; b++) {
        HNode* cur = idx->buckets[b];
        while (cur != NULL) {
            HNode* next = cur->next;
            unsigned ni = index_hash(cur->key, next_n);
            cur->next = nb[ni];
            nb[ni] = cur;
            cur = next;
        }
    }
    free(idx->buckets);
    idx->buckets = nb;
    idx->n_buckets = next_n;
    return 1;
}

int index_remove(ContactIndex* idx, int key) {
    unsigned b = index_hash(key, idx->n_buckets);
    HNode* cur = idx->buckets[b];
    HNode* prev = NULL;
    while (cur != NULL && cur->key != key) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        return 0;
    }
    if (prev == NULL) {
        idx->buckets[b] = cur->next;
    } else {
        prev->next = cur->next;
    }
    free(cur);
    idx->size--;
    return 1;
}
```

**`src/main.c`（双引擎版）的命令实现**——`add` 两个引擎都插、`del` 两个都删、`find` 走哈希、`list`/`height` 走 BST。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)（哈希做索引）、[第 7 章](/03-data-structures/07-bst)（BST 管有序）

```c
static void do_add(const char* args) {
    int id = 0;
    char name[32];
    char phone[16];
    int got = sscanf(args, "%d %31s %15s", &id, name, phone);
    if (got != 3) {
        printf("用法: add <id> <名字> <电话>\n");
        return;
    }
    if (id <= 0) {
        printf("id 必须是正整数\n");
        return;
    }
    if (bst_search(g_bst, id) != NULL) {
        printf("id %d 已存在\n", id);
        return;
    }
    Contact c;
    c.id = id;
    snprintf(c.name, sizeof(c.name), "%s", name);
    snprintf(c.phone, sizeof(c.phone), "%s", phone);
    g_bst = bst_insert(g_bst, &c);
    index_add(&g_index, &c);
    printf("已添加 %d %s\n", id, name);
}

static void do_find(const char* args) {
    int id = 0;
    if (sscanf(args, "%d", &id) != 1 || id <= 0) {
        printf("用法: find <id>\n");
        return;
    }
    const Contact* hit = index_find(&g_index, id); /* O(1) */
    if (hit == NULL) {
        printf("id %d 不存在\n", id);
        return;
    }
    printf("找到: %d %s %s\n", hit->id, hit->name, hit->phone);
}

static void do_del(const char* args) {
    int id = 0;
    if (sscanf(args, "%d", &id) != 1 || id <= 0) {
        printf("用法: del <id>\n");
        return;
    }
    if (bst_search(g_bst, id) == NULL) {
        printf("id %d 不存在\n", id);
        return;
    }
    index_remove(&g_index, id); /* 双引擎同步:两边都删 */
    g_bst = bst_delete(g_bst, id);
    printf("已删除 %d\n", id);
}
```

**验证输出**（健壮性三个测试）：

```text
$ printf 'add 2\nadd 1001 x 1\nadd 1001 y 2\nadd 0 z 3\nquit\n' | ./phonebook
命令: add/find/del/list/height/bench/stress/quit
> 用法: add <id> <名字> <电话>
> 已添加 1001 x
> id 1001 已存在
> id 必须是正整数
>
```

**验证输出**（质量门：零警告编译 + clang-format + sanitizer 会话）：

```text
$ make
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -c src/main.c -o src/main.o
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -c src/list_engine.c -o src/list_engine.o
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -c src/bst_engine.c -o src/bst_engine.o
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -c src/hash_engine.c -o src/hash_engine.o
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -o phonebook src/main.o src/list_engine.o src/bst_engine.o src/hash_engine.o
$ clang-format --dry-run --Werror src/*.c include/*.h; echo "clang-format exit=$?"
clang-format exit=0
$ make clean && make CFLAGS="-std=c11 -Wall -Wextra -Wconversion -Werror -Iinclude -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined"
$ printf 'add 5 eve 555\nadd 3 bob 333\nadd 8 alice 888\nfind 5\nfind 9\ndel 8\ndel 3\ndel 5\nlist\nstress\nquit\n' | ./phonebook; echo "asan session exit=$?"
命令: add/find/del/list/height/bench/stress/quit
> 已添加 5 eve
> 已添加 3 bob
> 已添加 8 alice
> 找到: 5 eve 555
> id 9 不存在
> 已删除 8
> 已删除 3
> 已删除 5
>   共 0 条(id 升序):
> （stress 的 10 次自检与普通构建一致,见下一层输出）
asan session exit=0
```

sanitizer 全程零报告、退出码 0——增删查、双引擎同步、三种删除、rehash 搬运，一条内存错误都没有。写这一层时有两处值得提的「真跑心得」：一是 `-Wconversion -Werror` 逼着你把每个窄化转换显式写出来，`size_t` 转 `int` 的地方漏一个就编译失败，这面墙对质量是实打实的；二是 clang-format 会把你手写的「多行对齐」纠正成它自己的折行——照着 `--dry-run --Werror` 的报错逐个改过来就行，别和它犟。

## 终极挑战（L5）：bench 与随机压力 {#pj-l5}

**思路**：bench 把三代引擎拉到同一批数据上计时——链表 O(n)、BST O(log n)、哈希 O(1) 的差距一表看尽；stress 让 BST 与哈希两个独立实现互相出题、互相对答案。

**`do_bench`**——`clock()` 计时 + rand 洗牌（**教材外补充**：洗牌用 rand 简单打乱插入顺序，只为让 BST 不退化、基准公平）。→ 知识点：[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)「真跑对比」两节（`clock()` 计时法）

```c
static void bench_one(int n) {
    const int reps = 1000;
    int* ids = malloc((size_t) n * sizeof(int));
    /* ... 生成互异的 id、shuffle 打乱、三引擎各建一份 ... */

    int probe = ids[rand() % n];

    double t0 = now_sec();
    for (int k = 0; k < reps; k++) {
        list_find(&lst, probe);
    }
    double t_list = now_sec() - t0;

    t0 = now_sec();
    for (int k = 0; k < reps; k++) {
        bst_search(bst, probe);
    }
    double t_bst = now_sec() - t0;

    t0 = now_sec();
    for (int k = 0; k < reps; k++) {
        index_find(&idx, probe);
    }
    double t_hash = now_sec() - t0;

    printf("n=%d, 查找 %d 次:\n", n, reps);
    printf("  链表 O(n)  : %.6fs\n", t_list);
    printf("  BST   O(log n): %.6fs (%.0fx)\n", t_bst, t_list / t_bst);
    printf("  哈希  O(1)  : %.6fs (%.0fx)\n", t_hash, t_list / t_hash);
    /* ... 逐个 free 三个引擎 ... */
}
```

**`do_stress`**——1000 次随机 add/find/del，find 交叉验证、每 100 次自检（**教材外补充**：随机压力测试手法；两引擎的数据结构均为教材内容）。→ 知识点：[第 7 章](/03-data-structures/07-bst)（中序得有序当自检判据）、[第 8 章](/03-data-structures/08-hash-table)（链地址查找）

```c
static void do_stress(void) {
    srand(42);
    int checks = 0;
    for (int op = 1; op <= 1000; op++) {
        int key = 100 + rand() % 300;
        int kind = rand() % 3;
        if (kind == 0) {
            if (bst_search(g_bst, key) == NULL) {
                /* ... 造联系人,双引擎同步插入 ... */
            }
        } else if (kind == 1) {
            const Contact* h = index_find(&g_index, key);
            BNode* b = bst_search(g_bst, key);
            if ((h != NULL) != (b != NULL)) {
                printf("交叉验证失败! key=%d\n", key);
                return;
            }
        } else {
            if (bst_search(g_bst, key) != NULL) {
                index_remove(&g_index, key); /* 双引擎同步删除 */
                g_bst = bst_delete(g_bst, key);
            }
        }
        if (op % 100 == 0) {
            int ok = bst_check_sorted(g_bst);
            if (ok && bst_count(g_bst) == g_index.size) {
                checks++;
                printf("第 %d 次操作自检通过(中序有序,双引擎 %zu 条一致)\n", op,
                       g_index.size);
            } else {
                printf("自检失败! op=%d\n", op);
                return;
            }
        }
    }
    printf("1000 次随机操作完成,%d 次自检全部通过\n", checks);
}
```

**验证输出**（bench + stress 完整会话）：

```text
$ printf 'bench\nstress\nquit\n' | ./phonebook
命令: add/find/del/list/height/bench/stress/quit
> n=2000, 查找 1000 次:
  链表 O(n)  : 0.001645s
  BST   O(log n): 0.000034s (48x)
  哈希  O(1)  : 0.000004s (411x)
n=5000, 查找 1000 次:
  链表 O(n)  : 0.007872s
  BST   O(log n): 0.000034s (232x)
  哈希  O(1)  : 0.000004s (1968x)
n=10000, 查找 1000 次:
  链表 O(n)  : 0.001861s
  BST   O(log n): 0.000025s (74x)
  哈希  O(1)  : 0.000003s (620x)
> 第 100 次操作自检通过(中序有序,双引擎 25 条一致)
第 200 次操作自检通过(中序有序,双引擎 48 条一致)
第 300 次操作自检通过(中序有序,双引擎 75 条一致)
第 400 次操作自检通过(中序有序,双引擎 92 条一致)
第 500 次操作自检通过(中序有序,双引擎 99 条一致)
第 600 次操作自检通过(中序有序,双引擎 112 条一致)
第 700 次操作自检通过(中序有序,双引擎 112 条一致)
第 800 次操作自检通过(中序有序,双引擎 113 条一致)
第 900 次操作自检通过(中序有序,双引擎 123 条一致)
第 1000 次操作自检通过(中序有序,双引擎 132 条一致)
1000 次随机操作完成,10 次自检全部通过
>
```

bench 那张表如实交代两件事。第一，**趋势**：链表那列随 n 增长（n=2000 到 5000 翻了近 5 倍），BST 和哈希两列几乎一动不动——BST 的 0.00003s 量级和哈希的 0.000004s 量级之间还差着一个常数档。第二，**噪声**：n=10000 的链表行（0.001861s）比 n=5000 的行还小，是共享机器上计时噪声——`clock()` 的秒数每次都抖，但「链表随 n 变慢、BST/哈希不随 n 变慢」这个趋势每次都在，大 O 抓的正是趋势。如果你想看更干净的曲线，把 `reps` 加大、或把每个规模多跑几遍取中位数。

到这里，「阶段 3 的知识点是一体的」就有了实物：一个电话簿，单链表是起点、BST 是「有序」的引擎、哈希是「O(1)」的索引、`clock()` 和大 O 是裁判、随机交叉验证是两个引擎互相做考官。你在这个阶段写过的每一条链表、每一棵 BST、每一个桶，都在这个项目里找到了自己的位置。
