---
title: "结构体、联合、枚举与内存对齐:把多个字段打包成一个整体"
description: "前面 12 章的变量都是「一个名字对一个值」——int、char、数组。这一章讲 C 怎么把多个相关字段(一个点=x+y、一个学生=学号+姓名+成绩)打包成一个整体。真跑结构体基础(定义/初始化/逐字段 vs 指定初始化器 .x=1/字段访问 ./结构体指针的箭头 ->/结构体数组/typedef struct Node 链表惯用法);然后是本章重头戏——内存对齐与填充:同一个 struct,字段顺序不同 sizeof 居然不一样(struct A{char;int;char} = 12 字节,A 字段之间塞了 6 字节 padding 浪费一半;重排成 struct B{int;char;char} = 8 字节),offsetof 看每个字段真实偏移,C11 的 _Alignas 把变量对齐拔到 16/32(但拔高不压低——_Alignas(2) int x 直接编译失败);联合 union 让 int 和 float 共享同一块 4 字节内存(写 float 读 int 看到 IEEE754 位模式 0x4048F5C3=1078523331);枚举 enum 给整数起名字(本质还是 int,赋 42 编译器不拦);位域 bitfield 按位分配(a:3 b:5)但 sizeof 取决于基类型(unsigned 是 4 字节、unsigned char 是 1 字节),3 位存 9 被截断成 1;最后柔性数组 FAM(C99)一次 malloc 分配结构体+可变尾巴。全 gcc16+clang22 真跑。"
chapter: 1
order: 13
tags:
  - host
  - struct
  - type
difficulty: intermediate
reading_time_minutes: 15
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 2 章:整型家族与 sizeof(sizeof 运算符)"
  - "第 10 章:数组(数组下标)"
  - "阶段2·第1章:指针是什么(结构体指针 ->)"
  - "阶段2·第10章:复杂声明与 typedef(typedef 给类型起别名)"
related:
  - "阶段3·第1章:单链表(typedef struct Node + 结构体指针 = 链表节点)"
  - "阶段3·第6章:二叉树(struct Node{left,right})"
---

# 结构体、联合、枚举与内存对齐:把多个字段打包成一个整体

## 引言:从一个名字对一个值,到一组字段一个整体

前面 12 章我们写的变量都是「一个名字对一个值」——`int x`、`char c`、`double arr[10]`,类型再怎么变,一个变量只装一样东西。可现实里的数据几乎从来不是孤零零一个值:一个点是 `x` 和 `y` 两个坐标、一个学生是学号加姓名加成绩、一个网络包是包头加若干字段加载荷。你当然可以用三五个散装变量去凑(`int id; char name[32]; double score;`),可一旦要把「这个学生」整体传给一个函数、存进一个数组、从文件里读写——散装变量就抓瞎了,你没法把它们当一个东西搬来搬去。

C 给的答案就是**结构体**(struct):把多个、还可能是不同类型的字段,打包成一个整体,起一个名字,之后就能当一个值来传递、存储、操作。这一章我们先把结构体本身搞通(怎么定义、初始化、用 `.` 和 `->` 访问字段),再把它和内存扯上关系——你会看到编译器在你毫不知情时往字段之间塞了一堆叫「填充字节」(padding)的东西,让 `sizeof(struct)` 远比你以为的大,这是写 C 谁都要踩的一坑。最后顺带把和结构体形影不离的三个小伙伴——**联合**(union)、**枚举**(enum)、**位域**(bitfield)——一次讲清,它们在阶段 3 链表/树、阶段 5 系统编程、以及任何一块 MCU 的头文件里都随处可见。

## 结构体基础:定义、初始化、`.` 和 `->`

定义一个结构体,用的是 `struct` 关键字加一对花括号,把字段一个个列在里面(§6.7.2.1)。结尾那个分号千万别漏——漏了它编译器报的错往往指向下一行,让你一头雾水找半天。一个最小的点(`Point`)真跑一遍,把定义、初始化、访问字段、结构体指针、结构体数组、还有链表节点惯用法一口气覆盖:

```c
#include <stdio.h>

/* 命名一个 struct 标签:用变量时得带 struct 前缀 */
struct PointTag {
    int x;
    int y;
};

/* typedef 给 struct 起别名,用起来不用写 struct */
typedef struct {
    int x;
    int y;
} Point;

/* 链表节点惯用法:typedef 和 struct 标签同名,自引用用 struct Node* */
typedef struct Node {
    int data;
    struct Node* next; /* 这里还见不到 Node,只能写 struct Node* */
} Node;

int main(void) {
    /* 顺序初始化:按字段定义顺序填 */
    struct PointTag a = {3, 4};
    /* 指定初始化器(C99):.字段 = 值,顺序无关、未填的清零 */
    Point b = {.y = 10, .x = 20};
    Point zero = {0}; /* 全部清零 */

    printf("a = (%d, %d)\n", a.x, a.y);
    printf("b = (%d, %d)\n", b.x, b.y);
    printf("zero = (%d, %d)\n", zero.x, zero.y);

    /* 字段访问:变量名用 . */
    b.x = 99;
    printf("b.x 改后 = %d\n", b.x);

    /* 结构体指针:用 -> 箭头访问字段 */
    Point* p = &b;
    p->y = 88; /* 等价于 (*p).y = 88 */
    printf("p->y 改后 = %d, b.y = %d\n", p->y, b.y);

    /* 结构体数组 */
    Point pts[3] = {{1, 1}, {2, 2}, {3, 3}};
    for (int i = 0; i < 3; i++) {
        printf("pts[%d] = (%d, %d)\n", i, pts[i].x, pts[i].y);
    }

    /* typedef struct Node 自引用:搭一个 1 -> 2 -> 3 的小链表 */
    Node n3 = {3, NULL};
    Node n2 = {2, &n3};
    Node n1 = {1, &n2};
    Node* head = &n1;
    printf("链表: ");
    for (Node* cur = head; cur != NULL; cur = cur->next) {
        printf("%d ", cur->data);
    }
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra struct_basic.c -o sb && ./sb
a = (3, 4)
b = (20, 10)
zero = (0, 0)
b.x 改后 = 99
p->y 改后 = 88, b.y = 88
pts[0] = (1, 1)
pts[1] = (2, 2)
pts[2] = (3, 3)
链表: 1 2 3 
```

