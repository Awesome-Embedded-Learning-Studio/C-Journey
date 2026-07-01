---
title: "单链表:节点、指针、把内存串成一条链"
description: "阶段3 开篇。这一章用阶段2 攒下的「指针 + malloc」搭第一个真正的数据结构——单链表。先讲动机:数组连续、长度编译期定死、头插 O(n);链表离散、运行期动态增减、头插 O(1),靠每个节点里的 next 指针把散在堆上的节点串成一条链。然后逐个真跑每个操作:节点定义(自引用 struct Node* 而非 Node*——typedef 别名此刻没生效,这是个经典坑,就地讲)、new_node(malloc + 设 data + next=NULL,忘置 NULL 是野指针)、头插 push_front(新节点 next 指旧头、返回新头——返回是因为头可能变)、尾插 push_back(顺 next 走到 NULL 接上,空表特殊情况)、遍历 print_list、按值删除 remove(记前驱 prev,删头节点 prev==NULL 特殊分支)、释放整表 free_list(关键坑:先存 next 再 free 当前,否则 free 完读 next 就是 use-after-free——ASan 真跑 heap-use-after-free + gcc -Wuse-after-free 双重抓;正确写法 ASan 复核退出码 0 无泄漏)。所有代码块即真跑版,gcc16+clang22 双跑 -std=c11 -Wall,free 用 -fsanitize=address 复核无泄漏。"
chapter: 3
order: 1
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第1章:指针是什么(指针装地址、next 指针装下一个节点的地址)"
  - "阶段2·第2章:指针算术(p = p->next 顺 next 走链)"
  - "阶段2·第6章:动态内存入门(malloc 一个节点、必查 NULL)"
  - "阶段2·第7章:动态内存的坑(free 整表的 use-after-free 坑、ASan)"
related:
  - "阶段3·第2章:双向链表(prev/next、O(1) 删除已知节点,对比单链表 O(n))"
  - "阶段3·第12章:算法复杂度与大 O(链表头插 O(1) vs 数组头插 O(n) 的真跑对照)"
---

# 单链表:节点、指针、把内存串成一条链

## 引言:为什么需要链表

阶段1 我们在数组上花了一整章(阶段1·Ch10),记住了它的两条铁律:**连续存放、长度编译期写死**。这两条在「事先就知道要存几个、之后也不增减」的场景下完全够用,但一旦遇到「存多少、运行时才知道,可能加、可能删」的需求,数组就开始难受——你要么一开始就开一个很大的数组赌它够用(浪费内存)、要么开小了动不动越界(崩)。更扎心的是在数组**头部**插一个元素:为了让新元素占到下标 0,后面所有元素都得整体往后挪一格,数据越多挪得越久(这就是 O(n) 的头插,阶段3·Ch12 会正式给它一个名字)。

链表就是为了解决「动态增减 + 头部快速增删」这两个痛点而生的,它的思路很直接:**别再要求内存连续了**。链表把数据存进一个一个独立申请的小块内存(叫**节点 node**),每个节点除了存数据,还额外揣一个指针 `next`,指向「下一个节点在哪」。于是节点虽然散落在堆的各处,逻辑上却被这些 `next` 指针串成了一条链——顺着第一个节点(head)的 `next` 走,能挨个摸到第二个、第三个……直到某个节点的 `next` 是 NULL,这条链就到头了。

链表正是阶段2 攒下的「指针 + malloc」的第一个综合应用:节点用 `malloc` 从堆上申请(阶段2·Ch6)、节点和节点之间靠指针连起来(阶段2·Ch1 的「指针装地址」)、整表用完得逐个 `free` 干净(阶段2·Ch7 的 ASan 在盯着)。这一章我们手写一遍单链表的节点定义、增删改查、整表释放,把前面 12 章的指针功底落到一个真正能跑的数据结构上。

## 节点定义:自引用的那个坑

链表的基本单位是节点,一个节点装两样东西:一份**数据**(这里先用 `int`)、一个指向「下一个节点」的**指针**。直觉上我们会这么写:

```c
typedef struct Node {
    int data;
    struct Node* next; /* 自引用:此刻 typedef 别名 Node 还没生效,必须写 struct Node* */
} Node;
```

