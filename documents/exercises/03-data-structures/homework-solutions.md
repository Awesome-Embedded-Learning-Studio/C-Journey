---
title: "阶段 3 课后练习参考答案（Homework）"
description: "阶段 3（数据结构与算法）课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出（gcc 16.1.1 / clang 22.1.8 / WSL Arch 实跑，内存相关题目附 ASan/UBSan 复核）。"
chapter: 3
order: 1
tags:
  - host
  - data-structures
  - algorithm
difficulty: intermediate
reading_time_minutes: 70
platform: host
c_standard: [11]
prerequisites:
  - "阶段 3 课后练习（Homework）"
related:
  - "阶段 3 各章"
---

# 阶段 3 课后练习参考答案（Homework）

> 所有命令与输出在 WSL Arch（gcc 16.1.1、clang 22.1.8）下真实运行得到。计时类输出（第 12 章、Lab、Project 的 bench）每次跑会因机器负载略有波动，但「量级之间的比值」是稳定的。内存相关题目的 ASan 复核都给了退出码，`exit=0` 即无泄漏、无越界。

## 3.1-A {#hw-3-1-a}

**难度 L1** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-1-a)

**思路**：尾插按时间顺序记、头插补录早间读数；`push_front` 必须返回新头是因为「头」这个身份可能换人，值传递改不了调用者的变量。

1. 节点用自引用 `struct Node* next`（typedef 别名此刻还没生效）；`new_node` 里 `malloc` 后必查 NULL、`next` 置 NULL。→ 知识点：[第 1 章：单链表:节点、指针、把内存串成一条链](/03-data-structures/01-singly-linked-list)「节点定义:自引用的那个坑」「创建节点:malloc + 置 next = NULL」两节
2. `push_back` 顺 next 走到尾再接（空表是新头）；`push_front` 新节点 next 指旧头、返回新头。返回新头是因为函数只能改调用者指针**指向的节点**、改不了指针变量本身——凡可能动头的操作都返回新头让调用者接。→ 知识点：[第 1 章](/03-data-structures/01-singly-linked-list)「头插与遍历:O(1) 的代价是返回新头」一节
3. 头插两步固定动作与表长无关（O(1)）；尾插要先顺 next 走到尾（O(n)），这是链表「尾部便宜、头部更便宜」的固有代价。→ 知识点：[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)「前面所有操作，一张大 O 表落位」一节

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