这里有几个要点揉在一起,我们一个个拆。先看**两种声明方式**:`struct PointTag { ... };` 定义了一个带标签(tag)的结构体类型,之后声明变量得老老实实写 `struct PointTag a;`(标签就是花括号前面那个名字);而 `typedef struct { ... } Point;` 直接给这个匿名结构体起了个别名 `Point`,之后写 `Point b;` 就行,清爽得多。两种在功能上等价,工程里 `typedef` 的写法更普遍——你翻任何一块 MCU 的 SDK,满眼都是 `typedef struct { ... } XxxConfig;`。

但这两种写法在**自引用**时会分叉,这是写链表/树这种「节点里有个指针指向同类型节点」的数据结构时必踩的一个点。看上面那个 `Node`:结构体里有个 `next` 指针,指向「另一个 `Node`」。可问题来了——`typedef struct Node { ... } Node;` 这一行的 `Node` 别名,要到这一行**结束**(那个分号写完)才生效;而 `next` 字段的声明在这一行**中间**,这时候 `Node` 这个名字编译器还没看见。所以 `next` 的类型只能写成 `struct Node* next;`(用还没结束的标签名,标签在花括号一打开就可见了),绝不能写 `Node* next;`——写后者编译器会一脸懵地问你「`Node` 是啥」。这是链表节点的标准写法:标签和 typedef 同名(`typedef struct Node { ...; struct Node* next; } Node;`),自引用用 `struct Node*`,外部用 `Node`,两不耽误。阶段 3 第 1 章单链表全靠这一招。

初始化这块,**指定初始化器**(designated initializer,§6.7.9p6,我们第 10 章数组那里见过 `[2]=9` 的数组版本)是 C99 给的大礼:`Point b = {.y = 10, .x = 20};` 想填哪个字段写哪个、顺序随便、没填的字段自动清零。比起老式的顺序初始化 `Point b = {20, 10};`(必须记位置、结构体改字段顺序就全得跟着改),它自文档化、不依赖字段顺序、还顺带帮你清零——现代 C 代码里只要能写就优先写它。`Point zero = {0};` 则是「全部清零」的惯用法(部分初始化、剩下的自动 0,§6.7.9p21),第 10 章数组见过一样的招。

字段访问靠两个运算符分庭抗礼:变量名直接访问用 `.`(点),指针访问用 `->`(箭头)。`p->y` 就是 `(*p).y` 的语法糖——因为 `.` 的优先级比 `*` 高,写 `*p.y` 会被理解成 `*(p.y)`,所以要么老实写 `(*p).y`、要么用箭头 `p->y`,后者才是你天天会写的样子。**只要函数参数是结构体指针,你几乎必定在用 `->`**——传指针而不是传整个结构体,既能省掉一笔不小的拷贝开销,又能让函数改调用者的数据,这一点我们阶段 2 第 3 章讲「用指针改调用者的变量」时已经反复强调过。`pts[3]` 那一段顺便演示了**结构体数组**:本质就是「数组的每个元素是一个结构体」,初始化时一层花括号套一层花括号,访问就 `pts[i].x`——后面阶段 3 写链表、树、哈希表时,节点数组就是这么组织的。

## 内存对齐与填充:同一个 struct,字段顺序不同,sizeof 居然不一样

到这儿事情就开始有意思了。先问你一个问题:下面这个结构体占多少字节?

```c
typedef struct {
    char c;  /* 1 字节 */
    int  i;  /* 4 字节 */
    char d;  /* 1 字节 */
} A;        /* 直觉 1+4+1=6,实际? */
```

直觉上 1 + 4 + 1 = 6 字节,对吧?但真跑一遍你会大吃一惊——`sizeof(A)` 是 **12**。那多出来的 6 个字节去哪了?答案是编译器在字段之间和末尾**塞了填充字节**(padding)。我们把同一个结构体的字段重排一下,真跑对比:

```c
#include <stddef.h> /* offsetof */
#include <stdio.h>

/* 字段顺序乱:char / int / char */
typedef struct {
    char c; /* 1 字节 */
    int i;  /* 4 字节 */
    char d; /* 1 字节 */
} A;        /* 直觉 1+4+1=6,实际? */

/* 字段重排:int 放前、两个 char 挤后面 */
typedef struct {
    int i;
    char c;
    char d;
} B;

/* 三种类型混排:更夸张 */
typedef struct {
    char a;
    char* p; /* 8 字节 */
    char b;
    double z; /* 8 字节 */
    char c;
} C;

int main(void) {
    printf("sizeof(A) = %zu\n", sizeof(A));
    printf("  A.c offset = %zu\n", offsetof(A, c));
    printf("  A.i offset = %zu\n", offsetof(A, i));
    printf("  A.d offset = %zu\n", offsetof(A, d));

    printf("sizeof(B) = %zu\n", sizeof(B));
    printf("  B.i offset = %zu\n", offsetof(B, i));
    printf("  B.c offset = %zu\n", offsetof(B, c));
    printf("  B.d offset = %zu\n", offsetof(B, d));

    printf("sizeof(C) = %zu\n", sizeof(C));
    printf("  C.a offset = %zu\n", offsetof(C, a));
    printf("  C.p offset = %zu\n", offsetof(C, p));
    printf("  C.b offset = %zu\n", offsetof(C, b));
    printf("  C.z offset = %zu\n", offsetof(C, z));
    printf("  C.c offset = %zu\n", offsetof(C, c));

    /* 对照:字段大小之和 vs 实际 sizeof */
    printf("--- 字段之和 vs sizeof ---\n");
    printf("A 字段和 = %zu, sizeof = %zu, 浪费 %zu 字节\n",
           sizeof(char) + sizeof(int) + sizeof(char), sizeof(A),
           sizeof(A) - (sizeof(char) + sizeof(int) + sizeof(char)));
    printf("B 字段和 = %zu, sizeof = %zu, 浪费 %zu 字节\n",
           sizeof(int) + sizeof(char) + sizeof(char), sizeof(B),
           sizeof(B) - (sizeof(int) + sizeof(char) + sizeof(char)));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra alignment.c -o al && ./al
sizeof(A) = 12
  A.c offset = 0
  A.i offset = 4
  A.d offset = 8
sizeof(B) = 8
  B.i offset = 0
  B.c offset = 4
  B.d offset = 5
sizeof(C) = 40
  C.a offset = 0
  C.p offset = 8
  C.b offset = 16
  C.z offset = 24
  C.c offset = 32
--- 字段之和 vs sizeof ---
A 字段和 = 6, sizeof = 12, 浪费 6 字节
B 字段和 = 6, sizeof = 8, 浪费 2 字节
```

**A 和 B 字段一模一样、只是顺序不同,A 是 12 字节、B 是 8 字节**——A 白白浪费了一半空间。这件事的根,在处理器访问内存的方式上。CPU 访问内存不是一个字节一个字节读的,它更喜欢按 2、4、8 字节的边界来——一个 `int`(4 字节)如果放在地址是 4 的倍数的地方,CPU 一次就能读出来;要是它不幸跨了一个 4 字节边界(比如放在地址 1),CPU 可能得分两次读、再拼起来,性能打折;某些老架构(像 ARM7TDMI 那种)更狠,直接抛硬件异常给你看。所以编译器为了性能(有些时候是为了正确性),会主动在字段之间塞 padding,让每个字段都落在它「喜欢」的地址上。

对齐规则其实就两条(§6.7.2.1 + §6.2.5p2 那一片)。**第一条**:每个字段的起始偏移,必须是它自己「对齐要求」的整数倍——`char` 对齐 1(任何地址都行)、`int` 对齐 4、指针和 `double` 在 64 位机上对齐 8,基本类型的对齐要求通常就等于它的大小。**第二条**:结构体的总大小,必须是它**最大成员对齐要求**的整数倍——这是为了让结构体数组里**每一个**元素都能满足对齐(数组是紧密排列的,第 i 个元素的地址是 `base + i*sizeof(结构体)`,要是结构体本身不对齐到最大成员,下一个元素的字段就可能歪掉)。

照这两条拆 `A`:`c` 在偏移 0(对齐 1,任意地址都满足),占 1 字节;下一个是 `i`,对齐 4,但下一个可用偏移是 1、不是 4 的倍数,所以编译器塞 3 个字节 padding 让 `i` 从偏移 4 开始;`d` 在偏移 8(对齐 1,没问题);最后结构体最大对齐是 4(`int`),当前到偏移 9、得补到 12。所以 6 字节的实际数据、撑成了 12 字节,一半是 padding。再看重排后的 `B`:`i` 在偏移 0(天然对齐 4),`c` 在偏移 4、`d` 在偏移 5(都只要对齐 1),最后到偏移 6、补到 8(最大对齐 4 的倍数)——只浪费 2 字节。诀窍一句话:**对齐要求大的字段往前放、小的往后挤**,把 padding 摊到最少。更夸张的是 `C`:它有 `char*` 和 `double`(都 8 字节对齐),所以每个 `char` 后面都塞了 7 个字节 padding,字段和才 19 字节、`sizeof` 撑到 40——这就是为什么在内存敏感的场合(嵌入式、大规模结构体数组),养成「按对齐从大到小排字段」的习惯能省下真金白银。

