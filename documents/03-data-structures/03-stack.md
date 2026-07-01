---
title: "栈:LIFO、数组与链表两种实现、括号匹配实战"
description: "阶段3·第3章。栈就是一摞盘子——后放上去的先拿下来,规矩只有一条:只能在顶端进出。这一章我们用两种方式手搓一个栈:先是用定长数组 + 一个 top 下标(push 用 data[++top]=x、pop 用 data[top--]),它最直白地暴露了栈「下标管理」的本质,代价是栈满写死、越界就是 UB(呼应阶段1 Ch10 数组越界);再用单链表把头当栈顶做动态版,大小随 push 长随 pop 缩,但坑在 pop——必须先把值和 next 存好再 free 节点,顺序反了就是 UAF(ASan 当场抓)。两种实现的 push/pop/peek 全是 O(1)。最后用栈做括号匹配实战:扫描字符串、遇左括号 push、遇右括号 pop 比对,真跑出 \"()()\" 合法、\"(()\" 非法(扫完栈非空)、\"())\" 非法(空栈 pop=右括号多了)。全 gcc16+clang22 真跑,链表版过 ASan。"
chapter: 3
order: 3
tags:
  - host
  - data-structures
  - pointers
  - struct
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 10 章:数组(数组栈实现、越界 UB)"
  - "阶段3·第1章:单链表(链表栈实现)"
  - "阶段2·第12章:内存布局(函数调用栈)"
related:
  - "阶段3·第4章:队列(FIFO 对照)、第12章:算法复杂度(push/pop O(1))"
---

# 栈:LIFO、数组与链表两种实现、括号匹配实战

## 引言:一摞盘子

栈这个词听着唬人,其实你天天在用它——只是没意识到。食堂码盘子:你把盘子一个个摞上去,拿的时候只能从最上面拿,底下那个想拿?先把上面的全搬走。这就是栈的全部规矩,四个字母:**LIFO**(Last In, First Out,后进先出)。盘子只能从顶端进出,中间的、底下的,你看不见也碰不着。

这规矩听着像在给自己设限,但恰恰是这种「限制」让它成了最趁手的工具之一。最典型的例子就在你眼皮底下:C 程序每调用一个函数,系统就在「函数调用栈」上压一帧(存返回地址、局部变量、参数);函数返回时,把最顶上那一帧弹掉,控制权回到调用它的那一帧。阶段2 第12 章讲内存布局时我们看过,栈区是向低地址增长的、函数一嵌套地址就往下沉——那个「往下沉」就是一层层 push,「弹掉」就是 pop。没有栈这种结构,递归根本没法实现(每一层递归都得把自己的状态压下去、回头再取出来)。除了函数调用,表达式求值、括号匹配、浏览器的「后退」、编辑器的「撤销」,底下都是栈。

这一章我们动手用 C 搓两个栈:一个用定长数组做(最直白,但要管「栈满」)、一个用链表做(动态、不会满,但要管 free 的顺序)。最后拿它解决一个经典小问题——括号匹配,顺手验证 LIFO 的威力。栈这个结构的所有操作——push(压栈)、pop(弹栈)、peek(看一眼栈顶)——都是 **O(1)** 的,这是它最讨人喜欢的地方(复杂度的事第 12 章细聊)。

## 栈 = 一个数组 + 一个 top 下标

栈的核心只有一个动作点:**栈顶**。数据从栈顶进、从栈顶出,其它地方碰都不让碰。所以用数组实现栈,自然就是「一个数组存数据、一个变量记栈顶在哪」。ISO C 里数组下标访问的语义在 §6.5.2.1,我们这里就是用它最朴素的形式。