Node* push_front(Node* head, int data) {
    Node* n = new_node(data);
    if (n == NULL) {
        return head;
    }
    n->next = head;
    return n;
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
    /* 一天的室内温度读数:先记录了 12:00/14:00/16:00 三次 */
    Node* head = NULL;
    head = push_back(head, 22);
    head = push_back(head, 25);
    head = push_back(head, 24);
    printf("按记录顺序: ");
    print_list(head);

    /* 早上 8:00 的读数后来才补录,头插到最前 */
    head = push_front(head, 21);
    printf("补记早间后: ");
    print_list(head);

    return 0;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw31a.c -o hw31a && ./hw31a
按记录顺序: 22 25 24
补记早间后: 21 22 25 24
```

## 3.1-B {#hw-3-1-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-1-b)

**思路**：在教材 `remove_value` 的「记前驱」框架上，把「命中就 return」换成「继续遍历」，删头（`prev == NULL`）和删中间两条路径各走各的接法，命中一个 `free` 一个并计数。

1. 循环里命中 `val` 时：`prev == NULL` 说明要删的是头节点，`head` 后移一位；否则 `prev->next = cur->next` 让前驱跨过当前。两条路径都要先存住 `cur->next` 再 `free(cur)`——否则 free 完读 next 就是 use-after-free。→ 知识点：[第 1 章](/03-data-structures/01-singly-linked-list)「按值删除:记好前驱」「释放整表:先存 next，再 free 当前」两节
2. 数据 `{3,7,3,2,3,5}` 里 3 出现在头、中、尾三处，正好逼着两条分支都走到。→ 知识点：[第 1 章](/03-data-structures/01-singly-linked-list)（删头特判是单链表删除绕不开的分支）
3. ASan 复核：三个被删节点 + 三个幸存节点全部正确释放，退出码 0。→ 知识点：[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

```c
Node* remove_all(Node* head, int val, int* removed) {
    Node* cur = head;
    Node* prev = NULL;
    int count = 0;
    while (cur != NULL) {
        if (cur->data == val) {
            Node* victim = cur;
            if (prev == NULL) {
                head = cur->next; /* 删头:头后移一位 */
                cur = head;
            } else {
                prev->next = cur->next; /* 前驱跨过当前 */
                cur = cur->next;
            }
            free(victim);
            count++;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    *removed = count;
    return head;
}
```

（`new_node`/`push_back`/`print_list` 与 3.1-A 相同，`main` 建表 `{3,7,3,2,3,5}` 后调用 `remove_all(head, 3, &removed)`，最后逐个 free 剩余节点。）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw31b.c -o hw31b && ./hw31b
清洗前: 3 7 3 2 3 5
剔除 3 个异常值
清洗后: 7 2 5
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw31b.c -o hw31b_asan && ./hw31b_asan; echo "asan_exit=$?"
清洗前: 3 7 3 2 3 5
剔除 3 个异常值
清洗后: 7 2 5
asan_exit=0
```

## 3.2-A {#hw-3-2-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-2-a)

**思路**：哨兵让「空表」也有一条首尾相接的环——`dummy->prev == dummy->next == dummy`，于是所有插入都面对「两个真邻居」，特判消失。

1. 建空表：哨兵的 prev/next 都指向自己，`is_empty` 就是 `dummy->next == dummy`；`count` 从 `dummy->next` 走到 `dummy`（不含）数节点。→ 知识点：[第 2 章：双向链表:prev+next,O(1) 删除与前驱遍历](/03-data-structures/02-doubly-linked-list)「带头哨兵:把边界全拍平」一节
2. `push_back` 四行（`tail = dummy->prev` 起步）对空表和有数据的表一字不改都成立——空表时 `dummy->prev` 就是 dummy 自己，新节点 prev/next 都指 dummy，于是「插第一个节点」没有任何 if 分支。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)（哨兵拍平边界的机制）
3. 反向打印从 `dummy->prev` 顺 prev 走回 dummy，这是单链表做不到的方向。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)（反向遍历）

```c
int count(const List* L) {
    int n = 0;
    for (Node* p = L->dummy->next; p != L->dummy; p = p->next) {
        n++;
    }
    return n;
}

int is_empty(const List* L) {
    return L->dummy->next == L->dummy;
}
```

（其余函数照教材：`node_new`/`list_new`/`push_back`/`push_front`/`print_forward`/`print_backward`；`main` 依次打印空表状态、尾插 1 2 3、头插 0，最后先存 next 逐个 free 真节点、再 free 哨兵和容器。）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw32a.c -o hw32a && ./hw32a
空表: count=0, 空=1
尾插 1 2 3 后: count=3
正向: 1 2 3
反向: 3 2 1
头插 0 后: count=4
正向: 0 1 2 3
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw32a.c -o hw32a_asan && ./hw32a_asan; echo "asan_exit=$?"
空表: count=0, 空=1
尾插 1 2 3 后: count=3
正向: 1 2 3
反向: 3 2 1
头插 0 后: count=4
正向: 0 1 2 3
asan_exit=0
```

## 3.2-B {#hw-3-2-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-2-b)

**思路**：`move_to_front` = 「摘除 + 头插」的组合拳——两行重连把 n 跨过去，四行把 n 塞到 dummy 与原首之间；两步都是固定动作，O(1)。

1. 摘除：`n->prev->next = n->next; n->next->prev = n->prev;` 前驱后继互相跨过 n——带头哨兵之后 n 的 prev/next 永远非 NULL，两行统一成立。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)「O(1) 删除:prev 在手，不用找前驱」一节
2. 头插：`head = dummy->next` 拿原首，四行把 n 接进 dummy 和原首之间。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)「带头哨兵」一节的 `push_front`
3. 单链表做不到的原因是它只有 next、没有 prev——给定节点指针也拿不到前驱，想挪位置得 O(n) 从头找；双向链表 prev 在手，两步固定。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)「引言:单链表留的一个痛」

```c
/* 把已存在的节点 n 移到表头:两行重连摘下来 + 四行头插,全程 O(1) */
void move_to_front(List* L, Node* n) {
    if (n == NULL || n == L->dummy) {
        return;
    }
    n->prev->next = n->next; /* 前驱跨过 n */
    n->next->prev = n->prev; /* 后继跨过 n */
    Node* head = L->dummy->next;
    n->prev = L->dummy; /* 插到 dummy 与原首之间 */
    n->next = head;
    head->prev = n;
    L->dummy->next = n;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw32b.c -o hw32b && ./hw32b
初始:      1 2 3
用 3 之后: 3 1 2
再用 2:    2 3 1
2 已在前:  2 3 1
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw32b.c -o hw32b_asan && ./hw32b_asan; echo "asan_exit=$?"
初始:      1 2 3
用 3 之后: 3 1 2
再用 2:    2 3 1
2 已在前:  2 3 1
asan_exit=0
```

「2 已在前」那次把已经在最前的节点再移一次，结构纹丝不动——摘除+头插对「已就位」的节点也是安全幂等的，这正是哨兵把边界拍平后的从容。

## 3.3-A {#hw-3-3-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-3-a)

**思路**：`top` 是栈顶下标，`data[++top]` 先抬下标再写、`data[top--]` 先取值再降下标；空栈 pop 和满栈 push 都必须在边界拦下来。

1. `push` 用前置 `++`：先把 top 挪到下一个空位、再写进去——写完 `data[top]` 永远是栈顶。`pop` 用后置 `--`：先把 `data[top]` 的值取出来交给调用者、再把 top 降回前一个元素。→ 知识点：[第 3 章：栈:LIFO、数组与链表两种实现、括号匹配实战](/03-data-structures/03-stack)「push:先抬 top 再写」「pop:先取值再降 top」两节
2. 音符按 1 2 3 4 5 压栈后全部弹出得 5 4 3 2 1——LIFO 就是「倒放」。→ 知识点：[第 3 章](/03-data-structures/03-stack)「引言:一摞盘子」
3. 容量 5 的栈压到第 6 个被 `is_full` 拦住返回失败——拦住的是 `data[5]` 的越界写（UB），这是数组栈那道「容量写死」的墙，第 5 章会用 realloc 拆掉它。→ 知识点：[第 3 章](/03-data-structures/03-stack)「栈 = 一个数组 + 一个 top 下标」一节（栈满必须拦）

```c
#define STACK_SIZE 5

typedef struct {
    int data[STACK_SIZE];
    int top;
} Stack;

int push(Stack* s, int x) {
    if (is_full(s)) {
        return 0;
    }
    s->data[++s->top] = x;
    return 1;
}

int pop(Stack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    *out = s->data[s->top--];
    return 1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw33a.c -o hw33a && ./hw33a
压满 5 个后 is_full = 1
倒放: 5 4 3 2 1
全弹出后 is_empty = 1
第 6 个 push 被拦: 1
```

## 3.3-B {#hw-3-3-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-3-b)

**思路**：教材 `check_paren` 的扩展——弹出后多一步「类型比对」，对不上就是 `MISMATCH`；三种括号的嵌套天然是 LIFO。

1. `close_of(open)` 把开括号映射成对应的闭括号；遇闭括号弹出栈顶，比对类型，不匹配返回 `MISMATCH`。→ 知识点：[第 3 章](/03-data-structures/03-stack)「实战:括号匹配」一节（教材正文留的练习：类型比对）
2. 单计数器处理不了三种括号：它只知道「还剩几个没闭」，不知道栈顶那个是 `(` 还是 `[` 还是 `{`——`([)]` 这类「交叉嵌套」必须靠栈记住每个待配对的**类型**。→ 知识点：[第 3 章](/03-data-structures/03-stack)（栈记录「每个待配对的左括号是什么类型」）
3. 其他字符跳过，所以 `{a+(b*[c-d])}` 也能判；空串扫完栈空，合法。→ 知识点：[第 3 章](/03-data-structures/03-stack)（其余字符直接跳过）

```c
typedef enum { OK, EXTRA_RIGHT, EXTRA_LEFT, MISMATCH } CheckResult;

/* 开括号对应的闭括号 */
static char close_of(char open) {
    switch (open) {
    case '(':
        return ')';
    case '[':
        return ']';
    default:
        return '}';
    }
}

static CheckResult check_brackets(const char* s) {
    CharStack st;
    stack_init(&st);
    for (size_t i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            push(&st, c);
        } else if (c == ')' || c == ']' || c == '}') {
            char top;
            if (!pop(&st, &top)) {
                return EXTRA_RIGHT; /* 栈空还想弹:右括号多了 */
            }
            if (close_of(top) != c) {
                return MISMATCH; /* 弹出来的开括号对不上类型 */
            }
        }
        /* 其余字符(字母/数字/运算符)直接跳过 */
    }
    if (!is_empty(&st)) {
        return EXTRA_LEFT; /* 扫完栈还非空:左括号没配对 */
    }
    return OK;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw33b.c -o hw33b && ./hw33b
"{[()]}" -> 合法
"([)]" -> 非法(类型不匹配)
"{{}" -> 非法(左括号没配对)
"[]))" -> 非法(右括号多了)
"{a+(b*[c-d])}" -> 合法
"" -> 合法
```

（clang 22 编译跑出逐字一致的输出。）

## 3.4-A {#hw-3-4-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-4-a)

**思路**：环形缓冲「留一格」方案——满 = `(tail+1) % N == head`、空 = `head == tail`；两个条件泾渭分明，因为 tail 永远追不上 head。

1. 入队写 `data[tail]` 后 `tail = (tail+1) % N` 回绕；出队读 `data[head]` 后 `head = (head+1) % N` 回绕。→ 知识点：[第 4 章：队列:FIFO、环形缓冲与链表实现](/03-data-structures/04-queue)「环形缓冲:把数组掰成环」一节
2. 满判据 `(tail+1)%N == head`：tail 再走一步就撞上 head，说明只剩那一格「空气缓冲」；空判据 `head == tail`：两个指针指同一格。留一格让「满」和「空」不再共用同一个条件——这就是那格刻意的浪费换来的东西。→ 知识点：[第 4 章](/03-data-structures/04-queue)「怎么区分空和满」的两种标准解法
3. 输出里 `push 50 60` 之后 `head=2 tail=1`：tail 从 4 回绕到 0、60 写进了 `data[0]`（早先出队废弃的格子被重新利用），最后全出队得 `30 40 50 60`——FIFO 正确。→ 知识点：[第 4 章](/03-data-structures/04-queue)「回绕发生」的真跑讲解

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw34a.c -o hw34a && ./hw34a
push 10 20 30 40:
  [装满 4 个] head=0 tail=4 (empty=0 full=1)
第 5 个 push 50 返回 -1(被拦)
pop -> 10
pop -> 20
  [出队 2 个] head=2 tail=4 (empty=0 full=0)
  [再入 50 60(tail 回绕)] head=2 tail=1 (empty=0 full=1)
剩余按序出队: 30 40 50 60
  [出完] head=1 tail=1 (empty=1 full=0)
```

## 3.4-B {#hw-3-4-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-4-b)

**思路**：报数这个动作就是「队头挪到队尾」的旋转——报 1、2 的人 `queue_pop` 再 `queue_push`（围成一圈），报 3 的人直接 `queue_pop` 出列。

1. 链表队列的两端各管一头：队头（`head`）是「当前报数的人」出队的地方，队尾（`tail`）是「报完没出列的人」重新入队的地方——围成一圈靠的就是「弹出立即入队」这个旋转。→ 知识点：[第 4 章](/03-data-structures/04-queue)「链表实现:动态大小，没有『满』」一节
2. 报数到 3 的人只 pop 不 push，就是出列；循环到队空为止。出列顺序 `3 6 2 7 5 1 4` 是 n=7、k=3 的经典答案。→ 知识点：[第 4 章](/03-data-structures/04-queue)（enqueue/dequeue 的组合拳）
3. 选链表队列而不是数组：这题人数和出列过程全动态、只增删不按下标访问，环形缓冲的容量写死反而添乱——这正是第 4 章小结里「大小猜不准用链表」的选型直觉。→ 知识点：[第 4 章](/03-data-structures/04-queue)「小结」（数组 vs 链表的选型）

```c
int main(void) {
    const int n = 7;
    const int k = 3;

    Queue q;
    queue_init(&q);
    for (int i = 1; i <= n; i++) {
        queue_push(&q, i);
    }

    printf("n=%d, 报数到 %d 出列,出列顺序: ", n, k);
    while (!queue_empty(&q)) {
        /* 报 1、2 的人不出列,从队头挪到队尾(围成一圈) */
        for (int step = 1; step < k; step++) {
            int v;
            queue_pop(&q, &v);
            queue_push(&q, v);
        }
        /* 报 3 的人出列 */
        int out;
        queue_pop(&q, &out);
        printf("%d ", out);
    }
    printf("\n");
    return 0;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw34b.c -o hw34b && ./hw34b
n=7, 报数到 3 出列,出列顺序: 3 6 2 7 5 1 4
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw34b.c -o hw34b_asan && ./hw34b_asan; echo "asan_exit=$?"
n=7, 报数到 3 出列,出列顺序: 3 6 2 7 5 1 4
asan_exit=0
```

## 3.5-A {#hw-3-5-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-5-a)

**思路**：`size` 是「装了几个」、`capacity` 是「能装几个」——分开存，push 前看一眼 `size == capacity` 就知道该不该扩容。

1. 四件套照教材：`vec_init` 开初始容量、`vec_push` 满则翻倍扩容（realloc tmp 模式）再写 `data[size]`、`vec_get` 带边界检查、`vec_free` 释放并归零。→ 知识点：[第 5 章：动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)](/03-data-structures/05-dynamic-array)「四件套:init / push / get / free」「扩容的 tmp 模式」两节
2. 初始容量 2、push 7 个：轨迹 `2 -> 4 -> 8` 两次翻倍，数据 10..70 一个不少——realloc 可能「搬家」，靠 `v->data = tmp` 接住新地址。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)「真跑:10 个元素、4 → 8 → 16 的扩容轨迹」一节的同款轨迹
3. `vec_get(下标 7)` 返回 0 被拦（越界）、`vec_get(下标 6)` 返回 70——注意答案代码把返回值先存进 `ok` 变量再打印。如果图省事写成 `printf("...", vec_get(&v, 6, &out), out)`：实参求值顺序是**未指定**的（§6.5.2.2¶10），更糟的是这里 `out` 一边被 `vec_get` 写、一边被另一个实参读、彼此无顺序关系，按 §6.5p2 严格说已是**未定义行为**（比「未指定」更严重）——真跑 gcc 打出 `值=0`、clang 打出 `值=70`，两编译器行为不同就是无顺序关系的活证。这正是教材第 4 章 size_queue 讲过的「有副作用的函数别塞进一条语句」的坑，这里又撞了一次。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)「越界访问」一节、[第 4 章](/03-data-structures/04-queue)（函数实参求值顺序未指定）

```c
static int vec_grow(Vec* v) {
    size_t new_cap = v->capacity * 2;
    int* tmp = realloc(v->data, new_cap * sizeof(int));
    if (tmp == NULL) {
        return 0;
    }
    printf("  [grow] %zu -> %zu\n", v->capacity, new_cap);
    v->data = tmp;
    v->capacity = new_cap;
    return 1;
}

int vec_push(Vec* v, int x) {
    if (v->size == v->capacity) {
        if (!vec_grow(v)) {
            return 0;
        }
    }
    v->data[v->size] = x;
    v->size++;
    return 1;
}

int vec_get(const Vec* v, size_t i, int* out) {
    if (i >= v->size) {
        return 0;
    }
    *out = v->data[i];
    return 1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw35a.c -o hw35a && ./hw35a
init: capacity=2
push 10 -> size=1 capacity=2
push 20 -> size=2 capacity=2
  [grow] 2 -> 4
push 30 -> size=3 capacity=4
push 40 -> size=4 capacity=4
  [grow] 4 -> 8
push 50 -> size=5 capacity=8
push 60 -> size=6 capacity=8
push 70 -> size=7 capacity=8
内容: 10 20 30 40 50 60 70
vec_get(下标 7) 返回 0(越界被拦)
vec_get(下标 6) 返回 1,值=70
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw35a.c -o hw35a_asan && ./hw35a_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.5-B {#hw-3-5-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-5-b)

**思路**：栈只在一端动，`size` 既是元素个数又是栈顶下标+1——push 写 `data[size]` 再 `size++`、pop 先 `size--` 再读 `data[size]`，满了翻倍 realloc。

1. `push` 满则 `grow`（翻倍、tmp 模式），之后写 `data[size]`；`pop` 只把 `size` 减 1、把旧栈顶值带出——**不缩容**。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)（realloc tmp 模式）、[第 3 章](/03-data-structures/03-stack)「数组栈」的 top 约定（size 与 top+1 的对应）
2. 容量轨迹 `4 -> 8 -> 16`：初始 4、push 10 个触发两次翻倍——「满了自动扩」正是教材第 3 章预告的、拆掉 `STACK_SIZE` 那堵墙的做法。→ 知识点：[第 3 章](/03-data-structures/03-stack)「这个数组栈最大的麻烦」一段的预告
3. pop 不缩容的理由：pop 是 O(1) 的、缩容也要 O(n) 搬家；如果「pop 到一半就缩、push 回去又扩」，反复横跳把每次操作都拖成 O(n)——按第 12 章的均摊思想，扩得快缩得懒才是 O(1)。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)「空间复杂度与均摊」一节

```c
typedef struct {
    int* data;
    size_t size;      /* 栈里元素个数(=栈顶下标+1) */
    size_t capacity;
} VecStack;

int push(VecStack* s, int x) {
    if (s->size == s->capacity) {
        if (!grow(s)) {
            return 0;
        }
    }
    s->data[s->size] = x;
    s->size++;
    return 1;
}

int pop(VecStack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    s->size--;
    *out = s->data[s->size];
    return 1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw35b.c -o hw35b && ./hw35b
初始容量 4,连续 push 1..10:
  push  1 -> size=1 capacity=4
  push  2 -> size=2 capacity=4
  push  3 -> size=3 capacity=4
  push  4 -> size=4 capacity=4
  [grow] 4 -> 8
  push  5 -> size=5 capacity=8
  push  6 -> size=6 capacity=8
  push  7 -> size=7 capacity=8
  push  8 -> size=8 capacity=8
  [grow] 8 -> 16
  push  9 -> size=9 capacity=16
  push 10 -> size=10 capacity=16
peek: 10
pop: 10 9 8 7 6 5 4 3 2 1
空栈 pop 被拦: 1, 最终 capacity=16(只扩不缩)
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw35b.c -o hw35b_asan && ./hw35b_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.6-A {#hw-3-6-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-6-a)

**思路**：手动连接造树；三种遍历的递归骨架一字不差，只有「打印根」那一行摆在递归左、递归右的前/中/后；计数函数是遍历的直接变式。

1. 建树：`new_node` 之后链式赋值 `root->left->right = new_node(6)`——阶段2 的「顺着指针解下去」。→ 知识点：[第 6 章：二叉树基础:节点、前/中/后序遍历、递归与释放](/03-data-structures/06-binary-tree)「构建一棵小树:手动连接」一节
2. 前/中/后序：`if (root == NULL) return;` 是递归基线，之后就是 `printf` 与两次递归调用的排列组合。→ 知识点：[第 6 章](/03-data-structures/06-binary-tree)「遍历三态:前序、中序、后序」一节
3. `count_nodes` = 1 + 左子树节点数 + 右子树节点数；`count_leaves` 多一层判断——左右都 NULL 才计 1。它们只是把「访问根」换成了「数数」。→ 知识点：[第 6 章](/03-data-structures/06-binary-tree)（递归遍历的变式）
4. 中序「左→根→右」配第 7 章 BST 的「左全小右全大」，输出天然升序——这是下一章的招牌伏笔。→ 知识点：[第 7 章：二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序](/03-data-structures/07-bst)「性质:左小右大，且层层成立」一节

```c
int count_nodes(const Node* root) {
    if (root == NULL) {
        return 0;
    }
    return 1 + count_nodes(root->left) + count_nodes(root->right);
}

int count_leaves(const Node* root) {
    if (root == NULL) {
        return 0;
    }
    if (root->left == NULL && root->right == NULL) {
        return 1;
    }
    return count_leaves(root->left) + count_leaves(root->right);
}
```

（树形：根 8，左 3 右 10；3 的左 1 右 6；10 的右 14。释放用教材后序 `free_tree`。）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw36a.c -o hw36a && ./hw36a
preorder:  8 3 1 6 10 14
inorder:   1 3 6 8 10 14
postorder: 1 6 3 14 10 8
节点数 = 6, 叶子数 = 3
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw36a.c -o hw36a_asan && ./hw36a_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

叶子是 1、6、14 三个——它们的左右子都是 NULL，正是 `count_leaves` 数出来的 3。

## 3.6-B {#hw-3-6-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-6-b)

**思路**：层序 = 广度优先——用队列记「下一个该轮到谁」：出队一个就访问它，顺手把它的孩子排到队尾。

1. 队列装 `Node*`（第 4 章 size 计数版，元素从 int 换成指针）；`level_order`：根入队，循环「出队 → 访问 → 左右孩子入队」。→ 知识点：[第 4 章](/03-data-structures/04-queue)（队列是 BFS 的标配）、[第 6 章](/03-data-structures/06-binary-tree)「遍历三态」一节的对照
2. 为什么纯递归写不了层序：前/中/后序的递归本质是「深度优先」——先一路扎到最深的叶子再回头；而层序要「一层一层横着扫」，递归栈（LIFO）帮不上忙，必须用一个 FIFO 的队列记下「这层后面还有谁、下一层开头是谁」。这就是 DFS 与 BFS 的分野（教材外补充的 BFS 概念）。→ 知识点：[第 3 章](/03-data-structures/03-stack)（栈与 DFS 的对应，反衬队列与 BFS）、[第 4 章](/03-data-structures/04-queue)（FIFO 语义）
3. 结果 `8 3 10 1 6 14` 正好按层读——第 0 层 8、第 1 层 3 10、第 2 层 1 6 14。→ 知识点：[第 6 章](/03-data-structures/06-binary-tree)（同一棵树、第四种走法）

```c
/* 层序遍历:出队一个节点就访问它,再把它的左右孩子入队 */
void level_order(Node* root) {
    Queue q;
    queue_init(&q);
    queue_push(&q, root);
    while (!queue_empty(&q)) {
        Node* cur = queue_pop(&q);
        printf("%d ", cur->data);
        if (cur->left != NULL) {
            queue_push(&q, cur->left);
        }
        if (cur->right != NULL) {
            queue_push(&q, cur->right);
        }
    }
    printf("\n");
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw36b.c -o hw36b && ./hw36b
level order: 8 3 10 1 6 14
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw36b.c -o hw36b_asan && ./hw36b_asan; echo "asan_exit=$?"
level order: 8 3 10 1 6 14
asan_exit=0
```

## 3.7-A {#hw-3-7-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-7-a)

**思路**：插入是「左小右大」的自然推论——比根小往左、大往右、空位挂新节点；同一组数、不同顺序，长出的树形不同。

1. `insert` 递归返回新子树根、上层 `root->left = insert(...)` 接住——空树建根和普通插入同一套逻辑，`main` 里 `root = insert(root, v)` 每次接返回值绝不能省。→ 知识点：[第 7 章](/03-data-structures/07-bst)「插入:比根小往左、大往右、空了就挂新节点」一节
2. 中序 `1 2 3 4 5 6 7`：插入顺序 `{4,2,6,1,3,5,7}` 是乱的，中序吐出来却升序——BST 的招牌。`find_min` 一路向左走到底得 1；`search(5)` 命中、`search(8)` 落空。→ 知识点：[第 7 章](/03-data-structures/07-bst)「性质」「查找:O(log n) 的二分」两节
3. 树形由插入顺序决定——升序插入会一路向右挂成链表，这就是下一节「退化」的伏笔（第 7 章末尾真跑过）。→ 知识点：[第 7 章](/03-data-structures/07-bst)「退化:有序插入，BST 退化成链表」一节

```c
Node* insert(Node* root, int value) {
    if (root == NULL) {
        return new_node(value);
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else {
        root->right = insert(root->right, value);
    }
    return root;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw37a.c -o hw37a && ./hw37a
inorder: 1 2 3 4 5 6 7
find_min = 1
search(5) = 找到
search(8) = 没找到
```

## 3.7-B {#hw-3-7-b}

**难度 L4** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-7-b)

**思路**：范围检查法把「左小右大、层层成立」翻译成递归不变量：每个节点的值必须落在 `(min, max)` 开区间里，向左收紧上界、向右收紧下界。

1. `is_bst_range(root, min, max)`：空树为真；当前值越出 `(min, max)` 为假；否则左子树用 `(min, root->data)`、右子树用 `(root->data, max)` 递归。→ 知识点：[第 7 章](/03-data-structures/07-bst)「性质:左小右大，且层层成立」一节（性质里「所有」二字的含义）
2. 为什么「只和左右孩子比」不够：把 1 改成 10 后，10 挂在 3 的左子——如果只看 10 和它父亲 3 比，10 > 3 一眼就破绽了；真正的麻烦在于「隔代违规」——某个孙子辈的值可能比祖先还大/还小，只有把区间一路收紧下去才能抓住。范围检查法每个节点都和「从根到它的整条路径」比对，一处不漏。→ 知识点：[第 7 章](/03-data-structures/07-bst)（隔代违规必须靠区间收紧扣住）
3. 删 8（双子，后继 9 顶上）后中序 `1 3 4 5 7 9` 仍有序、`is_bst` 仍为真——教材的三种删除没有破坏性质，体检仪盖了章。→ 知识点：[第 7 章](/03-data-structures/07-bst)「删除:三种情况，逐个拆透」一节（情况三：找后继→拷值→递归删后继）

```c
/* 范围检查法:每个节点的值必须落在 (min, max) 开区间里,层层收紧 */
static int is_bst_range(const Node* root, int min, int max) {
    if (root == NULL) {
        return 1;
    }
    if (root->data <= min || root->data >= max) {
        return 0;
    }
    return is_bst_range(root->left, min, root->data) &&
           is_bst_range(root->right, root->data, max);
}

static int is_bst(const Node* root) {
    return is_bst_range(root, INT_MIN, INT_MAX);
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw37b.c -o hw37b && ./hw37b
建好后的树是合法 BST? 1
把节点 1 改成 10 后是合法 BST? 0
删 8 后中序: 1 3 4 5 7 9
删 8 后是合法 BST? 1
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw37b.c -o hw37b_asan && ./hw37b_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.8-A {#hw-3-8-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-8-a)

**思路**：五个 key 全 `% 5 == 3`，是「哈希函数没选好 + key 有规律」的退化实锤——bucket[3] 挂成一条 5 节点的链表，查找退回顺序比。

1. 头插让后插的在链表最前：bucket[3] 打印 `23 18 13 8 3`（反序），bucket[1] 打印 `6 1`。→ 知识点：[第 8 章：哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找](/03-data-structures/08-hash-table)「插入:头插，冲突就往链表前面挂」一节
2. 负载因子 1.40 意味着平均每桶 1.4 个节点、实际全挤在一个桶里——bucket[3] 里找最先插的 3 要比 5 次，这就是「退化成链表」的代价。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)「负载因子与扩容:别让链表太长」一节
3. `search(13)` 定位 bucket[3] 后**第三个**节点命中（链为 `23 → 18 → 13`，比较 3 次）；`search(99)` 落在空桶 bucket[4] 直接返回——「定位 + 顺序找」的组合。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)「查找:先哈希定位桶，再在桶链表里找」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw38a.c -o hw38a && ./hw38a
bucket[0]:
bucket[1]: 6 1
bucket[2]:
bucket[3]: 23 18 13 8 3
bucket[4]:
负载因子 = 1.40(退化预警)
search(13) -> found
search(99) -> not found
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw38a.c -o hw38a_asan && ./hw38a_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.8-B {#hw-3-8-b}

**难度 L4** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-8-b)

**思路**：把教材固定数组版的 rehash 升级成「桶数组本身就是 malloc 的」：扩容 = calloc 新桶数组 → 节点摘下用新桶数重新哈希 → 头插 → free 旧桶数组。节点全程复用。

1. `ht_init` 用 `calloc` 分配桶数组（顺带清零）；`ht_rehash` 沿质数表找下一个质数，遍历每条旧桶链、`cur->next` 先存再改，重算哈希后头插进新桶。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)「负载因子与扩容」一节（教材交代的真实库做法）
2. 为什么必须重哈希：`hash(key) = key % n_buckets` **依赖桶数**——桶数从 5 变 11，`7 % 5 == 2` 变成 `7 % 11 == 7`，只把桶数组变大、节点不搬，search 就再也找不到它们了。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)（rehash 的机制）
3. 真跑：10 个全 `≡ 2 (mod 5)` 的 key 逼出两次 rehash（5→11 时 load=0.80、11→23 时 load=0.82），最终 23 桶、load 0.43；`ht_destroy` 逐桶 free 节点再 free 桶数组，ASan 复核 0。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)「释放」一节、[阶段 0 第 10 章](/00-dev-environment/11-sanitizer-gate)

```c
static int ht_rehash(HashTable* t, size_t new_n) {
    Node** nb = calloc(new_n, sizeof(Node*));
    if (nb == NULL) {
        return 0;
    }
    for (size_t b = 0; b < t->n_buckets; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next; /* 先存,下面要改 cur->next */
            unsigned ni = hash_key(cur->key, new_n);
            cur->next = nb[ni];
            nb[ni] = cur;
            cur = next;
        }
    }
    free(t->buckets); /* 旧桶数组是 malloc 来的,换掉它 */
    t->buckets = nb;
    t->n_buckets = new_n;
    return 1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw38b.c -o hw38b && ./hw38b
  [rehash] 5 -> 11 桶(load=0.80)
  [rehash] 11 -> 23 桶(load=0.82)
最终: 23 桶,size=10,load=0.43
search(27) -> found
search(99) -> not found
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw38b.c -o hw38b_asan && ./hw38b_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.9-A {#hw-3-9-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-9-a)

**思路**：交换次数 = 逆序对个数（只换相邻逆序对的排序都如此）；比较次数的差别来自「冒泡每轮扫完、插入能提前 break」。

1. `{7,3,9,1,4}` 的逆序对：`(7,3)(7,1)(7,4)(3,1)(9,1)(9,4)` 共 6 对——每个相邻交换只消除一对逆序，所以冒泡和插入的交换次数都是 6，这不是巧合是必然。→ 知识点：[第 9 章：排序入门:冒泡、插入、选择(O(n²) 三件套)](/03-data-structures/09-sorting-quadratic)「插入排序」一节的逆序对讲解
2. 比较次数冒泡 10、插入 8：冒泡每轮把剩余区间整个扫一遍，插入的内层 `break` 让「前段已有序」时立刻停手，省下 2 次。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)（swapped 标志与内层 break）
3. 两个算法结果都是 `1 3 4 7 9`。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)「三者对比」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw39a.c -o hw39a && ./hw39a
冒泡结果: 1 3 4 7 9 | 比较 10 次,交换 6 次
插入结果: 1 3 4 7 9 | 比较 8 次,交换 6 次
逆序对数(手算)= 6,两种只换相邻的排序交换次数都等于它
```

## 3.9-B {#hw-3-9-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-9-b)

**思路**：稳定 = 相等的键保住原有相对先后；插入排序只换相邻且严格 `>` 才换，稳；选择排序跨距交换，把相等的元素隔空甩过去，不稳。

1. 插入排序（严格 `> salary`）排完 `B:3000 A:5000 B:5000 A:8000`——两个 5000 里 A 部门者仍在 B 部门者**前面**，部门序保住了。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)「稳定性」一节（相邻交换 + 严格大于 = 稳定）
2. 选择排序排完 `B:3000 B:5000 A:5000 A:8000`——两个 5000 **翻了**。第一轮它把 3000 从下标 3 直接甩到下标 0，A:5000 被弹到队尾；跨距交换跳过了中间的相等元素，相对先后瞬间破坏。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)（跨距交换的原罪）
3. 比较符号的讲究：写成 `>=` 也换，相等的就被换位、稳定立刻丢——「想稳定就用严格 `>`」要刻进肌肉记忆。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)（稳定性前提是严格大于）

```c
static void insertion_sort(Emp* a, size_t n) {
    for (size_t i = 1; i < n; i++) {
        for (size_t j = i; j > 0; j--) {
            if (a[j - 1].salary > a[j].salary) { /* 严格 >:相等不换 */
                Emp tmp = a[j - 1];
                a[j - 1] = a[j];
                a[j] = tmp;
            } else {
                break;
            }
        }
    }
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw39b.c -o hw39b && ./hw39b
原表(部门已有序):      A:5000 B:5000 A:8000 B:3000
插入排序(稳定)后:      B:3000 A:5000 B:5000 A:8000
选择排序(不稳定)后:    B:3000 B:5000 A:5000 A:8000
```

## 3.10-A {#hw-3-10-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-10-a)

**思路**：归并 = 对半切、各自排好、merge 两个有序段；merge 借 O(n) 临时数组（只拷左半边）、用完即 free；`<=` 让相等的元素先取左边的。

1. `mergesort(a, 0, 4)` 排 `{9,4,7,1,3}` 得 `1 3 4 7 9`；$mid = lo + \frac{hi-lo}{2}$ 防溢出。→ 知识点：[第 10 章：快排与归并:分治、O(n log n)、对照 qsort](/03-data-structures/10-quicksort-mergesort)「归并:对半切、各自排好、合并」一节
2. 稳定性验证：`merge` 里 `left[i] <= a[j]`——相等时先取左半边，`5A` 回写后仍在 `5B` 前。换成 `<` 就变成先取右半边，稳定性丢失。→ 知识点：[第 10 章](/03-data-structures/10-quicksort-mergesort)「性能对照」一节的稳定性分析
3. 每次 `merge` 的 `malloc` 都配 `free(left)`——ASan 复核 0。→ 知识点：[第 10 章](/03-data-structures/10-quicksort-mergesort)（借了就得还）

```c
static void merge(int* a, int lo, int mid, int hi) {
    int n_left = mid - lo + 1;
    int* left = malloc((size_t) n_left * sizeof(int));
    if (left == NULL) {
        return;
    }
    for (int i = 0; i < n_left; i++) {
        left[i] = a[lo + i];
    }
    int i = 0, j = mid + 1, k = lo;
    while (i < n_left && j <= hi) {
        if (left[i] <= a[j]) { /* <=:相等先取左半边,保稳定 */
            a[k++] = left[i++];
        } else {
            a[k++] = a[j++];
        }
    }
    while (i < n_left) {
        a[k++] = left[i++];
    }
    free(left);
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw310a.c -o hw310a && ./hw310a
mergesort {9,4,7,1,3}: 1 3 4 7 9
含相等键: 1Y 2X 5A 5B (5A 仍在 5B 前 = 稳定)
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw310a.c -o hw310a_asan && ./hw310a_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.10-B {#hw-3-10-b}

**难度 L4** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-10-b)

**思路**：升序数据对「固定取尾 pivot」的 Lomuto 是最坏输入——每层只切掉 1 个元素；三数取中让 pivot 落在中位数附近，把数组切得均衡。

1. 固定版在升序 1..20 上比较 190 次：$19 + 18 + ... + 1$——每层 pivot 都是最大值、切出「左 n-1 右 0」，最坏 O(n²) 的实锤。→ 知识点：[第 10 章](/03-data-structures/10-quicksort-mergesort)「最坏的坑:有序数据 + 选首/尾当 pivot」一节
2. 三数取中版比较 54 次：三数排成升序后 **中位数换到 pivot 位** `a[hi]`，第一层 pivot 取到 10、把 20 个元素切得近乎对半，往下每层都均衡，比较次数掉到 O(n log n) 量级。→ 知识点：[第 10 章](/03-data-structures/10-quicksort-mergesort)「三数取中」一段
3. **真实翻车现场**：这题的第一版答案把三数排完升序后**没有**把中位数挪到 pivot 位——三个 if 排完，落在 `a[hi]` 上的是**最大值**而不是中位数，pivot 还是最大的那个，比较次数和固定版一模一样（190 次）。「中位数停在 a[mid] 上」和「中位数当上 pivot」是两回事，少那一行 `swap(a, mid, hi)`，整个优化等于白写。真跑抓出来的。→ 知识点：[第 10 章](/03-data-structures/10-quicksort-mergesort)（pivot 的选择是性能命门）

```c
/* 三数取中:先把 a[lo]/a[mid]/a[hi] 排成升序,此时 a[mid] 是中位数,
   再把它换到 pivot 位 a[hi] 走 Lomuto */
static int partition_med3(int* a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[mid] < a[lo]) {
        swap(a, mid, lo);
    }
    if (a[hi] < a[lo]) {
        swap(a, hi, lo);
    }
    if (a[hi] < a[mid]) {
        swap(a, hi, mid);
    }
    swap(a, mid, hi); /* 关键:中位数挪到 pivot 位,别让最大值留在那 */
    int pivot = a[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        cmp_count++;
        if (a[j] <= pivot) {
            i++;
            swap(a, i, j);
        }
    }
    swap(a, i + 1, hi);
    return i + 1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw310b.c -o hw310b && ./hw310b
固定 pivot 排序升序 1..20: 比较 190 次,结果有序=是
三数取中 排序升序 1..20: 比较 54 次,结果有序=是
```

（不加 `swap(a, mid, hi)` 那一行的翻车版，第二行会打出和第一行相同的 190 次。）

## 3.11-A {#hw-3-11-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-11-a)

**思路**：闭区间 `[lo, hi]` 配套写法——`lo <= hi` 循环、每轮砍掉含 mid 的一半；`mid` 用 $lo + \frac{hi-lo}{2}$ 防 $lo+hi$ 溢出。

1. 查 23：`[0,9] mid=4 a[4]=16<23 → lo=5`；`[5,9] mid=7 a[7]=56>23 → hi=6`；`[5,6] mid=5 a[5]=23` 命中，下标 5。查 30 一路砍到区间空返回 -1。→ 知识点：[第 11 章：二分查找:有序数组、O(log n)、bsearch](/03-data-structures/11-binary-search)「迭代版:每次砍一半」一节
2. $\frac{lo + hi}{2}$ 在大下标上 $lo + hi$ 先溢出：`int` 装不下 35 亿，有符号溢出是 UB（§6.5p5），溢出后 mid 变负、`a[mid]` 越界——JDK 的 `Arrays.binarySearch` 都栽过这个坑。$hi - lo$ 恒非负且不超数组长，$lo + \frac{hi-lo}{2}$ 永不溢出。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)「那个 mid 为什么不写 (lo+hi)/2」一节

```c
static int bsearch_iter(const int* a, int n, int key) {
    int lo = 0;
    int hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2; /* 防 lo+hi 溢出 */
        if (a[mid] == key) {
            return mid;
        }
        if (a[mid] < key) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw311a.c -o hw311a && ./hw311a
数组: 2 5 8 12 16 23 38 56 72 91
查 23 -> 下标 5
查 30 -> -1
```

## 3.11-B {#hw-3-11-b}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-11-b)

**思路**：`lower_bound` 的答案候选是「第一个 >= key」，天然适合半开区间 `[lo, hi)`：`lo` 装候选、`hi` 装确定不可能的，命中候选时 `hi = mid` 而不是 `mid - 1`。

1. 半开区间配套：循环 `lo < hi`（区间非空）、`a[mid] < key` 时 `lo = mid + 1`（mid 确定不合格）、否则 `hi = mid`（mid 可能是答案，保留）——区间每轮严格缩小，不丢候选也不死循环。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)「边界:lo <= hi 还是 lo < hi」一节（区间约定、条件、边界动法三者必须配套）
2. 含重复的 `{1,3,3,3,5,7,9}`：`lower_bound(3) = 1`（第一个 3）、`lower_bound(4) = 4`（4 不在，落在第一个 >4 的 5 上）、`lower_bound(1) = 0`、`lower_bound(10) = 7`（全小于，返回 n）。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)（二分的边界变式）
3. 如果把这题的 `lo < hi` 配闭区间的 `hi = mid - 1`，等于在「mid 可能是答案」时把它扔了——答案直接被丢出区间。反过来闭区间 `lo <= hi` 配 `hi = mid` 会死循环。混着抄必出 bug。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)（「不能混着抄」）

```c
/* 返回第一个 a[i] >= key 的下标;全小于 key 时返回 n。
   半开区间 [lo, hi):候选答案落在 lo 里,所以命中时 hi = mid(不 -1) */
static int lower_bound(const int* a, int n, int key) {
    int lo = 0;
    int hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < key) {
            lo = mid + 1;
        } else {
            hi = mid; /* a[mid] >= key,可能是答案,保留 mid */
        }
    }
    return lo;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw311b.c -o hw311b && ./hw311b
lower_bound(3) = 1(第一个 >=3 的位置)
lower_bound(4) = 4(4 不在表里,第一个 >4 的位置)
lower_bound(1) = 0
lower_bound(10) = 7(全小于 10,返回 n)
```

（clang 22 编译跑出逐字一致的输出。）

## 3.12-A {#hw-3-12-a}

**难度 L2** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-12-a)