标准库给的 `offsetof` 宏(§7.19,定义在 `<stddef.h>`)是调试对齐问题的利器——它精确告诉你某个字段在结构体里的字节偏移。上面真跑里 `A.i offset = 4`、`A.d offset = 8` 一眼看穿 padding 在哪儿。设计二进制协议帧、排查「为什么 `sizeof` 跟我想的不一样」时,写完结构体就 `offsetof` 打一遍是值得养成的习惯。

> 想亲手改字段顺序、看 `sizeof` 怎么跳?试试下面这个(点「运行」就行)。把 `char c` 挪到 `int` 后面、删掉某个字段、或者加一个 `double`,每次点运行看 `sizeof` 和 `offsetof` 怎么变——这就是「字段顺序影响内存占用」最直白的演示:

<OnlineCompilerDemo
  title="亲手玩:字段顺序改 sizeof"
  description="struct A{char;int;char} = 12 字节、struct B{int;char;char} = 8 字节——字段一样、顺序不同,sizeof 差一半。改字段顺序、加字段、删字段,看 sizeof 和 offsetof 怎么跳,亲手感受编译器在字段之间塞的 padding。"
  allow-run="true"
  run-options="-std=c11 -Wall -Wextra"
  sourcePath="/demos/struct_alignment.c"
/>

## C11 的对齐控制:`_Alignas` 与 `alignof`

C11 之前想手动控制对齐,只能靠编译器扩展(GCC 的 `__attribute__((aligned(n)))`、MSVC 的 `__declspec(align(n))`)。C11 终于把它标准化了:`_Alignas` 强制对齐、`_Alignof` 查询对齐,配上 `<stdalign.h>` 里更顺手的宏别名 `alignas` / `alignof`(§6.7.5 对齐说明符、§7.15 `<stdalign.h>`)。真跑一遍:

```c
#include <stdalign.h> /* alignas / alignof 宏 */
#include <stdio.h>

typedef struct {
    char c;
    int i;
    char d;
} Natural; /* 自然对齐:最大成员 int 对齐 4 */

int main(void) {
    /* alignof 查类型的对齐要求(标准用法,接类型名) */
    printf("alignof(char)   = %zu\n", alignof(char));                               /* 1 */
    printf("alignof(int)    = %zu\n", alignof(int));                                /* 4 */
    printf("alignof(double) = %zu\n", alignof(double));                             /* 8 */
    printf("alignof(Natural)= %zu  (struct 取最大成员的对齐)\n", alignof(Natural)); /* 4 */
    printf("---\n");

    /* _Alignas 写在变量声明里,把变量整体对齐拔高 */
    Natural plain;            /* 自然对齐 4 */
    _Alignas(16) Natural n16; /* 拔到 16 */
    _Alignas(32) char buf[4]; /* 缓冲区拔到 32(DMA 常见要求) */

    printf("plain 的地址 %% 4  == %lu\n", (unsigned long)&plain % 4);
    printf("n16  的地址 %% 16 == %lu  (n16 起始地址被按 16 对齐)\n", (unsigned long)&n16 % 16);
    printf("buf  的地址 %% 32 == %lu  (DMA 缓冲区按 32 对齐)\n", (unsigned long)buf % 32);
    printf("---\n");

    /* _Alignas 能拔高不能压低:_Alignas(2) int x; 会编译失败
       (int 自然对齐 4,不能降到 2) —— 真坑:别以为 alignas 万能。
       想要「更紧」的结构只能靠字段重排或 __attribute__((packed)),
       alignas 没法把已有类型的对齐往下压。 */
    printf("alignas 只能拔高,不能压低(详见正文编译报错)\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra alignas.c -o aa && ./aa
alignof(char)   = 1
alignof(int)    = 4
alignof(double) = 8
alignof(Natural)= 4  (struct 取最大成员的对齐)
---
plain 的地址 % 4  == 0
n16  的地址 % 16 == 0  (n16 起始地址被按 16 对齐)
buf  的地址 % 32 == 0  (DMA 缓冲区按 32 对齐)
---
alignas 只能拔高,不能压低(详见正文编译报错)
```

`alignof(Natural)` 是 4,印证了上面那条规则——结构体的对齐等于它最大成员的对齐(`Natural` 里 `int` 对齐 4)。`_Alignas(16) Natural n16;` 把变量 `n16` 的对齐从自然 4 拔到 16,真跑出来它的地址确实是 16 的倍数;`_Alignas(32) char buf[4];` 把一个缓冲区拔到 32 字节对齐——这种写法在嵌入式里给 DMA 准备缓冲区时极其常见(DMA 经常要求缓冲区起始地址是 16/32 字节对齐的,不然传输会出错)。

但这儿有个真坑,我一开始就栽了一下:**`_Alignas` 只能拔高、不能压低**。我以为写个 `_Alignas(2) int x;` 能把 `int` 的对齐从自然的 4 压到 2(想要更紧凑的布局),结果 gcc 当场报错:

```c
int main(void) {
    _Alignas(2) int x; /* int 自然对齐 4,想压到 2 → 编译错误 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra -c alignas_shrink.c
alignas_shrink.c: In function 'main':
alignas_shrink.c:2:21: error: '_Alignas' specifiers cannot reduce alignment of 'x'
    2 |     _Alignas(2) int x; /* int 自然对齐 4,压到 2 → 编译错误 */
      |                     ^
```