约定有很多种,我们挑最常见的:**top 记的是「栈顶元素的下标」,空栈时 top = -1**。为什么是 -1?因为空栈里一个元素都没有,不存在「栈顶元素的下标」这回事,得用一个非法下标来表示「空」,-1 最直观(数组下标从 0 开始,-1 表示「比第一个还前面」)。另一种约定是 King《C Programming: A Modern Approach》里用的——top 记「下一个可写位置」,空栈时 top = 0,push 写 `contents[top++]`、pop 读 `contents[--top]`。两种都对、都常见,你得在看到别人代码时先搞清楚他用的是哪套约定,不然 push/pop 的边界条件会算错。我们这里用 -1 这套,因为它和「top 是栈顶下标」的直觉最贴。

结构定义长这样:

```c
#define STACK_SIZE 100

/* 数组栈:data 数组 + top 栈顶下标。-1 表空:栈里一个元素都没有。 */
typedef struct {
    int data[STACK_SIZE];
    int top;
} Stack;
```

`data` 是真正装数据的数组,`STACK_SIZE` 写死成 100(后面会讲这带来的麻烦)。`top` 就是栈顶下标,初始化成 -1 表示空栈。整件事就这么简单——一个数组,一个下标,完了。

### push:先抬 top 再写

压栈的核心是 `data[++top] = x` 这一行。注意是 **前置 `++`**:先把 top 加 1(挪到下一个空位),再写进去。来推一遍:空栈 top=-1,push 第一个元素 1,`++top` 让 top 变成 0,写到 `data[0]`;再 push 2,`++top` 成 1,写 `data[1]`;再 push 3,`++top` 成 2,写 `data[2]`。这样数组下标 0..top 就装满了元素,`data[top]` 永远是栈顶。

但写之前必须先检查「栈满没」——`top == STACK_SIZE - 1` 说明数组已经塞到 `data[99]` 了,再 `++top` 就会写到 `data[100]`,那是数组越界,是 UB(阶段1 第10 章专门讲过:数组越界 UBSan 报 `index 100 out of bounds`、ASan 报 `stack-buffer-overflow`)。栈满了就得拦下来,不能硬塞:

```c
/* push:先抬 top 再写。data[++top] = x。返回 0 表示栈满失败。 */
int push(Stack* s, int x) {
    if (is_full(s)) {
        return 0; /* 栈满:再写就越界了(数组下标 UB,呼应阶段1 Ch10) */
    }
    s->data[++s->top] = x;
    return 1;
}
```

返回值用 int 表示成功/失败(1 成功、0 栈满),让调用者有机会处理。这里是把「栈满」当软失败返回,而不是直接 abort——因为实际程序里栈满往往是可恢复的(比如等一会儿再 push、或者换个更大的栈),不该一崩了之。

### pop:先取值再降 top

弹栈是 push 的镜像:`data[top--]`。注意是 **后置 `--`**:先取 `data[top]` 的值,再把 top 减 1。空栈 top=-1,push 三个后 top=2(指向 `data[2]`=3);pop 第一次取 `data[2]`=3,然后 top 减成 1;pop 第二次取 `data[1]`=2,top 减成 0;pop 第三次取 `data[0]`=1,top 减成 -1(回到空栈)。这就是「后进先出」——最后 push 的 3 第一个被弹出来。

但 pop 同样有边界:空栈(top=-1)时 `data[top--]` 会读 `data[-1]`,负数下标同样是越界 UB。所以 pop 之前必须先查空栈:

```c
/* pop:先取值再降 top。data[top--]。空栈返回 0 表示失败。 */
int pop(Stack* s, int* out) {
    if (is_empty(s)) {
        return 0; /* 空栈 pop 是 UB 之源,必须拦 */
    }
    *out = s->data[s->top--];
    return 1;
}
```

这里 pop 不直接返回弹出的值,而是用 `int* out` 把值带出来、返回值留给「成功/失败」——因为如果用返回值同时表达「弹出的值」和「失败」,就分不清「弹出来的是 0」还是「栈空失败了」(0 既可能是数据、也可能是错误码)。这是 C 里常见的「一个返回值不够用、就拿指针参数补」的套路,阶段2 第3 章讲「用指针改调用者的变量」时说过这道。`scanf` 返回成功匹配项数、值通过指针带出,也是同一个道理。

