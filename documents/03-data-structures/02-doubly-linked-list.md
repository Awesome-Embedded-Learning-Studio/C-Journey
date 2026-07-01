---
title: "双向链表:prev+next,O(1) 删除与前驱遍历"
description: "阶段3·第2章。承接单链表,讲双向链表:每个节点除了 next 还多存一个 prev 指向前驱,于是给定一个节点指针能 O(1) 把它摘掉(单链表得 O(n) 从头找前驱)、还能反向遍历。代价是每节点多 8 字节、插入/删除多改两个指针。本章重点工程技巧是「带头哨兵(dummy head)」:一个不存数据的哨兵节点首尾相接,让头插/尾插/删头/删尾全都不用特判 head 是否变、prev/next 是否为 NULL。真跑三件事:带哨兵建表 1 2 3 正反打印得 1 2 3 / 3 2 1;O(1) 删头删尾删中间(删完判空 dummy->next==dummy 为真);5 节点表删尾部节点单链表要走 4 步找前驱、双向固定 2 步重连。ASan 复核整表 free(含 dummy)无泄漏。全 gcc16+clang22 真跑。"
chapter: 3
order: 2
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第1章:单链表(节点、next 指针、free 整表)"
  - "阶段2·第6章:动态内存入门、第7章:坑(ASan)"
related:
  - "阶段3·第12章:算法复杂度(双向链表 O(1) 删除 vs 单链表 O(n))"
---

# 双向链表:prev+next,O(1) 删除与前驱遍历

## 引言:单链表留的一个痛

第 1 章我们用单链表把链式存储讲通了——每个节点只有 `data` 和一个 `next`,顺着 `next` 一路走就能遍历整条表。可单链表有一个绕不过去的痛:**删一个节点,得先知道它的前驱**。因为摘节点要做 `prev->next = del->next`,前驱的 `next` 指针得改向,可单链表每个节点只记得「下一个」、不记得「上一个」。于是当你手里只有一个「要删的节点指针 `del`」时,只能从头顺着 `next` 一路走、走到「某个节点的 `next == del`」为止——那个节点才是前驱,这一趟是 O(n) 的。

更扎心的是,删头节点还要特判:`head = del->next`,头指针本身变了;删中间节点又走另一套逻辑。单链表的删除函数因此总绕不开 if-else 分头处理。这一章我们给每个节点再配一个 `prev` 指针指向前驱,做成**双向链表**(doubly linked list)——`prev` 在手,删除给定节点指针直接 O(1) 摘掉、不用从头找前驱;还能从尾往头反向遍历(单链表做不到)。代价当然有:每个节点多一个指针(64 位机上多 8 字节),插入删除都要多改两个指针而不是一个。但很多场景下这点内存换 O(1) 删除非常划算,这就是为什么 Linux 内核的 `list_head`、C++ STL 的 `std::list` 都用双向(而且都带哨兵)。

这一章我们还要把一个工程常用技巧正式讲透:**带头哨兵(dummy head)**。给链表配一个不存数据的哨兵节点,让它首尾相接(空表时哨兵的 `prev`/`next` 都指向自己),于是头插、尾插、删头、删尾全部走同一套代码、零特判——`head` 会不会变、`prev`/`next` 会不会是 NULL 这些边界全消失。这是把单链表那一堆 if-else 彻底拍平的关键一招。

## 节点:多了一个 prev

先把节点结构体写出来。和单链表比,就是多了个 `prev`:

```c
typedef struct Node {
    int data;
    struct Node* prev;
    struct Node* next;
} Node;
```

这里有个 C 的细节值得停一下:结构体在还没定义完的时候就用自己的名字 `struct Node` 来声明成员(§6.7.2.1),这叫**自引用**。关键是成员类型必须是「指向本结构体的指针」(`struct Node* prev`),不能是「本结构体本身」——如果你写 `struct Node prev;` 编译器会问你「这结构体到底多大」,于是无限嵌套、无法计算大小,直接报错 `field has incomplete type`。指针大小是固定的(阶段2·第1章真跑过,64 位机一律 8 字节),跟它指向的东西多大无关,所以「指向自己的指针」能合法存在,把这个无限嵌套的悖论破掉。单链表的 `next` 也是同一个道理,这里双向链表只是 `prev`/`next` 各来一份。

