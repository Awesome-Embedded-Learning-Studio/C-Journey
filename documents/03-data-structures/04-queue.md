---
title: "队列:FIFO、环形缓冲与链表实现"
description: '阶段3·第4章。队列是 FIFO(先进先出)结构,这一章用两种方式亲手实现它:先拿朴素数组(head/tail 都只往后走)真跑出"撞墙"坑——出几次队头部格子就废了、tail 撞到 N 写不进去;再用环形缓冲(circular buffer)把数组当环用、tail 在 N 处回绕到 0,真跑演示回绕(tail 从 4 回 0、push 60 写到 data[0]、最终 FIFO 出队得 30 40 50 60),并解决"满 vs 空都判 head==tail"的歧义——标准解法是"留一格不填"((tail+1)%N==head 判满、head==tail 判空),另附 size 计数对照方案;最后用带 head/tail 双指针的链表实现(入队 O(1) 尾插、出队 O(1) 头删、动态大小无"满"),ASan+UBSan 复核 free 无泄漏(退出码 0)。全程 gcc16+clang22 双跑,FIFO 出队 1 2 3 对照栈的 3 2 1。'
chapter: 3
order: 4
tags:
  - host
  - data-structures
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第3章:栈(LIFO 对照)"
  - "第 10 章:数组(环形缓冲)、阶段3·第1章:单链表(链表队列)"
related:
  - "阶段3·第12章:算法复杂度(enqueue/dequeue O(1))"
---

# 队列:FIFO、环形缓冲与链表实现

## 引言:排队这件小事

上一章我们写了栈——后进先出(LIFO),像一摞盘子,最后放上去的先被拿走。这一章换个规矩:**先进先出**(FIFO,First-In First-Out),像超市排队结账——先到的人先被服务。这个规矩就一个字:等。数据从一端进、从另一端出,进的那端叫**队尾**(tail),出的那端叫**队头**(head);「入队」操作 `enqueue` 在队尾追加一个元素,「出队」操作 `dequeue` 从队头拿走一个元素。两件事都是 O(1)——入队不用挪别人、出队只动头一个,这点和栈的 push/pop 一样省事。

队列不是教科书里凭空造出来的玩具。你想想,操作系统调度任务凭什么保证「先来先服务」?靠的就是队列——进程排着队等 CPU。键盘敲一串字符、网卡收到一包数据,内核都得先把它们塞进缓冲区排队、再让上层按顺序取走,这种缓冲区的原型就是队列。生产者往里塞、消费者从里取,两端各忙各的、中间靠队列这个「先来先走」的契约对接——这一章我们把它的骨架用 C 敲出来。

规矩定好了,问题就一个:用什么东西装这一排等着的元素?和栈一样,两条路——**数组**或**链表**。但队列比栈多一个麻烦:栈只在数组的一头进出,top 永远往一个方向走;队列是两头动,head 往后走(出队)、tail 也往后走(入队),这一下就出事了。我们先从最直白的数组实现开始,亲手把那个「撞墙」的坑踩出来,再看环形缓冲怎么漂亮地绕过去。

## 数组实现之「朴素」:撞墙的坑

最直白的想法是这样的:开一个数组 `data[N]`,用 `head` 记录队头在哪个下标、`tail` 记录队尾下一个该写入的下标,两个都从 0 开始。入队就 `data[tail] = x; tail++;`,出队就读 `data[head]` 然后 `head++`。听起来没毛病,跑一下就出事了:

