---
title: "动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)"
description: "阶段3·第 5 章。阶段1 第 10 章的数组把长度写死在编译期,这一章拆掉那道墙:动态数组 = 一块 malloc 来的连续内存 + capacity(容量) + size(已用),push 满了就 realloc 翻倍扩容。这就是 C++ std::vector、Python list、Java ArrayList 背后同一套内核。我们用 struct { int* data; size_t size; size_t capacity; } 把三个状态收进一个盒子,真跑 init/push/get/free 四件套:连续 push 10 个元素,眼看着 capacity 从 4 → 8 → 16 两次翻倍,打出来的轨迹正好揭示「分摊 O(1)」是怎么来的;扩容严格走阶段2 第 6 章的 realloc tmp 模式(失败时原指针不丢);越界访问单独用一段 ASan 实测对照——裸 v.data[size] 是 UB、ASan 报 heap-buffer-overflow READ,带边界检查的 vec_get 才是工程做法。全 gcc16+clang22 真跑,ASan 复核无泄漏。"
chapter: 3
order: 5
tags:
  - host
  - data-structures
  - memory
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "第 10 章:数组(下标、越界、a[i]≡*(a+i))"
  - "阶段2·第6章:动态内存入门(malloc/realloc 的 tmp 模式)、第7章:动态内存的坑(ASan)"
related:
  - "阶段3·第12章:算法复杂度(push 分摊 O(1))"
  - "阶段3·第9/10章:排序算法(动态数组当容器)"
---

# 动态数组:capacity/size、realloc 扩容、push 的分摊 O(1)

## 引言:数组那道墙

阶段1 第 10 章我们写数组时反复强调一句话:**长度在编译期定死**。`int a[10];` 一旦写下去,这块数组就只能装 10 个 `int`,多一个都塞不下——你想 `a[10] = ...` 就是越界、是未定义行为(§6.5.2.1)。可现实里数据量往往是运行期才知道的:读一个文件有多少行、收一个网络包有多长、用户要点多少次「添加」,这些编译期根本算不出来。数组那道「长度写死」的墙,卡的就是这种场景。

这一章我们拆掉这道墙。动态数组(dynamic array,有的书叫 growable array)的思路朴素到只有一句话:**要装的东西可能变多,那就准备一块能换大的内存,装满了就拿一块更大的、把旧的搬过去**。它本质上还是数组——内存物理上连续、用下标 `data[i]` 访问——只不过那块内存是 `malloc` 来的(阶段2 第 6 章),长度不再写死在源码里,而是存在一个变量里、能随时 `realloc` 调整。

这套东西不是什么冷门构造,它是 C++ 的 `std::vector`、Python 的 `list`、Java 的 `ArrayList`、Rust 的 `Vec<T>` 背后**同一套内核**。学完这一章,你再去看任何一个高级语言的「可变长数组」,会发现它们的扩容策略、`size`/`capacity` 二元状态、分摊 O(1) 的 push——全是从我们这一章手搓的这个 `struct` 里长出来的。

## 三个状态收进一个盒子:capacity 与 size

要把动态数组写明白,第一步是把它的状态想清楚。一个朴素的做法是阶段2 第 6 章见过的:`int* a = malloc(n * sizeof(int));`,然后 `a[i]` 当数组用。可这块内存一旦 `malloc` 出来,它多大就是多大,你拿不到「现在装了几个、还能再装几个」这两条信息——除非自己额外存变量。一个真正能 push、能扩容的容器,手里必须同时攥着**三样东西**:

```c
typedef struct {
    int* data;       /* 指向 malloc 来的连续内存块 */
    size_t size;     /* 已用槽位数(下次 push 写 data[size]) */
    size_t capacity; /* 已开的容量(data 总共能放 capacity 个) */
} Vec;
```

`data` 是指针,指向堆上那块连续的 `int` 数组(阶段2 第 6 章的 `malloc`、阶段2 第 2 章的下标 `data[i] ≡ *(data+i)`);`size` 是**当前已经装了几个**;`capacity` 是**这块内存总共能装几个**。两者的差 `capacity - size`,就是「还能再 push 几个而不扩容」的余量。

这两个数一定要分开记,而且一定要取两个不同的名字——很多人新手期会偷懒只存一个 `size`,把容量隐含在 `malloc` 的参数里,结果 push 的时候就忘了「这还能不能再装」,要么越界写进别人的内存、要么莫名其妙地不敢扩容。`size` 和 `capacity` 的区分,是动态数组的命根子:看一眼 `data[size]` 安全不安全,等价于看一眼 `size < capacity` 成立不成立。

## 四件套:init / push / get / free