这里有个新手必踩的坑,就在 `next` 那一行的写法上。你可能会问:既然下面已经 `typedef struct Node {...} Node;` 给这个结构起了别名 `Node`,那 `next` 干脆写成 `Node* next` 不是更简洁吗?真跑给你看:

```c
typedef struct Node {
    int data;
    Node* next; /* 写成别名 Node*:此刻 Node 还不存在 */
} Node;
```

```text
$ gcc -std=c11 -Wall selfref_bad.c -c -o /dev/null
selfref_bad.c:3:5: error: unknown type name ‘Node’
    3 |     Node* next; /* 写成别名 Node*:此刻 Node 还不存在 */
      |     ^~~~
```

gcc 直接报 `unknown type name ‘Node’`,clang 更贴心,提示你加 `struct` 关键字:

```text
$ clang -std=c11 -Wall selfref_bad.c -c -o /dev/null
selfref_bad.c:3:5: error: must use 'struct' tag to refer to type 'Node'
    3 |     Node* next; /* 写成别名 Node*:此刻 Node 还不存在 */
      |     ^
      |     struct
1 error generated.
```

原因在于 **C 的名字是「声明到哪、生效到哪」**:`typedef struct Node { ... } Node;` 这一行得**从左到右读完**才生效,而 `next` 这个成员声明出现在结构体**内部**、出现在右边的 `} Node` **之前**——此刻编译器还不知道 `Node` 这个别名,它只认得 `struct Node` 这个带 `struct` 标签的全名。所以自引用指针必须老老实实写全名 `struct Node* next`(§6.7.2.1 讲结构声明与成员)。好在「指针大小是固定的、和指向的类型多大无关」(阶段2·Ch1 真跑过 64 位机上任何指针都是 8 字节),所以编译器就算还不知道 `struct Node` 长什么样、也已经知道 `struct Node*` 是 8 字节,这种「不完全类型指针」是完全合法的——结构体里塞一个指向自己的指针,没有循环定义的悖论。

## 创建节点:malloc + 置 next = NULL

节点类型有了,我们来写一个工厂函数,专门负责申请并初始化一个新节点:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