```c
#include <stdio.h>

#define N 5

int main(void) {
    int data[N];
    int head = 0; /* 队头下标,出队时往后走 */
    int tail = 0; /* 队尾下一个写入位置,入队时往后走 */

    /* 入队 10 20 30 40 50,把数组填满 */
    for (int i = 1; i <= 5; i++) {
        data[tail] = i * 10;
        tail++;
    }
    printf("入队 5 个后: head=%d tail=%d\n", head, tail);

    /* 出队 3 个(10 20 30),head 往后走 */
    printf("出队: ");
    for (int i = 0; i < 3; i++) {
        printf("%d ", data[head]);
        head++;
    }
    printf("\n出队 3 个后: head=%d tail=%d\n", head, tail);
    printf("数组里还剩 %d 个元素,但 tail 已经 = %d(撞墙)\n", tail - head, tail);

    /* 想再入队:tail == N,没空间了——可数组前 3 格明明空着! */
    printf("想再入队 60: ");
    if (tail == N) {
        printf("tail=%d == N,写不进去!前面 %d 格空着却用不了\n", tail, head);
    }

    /* 打印整张数组看清空间浪费 */
    printf("data 现状: ");
    for (int i = 0; i < N; i++) {
        printf("[%d]=%d ", i, data[i]);
    }
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall naive_queue.c -o nq && ./nq
入队 5 个后: head=0 tail=5
出队: 10 20 30 
出队 3 个后: head=3 tail=5
数组里还剩 2 个元素,但 tail 已经 = 5(撞墙)
想再入队 60: tail=5 == N,写不进去!前面 3 格空着却用不了
data 现状: [0]=10 [1]=20 [2]=30 [3]=40 [4]=50 
```

看出来了吗——FIFO 的顺序是对了(`出队: 10 20 30`,先进来的先出去,这正是队列该有的样子),可空间出问题了。出队 3 个之后 `head` 走到 3、`tail` 还停在 5,这时候我想再入队一个 `60`,程序告诉我 `tail=5 == N`,写不进去。可你看看最后一行 `data 现状`:`[0]=10 [1]=20 [2]=30` 这前三格里的数据已经出队、逻辑上不再属于队列了,可它们还霸着数组的前三个位置,空着却用不了。队列实际只剩 2 个有效元素(`[3]` 和 `[4]`),但整个数组却「满了」——因为 `tail` 撞到了 `N` 这堵墙。

这就是朴素数组队列的致命伤:**head 只往后走、tail 也只往后走,数组前半段被出队「废弃」的空间永远回收不回来**。用不了几次入队出队,数组前半截全是废格子、后半截 `tail` 撞墙,队列就这么「假满」了。最笨的解法是每次出队把全体元素往前挪一格(让 head 回到 0),但那是 O(n),把 O(1) 的出队硬生生拖成 O(n),不划算。真正聪明的解法,是把数组从「一条直线」掰成「一个环」。

## 环形缓冲:把数组掰成环

既然 `tail` 撞墙是因为它「只能往前」,那我们就让它撞墙的时候**绕回来**——数组下标走到 `N-1` 之后,下一个不是 `N`(那越界了),而是回到 `0`。这个「头尾相接」的数组,就叫**环形缓冲**(circular buffer)。实现上不真要把内存弯过来,只是在下标自增那一步加一个取模:`tail = (tail + 1) % N`,这样 `N-1` 之后算出来就是 `0`。`head` 同理。

但环一掰出来,马上冒出一个绕不开的歧义:**怎么区分「空」和「满」?**。直线数组那会儿,`head == tail` 我们心安理得地当「空」(没东西),可到了环上这就不成立了——`head == tail` 既可能是「一个元素都没有」,也可能是「装满了、tail 绕一圈追上了 head」(你看,环上 tail 一直往前、head 一直往前,转满一圈 tail 就从后面追上 head 了)。同一个条件,两种含义,这就是环形缓冲最经典的坑。判错了,要么把满队列当成空、丢数据,要么把空队列当成满、拒绝入队。

业界有两条标准解法,挑一条就行。第一条——也是我们这里要用的——**「留一格不填」**:故意让 `tail` 在追上 `head` 的前一格就停,永远保留一个空位当「空气缓冲」。这样一来,「满」的判据变成 `(tail + 1) % N == head`(tail 再走一步就撞上 head,说明只剩那一格缓冲了),「空」还是 `head == tail`(俩指针指同一格,啥也没有),两个条件泾渭分明。代价是 N 格容量只能用 N-1 格——这点浪费换 O(1) 的判满判空,值。第二条是**加一个 `size` 计数器**:额外存「当前有几个元素」,`size == 0` 判空、`size == N` 判满,一格都不浪费,但多一个字段、每次入队出队要维护它。下面我们用「留一格」方案把环形缓冲完整跑一遍:

```c
#include <stdio.h>

#define N 5 /* 数组大小 5,但最多只存 4 个(留一格) */

typedef struct {
    int data[N];
    int head; /* 队头下标:出队读这里 */
    int tail; /* 队尾下一个写入位置 */
} Queue;

void queue_init(Queue* q) {
    q->head = 0;
    q->tail = 0;
}

/* 空:head == tail(都指同一格,啥也没存) */
int queue_empty(const Queue* q) {
    return q->head == q->tail;
}

/* 满:(tail+1)%N == head——故意让 tail 追上 head 前一格就停,留一格当"空气" */
int queue_full(const Queue* q) {
    return (q->tail + 1) % N == q->head;
}

/* 入队:写到 tail 位置,tail 回绕后移 */
int queue_push(Queue* q, int x) {
    if (queue_full(q)) {
        return -1; /* 满了,入队失败 */
    }
    q->data[q->tail] = x;
    q->tail = (q->tail + 1) % N; /* 回绕:% N 让 N-1 之后回到 0 */
    return 0;
}

/* 出队:读 head 位置,head 回绕后移 */
int queue_pop(Queue* q, int* out) {
    if (queue_empty(q)) {
        return -1; /* 空了,出队失败 */
    }
    *out = q->data[q->head];
    q->head = (q->head + 1) % N;
    return 0;
}

/* 打印内部状态:看清 head/tail 在环上怎么走 */
void queue_debug(const Queue* q, const char* tag) {
    printf("  [%s] head=%d tail=%d  data: ", tag, q->head, q->tail);
    for (int i = 0; i < N; i++) {
        printf("%d ", q->data[i]);
    }
    printf("(empty=%d full=%d)\n", queue_empty(q), queue_full(q));
}

int main(void) {
    Queue q;
    queue_init(&q);
    queue_debug(&q, "init");

    /* 第一幕:入队 1 2 3,出队得 1 2 3(FIFO,对照栈的 3 2 1) */
    queue_push(&q, 1);
    queue_push(&q, 2);
    queue_push(&q, 3);
    queue_debug(&q, "push 1 2 3");

    int v;
    printf("pop 三次: ");
    while (queue_pop(&q, &v) == 0) { /* 一直出到空 */
        printf("%d ", v);
    }
    printf("\n");
    queue_debug(&q, "全出完");

    /* 第二幕:真跑回绕——反复入队出队,让 head/tail 跨过 N-1 回到 0 */
    printf("\n--- 回绕测试:先填到快满,再出几个,再入队,看 head/tail 跨过边界 ---\n");
    queue_init(&q);
    queue_push(&q, 10);
    queue_push(&q, 20);
    queue_push(&q, 30);
    queue_push(&q, 40); /* 留一格,这已经是满(N=5 但只存 4) */
    queue_debug(&q, "push 10 20 30 40(满)");
    printf("  再 push 50 会失败:返回 %d\n", queue_push(&q, 50));

    queue_pop(&q, &v); /* 出 10,head 走到 1 */
    printf("  pop -> %d\n", v);
    queue_pop(&q, &v); /* 出 20,head 走到 2 */
    printf("  pop -> %d\n", v);
    queue_debug(&q, "出了两个");

    /* 关键:tail 还在 4,这时 push 会写到 data[4],然后 tail=(4+1)%5=0 回绕! */
    queue_push(&q, 50);
    queue_debug(&q, "push 50(tail 从 4 回绕到 0)");
    queue_push(&q, 60); /* 写到 data[0],head=2 tail=2 会撞?不:tail=1 */
    queue_debug(&q, "push 60(写到 data[0])");

    printf("\n--- 把剩下的全出完,验证 FIFO 顺序 ---\n");
    printf("pop 剩余: ");
    while (queue_pop(&q, &v) == 0) {
        printf("%d ", v);
    }
    printf("\n");
    queue_debug(&q, "空了");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall ring_queue.c -o rq && ./rq
  [init] head=0 tail=0  data: 0 0 0 0 0 (empty=1 full=0)
  [push 1 2 3] head=0 tail=3  data: 1 2 3 0 0 (empty=0 full=0)
pop 三次: 1 2 3 
  [全出完] head=3 tail=3  data: 1 2 3 0 0 (empty=1 full=0)

--- 回绕测试:先填到快满,再出几个,再入队,看 head/tail 跨过边界 ---
  [push 10 20 30 40(满)] head=0 tail=4  data: 10 20 30 40 0 (empty=0 full=1)
  再 push 50 会失败:返回 -1
  pop -> 10
  pop -> 20
  [出了两个] head=2 tail=4  data: 10 20 30 40 0 (empty=0 full=0)
  [push 50(tail 从 4 回绕到 0)] head=2 tail=0  data: 10 20 30 40 50 (empty=0 full=0)
  [push 60(写到 data[0])] head=2 tail=1  data: 60 20 30 40 50 (empty=0 full=1)

--- 把剩下的全出完,验证 FIFO 顺序 ---
pop 剩余: 30 40 50 60 
  [空了] head=1 tail=1  data: 60 20 30 40 50 (empty=1 full=0)
```