**思路**：①按第 12 章的大 O 表对号入座；②选择排序的比较次数是双循环的「实打实」计数，和输入内容无关。

1. 答案对照表：链表头插 O(1)、链表按值查找 O(n)、栈 push O(1)、环形缓冲入队 O(1)、动态数组 push 均摊 O(1)、BST 平衡插入 O(log n)、哈希表平均查找 O(1)、冒泡排序 O(n²)、快排平均 O(n log n)、二分查找 O(log n)。→ 知识点：[第 12 章：算法复杂度与大 O：一把尺子量遍前面所有算法](/03-data-structures/12-big-o-complexity)「前面所有操作，一张大 O 表落位」一节
2. 选择排序 n=6 比 15 次、n=10 比 45 次，都等于 $\frac{n(n-1)}{2}$——它的循环结构是「第 i 轮固定比 n-1-i 次」，一次不多一次不少，与数据是升序、降序还是乱序无关。冒泡有 swapped 提前退出、插入有内层 break，它们才有「最好情况」；选择排序没有这一说。→ 知识点：[第 9 章](/03-data-structures/09-sorting-quadratic)「选择排序」一节（比较次数写死）、[第 12 章](/03-data-structures/12-big-o-complexity)（最好/最坏/平均）

```c
static size_t selection_sort(int* a, size_t n) {
    size_t compares = 0;
    for (size_t i = 0; i + 1 < n; i++) {
        size_t min = i;
        for (size_t j = i + 1; j < n; j++) {
            compares++;
            if (a[j] < a[min]) {
                min = j;
            }
        }
        if (min != i) {
            int tmp = a[i];
            a[i] = a[min];
            a[min] = tmp;
        }
    }
    return compares;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw312a.c -o hw312a && ./hw312a
n=6: 比较 15 次,公式 n(n-1)/2 = 15,吻合
n=10: 比较 45 次,公式 n(n-1)/2 = 45,吻合
```