/* 创建一个节点:data 填好,next 置 NULL(忘置 NULL 是野指针坑) */
Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) { /* malloc 可能失败,必查(阶段2·Ch6) */
        return NULL;
    }
    n->data = data;
    n->next = NULL; /* 别忘了这一行,否则 next 是垃圾地址 */
    return n;
}
```

这里两件事不能省。第一是 `malloc` 之后**立刻查 NULL**(§7.22.3——堆不够用时 `malloc` 返回空指针,不查就解引用等于阶段2·Ch1 那个 NULL 段错误)。第二是 `n->next = NULL;` 这一行千万别手滑漏掉:`malloc` 给你的内存是**没初始化的**(阶段2·Ch6 真跑过 calloc 才清零、malloc 不清零),`n->next` 里是上一任主人留下的垃圾值,如果忘了置 NULL,这个新节点就会带一个随机 `next` 指针——后续遍历时顺着它乱跳,又是一个野指针坑(阶段2·Ch7 那批 ASan 抓的内存错误随时会重现)。一个 `= NULL` 能省掉后面无穷无尽的玄学 bug,值。

## 头插与遍历:O(1) 的代价是返回新头

链表相对数组最爽的操作就是**头插(push_front)**:在表的最前面塞一个新节点。数组做这事要把后面全部往后挪(O(n)),链表只需要把新节点的 `next` 指向当前的头、然后把「头」这个身份交给新节点——两步固定动作,和表有多长无关(这就是 O(1),阶段3·Ch12 会给名字)。

```c
/* 头插:新节点接在表头前面,返回新的头(头变了,所以返回新头) */
Node* push_front(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head; /* 分配失败,原表不动 */
    }
    n->next = head; /* 新节点的 next 指向旧头 */
    return n;       /* 新节点成为新头 */
}
```

注意这个函数的签名——它**返回一个 `Node*`**,而不是 `void`。这是因为头插可能让「头」这个位置换人:新节点插到最前面之后,它才是新的 head。如果我们写成 `void push_front(Node* head, ...)`,函数内部能改 `head->next`,但**改不了调用者手里的那个 `head` 变量本身**(阶段2·Ch3 讲过:值传递改不了调用者的变量)。所以这里走阶段2·Ch3 的标准套路——「要改调用者的变量,就返回新值让调用者自己接」:调用者拿到返回值重新赋给 `head`,头就更新了。后面你会看到,所有「可能改变头」的操作(头插、删头节点)都是这个返回新头的写法。

遍历就更直接了:从 head 出发,顺着每个节点的 `next` 一直走到 NULL,沿途处理每个节点。打印整表就是这么走的:

```c
/* 遍历:顺着 next 走到 NULL,沿途打印 */
void print_list(const Node* head) {
    const Node* cur = head;
    while (cur != NULL) {
        printf("%d ", cur->data);
        cur = cur->next; /* 往后挪一格 */
    }
    printf("\n");
}
```

`cur = cur->next;` 这一句是链表遍历的灵魂:`cur` 当前指向某个节点,`cur->next` 是「下一个节点的地址」,把它赋给 `cur`,`cur` 就跳到了下一个节点上;如此反复,直到 `cur` 变成 NULL,说明走到链尾了。这里参数写成 `const Node*`(阶段2·Ch4)是为了表明「遍历只读不改」,顺便也防止函数内部手滑改了节点数据。

把头插和遍历拼起来真跑一下,顺便再加上尾插(`push_back`,逻辑放在下一节讲)建一条有序的表,再用头插打乱它:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->next = NULL;
    return n;
}

/* 头插:新节点接在表头前面,返回新的头(头变了,所以返回新头) */
Node* push_front(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head;
    }
    n->next = head;
    return n;
}

/* 尾插:走到表尾再接,返回头(空表时头会变,所以也返回) */
Node* push_back(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head;
    }
    if (head == NULL) { /* 空表:新节点就是头 */
        return n;
    }
    Node* cur = head;
    while (cur->next != NULL) { /* 顺 next 走到尾(注意判 cur->next 不是 cur) */
        cur = cur->next;
    }
    cur->next = n; /* 接上去 */
    return head;
}

void print_list(const Node* head) {
    const Node* cur = head;
    while (cur != NULL) {
        printf("%d ", cur->data);
        cur = cur->next;
    }
    printf("\n");
}

int main(void) {
    /* 先用尾插建一条 1 -> 2 -> 3 */
    Node* head = NULL;
    head = push_back(head, 1);
    head = push_back(head, 2);
    head = push_back(head, 3);
    print_list(head); /* 1 2 3 */

    /* 再头插 0,得到 0 -> 1 -> 2 -> 3(头变了) */
    head = push_front(head, 0);
    print_list(head); /* 0 1 2 3 */

    return 0;
}
```

```text
$ gcc -std=c11 -Wall basic.c -o b && ./b
1 2 3
0 1 2 3
```

`push_back` 之所以也返回头,是因为「往空表里插第一个节点」会让头从 NULL 变成那个新节点——和头插同理,凡是可能动头的操作都得返回新头让调用者接。尾插的逻辑是「顺 `next` 走到最后一个节点,把新节点接在它后面」,留意循环判的是 `cur->next != NULL` 而不是 `cur != NULL`:前者会在「最后一个节点」处停下(因为它的 next 是 NULL),正好让我们能把新节点挂上去;后者会一路走到 NULL,那时候你已经没有「前一个节点」可以接了。对比一下:`push_front` 是 O(1)(直接接在头前面),`push_back` 是 O(n)(得先走到尾)——这就是链表的固有代价,阶段3·Ch2 的双向链表会用一个始终指向尾的 tail 指针把它也优化成 O(1)。

## 按值删除:记好前驱

删除比插入麻烦一点,因为删一个中间节点时,得让它的**前驱**的 `next` 越过它、直接指向它的后继,这样这条链才不会断。所以遍历时除了当前的 `cur`,还得额外记一个 `prev`(前驱)。还有个特殊情况要单独处理:如果删的恰好是**头节点**,它根本没有前驱,这时直接让头后移一位就行。