围绕这三个状态,我们要实现的最小可用集合是四个函数:**初始化、尾部追加、按下标读、释放**。先把骨架立起来,再一段段说为什么这么写。

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int* data;
    size_t size;
    size_t capacity;
} Vec;

/* 初始化:开 capacity 个槽,失败返回 0 */
int vec_init(Vec* v, size_t capacity) {
    v->data = malloc(capacity * sizeof(int));
    if (v->data == NULL) {
        return 0;
    }
    v->size = 0;
    v->capacity = capacity;
    return 1;
}

/* 扩容:capacity *= 2,用 tmp 模式(失败时原 data 仍有效) */
static int vec_grow(Vec* v) {
    size_t new_cap = v->capacity * 2;
    int* tmp = realloc(v->data, new_cap * sizeof(int));
    if (tmp == NULL) {
        return 0; /* 失败:v->data 不动,调用方继续可用 */
    }
    printf("  [grow] capacity %zu -> %zu\n", v->capacity, new_cap);
    v->data = tmp;
    v->capacity = new_cap;
    return 1;
}

/* push 到尾部:满了先扩容 */
int vec_push(Vec* v, int value) {
    if (v->size == v->capacity) {
        if (vec_grow(v) == 0) {
            return 0;
        }
    }
    v->data[v->size] = value;
    v->size++;
    return 1;
}

/* 取下标,越界返回 0、不写 *out */
int vec_get(const Vec* v, size_t index, int* out) {
    if (index >= v->size) {
        return 0;
    }
    *out = v->data[index];
    return 1;
}

void vec_free(Vec* v) {
    free(v->data);
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
}
```

`vec_init` 干两件事:`malloc` 一块初始容量(我们等会儿在 `main` 里传 `4`)的连续内存,再把 `size` 置 0、`capacity` 记下这块内存多大。`malloc` 的返回值必须判 NULL(阶段2 第 6 章的铁律,内存不够会返回空指针,不判就是埋雷),失败时函数返回 0 让调用方决定怎么办。`size` 初始化成 0 而不是 `capacity` 很关键——容量是「开了几个槽」,已用是「装了几个」,刚 init 时一个都没装。

`vec_push` 是动态数组的灵魂。它的逻辑只有一句:**`size` 撞到 `capacity` 了就先扩容,然后写到 `data[size]`、`size++`**。`data[size]` 这个写位置是有讲究的:`size` 既是「已用个数」、又恰好是「下一个空槽的下标」,因为下标从 0 开始数(阶段1 第 10 章见过 `a[i] ≡ *(a+i)`)。写完 `size++`,下一次 push 就会写到下一个槽。`vec_get` 同理,只是多了一道越界检查——`index >= size` 就拒绝,这个边界后面单独用一段实测说。

### 扩容的 tmp 模式:realloc 别写成一行

`vec_grow` 这段是整章最容易踩雷的地方,我们 deliberately 把它单拎出来。`realloc(ptr, new_size)` 的语义(ISO §7.22.3.5)是:把 `ptr` 指向的那块内存改成 `new_size` 大小,**它可能原地扩容、也可能另外找一块新内存把旧数据拷过去、然后释放掉旧块**——返回的是新地址,旧地址在 `realloc` 返回后就作废了。所以「realloc 必须接返回值」是阶段2 第 6 章钉过的铁律。

可这里还有一个更阴的坑:**`realloc` 是会失败的**(返回 NULL),如果你图省事写成 `v->data = realloc(v->data, ...);`,一旦失败,`v->data` 就被赋成了 NULL——而原来那块内存 `realloc` 并没有释放、你却**再也拿不到它的指针了**,既泄漏又把容器整个报废。所以工程上的标准写法是 **tmp 模式**:先拿一个临时指针 `tmp` 接 `realloc` 的返回值,判 NULL,成功了再把 `tmp` 赋回 `v->data`、更新 `capacity`。

```c
int* tmp = realloc(v->data, new_cap * sizeof(int));
if (tmp == NULL) {
    return 0; /* 失败:v->data 原封不动,调用方还能继续用旧的 */
}
v->data = tmp; /* 成功了才覆盖 */
v->capacity = new_cap;
```

这一段是阶段2 第 6 章 malloc/realloc 章直接搬过来的纪律,这里再强调一次:动态数组扩容失败不该把容器弄坏,让调用方拿到一个「扩容没成功但旧数据还在」的状态,远比「指针丢了、内存也泄漏了」体面。`size_t new_cap = v->capacity * 2` 这一行是扩容**倍数**的选择——这里选 2 倍,下一节我们用真跑数据看它为什么是「分摊 O(1)」的关键。

## 真跑:10 个元素、4 → 8 → 16 的扩容轨迹

光看代码不过瘾,我们写个 `main` 把它跑起来:初始容量给 4,连续 push 1 到 10,每一步打印 `size` 和 `capacity`,在 `vec_grow` 里也插一行打印,这样扩容的那几个瞬间我们能看得清清楚楚。

```c
int main(void) {
    Vec v;
    if (vec_init(&v, 4) == 0) {
        fprintf(stderr, "init failed\n");
        return 1;
    }
    printf("init: capacity=%zu\n", v.capacity);

    for (int i = 1; i <= 10; i++) {
        vec_push(&v, i);
        printf("push %2d -> size=%zu capacity=%zu\n", i, v.size, v.capacity);
    }

    printf("array:");
    for (size_t i = 0; i < v.size; i++) {
        printf(" %d", v.data[i]);
    }
    printf("\n");

    /* 越界访问演示:index == size 越界 */
    int out;
    int ok = vec_get(&v, v.size, &out);
    printf("vec_get(vec, size=%zu) -> ok=%d (越界被拦)\n", v.size, ok);

    vec_free(&v);
    printf("after free: size=%zu capacity=%zu\n", v.size, v.capacity);
    return 0;
}
```

把前面两段代码合到一个 `vec.c` 里,`gcc -std=c11 -Wall -Wextra` 编译跑一下:

```text
$ gcc -std=c11 -Wall -Wextra vec.c -o vec && ./vec
init: capacity=4
push  1 -> size=1 capacity=4
push  2 -> size=2 capacity=4
push  3 -> size=3 capacity=4
push  4 -> size=4 capacity=4
  [grow] capacity 4 -> 8