## 3.12-B {#hw-3-12-b}

**难度 L4** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-12-b)

**思路**：给两个动态数组各加一个 `copies` 计数器，把「扩容搬家」的总代价数出来——翻倍的拷贝总量是 O(n)，+1 的是 O(n²)。

1. 翻倍扩容 n=10000：扩容发生在容量 4→8→…→16384，拷贝总量 `4+8+…+8192 = 16380`（约 2n）；平均每次 push 写 2.6 个元素——偶尔 O(n) 的扩容摊到 n 次 push 上，均摊 O(1)。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)「真跑」一节的 4→8→16 轨迹、[第 12 章](/03-data-structures/12-big-o-complexity)「空间复杂度与均摊」一节
2. +1 扩容：容量 4 起步，第 k 次 push 都触发一次「搬 k 个」，总量 `4+5+…+9999 = 49994994`，平均每次 push 写 5000.5 个元素——把 O(1) 的 push 拖成 O(n)。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)（「每次只 +1 扩容」退化成 O(n²) 的教材断言）
3. 两者拷贝量相差 3052 倍，且这个倍数随 n 线性拉大——「扩容倍数」是动态数组的性能命门。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)（量级差距随 n 放大）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw312b.c -o hw312b && ./hw312b
push 10000 个元素:
  2 倍扩容: 搬家拷贝 16380 次,平均每次 push 写 2.6 个元素(均摊 O(1))
  +1 扩容:  搬家拷贝 49994994 次,平均每次 push 写 5000.5 个元素(退化成 O(n))
  两者拷贝量相差 3052 倍
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw312b.c -o hw312b_asan && ./hw312b_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.C-1 {#hw-3-c-1}

**难度 L3** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-c-1)

