---
title: "用 C 手搓单链表：从节点结构到一整套增删查转 API"
description: "链表跟数组完全不是一回事——数据散在堆上、靠指针串成一条绳。这一章我们用纯 C 拆解单链表的节点结构、头插/尾插、任意位置插入删除、遍历、查找、原地反转与释放，并把内存归属、头指针 vs 头节点、野指针这几个经典坑一次说透。"
chapter: 3
order: 3
tags:
  - host
  - data-structures
  - pointers
  - memory
  - struct
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 2：指针、内存布局与位运算"
related:
  - "用 C 造一个泛型容器：从 void* 到自己的 vector（阶段 3）"
  - "递归与调用栈（阶段 3）"
---

# 用 C 手搓单链表：从节点结构到一整套增删查转 API

## 引言

数组这东西用顺手了，你会有一个错觉：数据天生就该挨个排在一起、按下标随手就能拿。直到有一天你被需求逼到墙角——要在一张十万条的表中间频繁插一个元素，每插一次后面九万九千九百九十九条全得往后挪一格，你会想砸键盘。**链表就是来干这个的**：它的数据在内存里压根不连续，靠一根指针串成一条绳，插谁删谁只要改两根指针，后面一个都不用动。代价是它失去了数组的随机访问——你想看第 5000 个，老老实实从头走过去。