push  5 -> size=5 capacity=8
push  6 -> size=6 capacity=8
push  7 -> size=7 capacity=8
push  8 -> size=8 capacity=8
  [grow] capacity 8 -> 16
push  9 -> size=9 capacity=16
push 10 -> size=10 capacity=16
array: 1 2 3 4 5 6 7 8 9 10
vec_get(vec, size=10) -> ok=0 (越界被拦)
after free: size=0 capacity=0
```

(`clang -std=c11 -Wall -Wextra` 跑出来一字不差,这里就不重复贴了。)

这段输出值得逐行看。初始容量是 4,前 4 次 push(1 到 4)`size` 一步步追上 `capacity`,两者在 `push 4` 那一步撞上(`size=4 capacity=4`);第 5 次 push 时 `vec_push` 发现 `size == capacity`、调 `vec_grow`,打印出 `[grow] capacity 4 -> 8`,扩到 8 之后才把 5 写进去。然后又是 4 次 push 把 8 填满,第 9 次 push 再触发一次扩容,这次是 `[grow] capacity 8 -> 16`,之后 push 9、push 10 都落在 16 的容量里、不再扩容。最后打出来的数组 `1 2 3 4 5 6 7 8 9 10` 完整无缺——这两次扩容里哪怕发生过「搬家」(`realloc` 另找一块内存、把旧 8 个 int 拷过去),我们靠 `v->data = tmp` 这一句接住了新地址,数据一个没丢。

**就在这条 4 → 8 → 16 的轨迹里,藏着「push 的分摊 O(1)」这件事**。每次扩容要 `realloc`、可能拷一整块,看起来很贵;可扩容是「翻倍」的,容量从 4 涨到 16 只扩了 2 次,而这一路 push 进去了 10 个元素。把 10 次 push 加上 2 次扩容拷贝的总开销摊到 10 次 push 上,平均下来每次 push 的代价是个常数——这就是「分摊 O(1)」(amortized O(1))。如果改成每次只 `+1` 扩容(`new_cap = capacity + 1`),push n 个元素就要扩 n 次、总共拷 1+2+3+...+n 次,退化成 O(n²),那就崩了。翻倍(或 1.5 倍)是把分摊代价压成常数的关键选择,第 12 章讲算法复杂度时我们会再算一遍这笔账。

## free 不能省:ASan 复核无泄漏

动态数组的最后一步是把内存还回去。`vec_free` 三行:`free(v->data)` 释放堆上那块连续内存,然后把 `v->data` 置 NULL、`size`/`capacity` 归零。置 NULL 是阶段2 第 7 章见过的防悬垂套路——`free` 完的指针成了悬垂指针,如果之后有人误用 `v->data[i]` 就是 use-after-free(UB);置成 NULL 之后,哪怕误用也至少会在解引用 NULL 这一刻崩出来(阶段2 第 1 章,SEGV on address 0x0),而不是静悄悄读一段已经还给系统的内存。归零 `size`/`capacity` 是为了让这个 `Vec` 结构体处于一个干净的「未使用」状态,免得有人 `vec_free` 之后又去读 `v.size` 拿到一个没意义的旧值。

可这套说辞到底有没有落实,得靠工具来证明。阶段2 第 7 章我们用 ASan 一个一个抓过 use-after-free、double-free、堆越界、泄漏,这一章也照规矩复核一遍——给 `vec.c` 编个 ASan 版,跑一遍 push 10 个 + `vec_free`:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer vec.c -o vec_asan \
  && ./vec_asan; echo "exit=$?"
init: capacity=4
push  1 -> size=1 capacity=4
push  2 -> size=2 capacity=4
push  3 -> size=3 capacity=4
push  4 -> size=4 capacity=4
  [grow] capacity 4 -> 8
push  5 -> size=5 capacity=8
push  6 -> size=6 capacity=8
push  7 -> size=7 capacity=8
push  8 -> size=8 capacity=8
  [grow] capacity 8 -> 16
push  9 -> size=9 capacity=16
push 10 -> size=10 capacity=16
array: 1 2 3 4 5 6 7 8 9 10
vec_get(vec, size=10) -> ok=0 (越界被拦)
after free: size=0 capacity=0
exit=0
```