`prev` 和 `next` 的语义很直白:`next` 指向后继(下一个真节点)、`prev` 指向前驱(上一个真节点)。第一个真节点的 `prev` 和最后一个真节点的 `next` 怎么办?这正是下面哨兵要解决的问题——先卖个关子。

## 带头哨兵:把边界全拍平

现在说本章的重头戏:**dummy head(头哨兵)**。我们给整条链表配一个**不存有效数据**的节点当哨兵,它的 `data` 字段是废物(打印时绕过它),但它的 `prev`/`next` 参与链表的指针骨架。我用一个外层 `List` 结构体把这个哨兵包起来:

```c
typedef struct {
    Node* dummy;
} List;
```

建空表时,哨兵的 `prev` 和 `next` 都指向**它自己**——这叫首尾相接:

```c
List* list_new(void) {
    List* L = malloc(sizeof(List));
    if (L == NULL) {
        return NULL;
    }
    /* 哨兵不存有效数据;data 随便填,真打印时绕过它 */
    L->dummy = node_new(0);
    if (L->dummy == NULL) {
        free(L);
        return NULL;
    }
    L->dummy->prev = L->dummy; /* 空表:哨兵首尾相接 */
    L->dummy->next = L->dummy;
    return L;
}
```

这看起来有点怪——一个节点自己指向自己?但你想清楚它解决什么问题就豁然开朗了:**空表时 `dummy->next == dummy`、`dummy->prev == dummy`**。于是不管表空不空,`dummy->next` 永远是「第一个真节点」(空表时是 dummy 自己)、`dummy->prev` 永远是「最后一个真节点」(空表时也是 dummy 自己)。第一个真节点的 `prev` 永远指向 dummy、最后一个真节点的 `next` 也永远指向 dummy——**没有 NULL 了**。

这一点是哨兵全部威力的来源。在单链表里,头节点的 `prev` 是 NULL、尾节点的 `next` 是 NULL,所以删头要特判(`prev` 不存在)、删尾要特判(`next` 不存在);插第一个节点要特判(`head` 原本是 NULL)。有了哨兵,这些 NULL 全部消失,所有节点的 `prev`/`next` 都指向一个合法节点(要么是真节点、要么是 dummy),于是**头插和尾插是同一套代码、删头和删尾是同一套代码**,边界被彻底拍平。

先看最基础的建表和打印。下面这个程序建一条 `1 2 3`,正向、反向各打一遍,再演示头插:

```c
#include <stdio.h>
#include <stdlib.h>

/* 双向链表节点:数据 + 指向前驱的 prev + 指向后继的 next。
   节点结构体是自引用的(§6.7.2.1):成员 prev/next 的类型是「指向本结构体的指针」,
   不是「本结构体」,所以不会无限嵌套。 */
typedef struct Node {
    int data;
    struct Node* prev;
    struct Node* next;
} Node;

/* 整条链表:用一个 dummy head(哨兵节点)打底。
   dummy->next 是第一个真节点、dummy->prev 是最后一个真节点;
   空表时 dummy 自己首尾相接(prev/next 都指向自己)。 */
typedef struct {
    Node* dummy;
} List;

Node* node_new(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->prev = NULL;
    n->next = NULL;
    return n;
}

List* list_new(void) {
    List* L = malloc(sizeof(List));
    if (L == NULL) {
        return NULL;
    }
    /* 哨兵不存有效数据;data 随便填,真打印时绕过它 */
    L->dummy = node_new(0);
    if (L->dummy == NULL) {
        free(L);
        return NULL;
    }
    L->dummy->prev = L->dummy; /* 空表:哨兵首尾相接 */
    L->dummy->next = L->dummy;
    return L;
}

/* 尾插:新节点接在 dummy->prev(原尾)和 dummy 之间 */
void push_back(List* L, int data) {
    Node* tail = L->dummy->prev; /* 原尾(空表时就是 dummy 自己) */
    Node* n = node_new(data);

    n->prev = tail;
    n->next = L->dummy;
    tail->next = n;
    L->dummy->prev = n;
}

/* 头插:新节点接在 dummy 和 dummy->next(原首)之间 */
void push_front(List* L, int data) {
    Node* head = L->dummy->next; /* 原首(空表时就是 dummy 自己) */
    Node* n = node_new(data);

    n->prev = L->dummy;
    n->next = head;
    head->prev = n;
    L->dummy->next = n;
}

/* 正向打印:从 dummy->next 走到 dummy(不含) */
void print_forward(const List* L) {
    for (Node* p = L->dummy->next; p != L->dummy; p = p->next) {
        printf("%d ", p->data);
    }
    printf("\n");
}

/* 反向打印:从 dummy->prev 往前走到 dummy(不含) */
void print_backward(const List* L) {
    for (Node* p = L->dummy->prev; p != L->dummy; p = p->prev) {
        printf("%d ", p->data);
    }
    printf("\n");
}

int main(void) {
    List* L = list_new();

    push_back(L, 1);
    push_back(L, 2);
    push_back(L, 3);

    printf("正向: ");
    print_forward(L);
    printf("反向: ");
    print_backward(L);

    /* 再演示头插:在头部插 0,得 0 1 2 3 */
    push_front(L, 0);
    printf("头插 0 后正向: ");
    print_forward(L);

    /* free 整表(含 dummy):先存 next 再 free 当前,免得 free 后读到野地址 */
    Node* p = L->dummy->next;
    while (p != L->dummy) {
        Node* next = p->next;
        free(p);
        p = next;
    }
    free(L->dummy);
    free(L);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra basic.c -o basic && ./basic
正向: 1 2 3 
反向: 3 2 1 
头插 0 后正向: 0 1 2 3 
```

仔细看 `push_back` 这四行,它就是哨兵威力的浓缩:

```c
Node* tail = L->dummy->prev; /* 原尾 */
n->prev = tail;
n->next = L->dummy;
tail->next = n;
L->dummy->prev = n;
```

不管是往空表插第一个节点、还是往有数据的表插第十个节点,这段代码**一字不改**都成立。空表时 `dummy->prev` 就是 dummy 自己,于是新节点的 `prev` 指向 dummy、`next` 也指向 dummy,然后 dummy 的 `prev`/`next` 都改指新节点——空表就这么被填进第一个真节点,没有任何 `if (表为空)` 特判。换成单链表的尾插,你得先判断 `head == NULL`,这是两条截然不同的路径。`push_front` 是镜像的同样四行,把 `prev`/`next` 和 `dummy` 的角色对调一下,往表头塞——空表同样成立。

反向打印就更能体现双向链表相对单链表的另一个优势了:`print_backward` 从 `dummy->prev`(尾节点)出发,顺着 `prev` 一路往前走、走到 dummy 停。这在单链表里根本做不到(单链表只有 `next`,只能往一个方向走,想反向得先用三个指针把整条链反转,代价 O(n))。双向链表的 `prev` 让「从后往前」和「从前往后」一样便宜,这也是为什么浏览器的前进/后退、文本编辑器的撤销/重做这类需要双向移动的场景天然适合双向链表。

## O(1) 删除:prev 在手,不用找前驱

现在到双向链表相对单链表最直观的赢面:**给定一个节点指针,O(1) 摘掉它**。两行指针重连:

```c
n->prev->next = n->next; /* 前驱的后继跳过 n */
n->next->prev = n->prev; /* 后继的前驱跳过 n */
free(n);
```

因为 `n->prev` 就是前驱(不用从头找),`n->next` 就是后继,把它们互相接上,n 就被「跨过去」了,再 `free(n)`。这就是引言里说的那个单链表的痛点的解药。前提条件很明确:`n` 必须是一个真实存在于表里的真节点——不能是 dummy(删 dummy 会把整条链的骨架拆了)、更不能是野指针。我加了个防御性的早退:

```c
void delete_node(List* L, Node* n) {
    if (n == NULL || n == L->dummy) {
        return; /* 防御:删 dummy 会破坏整条链的骨架 */
    }
    n->prev->next = n->next;
    n->next->prev = n->prev;
    free(n);
}
```

下面这个程序演示删中间、删头、删尾三种情况,而且因为有了哨兵,这三者走的是**完全相同**的两行重连代码——这就是哨兵把边界拍平的实锤:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* prev;
    struct Node* next;
} Node;

typedef struct {
    Node* dummy;
} List;

Node* node_new(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->prev = NULL;
    n->next = NULL;
    return n;
}

List* list_new(void) {
    List* L = malloc(sizeof(List));
    if (L == NULL) {
        return NULL;
    }
    L->dummy = node_new(0);
    if (L->dummy == NULL) {
        free(L);
        return NULL;
    }
    L->dummy->prev = L->dummy;
    L->dummy->next = L->dummy;
    return L;
}

void push_back(List* L, int data) {
    Node* tail = L->dummy->prev;
    Node* n = node_new(data);
    n->prev = tail;
    n->next = L->dummy;
    tail->next = n;
    L->dummy->prev = n;
}

void print_forward(const List* L) {
    for (Node* p = L->dummy->next; p != L->dummy; p = p->next) {
        printf("%d ", p->data);
    }
    printf("\n");
}

/* O(1) 删除:给定一个真节点指针 n,直接拿它的 prev/next 把它「跨过去」。
   关键:不需要从头遍历找 n 的前驱(单链表的痛),prev 就在手边。
   前提:n 必须是链表里真实存在的真节点(不能传 dummy、不能传野指针)。 */
void delete_node(List* L, Node* n) {
    if (n == NULL || n == L->dummy) {
        return; /* 防御:删 dummy 会破坏整条链的骨架 */
    }
    n->prev->next = n->next; /* 前驱的后继跳过 n */
    n->next->prev = n->prev; /* 后继的前驱跳过 n */
    free(n);
}

/* 按值找第一个匹配的节点,返回指针(找不到返回 NULL)。
   这一步是 O(n) 的——但注意:删除本身是 O(1),「查找」是另一回事。 */
Node* find(const List* L, int target) {
    for (Node* p = L->dummy->next; p != L->dummy; p = p->next) {
        if (p->data == target) {
            return p;
        }
    }
    return NULL;
}