`peek`(也叫 top)就简单了——只看不动,返回栈顶元素的值但不弹出来,`data[top]` 直接读。同样要查空栈。

### 真跑一遍

把上面拼成一个完整程序,push 1 2 3、peek、再一个个 pop 出来看看是不是 3 2 1:

```c
#include <stdio.h>

#define STACK_SIZE 100

/* 数组栈:data 数组 + top 栈顶下标。-1 表空:栈里一个元素都没有。 */
typedef struct {
    int data[STACK_SIZE];
    int top;
} Stack;

void stack_init(Stack* s) {
    s->top = -1; /* -1 = 空栈约定 */
}

int is_empty(const Stack* s) {
    return s->top == -1;
}

int is_full(const Stack* s) {
    return s->top == STACK_SIZE - 1;
}

/* push:先抬 top 再写。data[++top] = x。返回 0 表示栈满失败。 */
int push(Stack* s, int x) {
    if (is_full(s)) {
        return 0; /* 栈满:再写就越界了(数组下标 UB,呼应阶段1 Ch10) */
    }
    s->data[++s->top] = x;
    return 1;
}

/* pop:先取值再降 top。data[top--]。空栈返回 0 表示失败。 */
int pop(Stack* s, int* out) {
    if (is_empty(s)) {
        return 0; /* 空栈 pop 是 UB 之源,必须拦 */
    }
    *out = s->data[s->top--];
    return 1;
}

/* peek:看一眼栈顶但不弹出。 */
int peek(const Stack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    *out = s->data[s->top];
    return 1;
}

int main(void) {
    Stack s;
    stack_init(&s);

    push(&s, 1);
    push(&s, 2);
    push(&s, 3);

    int top_val;
    if (peek(&s, &top_val)) {
        printf("peek: %d\n", top_val); /* 栈顶是最后 push 的 3 */
    }

    int v;
    while (pop(&s, &v)) { /* 边 pop 边打印,后进先出:3 2 1 */
        printf("pop: %d\n", v);
    }

    printf("空了? %s\n", is_empty(&s) ? "是" : "否");

    /* 空栈再 pop:被 is_empty 拦住,不会越界 */
    int dead;
    if (!pop(&s, &dead)) {
        printf("空栈 pop 已拦截\n");
    }

    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra array_stack.c -o as && ./as
peek: 3
pop: 3
pop: 2
pop: 1
空了? 是
空栈 pop 已拦截
```

`peek` 拿到的是 3——最后 push 的那个,栈顶。三个 pop 按 `3 2 1` 出来,正好是 push 顺序 `1 2 3` 的逆序,这就是 LIFO 的实锤。pop 完之后 `is_empty` 返回真,再 pop 一次被拦住、不会去碰 `data[-1]`。clang 跑出来一模一样(gcc16 + clang22 双跑,零警告)。

这个数组栈最大的麻烦你已经感受到了:`STACK_SIZE` 是写死的 100。你要是事先不知道会 push 多少个,要么把数组开得贼大浪费内存、要么赌它够用——一旦真超了就是「栈满」失败。第 5 章我们会用 `realloc` 做动态数组栈,栈满了就自动扩容,从根本上解决这事;现在先记住这个痛点。

## 链表栈:用头当栈顶,动态大小不爆

数组栈的两个痛点——「大小写死」「满了得自己处理」——根上都是因为数组是一块定长内存。换成链表就清爽了:每个元素都是一个 malloc 出来的节点,要用就 malloc 一个挂上去、不要就 free 掉,大小随 push/pop 动态变化,永远不会「栈满」(除非堆内存耗尽,malloc 返回 NULL)。这一节我们用阶段3 第1 章的单链表来做栈,逻辑极其简洁——**把链表的头当栈顶**。

先看节点定义,它和单链表的节点一模一样(§6.7.2.1 结构体):

```c
#include <stdlib.h>

/* 节点:数据域 + 指针域。和单链表(阶段3 Ch1)的节点一模一样。 */
typedef struct Node {
    int data;
    struct Node* next;
} Node;

/* 链表栈:只握一个栈顶指针(=链表头)。空栈 = top 为 NULL。 */
typedef struct {
    Node* top;
} ListStack;
```