逐行看输出,每一行都在印证这套机制的精妙。第一幕最关键:`push 1 2 3` 之后 `head=0 tail=3`,然后 `pop 三次: 1 2 3`——FIFO 成立,先进来的 `1` 第一个出去,这和栈(LIFO)出 `3 2 1` 正好相反,两种结构的脾气就差在这。出完之后 `head=3 tail=3`,两者相等,`empty=1`,正好印证「空 = head==tail」。

第二幕才是环形缓冲真正发威的地方。先把 10 20 30 40 塞进去,看 `[push 10 20 30 40(满)]` 那行:`head=0 tail=4`,注意 `full=1`——数组明明是 5 格(`N=5`)却判满了,因为这就是「留一格」的代价,`(tail+1)%N == head` 即 `(4+1)%5 == 0` 成立,满了。再 push 50 返回 `-1` 被拒。接着出两个(10、20),`head` 走到 2,这时关键来了:`tail` 还停在 4,push 50 写进 `data[4]`,然后 `tail=(4+1)%5=0`——**回绕发生**了,从数组的物理尾巴绕回了物理开头。再 push 60,写到 `tail=0` 也就是 `data[0]`(把早先出队废弃的那个格子重新用上了!),`tail` 走到 1。你看 `data` 那行变成 `60 20 30 40 50`——`data[0]=60` 是最新入队的,而逻辑上队列的顺序是 `head=2` 处的 `30` 开头:`30 40 50 60`。

最后一幕把它们全出完验证一遍:`pop 剩余: 30 40 50 60`——FIFO 顺序完全正确。最妙的是结尾那行 `head=1 tail=1 empty=1`,两个指针又指到同一处了,这又是「空」。而 `data` 数组里 `60 20 30 40 50` 这些「残留」是已出队元素的尸体,逻辑上不再属于队列,环只看 `head`/`tail`/`size`,不在乎格子里还留着什么。这就是环形缓冲的全部秘密:一段定长数组,两个会回绕的下标,一格刻意的浪费,换来 O(1) 的入队出队和清晰的满空判断——缓冲区的标准实现,几十年没变过。

## 满空判断的另一条路:size 计数

「留一格」干净,但有些场合你就是不想浪费那一格(比如每格很大、或者容量本身小到一格都心疼),那就走第二条路——多存一个 `size` 字段,显式记录当前有几个元素。判空看 `size == 0`,判满看 `size == N`,这样 N 格容量能存满 N 个,一格都不浪费,代价只是结构体多 4 个字节、每次 push/pop 多一条 `size++`/`size--`。两条路是等价的,选哪条看场景,这里把 size 方案也真跑一遍做对照(为了和上面 N=5 存 4 个形成对比,这里 N=4、能存满 4):