`exit=0`,而且整个输出里**没有** `ERROR: LeakSanitizer: detected memory leaks` 这一行——ASan 的 LeakSanitizer 在程序正常退出时会扫一遍堆,只要有任何一块 `malloc` 来的内存没被 `free`、就会打印这一句并让进程非零退出。它这次什么都没说,意思是 `vec_free` 把该还的都还了。(`clang -fsanitize=address` 跑出来同样干净。)为了确信 LeakSanitizer 这次不是「没开」,我们可以专门写一个故意不 `free` 的小程序跑一遍,它会立刻报 `Direct leak of 40 byte(s)`——也就是说工具是活的、它静默即代表真的没泄漏。

## 越界访问:UB、ASan 抓、还是边界检查

最后单独说越界。`vec_get` 那个 `if (index >= v->size) return 0;` 不是装饰——动态数组的下标访问和普通数组一样,越界是 UB(§6.5.2.1,阶段1 第 10 章实测过)。`main` 里我们故意用 `vec_get(&v, v.size, &out)` 试了一下「刚好取第 `size` 个」(也就是「已用范围之外的第 1 个」),`vec_get` 拦下来返回 `ok=0`,这是工程上的正确做法:**容器自己挡住越界、返回一个失败码让调用方处理**,而不是让 UB 流到外面。