注意节点里写的是 `struct Node* next` 而不是 `Node* next`——这是自引用结构体的铁律:typedef 别名 `Node` 在这一行还没生效,结构体内部引用自己必须用完整的 `struct ...` 名字。阶段3 第1 章单链表那里详细讲过这一点,这里直接照搬。空栈的约定也变了:不再是「top = -1」,而是「top 指针为 NULL」——一个节点都没有,栈顶指针自然指向空。

### push = 头插

链表栈的 push 就是单链表的「头插」(push_front):新建一个节点,把它塞到链表最前面,它就成了新栈顶。为什么用头插不用尾插?因为头插是 O(1)(直接改头指针就行),而尾插得先遍历到链表末尾、是 O(n)——栈的操作必须 O(1),否则就失去意义了。

```c
/* push = 头插(把新节点塞到链表最前,它就是新栈顶)。 */
void push(ListStack* s, int x) {
    Node* node = malloc(sizeof(Node));
    if (node == NULL) {
        return; /* malloc 失败:堆满了,这里只静默返回 */
    }
    node->data = x;
    node->next = s->top; /* 新节点接上旧栈顶 */
    s->top = node;       /* 新节点成为栈顶 */
}
```

三行就完事:`node->next = s->top` 让新节点指向原来的栈顶,`s->top = node` 让栈顶指针指向新节点。画出来就是「新节点 → 旧栈顶 → ... → NULL」。malloc 失败(堆耗尽)返回 NULL,这里只是静默 return——工程里你应该让调用者知道(比如返回错误码),这里为了聚焦栈逻辑先简化。

### pop = 头删,但顺序是命门

链表栈的 pop 是单链表的「头删」(pop_front):把栈顶节点取下来、栈顶指针下移到第二个节点。但这里有个能让你调一下午 bug 的坑——**必须先把值和 next 存好,再 free 节点**。顺序反了,先 free 再去读 `victim->data` 或 `victim->next`,就是 use-after-free(读一块已经还给堆的内存),阶段2 第7 章讲 ASan 时专门抓过这个,会报 `heap-use-after-free`。

```c
/* pop = 头删。坑:必须先把值和下一个节点存好,再 free,否则就是 UAF。 */
int pop(ListStack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    Node* victim = s->top; /* 要 free 的节点 */
    *out = victim->data;   /* 先把值取出来 */
    s->top = victim->next; /* 栈顶下移到第二个节点 */
    free(victim);          /* 再 free 节点本体 */
    return 1;
}
```

四步、顺序不能乱:`victim = s->top` 记住要 free 谁;`*out = victim->data` 把值取出来交给调用者;`s->top = victim->next` 栈顶下移;最后才 `free(victim)`。把 free 放最后,是因为前面三步都还要碰 `victim` 这块内存,free 了就不能再碰了。空栈的判断也从「top == -1」变成了「top == NULL」。

### 别忘了整栈释放

链表栈比数组栈多一个义务:**程序结束前要把所有节点 free 干净**。数组栈的 `data` 是结构体里直接嵌的数组,结构体变量本身生命周期结束(比如栈上的 `ListStack s` 出作用域)数组就跟着没了,不用你操心;链表栈的每个节点都是 malloc 出来的堆内存,堆内存不会自动回收,你忘了 free 它就泄漏(阶段2 第7 章的 LeakSanitizer 会在程序退出时报)。最省心的做法是直接复用 pop——pop 内部已经做了 free,所以 destroy 就是「一直 pop 到空」:

```c
/* 整栈释放:一个个 pop(pop 内部已 free 每个节点)。漏一个就是泄漏。 */
void stack_destroy(ListStack* s) {
    int dummy;
    while (pop(s, &dummy)) {
        /* pop 里已经 free 了节点,这里空循环体即可 */
    }
}
```