clang 一样拦:`error: requested alignment is less than minimum alignment of 4 for type 'int'`。所以别指望 `alignas` 帮你把结构体变紧——它只能往大了拔。想要「取消所有 padding、字段紧挨着排」(典型场景:二进制通信协议帧,字节流里没有 padding),得靠编译器扩展 `__attribute__((packed))`,但代价是访问未对齐字段在某些架构上会慢、甚至直接 fault(ARM7TDMI 之类)。更稳的做法是:通信层用 packed 结构体解析原始字节、立刻转成对齐的内部结构体用,解析和业务分开——这条经验嵌入式工程里几乎是标配。

## 联合 union:同一块内存,不同时间当不同类型

结构体是每个字段各占一块内存,联合(union,§6.7.2.5)正好反过来——**所有成员共享同一块起始地址相同的内存**。它的大小等于「最大那个成员」的大小(再按最大成员对齐补齐)。一个 `int`/`float` 的联合真跑一遍,你就懂它怎么「变戏法」:

```c
#include <stdio.h>

/* union:所有成员共享同一块内存 */
typedef union {
    int i;   /* 4 字节 */
    float f; /* 4 字节 */
} IntOrFloat;

/* 大小 = 最大成员(可能补齐到最大成员的对齐) */
typedef union {
    char c; /* 1 字节 */
    int i;  /* 4 字节 */
} Mix;      /* sizeof = 4 */

int main(void) {
    printf("sizeof(IntOrFloat) = %zu\n", sizeof(IntOrFloat)); /* 4 */
    printf("sizeof(Mix)        = %zu\n", sizeof(Mix));        /* 4 */

    IntOrFloat u;
    u.f = 3.14f; /* 当 float 写 */
    printf("当 float 读: f = %f\n", u.f);
    /* 同一块内存当 int 读:看 float 的位模式(类型双关) */
    printf("当 int 读:   i = %d (0x%08X)\n", u.i, (unsigned)u.i);

    /* 反过来:写 int 再读 float */
    u.i = 0x4048F5C3; /* 这是 3.14f 的 IEEE754 位模式 */
    printf("写 0x4048F5C3 后读 float: f = %f\n", u.f);

    /* 同址实锤:&u.i 和 &u.f 是同一个地址 */
    printf("&u.i = %p\n", (void*)&u.i);
    printf("&u.f = %p\n", (void*)&u.f);

    /* 坑:同一时刻只有「最后一次写的成员」有意义 */
    u.i = 42;
    printf("写 u.i=42 后,u.f = %f (这个 float 毫无意义,位模式是 42 的)\n", u.f);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra union_demo.c -o un && ./un
sizeof(IntOrFloat) = 4
sizeof(Mix)        = 4
当 float 读: f = 3.140000
当 int 读:   i = 1078523331 (0x4048F5C3)
写 0x4048F5C3 后读 float: f = 3.140000
&u.i = 0x7fff679fcfe4
&u.f = 0x7fff679fcfe4
写 u.i=42 后,u.f = 0.000000 (这个 float 毫无意义,位模式是 42 的)
```

`sizeof(IntOrFloat) = 4`——`int` 和 `float` 各 4 字节,共享同一块,所以联合总共 4 字节(不是 8)。最震撼的是那一行 `u.f = 3.14f;` 之后读 `u.i` 得到 `1078523331`,写成十六进制正好是 `0x4048F5C3`——这就是 `3.14f` 在 IEEE 754 浮点表示下的真实位模式(符号 1 位 + 指数 8 位 + 尾数 23 位,合起来正好这 32 个比特)。换句话说,内存里那 4 个字节一直没变,你当 `float` 读它就解释成 `3.14`、当 `int` 读它就解释成 `1078523331`,**值不一样是因为解读方式不一样,字节是同一份**。`&u.i` 和 `&u.f` 打印出来是同一个地址(`0x7fff679fcfe4`),就是这件事的实锤——这个地址每次运行因 ASLR 而不同,但两个一定相等。

这种「同一块内存换着类型读」的手法叫**类型双关**(type punning)。严格按 C 标准读,「写了一个成员再读另一个」原则上属于未定义行为,但 C99 起标准专门留了个口子(在联合那条附注里),允许通过联合做类型双关——所以你在无数 C 工程里能看到这种写法,而它跑得稳稳的。用途很实在:看浮点数的二进制位、把一个 32 位寄存器既当整体 `uint32_t` 又按位域拆开看、节省内存(同一块内存在不同时刻装不同类型的数据)。但坑也很明确——最后一行 `u.i = 42;` 之后读 `u.f`,得到一个毫无意义的 `0.000000`:因为这块内存现在的位模式是整数 42 的(`0x0000002A`),你硬当 `float` 解释就是个很小的数。**联合在同一时刻只有一个成员是「有意义」的,你得自己记住现在写的是哪个**——编译器不会帮你检查。所以单独的联合用处有限,真正发挥威力是和结构体、枚举组合成「tagged union」(标签联合体):加一个 `enum` 标签记下「当前存的是哪种类型」,再配 `switch` 按标签读,这就是 C 实现「一个变量多种类型」(多态)的经典手法。

## 枚举 enum:给整数起名字

枚举(§6.7.2.2)让你给一组相关的整数起有意义的名字,语法很轻:

```c
#include <stdio.h>

/* enum:给整数起名字,默认从 0 递增 */
typedef enum { RED, GREEN, BLUE } Color; /* RED=0, GREEN=1, BLUE=2 */

/* 自定义起始值 */
typedef enum {
    TEN = 10,
    ELEVEN, /* 11 */
    TWELVE  /* 12 */
} FromTen;

int main(void) {
    Color c = GREEN;
    printf("RED=%d GREEN=%d BLUE=%d\n", RED, GREEN, BLUE);
    printf("c = %d\n", c);
    printf("TEN=%d ELEVEN=%d TWELVE=%d\n", TEN, ELEVEN, TWELVE);

    /* enum 本质是 int:可以把非枚举值赋进去(类型安全漏洞) */
    Color bad = 42;
    printf("bad = %d (编译器不拦,但 42 不是任何 Color 值)\n", bad);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra enum_basic.c -o eb && ./eb
RED=0 GREEN=1 BLUE=2
c = 1
TEN=10 ELEVEN=11 TWELVE=12
bad = 42 (编译器不拦,但 42 不是任何 Color 值)
```

枚举值默认从 0 开始递增(`RED=0, GREEN=1, BLUE=2`),你可以用 `= 值` 显式指定起点,之后那个继续递增(`TEN=10, ELEVEN=11, TWELVE=12`)。比起满代码撒 `0/1/2` 这种「魔法数字」,`Color c = GREEN;` 自文档化、读起来一眼就懂——这就是枚举最大的价值。

但 C 的枚举有个让人又爱又恨的特点,你必须知道:**枚举值本质上就是 `int`**(§6.7.2.2p3,标准说枚举常量的类型是 `int`、枚举变量能装下枚举值的某个整数类型)。这意味着你完全可以把一个不属于任何枚举常量的整数塞进去,编译器一声不吭——上面真跑里最后那行 `Color bad = 42;` 就这么合法通过了、`bad` 打出 `42`,可 `42` 压根不是 `RED/GREEN/BLUE` 里的任何一个。C++ 后来搞了个 `enum class` 把这个口子堵了,C 没这福气。所以实践中常见的补救:给枚举末尾加个 `*_COUNT` 计数哨兵(比如 `BLUE` 之后再加一行 `COLOR_COUNT`,它的值正好是枚举项数),拿它做范围检查 `if (c >= 0 && c < COLOR_COUNT)`;或者干脆把合法校验挡在运行期。靠类型系统是挡不住的,这是 C 枚举的宿命。

## 位域 bitfield:按位分配字段

位域(§6.7.2.1p4 起)让你在结构体里**以位为单位**分配字段,语法是字段名后跟冒号和位数:

```c
#include <stdio.h>

/* 位域:按位分配字段,基类型 unsigned(4 字节) */
typedef struct {
    unsigned a : 3; /* 3 位:可存 0..7 */
    unsigned b : 5; /* 5 位:可存 0..31 */
} Bits;             /* 共 8 位,但落在 unsigned 存储单元里 */

/* 位域基类型用 unsigned char(1 字节单元) */
typedef struct {
    unsigned char a : 3;
    unsigned char b : 5;
} BitsChar; /* 8 位正好 1 字节 */

int main(void) {
    printf("sizeof(Bits)     = %zu (基类型 unsigned,8 位落在 4 字节单元里)\n", sizeof(Bits));
    printf("sizeof(BitsChar) = %zu (基类型 unsigned char,8 位正好 1 字节)\n", sizeof(BitsChar));

    Bits x;
    x.a = 5;  /* 5 在 0..7 范围内 */
    x.b = 20; /* 20 在 0..31 范围内 */
    printf("x.a = %u, x.b = %u\n", x.a, x.b);

    /* 坑:3 位存不下 9,被截断(编译器还会警告) */
    x.a = 9; /* 9 = 0b1001,只留低 3 位 0b001 = 1 */
    printf("x.a = 9 后读到 %u (3 位装不下,截断成 1)\n", x.a);
    x.b = 40; /* 40 = 0b101000,5 位装 0b01000 = 8 */
    printf("x.b = 40 后读到 %u (5 位装不下,截断成 8)\n", x.b);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra bitfield.c -o bf
bitfield.c:25:11: warning: unsigned conversion from 'int' to 'unsigned char:3' changes value from '9' to '1' [-Woverflow]
   25 |     x.a = 9; /* 9 = 0b1001,只留低 3 位 0b001 = 1 */
      |           ^
bitfield.c:27:11: warning: unsigned conversion from 'int' to 'unsigned char:5' changes value from '40' to '8' [-Woverflow]
   27 |     x.b = 40; /* 40 = 0b101000,5 位装 0b01000 = 8 */
      |           ^~
$ ./bf
sizeof(Bits)     = 4 (基类型 unsigned,8 位落在 4 字节单元里)
sizeof(BitsChar) = 1 (基类型 unsigned char,8 位正好 1 字节)
x.a = 5, x.b = 20
x.a = 9 后读到 1 (3 位装不下,截断成 1)
x.b = 40 后读到 8 (5 位装不下,截断成 8)
```