为了让你直观看到「不做这一道检查会怎样」,我们再写个最小例子,直接对一块容量 4 的 `malloc` 数组读 `a[4]`——也就是裸访问,看看 UB 的真实嘴脸:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* a = malloc(4 * sizeof(int)); /* 容量 4 */
    if (!a)
        return 1;
    for (int i = 0; i < 4; i++) {
        a[i] = (i + 1) * 10; /* 填 10 20 30 40 */
    }
    printf("a[0..3] = %d %d %d %d\n", a[0], a[1], a[2], a[3]);
    printf("a[4] (越界,UB) = %d\n", a[4]); /* 容量之外,UB */
    free(a);
    return 0;
}
```

普通编译跑一下,UB 静悄悄的样子特别有欺骗性:

```text
$ gcc -std=c11 -Wall oob.c -o oob && ./oob; echo "exit=$?"
a[0..3] = 10 20 30 40
a[4] (越界,UB) = 0
exit=0
```

`a[4]` 印出来是 `0`、`exit=0`,看起来「正常」——但这正是 UB 最坑人的地方:它**不保证**打印 0,可能打印别的垃圾、可能崩、可能被优化掉,**任何结果都是合法的**,因为「越界读」在标准眼里压根没定义。这一份输出里恰好读到 0,只是因为 glibc 的 `malloc` 给出来的堆内存恰好是被清过零的页;换个时间、换个分配大小、换个编译器优化等级,可能就完全是另一个数。开 ASan 再跑,真凶现形:

```text
$ gcc -std=c11 -Wall -fsanitize=address -g oob.c -o oob_asan && ./oob_asan
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in main /tmp/cj/s3ch5/oob.c:12
0x... is located 0 bytes after 16-byte region [0x...,0x...)
SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/cj/s3ch5/oob.c:12 in main
```

(ASan 报错里的进程号、地址值、寄存器值每次跑都不一样,是 ASLR 和堆地址随机化在做事,但「错误类型 `heap-buffer-overflow`、`READ of size 4`、点出的源码行 `oob.c:12`、16 字节区域」这些信息是稳定的,我这里把会变的部分省略成 `...`。)

ASan 报 `heap-buffer-overflow`、`READ of size 4`,而且告诉你这一下读在「16 字节区域之后 0 字节」——那块 `malloc(4 * sizeof(int))` 拿到的内存是 16 字节(4 个 int),`a[4]` 读的位置正好是这块区域**之外的第 1 个字节**,被 ASan 的 redzone(阶段0 第 10 章见过)逮个正着。`oob.c:12` 就是那一行 `printf("a[4] ...")`。所以这一节的两条结论合起来就是:**裸下标访问越界是 UB,靠它「恰好不崩」是赌命;容器的职责是在边界处把 UB 拦下来,`vec_get` 的那道 `if (index >= v->size)` 不是可有可无的装饰**。

## 小结

动态数组拆掉的就是阶段1 第 10 章那道「长度编译期写死」的墙,它的全部秘密收在 `typedef struct { int* data; size_t size; size_t capacity; } Vec;` 这一个盒子里(ISO §6.2.5p20 数组、§6.5.2.1 下标):`data` 指向 `malloc` 来的连续堆内存(§7.22.3.4),`size` 是已用槽位数、`capacity` 是已开容量,两者的区分是动态数组的命根子——`size == capacity` 就是该扩容的信号、`size < capacity` 就是还能再 push 的余量。`vec_init` 开初始容量、`vec_push` 满了就先扩容再写 `data[size]`、`vec_get` 带边界检查、`vec_free` 释放堆内存并把结构体归零,这四件套合起来就是一个最小可用的 vector。扩容走严格的 **realloc tmp 模式**(§7.22.3.5):`int* tmp = realloc(v->data, new_cap*sizeof(int)); if(tmp){v->data=tmp; v->capacity=new_cap;}`,先存临时变量再覆盖——直接 `v->data = realloc(v->data, ...)` 一旦失败就既泄漏又丢指针,这是阶段2 第 6 章钉过的坑、本章再次落实。倍数选 2 倍不是随便挑的:真跑连续 push 10 个,看 `capacity` 走 4 → 8 → 16 两次翻倍,这条轨迹正好揭示 push 的**分摊 O(1)**——扩容是稀疏的、拷贝总量是线性的,摊到每次 push 上是常数(第 12 章会正式算这笔账)。越界访问是 UB、阶段1 第 10 章就说过,本章用 ASan 实测再次印证:裸 `a[4]` 在普通编译下可能静悄悄印个 0(`exit=0`,骗人),ASan 一开立刻报 `heap-buffer-overflow READ of size 4`、点出 `oob.c:12`——所以容器必须在边界处自己挡,`vec_get` 那道 `if (index >= v->size) return 0;` 是工程做法、不是装饰。最后 ASan 复核 push + `vec_free` 全程 `exit=0`、无 `LeakSanitizer` 报告,证明这套实现把该还的堆都还了。这就是 C++ `std::vector`、Python `list`、Java `ArrayList` 背后同一套内核——下一章我们要换一种容器(链表),看它怎么用「节点 + 指针串起来」解决另一个完全不同的问题。

## 参考资源

- ISO/IEC 9899:2011 §6.2.5p20(数组类型)、§6.5.2.1(数组下标 `a[i] ≡ *(a+i)`,越界 UB)、§6.5.3.4(`sizeof`)、§7.22.3(内存管理函数总则)与 §7.22.3.4(malloc)、§7.22.3.5(realloc 的 tmp 模式与「可能搬家」语义)、§7.22.3.3(free)
- K. N. King《C Programming: A Modern Approach》第 17 章·17.3 Dynamically Allocated Arrays(malloc 给数组开空间、calloc 清零版、realloc 让数组「grow」)、17.4 Deallocating Storage
- cppreference:[`realloc`](https://en.cppreference.com/w/c/memory/realloc)(搬家行为与失败语义)、[`malloc`](https://en.cppreference.com/w/c/memory/malloc)
- 第 10 章:数组(下标、越界、`a[i] ≡ *(a+i)`)、阶段2·第 2 章:指针算术(`data[i]`)、阶段2·第 6 章:动态内存入门(malloc/realloc 的 tmp 模式)、阶段2·第 7 章:动态内存的坑(ASan 抓 UAF/越界/泄漏)、阶段0·第 10 章:Sanitizer 门禁
- 阶段3·第 12 章:算法复杂度与大 O(push 的分摊 O(1) 正式推导)、阶段3·第 9/10 章:排序算法(动态数组当容器用)