反复调 pop 直到栈空,既不会漏节点、也不会 double-free(pop 内部一次 free 一个)。这条「每块 malloc 配一次 free」是堆内存的铁律,阶段2 第6、7 章反复强调过。

### 真跑 + ASan 复核

完整程序和数组栈的演示一样:push 1 2 3、peek、pop 到空,最后再 push 一个 99 验证 destroy 能清干净(让 ASan 在程序退出时查泄漏):

```c
#include <stdio.h>
#include <stdlib.h>

/* 节点:数据域 + 指针域。和单链表(阶段3 Ch1)的节点一模一样。 */
typedef struct Node {
    int data;
    struct Node* next;
} Node;

/* 链表栈:只握一个栈顶指针(=链表头)。空栈 = top 为 NULL。 */
typedef struct {
    Node* top;
} ListStack;

void stack_init(ListStack* s) {
    s->top = NULL;
}

int is_empty(const ListStack* s) {
    return s->top == NULL;
}

/* push = 头插(把新节点塞到链表最前,它就是新栈顶)。 */
void push(ListStack* s, int x) {
    Node* node = malloc(sizeof(Node));
    if (node == NULL) {
        return; /* malloc 失败:堆满了,这里只静默返回 */
    }
    node->data = x;
    node->next = s->top; /* 新节点接上旧栈顶 */
    s->top = node;       /* 新节点成为栈顶 */
}

/* pop = 头删。坑:必须先把值和下一个节点存好,再 free,否则就是 UAF。 */
int pop(ListStack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    Node* victim = s->top; /* 要 free 的节点 */
    *out = victim->data;   /* 先把值取出来 */
    s->top = victim->next; /* 栈顶下移到第二个节点 */
    free(victim);          /* 再 free 节点本体 */
    return 1;
}

int peek(const ListStack* s, int* out) {
    if (is_empty(s)) {
        return 0;
    }
    *out = s->top->data;
    return 1;
}

/* 整栈释放:一个个 pop(pop 内部已 free 每个节点)。漏一个就是泄漏。 */
void stack_destroy(ListStack* s) {
    int dummy;
    while (pop(s, &dummy)) {
        /* pop 里已经 free 了节点,这里空循环体即可 */
    }
}

int main(void) {
    ListStack s;
    stack_init(&s);

    push(&s, 1);
    push(&s, 2);
    push(&s, 3);

    int top_val;
    if (peek(&s, &top_val)) {
        printf("peek: %d\n", top_val); /* 3 */
    }

    int v;
    while (pop(&s, &v)) { /* 3 2 1 */
        printf("pop: %d\n", v);
    }

    printf("空了? %s\n", is_empty(&s) ? "是" : "否");

    /* 再塞一个,验证 destroy 能清干净(ASan 在程序退出时查泄漏) */
    push(&s, 99);
    stack_destroy(&s);
    s.top = NULL;

    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra list_stack.c -o ls && ./ls
peek: 3
pop: 3
pop: 2
pop: 1
空了? 是
```