```c
/* 按值删除第一个等于 val 的节点,返回头(删头节点时头会变) */
Node* remove_value(Node* head, int val) {
    Node* cur = head;
    Node* prev = NULL; /* 记前驱,删中间节点要拿它接后继 */

    while (cur != NULL) {
        if (cur->data == val) {
            if (prev == NULL) {
                /* 删的恰好是头节点:头后移一位 */
                head = cur->next;
            } else {
                /* 删中间/尾:前驱的 next 跨过当前 */
                prev->next = cur->next;
            }
            free(cur);   /* 释放被删节点 */
            return head; /* 只删第一个,到此为止 */
        }
        prev = cur;
        cur = cur->next;
    }
    return head; /* 没找到,原样返回 */
}
```

`prev == NULL` 这个分支就是「删的是头节点」的判别——头节点没有前驱,所以 `prev` 一开始就是 NULL,命中这条分支说明我们恰好要删 head。其余情况 `prev` 都已经在前几轮循环里被赋了值,走 `else` 把前驱的 `next` 跨过当前节点(`prev->next = cur->next`),这条链就重新接好了。`free(cur)` 释放被摘下来的节点,然后立刻 `return`——这个版本只删**第一个**匹配的节点(常见的语义),如果你想删所有匹配项,把 `return head;` 换成继续遍历、同时注意 `free` 后别再用 `cur` 即可。

真跑一遍,建一条 `1 -> 2 -> 3 -> 4`,先删中间的 2、再删头节点 1:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->next = NULL;
    return n;
}

Node* push_back(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head;
    }
    if (head == NULL) {
        return n;
    }
    Node* cur = head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = n;
    return head;
}

void print_list(const Node* head) {
    const Node* cur = head;
    while (cur != NULL) {
        printf("%d ", cur->data);
        cur = cur->next;
    }
    printf("\n");
}