`unsigned a : 3;` 给 `a` 分 3 个比特(能存 0..7),`unsigned b : 5;` 分 5 个比特(能存 0..31),合起来 8 位——这就是位域的卖点:**精确到位的紧凑存储**,用来存标志位、协议头标志、硬件寄存器位段特别合适。3 位存 5 没问题,但 `x.a = 9` 就翻车了:`9 = 0b1001` 有 4 位,3 位装不下,编译器只留低 3 位 `0b001 = 1`,所以读回来是 `1`。gcc 和 clang 都会警告(`-Woverflow` / `-Wbitfield-constant-conversion`)——这个坑编译器能帮你抓到一部分,但你得开 `-Wall` 才看得到。

这里有个一开始真把我搞懵的事:`Bits` 和 `BitsChar` 字段一模一样(都是 `a:3, b:5`、共 8 位),但 `sizeof(Bits)` 是 **4**、`sizeof(BitsChar)` 是 **1**。差别全在**基类型**——位域字段是装在「以基类型为单元」的存储里的:`unsigned` 是 4 字节,所以哪怕你只用 8 位,编译器也按 4 字节单元给你开;`unsigned char` 是 1 字节,8 位正好填满 1 字节。更深的坑还在后头:C 标准对位域的几个关键细节**没有规定**——位域是从低位往高位排还是反过来、跨存储单元边界时怎么处理、有符号位域怎么符号扩展,这些全交给编译器实现(§6.7.2.1 里明确写了「实现定义」)。所以位域的**可移植性很差**:你在 gcc/x86 上写的位域映射,换到另一个编译器或架构,位的排列可能完全不一样。这就是为什么很多嵌入式项目(尤其跨平台库)宁愿用第 6 章那套手写位运算掩码(`#define CTRL_ENABLE (1U<<0)` + `|= &= ~`),也不碰位域——位运算可移植、不依赖编译器;位域省事、但只在你这棵编译器树上靠得住。真要用位域映射硬件寄存器,认准厂商给的标准头文件(像 STM32 的 CMSIS),那里的位域结构是厂商验证过和平台一致的,自己手写别瞎来。

## 柔性数组成员 FAM(C99):一次 malloc,装下结构体加可变尾巴

最后一个,柔性数组成员(Flexible Array Member,§6.7.2.1p18,C99 引入)。它允许你在结构体**末尾**放一个大小不写的数组 `data[]`,然后一次 `malloc` 把结构体和后面那段「可变长度的尾巴数据」一起分配。一个最小的缓冲区(`Buf`)真跑:

```c
#include <stdio.h>
#include <stdlib.h>

/* 柔性数组成员(C99):末尾的不定长数组 data[] */
typedef struct {
    int len;
    int data[]; /* 不占 struct 的 sizeof,只是一段「跟着结构体的尾巴内存」 */
} Buf;

int main(void) {
    int n = 5;
    /* 一次 malloc 把结构体 + n 个 int 的尾巴一起分配 */
    Buf* b = malloc(sizeof(Buf) + n * sizeof(int));
    if (b == NULL) {
        return 1;
    }
    b->len = n;
    for (int i = 0; i < n; i++) {
        b->data[i] = i * i; /* 0 1 4 9 16 */
    }

    printf("sizeof(Buf) = %zu (不含 data,只是一个 int 的 len)\n", sizeof(Buf));
    printf("实际分配 = %zu 字节 (sizeof(Buf) + %d*%zu)\n", sizeof(Buf) + (size_t)n * sizeof(int), n,
           sizeof(int));
    printf("b->len = %d\n", b->len);
    printf("b->data: ");
    for (int i = 0; i < b->len; i++) {
        printf("%d ", b->data[i]);
    }
    printf("\n");

    free(b); /* 一次 free 连结构体带尾巴一起释放 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra fam.c -o fam && ./fam
sizeof(Buf) = 4 (不含 data,只是一个 int 的 len)
实际分配 = 24 字节 (sizeof(Buf) + 5*4)
b->len = 5
b->data: 0 1 4 9 16 
```

`int data[];` 这个写法是柔性数组成员——它是「不完整类型的数组」,在结构体里**不占 `sizeof`**(`sizeof(Buf) = 4`,只是 `len` 那个 `int`)。它真正的含义是:「这个结构体的末尾,可能跟着一段连续的 `int` 内存,具体多少个由你 `malloc` 时决定」。用法就是那一行 `malloc(sizeof(Buf) + n * sizeof(int))`——结构体本体的字节数,加上 `n` 个 `int` 的尾巴,一次分配到位;之后 `b->data[i]` 就能像普通数组那样访问,`b->data[0]` 紧挨着 `b->len` 后面。`free` 也只要一次,连结构体带尾巴一起释放,不会漏。

FAM 解决的是「结构体带一段长度可变的尾巴数据」这个需求——网络包(定长包头 + 变长载荷)、变长字符串、动态数组,在没有 FAM 的年代,人们要么分两次 `malloc`(结构体一次、数据一次,释放也得两次、容易漏)、要么搞个叫「struct hack」的脏技巧(在结构体末尾放个 `data[1]`、再多分配点空间,靠越界访问凑),后者是**未定义行为**。C99 的 FAM 才是干净、标准的做法。阶段 3 写动态数据结构、阶段 5 写协议解析时,你会反复用到它。有一点务必记住:**含柔性数组成员的结构体不能按值传递或整体拷贝**——因为 `sizeof` 不知道尾巴多大,`=` 或传参会只拷 `sizeof(Buf)` 那点(尾巴丢光)。FAM 结构体永远只能通过指针操作。