**思路**：三章各管一段流水线——动态数组解决「数量未知」的存储，归并解决「要排序」，二分解决「排完要快速查」；顺序不能乱，二分的前提是有序。

1. 动态数组初始容量 2 装 8 个成绩（扩容发生在内部，调用方只管 push）；归并排 `v->data[0..size-1]`（`merge` 借临时数组、用完即 free）。→ 知识点：[第 5 章](/03-data-structures/05-dynamic-array)（四件套）、[第 10 章](/03-data-structures/10-quicksort-mergesort)「归并」一节
2. 二分查 74 命中下标 3、查 60 返回 -1。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)「迭代版」一节
3. 为什么不能先二分再排序：二分「砍掉一半」的前提是「中间元素比 key 小，左边就全小」——这个前提只对有序数组成立；乱序数组里砍掉的可能是答案（教材原话：「结果纯属瞎蒙」）。所以流水线的顺序是硬约束。→ 知识点：[第 11 章](/03-data-structures/11-binary-search)「引言:排序换速度」一节

```c
int main(void) {
    Vec v;
    vec_init(&v, 2);
    int scores[] = {72, 95, 58, 88, 67, 91, 74, 83};
    size_t n = sizeof(scores) / sizeof(scores[0]);
    for (size_t i = 0; i < n; i++) {
        vec_push(&v, scores[i]);
    }

    mergesort(v.data, 0, (int) v.size - 1);

    printf("排序后: ");
    for (size_t i = 0; i < v.size; i++) {
        printf("%d ", v.data[i]);
    }
    printf("\n");
    printf("查 74 -> 下标 %d\n", bsearch_iter(v.data, (int) v.size, 74));
    printf("查 60 -> %d(不在表里)\n", bsearch_iter(v.data, (int) v.size, 60));

    free(v.data);
    return 0;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw3c1.c -o hw3c1 && ./hw3c1
排序后: 58 67 72 74 83 88 91 95
查 74 -> 下标 3
查 60 -> -1(不在表里)
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw3c1.c -o hw3c1_asan && ./hw3c1_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.C-2 {#hw-3-c-2}

**难度 L4** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-c-2)

**思路**：两个结构各干各的强项——哈希集合用 O(1) 平均判「见没见过」，单链表按插入顺序保「先来后到」；哈希保不了序、链表判重是 O(n)，合起来才两全。

1. `set_add` 先沿桶链表查一遍，没查到才头插并返回 1；`append` 用第 1 章尾插把首次见到的编号接进链表。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)（链地址查找+头插）、[第 1 章](/03-data-structures/01-singly-linked-list)（尾插保序）
2. 复杂度：每个元素 O(1) 平均判重 + O(1) 尾插，整体 O(n)；朴素做法对每个新元素扫一遍已保留的链表，最坏 O(n²)。哈希表把「查重」从线性的瓶颈里解放出来。→ 知识点：[第 12 章](/03-data-structures/12-big-o-complexity)（量级对照）
3. 输出 `3 1 2 4`、拦下重复 3 次；`set_destroy` 逐桶 free、`free_list` 逐节点 free，ASan 复核 0。→ 知识点：[第 8 章](/03-data-structures/08-hash-table)「释放」一节、[第 1 章](/03-data-structures/01-singly-linked-list)「释放整表」一节

```c
int main(void) {
    int seq[] = {3, 1, 3, 2, 1, 4, 3};
    size_t n = sizeof(seq) / sizeof(seq[0]);

    HashSet seen = {0};
    LNode* head = NULL;
    int dupes = 0;

    for (size_t i = 0; i < n; i++) {
        if (set_add(&seen, seq[i])) {
            head = append(head, seq[i]); /* 第一次见,记进链表 */
        } else {
            dupes++; /* 见过,拦下 */
        }
    }

    printf("保序去重: ");
    for (LNode* cur = head; cur != NULL; cur = cur->next) {
        printf("%d ", cur->data);
    }
    printf("\n");
    printf("扫描 %zu 个,拦下重复 %d 次\n", n, dupes);

    set_destroy(&seen);
    free_list(head);
    return 0;
}
```

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw3c2.c -o hw3c2 && ./hw3c2
保序去重: 3 1 2 4
扫描 7 个,拦下重复 3 次
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw3c2.c -o hw3c2_asan && ./hw3c2_asan; echo "asan_exit=$?"
（输出同普通构建,省略重复部分）
asan_exit=0
```