```c
#include <stdio.h>

#define N 4

typedef struct {
    int data[N];
    int head;
    int tail;
    int size; /* 显式计数:当前有几个元素 */
} Queue;

int queue_empty(const Queue* q) {
    return q->size == 0; /* 空:计数为 0 */
}

int queue_full(const Queue* q) {
    return q->size == N; /* 满:计数等于容量(一格都不浪费) */
}

void queue_push(Queue* q, int x) {
    if (queue_full(q)) {
        printf("  push %d 失败(满)\n", x);
        return;
    }
    q->data[q->tail] = x;
    q->tail = (q->tail + 1) % N;
    q->size++;
}

int queue_pop(Queue* q) {
    int v = q->data[q->head];
    q->head = (q->head + 1) % N;
    q->size--;
    return v;
}

int main(void) {
    Queue q = {{0}, 0, 0, 0};
    /* N=4,能存满 4 个(对照"留一格"只能存 3) */
    queue_push(&q, 1);
    queue_push(&q, 2);
    queue_push(&q, 3);
    queue_push(&q, 4);
    printf("push 4 个: size=%d (满=%d)\n", q.size, queue_full(&q));
    queue_push(&q, 5); /* 满了,失败 */
    /* 逐次出队存进变量再打印:别在一条 printf 里连续调有副作用的函数,
       函数参数求值顺序未指定,会得到乱序(阶段1·第5章求值顺序坑) */
    int a = queue_pop(&q);
    int b = queue_pop(&q);
    int c = queue_pop(&q);
    int d = queue_pop(&q);
    printf("pop: %d %d %d %d\n", a, b, c, d);
    printf("出完后: size=%d (空=%d)\n", q.size, queue_empty(&q));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall size_queue.c -o sq && ./sq
push 4 个: size=4 (满=1)
  push 5 失败(满)
pop: 1 2 3 4
出完后: size=0 (空=1)
```

`push 4 个: size=4 (满=1)`——N=4 存满 4 个,一格不浪费,这就是 size 方案的好处。`pop: 1 2 3 4` FIFO 顺序也对。这里有个细节顺带说一下:`main` 里我故意把四次 `queue_pop` 的结果先存进 `a b c d` 四个变量、再统一打印,而不是写成 `printf("%d %d %d %d", queue_pop(&q), queue_pop(&q), ...)`。后者看起来省事,但函数参数的求值顺序在 C 里是**未指定**的(§6.5.2.2,编译器可以先算最右边那个 pop、也可以先算最左边),真跑你会看到 gcc 和 clang 给出不同顺序——这玩意儿不是 UB(每种顺序都合法),但你的 FIFO 顺序就乱了。这条坑阶段1·第5章讲求值顺序时说过,这里又一次应验:**有副作用的函数,别在一条语句里连续调用还指望它按某个顺序**。

## 链表实现:动态大小,没有「满」

数组实现的容量是写死的(`N`),满了就拒。如果队列大小事先猜不准——有时只排几个、有时要排几万个——写死容量要么浪费内存、要么动不动就满。链表实现的办法是:**每个元素现用现要**,`malloc` 一个节点装它,用 `free` 还回去,大小随入队出队动态增减,理论上只受堆内存限制,没有「满」这回事(只有 `malloc` 失败)。这一节我们用阶段3·第1章学过的单链表搭一个队列,关键点是**配 head 和 tail 两个指针**——这样入队(尾插)和出队(头删)都是 O(1),不用每次都从头遍历到尾。

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* next;
} Node;

typedef struct {
    Node* head; /* 队头:出队删这里 */
    Node* tail; /* 队尾:入队插这里(O(1) 尾插,不用遍历到尾) */
} Queue;

void queue_init(Queue* q) {
    q->head = NULL;
    q->tail = NULL;
}

int queue_empty(const Queue* q) {
    return q->head == NULL; /* head 为空即空表,tail 自然也 NULL */
}

/* 入队:在尾部接一个新节点,tail 指过去。O(1),不用遍历 */
int queue_push(Queue* q, int x) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return -1; /* 分配失败(链表没有"满",只有"内存没了") */
    }
    n->data = x;
    n->next = NULL; /* 新节点是尾巴,next 为空 */
    if (q->tail == NULL) {
        /* 空表:head 和 tail 都指这个新节点 */
        q->head = n;
        q->tail = n;
    } else {
        /* 非空表:旧尾巴的 next 指向新节点,tail 前进 */
        q->tail->next = n;
        q->tail = n;
    }
    return 0;
}

/* 出队:删头节点,head 往后走。O(1) */
int queue_pop(Queue* q, int* out) {
    if (queue_empty(q)) {
        return -1;
    }
    Node* old = q->head; /* 记下旧头,待会 free */
    *out = old->data;
    q->head = old->next; /* head 后移 */
    if (q->head == NULL) {
        /* 删的是最后一个节点:tail 也要置空,否则变成悬垂指针 */
        q->tail = NULL;
    }
    free(old);
    return 0;
}