输出和数组栈一模一样——这正说明两种实现对外行为完全等价,差别只在内部(一个数组一个链表)。现在上 ASan 复核有没有泄漏:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -g list_stack.c -o ls_asan && ./ls_asan
peek: 3
pop: 3
pop: 2
pop: 1
空了? 是
$
```

ASan 没报任何 `heap-use-after-free`、`memory leaks`,退出码 0——说明每个节点都被正确 free 了,pop 里的「先存值/next、再 free」顺序是干净的,destroy 也把那个 99 节点回收了。clang 跑出来一致。如果你把 destroy 那行注释掉再跑 ASan,就会看到 `LeakSanitizer: detected memory leaks` 直接点名那块 40 字节(一个 Node)没释放——可以自己试一把感受一下。

### 两种实现怎么选

数组栈和链表栈各有各的脾气。数组栈内存连续、cache 友好、没有 malloc/free 的开销,但大小写死、满了得自己处理,而且 `data[100]` 直接嵌在结构体里,栈上的 `Stack s` 会变成一个 400 多字节的大块——放静态区或堆上没问题,放栈上当局部变量要小心别把系统栈吃太多(有点讽刺,栈结构自己用栈区)。链表栈大小动态、永不满(除非堆耗尽),每个节点要多花一个指针(8 字节)的内存、还要付出 malloc/free 的代价,访问也不如数组连续。工程里通常这样选:能预估上限的用数组栈(快、省),大小完全不可预测的用链表栈(灵活)。C++ 标准库的 `std::stack` 默认用 `std::deque`(双端队列,综合了两者的优点),那是后话了。

## 实战:括号匹配

栈最经典的应用之一就是括号匹配。问题很简单:给你一串可能含圆括号 `()` 的字符串,判断它「配不配对」——每个 `(` 都得有一个对应的 `)` 闭它,而且顺序不能乱。比如 `()()` 和 `((()))` 是合法的,`(()`(左括号多了)和 `())`(右括号多了)就不合法。

为什么这事非栈不可?你想想:扫描字符串时,遇到一个 `(` 你不知道它配不配对——得往后看,等遇到和它配的 `)` 才知道;但在此之前可能又冒出新的 `(`。最里面那个 `(` 一定配最接近它的 `)`,这正好是 LIFO——后遇到的 `(` 先被闭掉。所以做法是:遇 `(` 入栈,遇 `)` 就弹一个 `(` 出来配对。如果弹的时候栈空了,说明这个 `)` 找不到 `(` 配,右括号多了;扫完整个串如果栈还非空,说明有 `(` 没被闭,左括号多了。两种都叫不合法,只有「中途没弹空 + 扫完栈正好空」才是合法。

判定结果用一个枚举表达,让输出可读:

```c
#include <stdio.h>
#include <string.h>

#define STACK_SIZE 256

typedef struct {
    char data[STACK_SIZE];
    int top;
} CharStack;

void stack_init(CharStack* s) {
    s->top = -1;
}
int is_empty(const CharStack* s) {
    return s->top == -1;
}

int push(CharStack* s, char c) {
    if (s->top == STACK_SIZE - 1) {
        return 0;
    }
    s->data[++s->top] = c;
    return 1;
}

int pop(CharStack* s, char* out) {
    if (is_empty(s)) {
        return 0;
    }
    *out = s->data[s->top--];
    return 1;
}

/* 判定结果:用枚举让输出可读 */
typedef enum { OK, EXTRA_RIGHT, EXTRA_LEFT } CheckResult;

/* 扫描字符串,遇 '(' 入栈,遇 ')' 弹一个 '(' 比对。 */
CheckResult check_paren(const char* s) {
    CharStack st;
    stack_init(&st);

    for (size_t i = 0; s[i] != '\0'; i++) {
        if (s[i] == '(') {
            push(&st, '(');
        } else if (s[i] == ')') {
            char top;
            if (!pop(&st, &top)) {
                return EXTRA_RIGHT; /* 栈空还想弹:右括号多了 */
            }
        }
        /* 别的字符直接跳过 */
    }

    if (!is_empty(&st)) {
        return EXTRA_LEFT; /* 扫完了栈还非空:左括号没配对 */
    }
    return OK;
}
```

`check_paren` 的逻辑顺着上面那段思路写就行。这里只处理圆括号 `()`;要支持 `[]`、`{}` 也只是多几行——遇 `]` 弹出来的得是 `[`、遇 `}` 弹出来的得是 `{`,比对不上的就是类型不匹配,留作练习(提示:`char top; pop(&st, &top); if (top != ...) return MISMATCH;`)。这里栈装的是 `char`,因为括号本身就是字符。

跑一组测试用例,把三种情况都覆盖到:

```c
const char* result_name(CheckResult r) {
    switch (r) {
    case OK:
        return "合法";
    case EXTRA_RIGHT:
        return "非法(右括号多了)";
    case EXTRA_LEFT:
        return "非法(左括号没配对)";
    }
    return "?";
}

int main(void) {
    const char* cases[] = {
        "()()",   /* 合法 */
        "(()",    /* 左括号没配对 */
        "())",    /* 右括号多了 */
        "((()))", /* 合法,嵌套 */
        "",       /* 空串合法 */
        ")(",     /* 右先于左 */
    };
    size_t n = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < n; i++) {
        printf("\"%s\" -> %s\n", cases[i], result_name(check_paren(cases[i])));
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra paren.c -o paren && ./paren
"()()" -> 合法
"(()" -> 非法(左括号没配对)
"())" -> 非法(右括号多了)
"((()))" -> 合法
"" -> 合法
")(" -> 非法(右括号多了)
```

三个核心用例全对:`()()` 合法、`(()` 是「左括号没配对」(扫完栈里还剩一个 `(`)、`())` 是「右括号多了」(第二个 `)` 来的时候栈已经空了)。多出来的三个也好理解:嵌套的 `((()))` 合法,空串 `""` 合法(没括号当然配对),`)(` 非法——第一个 `)` 来时栈空,直接判「右括号多了」就返回了,根本走不到后面那个 `(`。clang 跑出来一字不差。

回头看,整个判定就一个核心动作:**遇右括号就弹一个左括号出来对**。这个「弹」正是栈的 LIFO——最先 push 的 `(` 要等所有内层的都闭了才轮到它被弹,完美贴合括号的嵌套结构。如果你不用栈、改用计数器(记左括号个数,遇右括号减一),也能处理单一种类括号;但一旦要同时支持 `()` `[]` `{}` 三种、还要检查类型匹配,计数器就抓瞎了,必须用栈记录「每个待配对的左括号是什么类型」。这就是栈在这类问题里不可替代的地方。

## 小结

栈这一章其实就讲了一件事:**只能在顶端进出的线性结构**。规矩越简单越好用——push、pop、peek 全是 O(1),没有任何花活。我们用两种方式落地它:数组栈是一个 `data[]` 加一个 `top` 下标,`data[++top] = x` 压栈、`data[top--]` 弹栈,代价是大小写死、栈满得自己处理(越界就是 UB,阶段1 第10 章那套);链表栈把单链表的头当栈顶,头插头删、大小动态永不满,代价是每个节点要多花一个指针、还要操心 free 的顺序——pop 时必须先把值和 next 存好再 free 节点,顺序反了就是 UAF,ASan 当场抓给你看。两种实现的对外行为完全等价,工程里按「能不能预估上限」来选,能预估用数组栈(快、省),不能预估用链表栈(灵活)。

栈真正的威力在于它是「需要记一串状态、又只能从最新那个开始处理」这类问题的天然解法。括号匹配就是最直观的例子:每个 `(` 都是「待处理的状态」,栈把它们按 LIFO 摞起来,`)` 来了就处理最顶上那个——先 push 进去的 `(` 一定后闭掉,这正是嵌套结构的本质。一旦你看穿了这层,就会发现函数调用、表达式求值、深度优先搜索、撤销栈,底下全是同一个栈在做同一件事:把状态压下去,回头再取出来。下一章队列是栈的镜像——FIFO,先进先出,我们一起对照着看,会更清楚「为什么这俩是所有容器的祖宗」。

## 参考资源

- King, *C Programming: A Modern Approach* (2nd ed.), Chapter 19 §19.4「A Stack Abstract Data Type」——分别用定长数组、动态数组、链表实现栈 ADT,本章数组栈的 `top` 约定对照了 King 的「top = 0 下一个可写位置」约定。
- K&R *The C Programming Language* (2nd ed.) §4.3——用外部变量实现一个栈,是 C 教材里最经典的栈示例。
- ISO/IEC 9899:2011 §6.5.2.1(数组下标)、§6.7.2.1(结构体与结构体成员)、§7.22.3(malloc/free 内存管理函数)。
- 阶段1 第 10 章「数组」(越界 UB)、阶段2 第 6/7 章「malloc/free 与 ASan」、阶段2 第 12 章「内存布局与函数调用栈」——本章反复呼应这几章。