## 3.C-3 {#hw-3-c-3}

**难度 L5** · 题面见 [homework](/exercises/03-data-structures/homework#hw-3-c-3)

**思路**：LRU 的本质是「一条按使用时间排序的链表 + 一张直接定位节点的表」。链表管顺序（最近在头、最久在尾），哈希管定位（key 到节点的 O(1) 直达）；每次 get/put 都先哈希定位、再把节点摘下来头插。

1. **两个链接必须分开**。节点同时活在两个结构里：哨兵双向链表的 prev/next、哈希桶链的 hnext。如果图省事让桶链共用 `next` 字段，`list_push_front` 把 `n->next` 指到链表邻居的那一刻，桶链的链接就被覆盖了——顺着桶链走会走到链表里去、绕进哨兵环，`find_node` 死循环。**真实翻车现场**：第一版就是共用一个 `next`，程序一跑 CPU 直接 100%、卡死在一个 `while` 里，WSL 上跑了几分钟都出不来——ASan 都来不及报错。修复就是加一个独立的 `hnext` 字段，两条链各走各的。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)（节点挂进两个结构时链接字段要分开）、[第 8 章](/03-data-structures/08-hash-table)（桶链）
2. `get`：哈希定位 → 没找到返回 -1；找到就两行摘除 + 四行头插（3.2-B 的 `move_to_front` 原样复用）。`put`：已存在就更新值并移到最前；不存在就新建节点，容量满时淘汰 `dummy->prev`（链表尾 = 最久未用）——摘除 + 从桶链摘掉（第 1 章记前驱删除）+ free。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)（摘除与头插）、[第 1 章](/03-data-structures/01-singly-linked-list)（记前驱删除）
3. LeetCode 官方示例序列的输出 `1 -1 -1 3 4`：get(1) 命中并把 1 移到最前；put(3) 淘汰最久未用的 2；put(4) 淘汰 1。ASan + clang 双跑退出码 0。→ 知识点：[第 2 章](/03-data-structures/02-doubly-linked-list)、[第 8 章](/03-data-structures/08-hash-table)（两个结构协同）