/* 释放整张表:逐个 free,杜绝内存泄漏(ASan 复核) */
void queue_destroy(Queue* q) {
    int dummy;
    while (queue_pop(q, &dummy) == 0) {
        /* pop 内部已经 free 了每个节点 */
    }
}

int main(void) {
    Queue q;
    queue_init(&q);

    /* 入队 1 2 3 */
    queue_push(&q, 1);
    queue_push(&q, 2);
    queue_push(&q, 3);
    printf("入队 1 2 3 后: head=%p tail=%p\n", (void*)q.head, (void*)q.tail);

    /* 出队三次,得 1 2 3(FIFO:先进先出) */
    int v;
    printf("pop 三次: ");
    queue_pop(&q, &v);
    printf("%d ", v);
    queue_pop(&q, &v);
    printf("%d ", v);
    queue_pop(&q, &v);
    printf("%d ", v);
    printf("\n出完后: head=%p tail=%p (都置 NULL,没悬垂)\n", (void*)q.head,
           (void*)q.tail);

    /* 重新入队一批,验证 destroy 不泄漏 */
    for (int i = 100; i < 105; i++) {
        queue_push(&q, i);
    }
    printf("再入队 100..104,准备 destroy\n");
    queue_destroy(&q);
    printf("destroy 完成: head=%p tail=%p\n", (void*)q.head, (void*)q.tail);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall list_queue.c -o lq && ./lq
入队 1 2 3 后: head=0x58bcdf7a8010 tail=0x58bcdf7a8050
pop 三次: 1 2 3 
出完后: head=(nil) tail=(nil) (都置 NULL,没悬垂)
再入队 100..104,准备 destroy
destroy 完成: head=(nil) tail=(nil)
```

`pop 三次: 1 2 3`,FIFO 又一次成立。`head`/`tail` 那串地址是堆上 `malloc` 返回的,每次运行都不一样(操作系统的 ASLR 在随机化堆基址,呼应阶段2·第1章讲过的 `%p` 地址随机化),所以别纠结具体数值——重点是出完之后两个指针都 `(nil)` 即 `NULL`,没有任何悬垂。链表队列的几个要点在这里都体现出来了:入队时,空表要走「head 和 tail 都指向新节点」这个分支(否则 tail 还是 NULL、`tail->next` 直接段错误),非空表才走「旧 tail 的 next 接上、tail 前进」;出队时正好反过来,删到最后一个节点要把 tail 也置 NULL(否则 tail 还指着那个刚被 free 的节点,成了悬垂指针,后面阶段2·第7章讲过这是 use-after-free 的温床)。这两条对称的边界处理,就是链表队列比单链表(只有 head)多出来的全部复杂度。

链表实现最大的好处是不用预先定容量、不存在「满 vs 空」的歧义(空就看 `head == NULL`,一清二楚),代价是每个元素都要 `malloc` 一个节点(每次入队一次堆分配、出队一次 `free`,还有 `next` 指针的 8 字节开销)。环形缓冲恰好相反:零堆分配、缓存友好(数组连续),但容量写死、要处理满空歧义。所以工程上的经验法则很清楚——**容量事先能定、对性能/缓存敏感(网络缓冲、串口收发)用环形缓冲;大小事先猜不准、入队出队不那么频繁用链表**。这条选型直觉,等你真的去写个任务调度器或者数据采集模块时,会一下子体会到。

最后必须做的一件事是拿 ASan 复核链表队列有没有内存泄漏——`malloc` 了不 `free` 是动态内存最经典的坑,阶段2·第7章我们用 ASan 一个个抓过。这里 `queue_destroy` 把整张表逐个 pop(每个 pop 内部 `free` 一个节点),跑一遍 ASan 看结果:

```text
$ gcc -std=c11 -Wall -fsanitize=address,undefined -fno-omit-frame-pointer list_queue.c -o lq_asan && ./lq_asan; echo "退出码 $?"
入队 1 2 3 后: head=0x782871be0010 tail=0x782871be0050
pop 三次: 1 2 3 
出完后: head=(nil) tail=(nil) (都置 NULL,没悬垂)
再入队 100..104,准备 destroy
destroy 完成: head=(nil) tail=(nil)
退出码 0
```

退出码 0,没有 `ERROR: AddressSanitizer: ...` 也没有 `detected memory leaks`——每个 `malloc` 出来的节点都被对应 `free` 掉了,FIFO 行为也完全正确。注意 ASan 下那两个堆地址变成了 `0x782871be...`(ASan 会把堆重映射到自己的影子内存区域,基址和普通运行不一样,这是 ASan 的工作方式,不是 bug)。链表队列到这里就干净利落地收口了。

## 小结

队列是 FIFO 结构,「先来的先走」——入队在队尾追加、出队从队头拿走,两端各动一头,enqueue/dequeue 都是 O(1)(§6.5.2.1 的数组下标访问 / §6.7.2.1 的结构体指针解引用)。最朴素的数组实现让 head/tail 都只往后走,真跑立刻暴露「撞墙」坑:出队几次之后数组前半段全是废格子、tail 撞到 N 写不进去,O(n) 的整体挪动又把 O(1) 拖垮;真正的解法是环形缓冲,`tail = (tail + 1) % N` 让下标在 N-1 处回绕到 0,把直线数组掰成环,出队腾出的格子入队时能被重新利用。环一掰出来,「满」和「空」都表现为 `head == tail`,这是环形缓冲最经典的坑——两条标准解法:留一格不填(`(tail+1)%N == head` 判满、`head == tail` 判空,N 格用 N-1),或加 size 计数(`size == 0` 判空、`size == N` 判满,一格不浪费但要维护计数);两条路等价,选型看场景。链表实现配 head/tail 双指针,入队尾插、出队头删都是 O(1),动态大小、没有「满」(只有 `malloc` 失败),代价是每个元素一次堆分配加 `next` 指针的 8 字节开销;空表入队要让 head/tail 都指向新节点、删到最后一个节点出队要让 tail 也置 NULL,这两条对称的边界处理是链表队列的全部难点。真跑验证了 FIFO(`enqueue 1 2 3` → `dequeue` 得 `1 2 3`,与栈的 `3 2 1` 相反),环形缓冲回绕(`tail` 从 4 绕回 0、`data[0]` 被新入队元素复用、最终 FIFO 出队得 `30 40 50 60`),链表队列在 ASan+UBSan 下退出码 0、无泄漏无悬垂。选型直觉:容量事先能定、缓存敏感(网络/串口缓冲)用环形缓冲;大小不定、入队出队不频繁用链表。下一章我们换一个角度——动态数组,看怎么用 `realloc` 让一段数组在运行期按需长大,把容量这个老问题从「写死」变成「自动扩」。

## 参考资源

- ISO/IEC 9899:2011 §6.5.2.1(数组下标 `data[i]` 真跑环形缓冲访问)、§6.7.2.1(结构体 `Queue` 含数组与 head/tail 字段、`Node` 含 `next` 自引用指针)、§6.5.6p8(指针/下标算术回绕 `% N` 的语义)
- K. N. King《C Programming: A Modern Approach》第 19 章·19.3-19.4(用栈 ADT 范例讲抽象数据类型的封装;队列作为章末练习 1/3/5 给出 FIFO 概念、数组实现用 size 计数、链表实现尾插头删)
- Robert C. Seacord《Effective C》第 5 章·Dynamic Memory(本章链表队列的 `malloc`/`free` 配对、`queue_destroy` 逐节点释放,呼应阶段2·第6/7章动态内存)
- 阶段3·第3章:栈(LIFO 对照、数组/链表双实现的范式)、第 1 章:单链表(`Node` 结构、`malloc` 节点、`free` 整表);阶段2·第 6 章:`malloc`/`free` 基础、第 7 章:ASan 抓 use-after-free/泄漏(本章链表 ASan 复核的方法论来源);阶段1·第 5 章:求值顺序坑(本章 size_queue 里连续 pop 的副作用提醒)
- 阶段3·第 12 章:算法复杂度(enqueue/dequeue 的 O(1) 分析、环形缓冲 vs 链表队列的选型权衡)