## 小结

C 把「一组相关字段」打包成一个整体的工具是结构体(§6.7.2.1,`typedef struct { ... } Name;`),声明变量后用 `.` 访问字段、用指针配 `->`(箭头,`(*p).` 的语法糖)改字段;初始化优先用 C99 指定初始化器 `.field = value`(顺序无关、未填字段自动清零、自文档化);自引用的链表/树节点有标准写法 `typedef struct Node { ...; struct Node* next; } Node;`——结构体内部自引用必须用 `struct Node*`(typedef 别名这一行还没生效),外部才能用 `Node`。结构体最容易被忽视的真相是**内存对齐与填充**:每个字段要落在自己「对齐要求」(基本类型通常等于其大小)的整数倍偏移上,结构体总大小还得是最大成员对齐的整数倍——于是同一组字段、顺序不同,`sizeof` 能差出 50%(`struct A{char;int;char}` 字段和 6 但 `sizeof` 12、重排成 `struct B{int;char;char}` 就 8);`offsetof`(`<stddef.h>`,§7.19)能精确看到每个字段的偏移,排查对齐问题全靠它;诀窍是「对齐大的字段往前放」。C11 的 `_Alignas`(变量声明里写,如 `_Alignas(16) Natural n16;`)能把变量对齐拔高(给 DMA 缓冲区对齐到 16/32 常见),`alignof`/`_Alignof` 查类型对齐——但 `alignas` 只能拔高、不能压低(`_Alignas(2) int x` 直接编译失败),想取消 padding 得靠非标准的 `__attribute__((packed))`(代价:某些架构访问未对齐字段会慢或 fault)。联合 union(§6.7.2.5)让所有成员共享同一块内存、大小取最大成员,经典用法是「写 `float` 读 `int`」做类型双关看 IEEE 754 位模式(`3.14f` 的位模式是 `0x4048F5C3`=十进制 `1078523331`)——但同一时刻只有一个成员有意义,你得自己记住,所以联合常配 `enum` 标签做成 tagged union 实现 C 的「多态」。枚举 enum(§6.7.2.2)给整数起名字(`RED=0,GREEN=1...`,可 `=值` 自定义起点),但本质是 `int`(`Color bad=42` 合法通过、编译器不拦类型安全漏洞)。位域 bitfield(`unsigned a:3;`)按位分配、能紧凑存标志位,但基类型决定 `sizeof`(`unsigned` 基的 8 位要占 4 字节、`unsigned char` 基才 1 字节),且位排列顺序、跨单元边界等都是实现定义、可移植性差——跨平台代码宁可用第 6 章的手写位运算掩码。柔性数组 FAM(C99 §6.7.2.1p18,`struct Buf { int len; int data[]; };`)在结构体末尾留不定长数组,一次 `malloc(sizeof(Buf)+n*sizeof(int))` 把结构体和变长尾巴一起分配、一次 `free` 释放,是变长协议包/动态数据的标准做法(不能按值拷贝、只走指针)。到这里,阶段 1 的 C 语言基底就完整了——你不仅有了写出「能跑且知道为什么」的程序所需的全部地基,还拿到了进入阶段 3 数据结构(链表 `struct Node* next`、二叉树 `struct Node{left,right}`、动态数组 realloc+FAM)的入场券。下一阶段,我们把这些复合类型和指针算术、动态内存全拧到一起,开始造真正的数据结构。

## 参考资源

- ISO/IEC 9899:2011 §6.7.2.1(structure、union、enumeration 与 bit-field 的定义,含 p4 位域、p18 柔性数组成员 FAM)、§6.7.2.2(枚举 enum,p3 枚举常量类型为 int)、§6.7.2.5(联合 union 的内存共享语义)、§6.7.5(C11 `_Alignas` 对齐说明符)、§7.15(`<stdalign.h>` 的 `alignas`/`alignof` 宏别名)、§7.19(`<stddef.h>` 的 `offsetof` 宏)、§6.5.3.4(`sizeof` 运算符)
- K. N. King《C Programming: A Modern Approach》第 16 章 Structures、Unions、Enumerations(struct 定义/初始化/`->`、union 共享内存、enum 本质是 int、位域的可移植性陷阱)
- Robert C. Seacord《Effective C》第 5 章(struct 内存布局与对齐、`_Alignas`/`alignof`、柔性数组成员 FAM、`__attribute__((packed))` 的代价)
- 第 2 章:整型家族与 sizeof(`sizeof` 运算符、`size_t` 与 `%zu`)、第 6 章:位运算与移位(手写位运算掩码,位域的可移植替代)、第 10 章:数组(指定初始化器 `[2]=9`、结构体数组)、阶段2·第3章:用指针改调用者的变量(结构体指针 `->`、传指针省拷贝)、阶段2·第10章:复杂声明与 typedef(`typedef` 给类型起别名、右左法则)
- 阶段3·第1章:单链表(`typedef struct Node { ...; struct Node* next; } Node;` 的完整实战)、阶段3·第5章:动态数组(realloc 扩容,与 FAM 的对照)、阶段3·第6章:二叉树(`struct Node{left,right}` 的自引用)