int main(void) {
    List* L = list_new();
    push_back(L, 1);
    push_back(L, 2);
    push_back(L, 3);

    printf("初始: ");
    print_forward(L);

    /* 删中间节点 2:O(1)(find 是 O(n),但 delete_node 本身是 O(1)) */
    Node* mid = find(L, 2);
    delete_node(L, mid);
    printf("删 2 后: ");
    print_forward(L);

    /* 删头(dummy 的下一个,即节点 1) */
    Node* head = L->dummy->next;
    delete_node(L, head);
    printf("删头后: ");
    print_forward(L);

    /* 删尾(dummy 的上一个,即节点 3) */
    Node* tail = L->dummy->prev;
    delete_node(L, tail);
    printf("删尾后: ");
    print_forward(L);

    /* 此时表空:dummy->next == dummy,打印应为空行 */
    printf("表是否空(dummy->next==dummy): %d\n", L->dummy->next == L->dummy);

    /* free 整表(此时只剩 dummy) */
    free(L->dummy);
    free(L);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra delete.c -o del && ./del
初始: 1 2 3 
删 2 后: 1 3 
删头后: 3 
删尾后: 
表是否空(dummy->next==dummy): 1
```

重点看「删头」和「删尾」这两段:删头是 `delete_node(L, L->dummy->next)`、删尾是 `delete_node(L, L->dummy->prev)`,它们和删中间的 `delete_node(L, mid)` 走的是**同一个函数、同一套两行重连**。在单链表里你绝无可能这么干——删头要更新外部 `head` 指针(头本身变了),删尾之前得先 O(n) 走到倒数第二个节点(因为它的 `next` 要改),而倒数第二个节点光靠尾指针是拿不到的。哨兵让删头删尾这件事在这里变成了「拿 dummy 的某个邻居节点指针、交给同一个删除函数」,代码完全统一。

这里有个非常容易被新手忽略的点,值得专门拎出来说:**「按值查找」是 O(n),「给定节点指针删除」是 O(1),这是两码事**。`find` 那个循环从头走到尾,当然是 O(n)。但很多现实场景里你手里已经有节点指针了——比如你正在遍历链表、对每个满足条件的节点就地删除,这时遍历本身 O(n) 是省不掉的,但「删当前节点」这一步是 O(1),不用为了删它再从头扫一遍找前驱。单链表就不行:就算你手里拿着节点指针,删它还是得 O(n) 找前驱。这就是「删除 O(1)」真正的含义——它省的是「找前驱」这一趟,不是「找到要删的节点」那一趟。`find` 是给「按值删」用的辅助,删之前的查找该花多少还是多少。

最后那个判空 `L->dummy->next == L->dummy` 也呼应了哨兵的设计:空表的判据就是「哨兵的 next 指向自己」。真跑得 `1`(真),证明三个节点全删干净后表确实回到了空表状态。

## 真跑对照:单链表删尾要 4 步,双向固定 2 步

口说无凭,我们用一个程序把单链表和带头哨兵的双向链表并排摆出来,删同一个**尾部节点**,直接数单链表为了找前驱走了几步:

```c
#include <stdio.h>
#include <stdlib.h>

/* 这个程序对照单链表 vs 双向链表(带头哨兵)删除给定节点指针的代价。
   单链表:只有 next,删给定节点要 O(n) 从头找前驱。
   双向链表(带头哨兵):prev 在手,O(1) 直接重连,删头删尾都不用特判。 */

/* ===== 单链表(只有 next) ===== */
typedef struct SNode {
    int data;
    struct SNode* next;
} SNode;

/* 删单链表节点 del:必须找到它的前驱 prev 才能做 prev->next = del->next。
   prev 只能从头顺着 next 走去找 —— O(n)。
   返回新头指针(删头时头会变,要特判)。 */
SNode* slist_delete(SNode* head, SNode* del, long* steps_out) {
    long steps = 0;
    if (head == del) {
        head = del->next; /* 删头:特判,更新头指针 */
    } else {
        SNode* prev = head;
        while (prev->next != del) { /* O(n):顺 next 找前驱 */
            prev = prev->next;
            steps++;
        }
        steps++;
        prev->next = del->next;
    }
    free(del);
    *steps_out = steps;
    return head;
}

/* ===== 双向链表(带头哨兵 dummy) ===== */
typedef struct DNode {
    int data;
    struct DNode* prev;
    struct DNode* next;
} DNode;

/* 带哨兵:尾节点的 next 指向 dummy(不是 NULL),dummy->prev 指向尾节点。
   于是删任意真节点 del:del->next 永远不是 NULL、del->prev 永远不是 NULL,
   两行重连搞定,删头删尾删中间完全统一,O(1),零特判。 */
void dlist_delete(DNode* del) {
    del->prev->next = del->next;
    del->next->prev = del->prev;
    free(del);
}

int main(void) {
    enum { N = 5 };
    int vals[N] = {10, 20, 30, 40, 50};

    /* 建单链表 10 20 30 40 50 */
    SNode* shead = NULL;
    SNode* stail = NULL;
    SNode* snodes[N];
    for (int i = 0; i < N; i++) {
        snodes[i] = malloc(sizeof(SNode));
        snodes[i]->data = vals[i];
        snodes[i]->next = NULL;
        if (shead == NULL) {
            shead = snodes[i];
        } else {
            stail->next = snodes[i];
        }
        stail = snodes[i];
    }

    /* 建带头哨兵的双向链表 10 20 30 40 50 */
    DNode* dummy = malloc(sizeof(DNode)); /* 哨兵,不存数据 */
    dummy->prev = dummy;
    dummy->next = dummy;
    DNode* dtail = dummy; /* 当前尾,初始就是哨兵(空表) */
    DNode* dnodes[N];
    for (int i = 0; i < N; i++) {
        dnodes[i] = malloc(sizeof(DNode));
        dnodes[i]->data = vals[i];
        dnodes[i]->prev = dtail;
        dnodes[i]->next = dummy; /* 新节点总在 dummy 之前 */
        dtail->next = dnodes[i];
        dummy->prev = dnodes[i]; /* 哨兵的 prev 总指向尾 */
        dtail = dnodes[i];
    }

    /* 删尾部节点 50(位置 4) */
    long ssteps = 0;
    shead = slist_delete(shead, snodes[N - 1], &ssteps);
    dlist_delete(dnodes[N - 1]);

    printf("删尾部节点(位置 %d,共 %d 个真节点):\n", N - 1, N);
    printf("  单链表:从头顺 next 走了 %ld 步才找到前驱(O(n),n 越大越慢)\n", ssteps);
    printf("  双向链表(带哨兵):prev 在手,2 步重连,O(1),删头删尾删中间一视同仁\n");

    /* free 剩余(单链 4 节点 / 双向 4 真节点 + 1 哨兵),ASan 复核无泄漏 */
    SNode* sp = shead;
    while (sp != NULL) {
        SNode* next = sp->next;
        free(sp);
        sp = next;
    }
    DNode* dp = dummy->next;
    while (dp != dummy) {
        DNode* next = dp->next;
        free(dp);
        dp = next;
    }
    free(dummy);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra compare.c -o cmp && ./cmp
删尾部节点(位置 4,共 5 个真节点):
  单链表:从头顺 next 走了 4 步才找到前驱(O(n),n 越大越慢)
  双向链表(带哨兵):prev 在手,2 步重连,O(1),删头删尾删中间一视同仁
```

5 个节点的表删尾部(位置 4),单链表从 head 开始顺 `next` 一路走到 `prev->next == del`,走了 4 步才摸到前驱;表越长,这个数字越大,这就是 O(n) 的含义——代价和表长成正比。双向链表那两行重连是**固定的**,跟节点在表里什么位置、表有多长都无关,所以是 O(1)。如果表有一万个节点,单链表删尾要平均走五千步、双向还是两步,差距就非常实在了。

这个程序还顺手印证了哨兵的另一个好处。注意 `dlist_delete` 里我**没有**检查 `del->next` 是不是 NULL——因为带头哨兵之后,尾节点的 `next` 指向 dummy、不是 NULL,所以 `del->next->prev = del->prev` 永远安全。如果用不带哨兵的裸双向链表(尾节点 `next` 是 NULL),删尾时这一行就解引用 NULL、当场段错误。我写这章时第一版对比程序就用了裸双向链表、删尾直接 SEGV 在 `del->next->prev` 上——这正是哨兵把 NULL 边界吃掉的价值,也是为什么工程实现几乎都带哨兵。

## 释放:从头到尾 free,ASan 把关

链表的节点都是从堆 `malloc` 来的(§7.22.3),用完必须逐个 `free`,否则就是内存泄漏(阶段2·第7章用 ASan 抓过)。释放的套路和单链表一样:从头走到尾,**先存住 `next`、再 `free` 当前**,因为 `free(p)` 之后 `p` 那块内存已经还给堆了,再读 `p->next` 是 use-after-free:

```c
Node* p = L->dummy->next;
while (p != L->dummy) {
    Node* next = p->next; /* 先存 next,free 后 p->next 就是 UAF */
    free(p);
    p = next;
}
free(L->dummy); /* 别忘了哨兵自己也是 malloc 来的 */
free(L);        /* 外层 List 容器也是 */
```

终止条件 `p != L->dummy` 又是哨兵的功劳——走到 dummy 说明绕了一圈回到起点,真节点全 free 完了。最后还要单独 `free(L->dummy)` 和 `free(L)`,因为哨兵节点和外层 `List` 容器都是单独 `malloc` 的,各有各的内存块,缺一个就是泄漏。我们用 ASan 把关(`-fsanitize=address`),它内置的 LeakSanitizer 会在程序退出时扫描堆、任何没 free 的块都会被报出来:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined basic.c -o basic_asan && ./basic_asan
正向: 1 2 3 
反向: 3 2 1 
头插 0 后正向: 0 1 2 3 
$ echo $?
0
```

退出码 `0`、没有 `ERROR: AddressSanitizer: detected memory leaks` —— 整表(4 个真节点 + 哨兵 + 容器)全 free 干净,零泄漏。`delete.c` 那个程序(删到只剩哨兵再 free 哨兵和容器)同样 ASan 跑一遍也是退出码 0。养成习惯:凡是写了 `malloc` 的程序,提交前都用 ASan 跑一遍,泄漏和 use-after-free 它都能当场抓。

## 小结

双向链表给每个节点配了一个 `prev` 指向前驱(自引用结构体,§6.7.2.1,成员是指向自身的指针而非自身、否则无限嵌套),和 `next` 一起把链表变成可双向走的结构——反向遍历(`print_backward` 从 `dummy->prev` 顺 `prev` 走)成了单链表做不到的事,给定节点指针删除也成了 O(1):`n->prev->next = n->next; n->next->prev = n->prev;` 两行重连就把节点「跨过去」、再 `free`,不用像单链表那样 O(n) 从头找前驱。代价是每节点多 8 字节(64 位机上一个指针)、插入删除多改两个指针而不是一个,换 O(1) 删除和双向遍历,在浏览器前进后退、内核链表、STL `std::list` 这些场景里非常划算。本章的工程重头戏是**带头哨兵 dummy head**:一个不存数据的哨兵节点首尾相接(空表时 `dummy->prev == dummy->next == dummy`),于是第一个真节点的 `prev` 永远是 dummy、最后一个真节点的 `next` 永远是 dummy——**没有 NULL 了**。这让头插、尾插、删头、删尾全部走同一套代码、零特判(`push_back` 那四行对空表和有数据的表一字不改都成立,删头删尾和删中间走同一个 `delete_node`),边界被彻底拍平;真跑对照 5 节点表删尾,单链表走 4 步找前驱、双向固定 2 步重连,差距随表长线性拉开。要分清两件事:**「按值查找」是 O(n)**(`find` 循环从头走),**「给定节点指针删除」才是 O(1)**——双向链表省的是「找前驱」那一趟,不是「找到要删的节点」那一趟。释放仍是逐节点 `free`(先存 `next` 防 UAF),最后单独 `free` 哨兵和 `List` 容器,ASan 复核退出码 0、零泄漏。哨兵不是双向链表的专利(单链表也能加哨兵),但双向+哨兵是工程上的黄金组合,下一章栈、第 4 章队列会继续用到链式节点,这套思路会反复出现。

## 参考资源

- ISO/IEC 9899:2011 §6.7.2.1(结构体与联合,自引用成员必须是指针、不能是结构体本身)、§7.22.3(内存管理函数 `malloc`/`free`)、§6.5.2.3(结构体成员访问 `.`/`->`)
- K. N. King《C Programming: A Modern Approach》第 17 章·Linked Lists(链表节点、`malloc` 节点、遍历与释放,King 主要讲单链表,本章在其基础上加 `prev` 与哨兵)
- Andrew Koenig《C Traps and Pitfalls》第 3 章(链表操作中的指针重连顺序、free 后访问的悬垂指针坑)
- Linux 内核 `include/linux/list.h`(`struct list_head` 双向链表 + 哨兵的工业级实现,`list_for_each` / `list_del` 就是本章 `delete_node` 的内核版)
- 阶段3·第 1 章:单链表(节点、`next`、`free` 整表)、阶段2·第 6 章:动态内存入门(`malloc`/`free`)、第 7 章:动态内存的坑(ASan 抓泄漏与 UAF)、阶段3·第 12 章:算法复杂度(O(1) vs O(n) 的形式化)