```c
typedef struct Node {
    int key;
    int value;
    struct Node* prev;  /* 双向链表 */
    struct Node* next;  /* 双向链表 */
    struct Node* hnext; /* 哈希桶链(单向) */
} Node;

int lru_get(LRUCache* c, int key) {
    Node* n = find_node(c, key);
    if (n == NULL) {
        return -1;
    }
    list_unlink(n);
    list_push_front(c, n); /* 刚用过,挪到最前 */
    return n->value;
}

void lru_put(LRUCache* c, int key, int value) {
    Node* n = find_node(c, key);
    if (n != NULL) {
        n->value = value;
        list_unlink(n);
        list_push_front(c, n);
        return;
    }
    Node* fresh = malloc(sizeof(Node));
    /* ... 初始化字段 ... */
    if (c->size == c->capacity) {
        Node* victim = c->dummy->prev; /* 最久未使用:链表尾 */
        list_unlink(victim);
        hash_remove(c, victim);
        free(victim);
        c->size--;
    }
    hash_insert(c, fresh);
    list_push_front(c, fresh);
    c->size++;
}
```

（完整代码含 `list_unlink`/`list_push_front`/`hash_insert`/`hash_remove`/`find_node`，即 3.2-B 与第 1、8 章手艺的组合；`hash_remove` 走 `hnext` 链记前驱删除。）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra hw3c3.c -o hw3c3 && ./hw3c3
get(1) = 1
get(2) = -1
get(1) = -1
get(3) = 3
get(4) = 4
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined hw3c3.c -o hw3c3_asan && ./hw3c3_asan; echo "asan_exit=$?"
get(1) = 1
get(2) = -1
get(1) = -1
get(3) = 3
get(4) = 4
asan_exit=0
$ clang -std=c11 -Wall -Wextra -fsanitize=address,undefined hw3c3.c -o hw3c3_clang_asan && ./hw3c3_clang_asan; echo "clang_asan_exit=$?"
get(1) = 1
get(2) = -1
get(1) = -1
get(3) = 3
get(4) = 4
clang_asan_exit=0
```