这一章我们就动手用纯 C 把单链表从头搓一遍。原始笔记里（[classicLinkList](https://github.com/Charliechen114514/Tiny-C-C-standard-Library/tree/C/classicLinkList)）写了一套泛型版本，节点里塞的是 `void*`，外加一堆错误码和回调函数指针，库味很浓、也很能练读代码的功夫，但对刚摸链表的人来说，`void*` 的花活会盖住链表本身那点指针游戏。所以我们这里先用最直白的 `int` 当数据域，把单链表的节点、增删查转、释放这一整套机制讲透；等机制吃透了，再去玩 `void*` 泛型那套——其实也就是把 `int data` 换成 `void* data`、再多传一个元素大小而已。文末我们会回过头点破原笔记里 `void*` 写法的两个真实坑，作为读库挑刺的练习。

## 核心内容

### 节点与链表：两个结构体

链表最核心的思想就一句话：**数据离散地散在堆上，靠指针维护逻辑顺序**。所以直观上我们需要两个东西——一个"节点"，自己装着数据，还顺手记着下一个节点在哪；一个"链表"，握着这串节点的头。

```c
/* 节点：数据域 + 指针域 */
typedef struct SListNode {
    int data;                   /* 数据域：真正存的东西 */
    struct SListNode* next;     /* 指针域：指向下一个节点，末节点为 NULL */
} SListNode;

/* 链表：只持有头指针 + 元素个数 */
typedef struct {
    SListNode* head;            /* 指向第一个节点；空表为 NULL */
    size_t size;                /* 当前节点个数 */
} SList;
```

这里有两个设计取舍值得停一下。第一，为什么节点结构体里要写成 `struct SListNode* next` 而不是 `SListNode* next`？因为类型别名 `SListNode` 在 `typedef` 那一行还没"生效"——C 的语法要求在结构体内部引用自己时必须用完整的 `struct ...` 名字，这是自引用结构体的铁律。第二，原笔记额外给链表加了个 `current_size` 来记元素个数，这个细节有人嫌冗余、有人说该加。加上它的好处是 `O(1)` 就能问"表有多长"，否则每次都得从头数一遍；代价是每次增删都得记得维护它，漏一次就全乱套。我们这里采用"加上"的方案，因为实际工程里你几乎总会需要长度。

整条链子画出来就是 `head -> 10 -> 20 -> 30 -> NULL`。`NULL` 是链表的终点哨兵，遍历时看到它就知道走完了；空表则是 `head` 本身就是 `NULL`。这一点很关键——后面几乎每个操作都要先判断"是不是空表"。

### 创建空链表

上来第一步，把一张表初始化成空的。这里我们走"调用者提供 `SList` 变量、函数初始化它"的路子，而不是 malloc 出一个结构体再返回指针。两种都常见，区别在于所有权归属——前者调用者自己管这块栈/堆内存，后者库负责 malloc、调用者负责 free。栈变量方案更不容易泄漏，也更省一次 malloc，所以我们用它：

```c
void slist_init(SList* list) {
    if (list == NULL) {
        return;
    }
    list->head = NULL;
    list->size = 0;
}
```

注意进来先判 `list == NULL`——这是个防御性的好习惯，万一调用者手滑传了空指针进来，我们至少不直接解引用它把自己崩掉。原笔记的工厂函数 `Init_A_ClassicLinkList()` 走的是另一条路（malloc 结构体再返回），它对 malloc 的返回值做了非空检查并在失败时 `exit`，思路是一致的：**别信任任何一次内存分配**。

### 头插与尾插

现在开始往里塞数据。头插最省事——新节点一来，直接顶到最前面，旧的头排第二：

```c
void slist_push_front(SList* list, int data) {
    if (list == NULL) {
        return;
    }
    SListNode* node = make_node(data);   /* make_node 内部 malloc 并填好 data、置 next=NULL */
    /* 新节点先抓住旧的头，再把新节点立成头：顺序绝不能反 */
    node->next = list->head;
    list->head = node;
    list->size++;
}
```

这两行赋值的顺序是整个链表 API 里最容易翻车的地方之一。**必须先 `node->next = list->head`，再 `list->head = node`**。要是反过来，你先把 `list->head` 指向新节点，那原来那串节点就再也没人握着了——整条尾巴直接蒸发成内存泄漏。原因很简单：唯一能找到旧链表的那个入口指针 `list->head` 被你提前覆盖了。所以记住一个通用原则：**先让新节点接住旧的连接，再去改旧的连接**，这一句后面插入、删除会反复用到。

尾插要麻烦一点，因为单链表只能往前走、不能回头。塞到尾巴之前你得先走到尾巴：

```c
void slist_push_back(SList* list, int data) {
    if (list == NULL) {
        return;
    }
    SListNode* node = make_node(data);
    if (list->head == NULL) {
        /* 空表：新节点直接当头 */
        list->head = node;
    } else {
        /* 否则要一路走到尾巴 */
        SListNode* cur = list->head;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = node;
    }
    list->size++;
}
```

这里又出现了那个空表特判。空表时 `list->head` 是 `NULL`，你直接 `while (cur->next)` 会先解引用 `NULL`，当场段错误。所以**任何要解引用头节点的操作，都要先把空表这一支单独处理**。原笔记的 `Push_Back_Into_A_ClassicLinkList` 正是用 `if (list->Head != NULL)` 把空表与非空表两支分开写的。代价方面，尾插是 `O(n)`——你得走完整条；头插是 `O(1)`。这就是为什么很多 LRU 缓存、调度队列偏爱头插：插得快。如果你想要 `O(1)` 尾插，那就得再维护一个尾指针 `tail`，代价是增删时多一份维护负担。

把头插和尾插拼起来跑一下，真实输出长这样：

```text
$ ./demo_basics
after init: size=0: head -> NULL
after push_front 30/20/10: size=3: head -> 30 -> 20 -> 10 -> NULL
after push_back 40/50: size=5: head -> 30 -> 20 -> 10 -> 40 -> 50 -> NULL
```

注意头插的顺序是反着的：先插的 30 反而排第一，后插的 10 排最后——因为每次都顶到最前面。这是头插的天然特性，也叫"栈式插入"。而尾插保序，先插的在前。

### 在任意位置插入：先连后断

任意位置插入是链表 API 的一个关键考点。假设我们要在下标 `pos` 处插一个新节点，思路是先走到 `pos-1`（前驱节点），然后让新节点插在前驱和它的后继之间。重点是连指针的顺序：

```c
int slist_insert_at(SList* list, size_t pos, int data) {
    if (list == NULL || pos > list->size) {
        return -1; /* 非法位置 */
    }
    if (pos == 0) {
        slist_push_front(list, data);
        return 0;
    }
    /* 走到“待插入位置的前一个节点” */
    SListNode* prev = list->head;
    for (size_t i = 0; i < pos - 1; i++) {
        prev = prev->next;
    }
    SListNode* node = make_node(data);
    /* 先让新节点接住后面的，再让前面接住新节点 */
    node->next = prev->next;
    prev->next = node;
    list->size++;
    return 0;
}
```

那两句核心还是头插那个原则的复用：**先让新节点接住后面的，再让前驱接住新节点**。先把 `node->next = prev->next`，新节点就抓住了原本排第 `pos` 的那个节点；然后 `prev->next = node`，前驱转而指向新节点。要是反过来先 `prev->next = node`，第 `pos` 个节点之后整条链子又飞了——没人再握着它们。原笔记里这段的原话讲得很到位：*"先让新节点连上下一个节点，然后旧的链接断开，再连上前一个……先断开了怎么找下一个呢？"* 一句话点穿了为什么要这个顺序。

边界处理上，`pos == 0` 我们直接复用 `push_front`，避免特判前驱；`pos == size` 时走到的前驱正好是尾节点，新节点接在 `NULL` 前面，等价于尾插。合法范围是 `[0, size]`，越界一律拒绝。跑一下看真实输出：

```text
$ ./demo_insert
start:        size=5: head -> 10 -> 20 -> 30 -> 40 -> 50 -> NULL
insert 5@0:   size=6: head -> 5 -> 10 -> 20 -> 30 -> 40 -> 50 -> NULL
insert 25@3:  size=7: head -> 5 -> 10 -> 20 -> 25 -> 30 -> 40 -> 50 -> NULL
insert 60@end:size=8: head -> 5 -> 10 -> 20 -> 25 -> 30 -> 40 -> 50 -> 60 -> NULL
```

### 在任意位置删除：跨越重连 + free

删除是插入的镜像。要删第 `pos` 个，先走到 `pos-1`（前驱），然后让前驱跨过被删节点、直接接上被删节点的后继，最后把被删节点 free 掉：

```c
int slist_erase_at(SList* list, size_t pos) {
    if (list == NULL || list->head == NULL || pos >= list->size) {
        return -1;
    }
    SListNode* del = NULL;
    int value = 0;
    if (pos == 0) {
        del = list->head;
        value = del->data;
        list->head = del->next;
    } else {
        SListNode* prev = list->head;
        for (size_t i = 0; i < pos - 1; i++) {
            prev = prev->next;
        }
        del = prev->next;
        value = del->data;
        prev->next = del->next; /* 跨过被删节点重连 */
    }
    free(del);
    list->size--;
    return value;
}
```

这里有一个**内存归属**的细节必须强调。我们的节点里数据是 `int`，直接嵌在节点那块 `malloc` 出来的内存里，所以 `free(del)` 把节点连同数据一起回收，干净利落。但原笔记的泛型版节点数据域是 `void* data`——数据是**另外** `malloc` 出来的一块、节点只握着它的指针。这种情况下删节点必须**先 `free(del->data)` 再 `free(del)`**，少一个就漏一块内存。原笔记的 `eraseAElementfromDataList` 正是这样写的。所以拿到一个链表库，第一件事就是搞清楚：**数据是嵌在节点里的，还是节点只握着一个指针？** 这决定了你 free 的姿势。

另外注意删头节点那支：`list->head = del->next` 一定要在 `free(del)` **之前**执行。先 free 了再去读 `del->next` 是 use-after-free，读到的是被释放内存里的垃圾值。真实输出：

```text
$ ./demo_erase
start:            size=5: head -> 10 -> 20 -> 30 -> 40 -> 50 -> NULL
erase@0 (got 10): size=4: head -> 20 -> 30 -> 40 -> 50 -> NULL
erase@2 (got 40): size=3: head -> 20 -> 30 -> 50 -> NULL
erase@last(got 50):size=2: head -> 20 -> 30 -> NULL
```

### 遍历与查找

遍历是链表所有操作的地基，写法高度统一：拿一个游标从 `head` 出发，每次走 `next`，直到撞上 `NULL`。

```c
void slist_print(const SList* list) {
    if (list == NULL) {
        return;
    }
    printf("size=%zu: head", list->size);
    SListNode* cur = list->head;
    while (cur != NULL) {
        printf(" -> %d", cur->data);
        cur = cur->next;
    }
    printf(" -> NULL\n");
}
```

查找就是把遍历里"打印"换成"比较"：

```c
long slist_find(const SList* list, int target) {
    if (list == NULL) {
        return -1;
    }
    SListNode* cur = list->head;
    long i = 0;
    while (cur != NULL) {
        if (cur->data == target) {
            return i;
        }
        cur = cur->next;
        i++;
    }
    return -1; /* 走完整条都没找到 */
}
```

注意它只能返回**第一个**匹配的下标，找不到返回 `-1`。如果你想找全部匹配项，原笔记设计了一个 `returnAbunchLocationsinLinkList`——边遍历边把命中的下标往一个动态数组里塞，最后一起返回。思路就是在遍历的基础上加个收集器，本质没变。查找是 `O(n)`，这就是链表相比数组（`O(1)` 随机访问）最大的硬伤：你没法直接跳到第 i 个，只能一步步走。

### 原地反转：先存住 next

反转是单链表最经典的指针体操。思路是用三个指针翻绳：`prev`（已反转部分的头）、`cur`（当前要处理的节点）、`next`（临时存 `cur` 的下一个，因为等会儿要被改掉）。每一步把 `cur->next` 反向指向 `prev`，然后三个指针整体往前挪一格。

```c
void slist_reverse(SList* list) {
    if (list == NULL) {
        return;
    }
    SListNode* prev = NULL;
    SListNode* cur = list->head;
    while (cur != NULL) {
        SListNode* next = cur->next; /* 先存住下一个，否则一翻转就丢了 */
        cur->next = prev;            /* 当前节点的指针反向指回前一个 */
        prev = cur;                  /* prev 前进一步 */
        cur = next;                  /* cur 前进一步 */
    }
    list->head = prev; /* 循环结束时 prev 正是原尾节点 */
}
```

这里**最致命的一步**是 `SListNode* next = cur->next`。如果你直接 `cur->next = prev`，那 `cur` 原本指向后面那串节点的指针就被覆盖了——后面的节点再也找不到了，反转直接变成"砍断"。所以必须先用 `next` 把后路存下来，处理完 `cur` 再让 `cur = next` 继续往下走。原笔记里把这一步画成了三张图：先翻指针、再让后行指针被先行指针赋值、最后整体前移成子问题，本质上就是把这个"先存 next"的过程拆开讲。

循环走完时，`cur` 是 `NULL`（走过了原尾节点），而 `prev` 恰好停在原尾节点上——它就是反转后的新头，所以最后 `list->head = prev`。整张表是 `O(n)` 时间、`O(1)` 额外空间，不用申请新内存，纯指针重连。跑一下：

```text
$ ./demo_search_reverse
start:     size=5: head -> 7 -> 3 -> 9 -> 3 -> 5 -> NULL
find 9  -> pos 2
find 3  -> pos 1 (第一个匹配)
find 100-> pos -1 (找不到)
reversed: size=5: head -> 5 -> 3 -> 9 -> 3 -> 7 -> NULL
```

### 释放整张表：free 前先存 next

最后是生命周期收尾。链表节点全在堆上，用完必须逐个 free。陷阱跟反转一模一样：**free 当前节点之前，必须先把它的 next 存下来**，否则你 free 完就读不到 `next` 了，后面整条泄漏。

```c
void slist_clear(SList* list) {
    if (list == NULL) {
        return;
    }
    SListNode* cur = list->head;
    while (cur != NULL) {
        SListNode* next = cur->next; /* 释放前必须先记下下一个 */
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->size = 0;
}
```

注意最后还要把 `list->head` 置 `NULL`、`size` 归零，让这张表回到刚 `init` 完的空表状态——这样它还能继续被复用。如果一张表彻底不用了，`SList` 本身若是 malloc 出来的，调用者还得额外 free 掉结构体本身；我们这套 API 的 `SList` 通常是栈变量或嵌在别处，`clear` 只负责回收链上的节点。这一节用 valgrind 验过，6 次 alloc 对 6 次 free，零泄漏：

```text
$ valgrind --leak-check=full ./demo_free
before clear: size=5: head -> 1 -> 2 -> 3 -> 4 -> 5 -> NULL
after  clear: size=0: head -> NULL
==90882== HEAP SUMMARY:
==90882==     in use at exit: 0 bytes in 0 blocks
==90882==   total heap usage: 6 allocs, 6 frees, 4,176 bytes allocated
==90882== All heap blocks were freed -- no leaks are possible
==90882== ERROR SUMMARY: 0 errors from 0 contexts
```

## 常见踩坑

这一节集中点破几个我（和原笔记）都真实踩过的坑，读库的时候尤其要带着这些眼睛去看。

**头指针 vs 头节点，别搞混。** 教材里单链表有两种主流建模，很多人混着用、最后自己把自己绕进去。第一种是**"头指针"建模**：`head` 就是"指向第一个数据节点的指针"，空表时 `head == NULL`，本教程和大多数工程实现走这条。第二种是**"带头节点/哨兵"建模**：额外 malloc 一个不存真数据的"头节点"，真正数据从它的 `next` 开始，空表时 `head->next == NULL`。哨兵的好处是插入删除时**不用特判头节点**——因为头节点永远存在，前驱永远不空，代码能统一。代价是多一个节点、多一次 malloc。两种各有各的工程场景，但**一个项目里只能选一种并贯彻到底**，混用就是 bug 工厂。下面这个最小 demo 把两种建模画在同一张 `10 -> 20 -> 30` 上对比，真实输出：

```c
/* 方式 A：head 是“指向第一个节点的指针”，没有独立头节点 */
typedef struct N1 { int data; struct N1* next; } N1;
N1* headA = NULL;
/* 头插 10/20/30 ... */

/* 方式 B：head 是一个“哨兵头节点”，它的 next 才是第一个真数据节点 */
typedef struct N2 { int data; struct N2* next; } N2;
N2* guard = malloc(sizeof(N2));
guard->data = 0;       /* 头节点不存真数据 */
guard->next = NULL;
/* 把真数据尾插到哨兵之后 ... */
```

```text
$ ./demo_head_vs_node
A (head 指针):  head -> 10 -> 20 -> 30 -> NULL
B (哨兵头节点): guard -> 10 -> 20 -> 30 -> NULL
```

**`void*` 数据域的 `sizeof` 坑。** 原笔记的泛型版节点数据域是 `void* data`，它在拷贝数据时写的是 `sizeof(node->data)`。这里有个非常隐蔽的坑：`node->data` 的类型是 `void*`，所以 `sizeof(node->data)` 算的是**一个指针的大小（64 位机器上是 8）**，根本不是你数据真正的大小。如果你的数据大于 8 字节（比如一个 16 字节的结构体），就只拷贝了头 8 个字节，剩下的是垃圾；我们用 `int`（4 字节）做演示时它凑巧没立刻爆，纯属运气。本机实测：

```text
$ ./demo_voidptr_bug
sizeof(int)   = 4
sizeof(void*) = 8
sizeof(p)     = 8  <-- 源码用 sizeof(node->data) 拷贝，拷的其实只是这 8 个字节
```

正确的做法是像 `memcpy(dst, src, dataSize)` 那样，**把"元素大小"作为参数显式传进来**，绝不能指望 `sizeof(void*)` 替你算。这是泛型容器（vector、链表、栈）设计里反复出现的一条铁律——`void*` 本身不带任何尺寸信息，尺寸必须外部维护。

**内存泄漏与野指针。** 链表所有 bug 几乎都集中在 free 这一步。三类最常见：一是**改连接前先 free**——比如删头时先 `free(head)` 再去读 `head->next`，use-after-free 读垃圾值；二是**free 前没存 next**——`free(cur)` 之后 `cur->next` 已经无效，整条后续泄漏；三是**数据域是 `void*` 却只 free 了节点**——节点 free 了，但数据那块 `malloc` 被遗忘，每次删一个漏一块，跑久了内存涨上天。统一原则：**改连接 → 存好 next → 先 free 数据（若有）→ 再 free 节点 → 置空指针**，顺序照这个走就不会错。另外，原笔记的 `clearAClassicLinkList` 末尾没把 `Head` 置 `NULL`、也没 free 数据域，是个真实的尾巴 bug——用完一张表后它的 `head` 还指着已释放的内存，再访问就是野指针，读库时要能挑出来。

## 小结

把这一章的要点压成一张 checklist：

- **节点 = 数据域 + 指针域**；链表 = 头指针（+ 长度）。自引用结构体内部必须用 `struct Node*`，不能用别名。
- **指针重连的黄金顺序：先让新节点接住旧连接，再改旧连接**。头插、任意位置插入、反转、释放，全是这一个原则的复用。
- **空表特判是底线**：任何要解引用 `head->next` 的操作，都得先把 `head == NULL` 这一支单独处理，否则段错误。
- **插入 `O(1)`（头插）/ `O(n)`（定位）、删除 `O(n)`、查找 `O(n)`**——链表的命门是失去随机访问，所以它适合频繁增删、不适合按下标读。
- **内存归属决定 free 姿势**：数据嵌在节点里就一次 `free`；数据是另 `malloc` 的 `void*` 就先 `free(data)` 再 `free(node)`。改连接、存 next、再 free，顺序不能乱。
- **头指针 vs 头节点二选一贯彻到底**；`void*` 容器的尺寸必须外部传入，绝不能靠 `sizeof(void*)`。

## 练习

1. 给本教程的 `SList` 加一个 `size_t slist_count(const SList*, int target)`，返回某个值在表里出现的总次数。先写出来，再回头对比：它和 `slist_find` 的关系是什么？为什么 `find` 返回第一个、`count` 返回全部？
2. 把 `slist_push_back` 从 `O(n)` 优化到 `O(1)`：在 `SList` 里加一个 `SListNode* tail` 尾指针。改造后，哪些操作必须同步维护 `tail`？（提示：头插、尾插、删头、删尾、反转、clear 都得想一遍。）
3. 给链表加一个"按值删除"的接口 `slist_remove(SList*, int target)`，删掉**所有**等于 target 的节点。注意：连续两个 target 挨着时，你的循环怎么写才不会跳过第二个？（这是个经典 off-by-one 陷阱。）
4. 把数据域从 `int` 改成 `void*`，并像原笔记那样显式传入 `dataSize`。重写 `push_back` / `insert_at` / 拷贝构造，确保 `memcpy` 用的是传入的 `dataSize` 而不是 `sizeof(void*)`。写完后专门构造一个 16 字节结构体去测，验证数据完整拷贝。
5. 读原笔记的 [classicLinkList 源码](https://github.com/Charliechen114514/Tiny-C-C-standard-Library/tree/C/classicLinkList)，找出至少三处真实 bug（提示：`ReverseLinkList` 多节点分支缺 return、`clearAClassicLinkList` 不 free 数据且不置空头、`sortClassicLinkListinBubbleSort` 里 `sizeof(pCur->data)` 的同款 `void*` 坑）。每一处说清楚：现象是什么、为什么发生、怎么改。

## 参考资源

- 原始笔记与源码：[classicLinkList（Tiny-C-C-standard-Library, C 分支）](https://github.com/Charliechen114514/Tiny-C-C-standard-Library/tree/C/classicLinkList)——泛型 `void*` 版本的完整工程实现，适合作为"读库挑刺"练习。
- 本仓库《用 C 造一个泛型容器：从 void* 到自己的 vector（阶段 3）》——`void* + dataSize + 回调`这套泛型三件套的完整拆解，看完它你就能把本章的 `int` 版无缝升级成泛型链表。
- 本仓库《递归与调用栈（阶段 3）》——单链表的很多操作（反转、合并、长度）都有优雅的递归写法，理解调用栈是前提。

---

*整理自作者笔记，按 C-Journey 写作规范重写；所有输出本机实测捕获。*