Node* remove_value(Node* head, int val) {
    Node* cur = head;
    Node* prev = NULL;

    while (cur != NULL) {
        if (cur->data == val) {
            if (prev == NULL) {
                head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            return head;
        }
        prev = cur;
        cur = cur->next;
    }
    return head;
}

int main(void) {
    /* 建 1 -> 2 -> 3 -> 4 */
    Node* head = NULL;
    head = push_back(head, 1);
    head = push_back(head, 2);
    head = push_back(head, 3);
    head = push_back(head, 4);
    print_list(head); /* 1 2 3 4 */

    /* 删中间值 2,得 1 -> 3 -> 4 */
    head = remove_value(head, 2);
    print_list(head); /* 1 3 4 */

    /* 删头节点 1,得 3 -> 4(验证删头特殊情况) */
    head = remove_value(head, 1);
    print_list(head); /* 3 4 */

    return 0;
}
```

```text
$ gcc -std=c11 -Wall remove.c -o r && ./r
1 2 3 4
1 3 4
3 4
```

删中间的 2,前驱是 1,`1->next` 从指向 2 改成指向 3,2 被 free,链变成 `1 -> 3 -> 4`。删头节点 1 时 `prev == NULL` 命中,head 直接后移到 3,1 被 free,链变成 `3 -> 4`——删头这条分支之所以重要,是因为它和「删中间」的指针接法不一样,少写这个分支你的链表就永远删不了第一个元素。顺带一提,「按值删除」要先找到那个节点,而找的过程是从头顺 next 走的,最坏要走到表尾——所以单链表的删除是 O(n)(即使摘除动作本身只要改一个指针、是 O(1),但定位那一步拖了后腿);这也是阶段3·Ch2 双向链表要改进的地方之一。

## 释放整表:先存 next,再 free 当前

链表用完要逐个 `free` 干净,否则就是内存泄漏(阶段2·Ch7 的 ASan 在程序退出时会报 `detected memory leaks`)。释放整表的逻辑看上去和遍历一样——顺 next 走,走到一个 free 一个——但这里藏着一个**会让人血压拉满的坑**:如果你照着遍历的写法,直接 `free(cur); cur = cur->next;`,那就完蛋了。

```c
/* 反例:写错了顺序——先 free 再读 next,这是 use-after-free */
void free_list_bad(Node* head) {
    Node* cur = head;
    while (cur != NULL) {
        free(cur);       /* cur 这块内存已被释放 */
        cur = cur->next; /* UB:读已释放内存里的 next */
    }
}
```

`free(cur)` 之后,`cur` 指向的那块堆内存就已经还给系统了,再去做 `cur->next` 就是在读一块已经不属于你的内存——这正是阶段2·Ch7 讲过的 **use-after-free**,是未定义行为。真跑用 ASan 抓:

```text
$ gcc -std=c11 -Wall -fsanitize=address uaf.c -o uaf && ./uaf
uaf.c: In function 'free_list_bad':
uaf.c:40:13: warning: pointer 'cur' used after 'free' [-Wuse-after-free]
   40 |         cur = cur->next; /* UB:读已释放内存里的 next */
      |         ~~~~^~~~~~~~~~~
uaf.c:39:9: note: call to 'free' here
   39 |         free(cur);       /* cur 这块内存已被释放 */
      |         ^~~~~~~~~
==352265==ERROR: AddressSanitizer: heap-use-after-free on address 0x6e4de55e0018
READ of size 8 at 0x6e4de55e0018 thread T0
    #0 in free_list_bad  uaf.c:40
    #1 in main           uaf.c:48
...
SUMMARY: AddressSanitizer: heap-use-after-free uaf.c:40 in free_list_bad
==352265==ABORTING
```

gcc 在编译期就甩了个 `-Wuse-after-free` 警告(`pointer 'cur' used after 'free'`,还贴心指出 free 发生在第 39 行),ASan 在运行期直接 abort,报告 `heap-use-after-free`、`READ of size 8`(读 next 这个指针占 8 字节)。正确的写法是**在 free 当前节点之前,先把它的 next 存到一个临时变量里**:

```c
/* 释放整表:关键坑——free 当前节点前,必须先把它的 next 存下来,
   否则 free 完再读 cur->next 就是 use-after-free(呼应阶段2 Ch7)。 */
void free_list(Node* head) {
    Node* cur = head;
    while (cur != NULL) {
        Node* next = cur->next; /* 先存!free 之后 cur 这块内存就不能再碰了 */
        free(cur);
        cur = next; /* 用存好的 next 往后走 */
    }
}
```

`Node* next = cur->next;` 这一行是整个函数的灵魂:趁 `cur` 还有效,先把「下一个节点在哪」抄下来;之后 `free(cur)` 把当前节点归还,但 `next` 这个局部变量里已经存好了地址,`cur = next;` 用它往后跳,完全不需要再碰已释放的内存。逻辑上和遍历一模一样,差别只在「读 next 的时机」——必须在 free 之前读。把正确的 `free_list` 接到主程序里,用 ASan 复核一遍,确认整表释放干净、零泄漏:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->next = NULL;
    return n;
}

Node* push_back(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head;
    }
    if (head == NULL) {
        return n;
    }
    Node* cur = head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = n;
    return head;
}

void print_list(const Node* head) {
    const Node* cur = head;
    while (cur != NULL) {
        printf("%d ", cur->data);
        cur = cur->next;
    }
    printf("\n");
}

void free_list(Node* head) {
    Node* cur = head;
    while (cur != NULL) {
        Node* next = cur->next; /* 先存!free 之后 cur 这块内存就不能再碰了 */
        free(cur);
        cur = next;
    }
}

int main(void) {
    Node* head = NULL;
    head = push_back(head, 10);
    head = push_back(head, 20);
    head = push_back(head, 30);
    print_list(head); /* 10 20 30 */

    free_list(head); /* 整表释放,ASan 复核应无泄漏 */
    head = NULL;     /* 防悬垂:释放完把头指针也置空 */

    printf("freed.\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -fsanitize=address free_list.c -o fl && ./fl; echo $?
10 20 30
freed.
0
```

退出码 `0`、ASan 没有任何 `detected memory leaks` 报告——三个节点都被妥善 free 掉了,这条链清清爽爽地结束了它的生命周期。`head = NULL;` 这一句是阶段2·Ch7 的好习惯:释放完立刻把头指针置空,杜绝之后手滑再用到悬垂的 `head`(free 之后 `head` 还指着那块已释放内存,是个悬垂指针)。顺带一提,这个 `free_list` 处理空表也是安全的——`head == NULL` 时进循环的条件 `cur != NULL` 直接为假,一次都不执行,完美。

## 小结

单链表是我们用阶段2 攒下的「指针 + malloc」搭起来的第一个真正的数据结构,它的全部精髓就一句话:**每个节点揣一个 next 指针,把散落在堆上的节点串成一条逻辑上的链**。节点定义里那个自引用必须写全名 `struct Node* next`(不能图省事写 `Node* next`,因为 typedef 别名 `Node` 在结构体内部还没生效——gcc/clang 都会报 `unknown type name`/`must use 'struct' tag`;好在指针大小固定、和指向类型多大无关,所以「不完全类型的指针」完全合法,§6.7.2.1)。建节点(`new_node`)要 `malloc`(§7.22.3)之后**立刻查 NULL**,并把 `next` 置 NULL——`malloc` 给的内存不带初始化,漏置 NULL 就是个随机地址的野指针。头插(`push_front`)是 O(1) 的:新节点接在头前面、返回新头,这种「凡是可能改变头的操作都返回新头让调用者接」的写法,是阶段2·Ch3「值传递改不了调用者变量」的标准对策;尾插(`push_back`)得顺 `next` 走到尾再接(循环判 `cur->next != NULL` 才能在最后一个节点处停下),是 O(n),这也是阶段3·Ch2 双向链表配 tail 指针要优化的点。遍历就是 `cur = cur->next` 一路走到 NULL。按值删除(`remove_value`)最易错的是要记前驱 `prev`,删中间节点时让前驱的 `next` 跨过当前、删头节点时(判 `prev == NULL`)让头后移一位——单链表删除整体是 O(n)(定位拖了后腿,摘除本身 O(1))。最后释放整表(`free_list`)那个坑最值得记住:**free 当前节点之前,必须先把它的 next 存到临时变量里**,否则 `free(cur); cur = cur->next;` 就是 use-after-free(阶段2·Ch7 的 ASan 真跑 `heap-use-after-free` + gcc 编译期 `-Wuse-after-free` 双重抓),正确写法 ASan 复核退出码 0 无泄漏。这一章把单链表从头到尾真跑通了一遍,下一章我们给每个节点再加一个指向前驱的 `prev` 指针,做成双向链表——它能在已知节点上 O(1) 删除(单链表做不到,因为单链表删节点必须先找到前驱),代价是每个节点多 8 字节、插入删除时要维护的指针多了一倍。

## 参考资源

- ISO/IEC 9899:2011 §6.7.2.1(结构与联合的声明:成员、自引用指针——指向自身结构类型的指针因指针大小已知而合法)、§7.22.3(内存管理函数:`malloc`/`free`、返回 NULL 表示失败)
- K. N. King《C Programming: A Modern Approach》第 17 章 Linked Lists(17.1 节点声明与自引用、17.2 创建节点、17.3 用头插建表、17.4 删除节点要记前驱、17.5 释放整表)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 6 章 Structures 第 6.5–6.7 节(self-referential structures、内存分配器例子里 `struct nlist *next` 自引用的实战)
- Robert Sedgewick《Algorithms in C》第 3 章 Elementary Data Structures 的链表小节(头插 O(1)、尾插、删除前驱的图解,为阶段3·Ch12 大 O 分析铺垫)
- 阶段2·第1章 指针是什么(next 指针装下一个节点的地址)、第3章 用指针改调用者的变量(push_front 为何返回新头)、第6章 malloc/free(节点申请、必查 NULL)、第7章 动态内存的坑(use-after-free、ASan)
- 阶段3·第2章 双向链表(prev/next、O(1) 删除已知节点)、第3章 栈(单链表实现的 LIFO)、第12章 算法复杂度与大 O(头插 O(1) vs 数组 O(n) 的正式定义)
