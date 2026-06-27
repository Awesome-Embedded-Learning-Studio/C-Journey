---
title: "动态数组 API 设计实战:从 malloc 到一个能装任何类型的容器"
description: "照着作者第一份过万字、单文件破千行的 standard_dynamic_array 笔记,把一个泛型动态数组的 API 设计完整拆开——工厂函数、resize、push/insert/delete 全家桶、函数指针驱动的打印与比较、错误码状态机,每一块都配本机实测的真实输出,顺手把笔记里几处会炸的笔误修掉。"
chapter: 3
order: 2
tags:
  - host
  - data-structures
  - generics
  - memory
  - pointers
difficulty: intermediate
reading_time_minutes: 18
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 2:指针、内存布局与位运算"
  - "Chapter 3:用 C 造一个泛型容器(从 void* 到自己的 vector)"
related:
  - "CMake 与模块化工程(阶段 4)"
  - "环形缓冲区(阶段 3)"
---

# 动态数组 API 设计实战:从 malloc 到一个能装任何类型的容器

## 引言

这是作者最早的一份"过万字、单文件破千行"的手搓库笔记,写于编程刚满 7 个月的时候——第一次手搓一个像样的小项目,第一次让单个程序突破 1000 行。说白了就是想用纯 C 复刻一个 C++ 的 `std::vector`:什么类型都能装,自己会长大,push 一个进去就多一个,删完还能把多余的空间还给操作系统。这件事 C 没有模板帮你,得自己一行一行搓。

我们这一章不重新发明轮子(隔壁那篇 [泛型容器](0-generic-containers-in-c.md) 已经把 `void* + dataSize + 回调` 的灵魂讲透了),而是顺着这份笔记的**API 设计思路**一路走下来:一个动态数组到底要暴露哪些函数?工厂函数为什么要有三四种?resize 什么时候该主动调?push 单个、push 一堆、insert 到中间,这几件事的内存操作到底差在哪?打印和比较凭什么要写成函数指针?错误码为什么不能只写个 `return` 就完事?这些问题的答案,合在一起就是一个能用的数据结构库。

整篇的代码都是本机真实编译跑过的(`gcc -std=c11 -Wall -Wextra`,valgrind 零错误),输出原样贴在每段代码下面,不编一句。笔记里那几处一编译就报警、一跑就崩的笔误,我们也一并指出来并修掉——这本身就是"读自己半年前的代码"最有价值的部分。

## 动态数组的雏形:realloc 一把梭

先把脑子里的模型立起来。一个动态数组,本质还是数组——内存物理上连续、存同类型元素——只不过它得能"伸缩"。要伸缩,就得用堆,用 `malloc`/`realloc`/`free` 这一套。最朴素的雏形长这样:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int* data = NULL;
    size_t current_size = 0;

    for (int i = 1; i <= 5; i++) {
        int* p = realloc(data, (current_size + 1) * sizeof(int));
        if (!p) {
            printf("realloc failed\n");
            free(data);
            return 1;
        }
        data = p;                 /* 关键:realloc 可能搬家,必须用返回值更新指针 */
        data[current_size] = i * 10;
        current_size++;
        printf("after push %d: current_size=%zu, data=%p\n",
               i * 10, current_size, (void*)data);
    }

    printf("final array: ");
    for (size_t i = 0; i < current_size; i++) {
        printf("%d ", data[i]);
    }
    printf("\n");

    free(data);
    return 0;
}
```

跑一下,真实输出是这样(`data` 的地址每次运行都可能不同,这里用 `<heap>` 占位):

```text
after push 10: current_size=1, data=<heap>
after push 20: current_size=2, data=<heap>
after push 30: current_size=3, data=<heap>
after push 40: current_size=4, data=<heap>
after push 50: current_size=5, data=<heap>
final array: 10 20 30 40 50
```

能跑,但散架着太捞了——`data` 和 `current_size` 两个变量分家,后面再多几个操作就乱成一锅粥。于是结构体该上场了。

## 把状态收进结构体:三把钥匙

光记一个 `data` 指针和 `current_size` 还不够。因为我们的容器是泛型的,装的是 `void*`,这个指针本身不知道自己指向的元素到底多大——你必须额外记一笔"单个元素几个字节",否则后面的 `memcpy`、指针偏移全都没法算。再加上一个"已经开了多少容量"的 `total_usable`,扩容判断才有的写。所以一个能用的动态数组,手里攥着**三把钥匙**:

```c
typedef struct {
    void*  data;            /* 连续内存块                      */
    size_t current_size;    /* 当前实际元素个数                */
    size_t total_usable;    /* 已开辟的可容纳元素个数(容量)  */
    size_t single_size;     /* 单个元素的字节大小(= 类型信息)*/
} DynamicArray;
```

`single_size` 这一笔,就是 C 没有 `template<T>` 的代偿——我们把"类型"变成运行时的一个 `size_t` 参数记下来。这一点理解透了,后面所有 API 的设计都是顺水推舟。

## 错误处理:别只写一句 `return`

笔记里反复强调的一个工程原则:**`malloc` / `realloc` 是会失败的,失败返回 `NULL`,你不判就是炸弹。** 但判了之后怎么处理,差别很大。

教程里最常见的写法是这样:

```c
if (!p) {
    return;   /* 教程能这么写,工程里这么写要出事 */
}
```

问题是,你 `return` 出去之后,调用方很可能接着用那个 `NULL` 指针,该崩还是崩,而且崩在离真正的错误很远的地方,debug 起来要命。所以笔记的做法是——把错误码和错误信息**一一对应**起来,出错时既打印一句人话,又带着错误码退出:

```c
typedef enum {
    Dyarr_Normal         = 0,   /* 正常返回            */
    Dyarr_Err_Malloc     = 1,   /* 开辟空间失败        */
    Dyarr_Null_Input     = -1,  /* 传入空值            */
    Dyarr_Invalid_Input  = -2,  /* 传入不合法的值      */
    Dyarr_UnFind         = -3,  /* 没有找到(查找语义) */
    Dyarr_Invalid_Free   = -4   /* 不合法的释放        */
} DyarrStatus;
```

配套一组报错宏,错误码和打印文本绑定死,日后排查一眼就知道是哪一类:

```c
#define SHOW_ERR_NULL    printf("\nSorry! Your input is NULL!\n")
#define SHOW_ERR_MALLOC  printf("\nSorry! Failed to malloc space for your data\n")
#define SHOW_ERR_INPUT   printf("\nYour input is invalid, reject to run function\n")
#define SHOW_ERR_UNFIND  printf("\nPositions unfind!\n")
#define SHOW_ERR_FREE    printf("\nFree the invalid space, reject to run function\n")
```

> 真正的坑在后面:这个库从头到尾几乎所有函数都返回 `DyarrStatus`,这是好事——调用方能拿到一个明确的状态。但笔记里有好几个函数(比如 `sort`、`clear`)写到一半忘了 `return Dyarr_Normal;`,在 `-Wall` 下会报 `control reaches end of non-void function` 的警告,严格点的编译器甚至直接拒绝。下面这份整理过的实现把这种尾巴都补齐了。

## 工厂函数:产生一个可操作的对象

"工厂函数"就是产生对象并返回给调用方的函数——`malloc` 本身就是最经典的工厂函数,你告诉它要几个字节,它还你一块堆内存。我们的动态数组也需要自己的工厂。

### 默认工厂:从零造一个空数组

用户往往根本不知道自己最终要装多少个数据,所以我们给一个默认容量(笔记里取 5),用户传的容量比默认小就用默认值,顺手挡掉传 0 的尴尬:

```c
DynamicArray* dyarr_init(size_t expected, size_t single_size) {
    DynamicArray* a = malloc(sizeof(DynamicArray));
    if (!a) {
        SHOW_ERR_MALLOC;
        exit(Dyarr_Err_Malloc);
    }
    a->data        = NULL;
    a->single_size = single_size;
    a->current_size = 0;
    a->total_usable = (expected >= Dyarr_DEFAULT_MALLOC) ? expected
                                                         : Dyarr_DEFAULT_MALLOC;
    a->data = malloc(a->total_usable * a->single_size);
    if (!a->data) {
        SHOW_ERR_MALLOC;
        exit(Dyarr_Err_Malloc);
    }
    return a;
}
```

### 升级工厂:从静态数组搬过来

更常见的场景是——手里已经有一个静态数组,想直接升级成动态数组。反复调用 `push_back` 性能太差,我们干脆提供一个"一边造结构体、一边把数据拷进来"的接口。

这里有一条笔记里特意用感叹号标红的原则:

> 千万别把入参 `data` 的地址直接交给 `DataPiece` 托管!你后续的 resize/delete 操作可能把那块内存抹掉,但 `DataPiece` 还指着它,贸然托管等于给自己埋一颗非法访问的雷。

正确做法是**单独开一块新空间**(笔记里预留 1.5 倍余量),再把源数据 `memcpy` 过去:

```c
DynamicArray* dyarr_from_static(void* src, size_t single_size, size_t n) {
    if (!src) {
        SHOW_ERR_NULL;
        exit(Dyarr_Null_Input);
    }
    DynamicArray* a = malloc(sizeof(DynamicArray));
    if (!a) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    size_t cap = (size_t)(n * 1.5);
    if (cap < Dyarr_DEFAULT_MALLOC) cap = Dyarr_DEFAULT_MALLOC;
    a->data = malloc(cap * single_size);
    if (!a->data) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    memcpy(a->data, src, n * single_size);   /* 来源是任意类型,只能按字节拷 */
    a->single_size  = single_size;
    a->current_size = n;
    a->total_usable = cap;
    return a;
}
```

### 复制工厂:拷贝另一个动态数组

模仿 C++ STL 的复制构造,思路和升级工厂几乎一样——开同样大的空间,把对方的 `data` 整块拷过来,三个 `size` 字段照抄:

```c
DynamicArray* dyarr_copy(const DynamicArray* other) {
    if (!other) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    DynamicArray* a = malloc(sizeof(DynamicArray));
    if (!a) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    a->data = malloc(other->total_usable * other->single_size);
    if (!a->data) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    memcpy(a->data, other->data,
           other->current_size * other->single_size);
    a->single_size  = other->single_size;
    a->current_size = other->current_size;
    a->total_usable = other->total_usable;
    return a;
}
```

三个工厂函数到这就齐了:从零造、从静态数组升级、从同类复制。一个可操作的对象,我们有了三种拿到它的方式。

## 容量调整:resize 的核心是 realloc

有了对象,接下来要让它能伸缩。调整容量说穿了就是一句话:

```c
void* new = realloc(dyarr->data, new_cap * dyarr->single_size);
```

但有两个细节不能漏。第一,`realloc` 可能搬家也可能失败,必须用返回值更新指针、必须判空。第二,**缩容**的时候,如果新容量比现有元素还少,多出来的元素得截断,否则 `current_size` 会指着已经还给系统的内存:

```c
DyarrStatus dyarr_resize(DynamicArray* a, size_t new_cap) {
    if (!a) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    void* p = realloc(a->data, new_cap * a->single_size);
    if (!p) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    a->data         = p;
    a->total_usable = new_cap;
    if (a->current_size > a->total_usable) {
        a->current_size = a->total_usable;   /* 缩容时截断,后面的归还系统 */
    }
    return Dyarr_Normal;
}
```

为了让调用方不用每次都自己算"该扩几倍",我们再封一个内部小工具 `dyarr_ensure`,容量不够就按 2 倍往上翻——后面所有 push/insert 都靠它兜底:

```c
static DyarrStatus dyarr_ensure(DynamicArray* a, size_t extra) {
    if (a->current_size + extra > a->total_usable) {
        size_t new_cap = a->total_usable;
        while (a->current_size + extra > new_cap) {
            new_cap = (new_cap == 0) ? Dyarr_DEFAULT_MALLOC : new_cap * 2;
        }
        return dyarr_resize(a, new_cap);
    }
    return Dyarr_Normal;
}
```

> 这一段其实替笔记收拾了一个真问题:笔记里 push 系列函数的扩容逻辑是手算一个 `datarate = datasize / total_usable`,但 `datasize` 在那里被当成"元素个数"用,变量名和含义对不上,而且整数除法很容易得到 0 直接跳过扩容。整理版把"算新容量"这件事统一收进 `dyarr_ensure`,逻辑清楚得多,也少一个 off-by-one 的雷区。

## 增:push 与 insert

增删查改里先看"增"。一个数据追加在尾部,还是插在中间,内存操作完全不一样。

### 追加单个:realloc + memcpy 到尾部

追加是最简单的:容量够就拷到 `current_size` 那个槽,然后 `current_size++`。位置计算是 `(char*)data + current_size * single_size`——注意必须先把 `void*` 转成 `char*` 才能做字节级指针运算,这是 C 的硬规矩:

```c
DyarrStatus dyarr_push_back(DynamicArray* a, void* data, size_t single_size) {
    if (!a || !data) {                       /* 笔记里写成 &&,两个都空才报错,漏判 */
        SHOW_ERR_NULL;
        exit(Dyarr_Null_Input);
    }
    if (single_size != a->single_size) {
        SHOW_ERR_INPUT;
        exit(Dyarr_Invalid_Input);
    }
    dyarr_ensure(a, 1);
    memcpy((char*)a->data + a->current_size * a->single_size,
           data, a->single_size);
    a->current_size++;
    return Dyarr_Normal;
}
```

### 插入中间:先把后段整体后移

insert 比 push 麻烦一步。你要在 `pos` 处塞一个新元素,就得先把 `[pos, current_size)` 这一整段往后挪一个身位,给 `pos` 腾出空位,再把新数据放进去。挪动这一步**必须用 `memmove` 而不是 `memcpy`**,因为源和目的区域是重叠的,`memcpy` 在重叠区是未定义行为:

```c
DyarrStatus dyarr_insert(DynamicArray* a, void* data, size_t pos) {
    if (!a || !data) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    if (pos > a->current_size) { SHOW_ERR_INPUT; exit(Dyarr_Invalid_Input); }
    dyarr_ensure(a, 1);
    memmove((char*)a->data + (pos + 1) * a->single_size,
            (char*)a->data + pos * a->single_size,
            (a->current_size - pos) * a->single_size);
    memcpy((char*)a->data + pos * a->single_size, data, a->single_size);
    a->current_size++;
    return Dyarr_Normal;
}
```

> 笔记里 insert 的原始写法把 memcpy 和 memmove 的角色搞反了,而且偏移长度写成了 `current_size - pos + 1`(多算了一个),真实跑起来要么插错位置、要么越界踩内存。整理版里这两处都按"后移 `(current_size - pos)` 个元素、再把新数据 memcpy 到 pos"修正了。这种 bug 不实测根本看不出来——这也是为什么每个例子都要真编译真跑。

笔记里其实还规划了好几个 push/insert 变体(`push` 一堆相同值、`push` 一个静态数组、`insert` 一堆相同值、`insert` 一个静态数组、两个动态数组拼接),核心循环和上面这两个完全同构,差别只在"循环 N 次逐个 memcpy"还是"一次性 memcpy 一整段",这里就不展开了,完整版可以照着 `dyarr_push_back` / `dyarr_insert` 的骨架自己扩。

## 删:逻辑删除、覆盖删除、彻底销毁

像人生要做减法一样,数据结构也得能删。删有三种粒度,对应的内存策略各不相同。

**尾删(pop)** 最轻——`current_size--` 就行,数据本身不动,下次 push 自然会覆盖它。笔记里强调过这是"逻辑删除",真要还内存得靠 resize:

```c
DyarrStatus dyarr_pop(DynamicArray* a) {
    if (!a) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    if (a->current_size == 0) { SHOW_ERR_INPUT; exit(Dyarr_Invalid_Input); }
    a->current_size--;
    return Dyarr_Normal;
}
```

**删指定位置** 是 insert 的逆操作:把 `pos` 之后的整段往前挪一个身位,盖掉 `pos`,然后 `current_size--`:

```c
DyarrStatus dyarr_delete_at(DynamicArray* a, size_t pos) {
    if (!a) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    if (pos >= a->current_size) { SHOW_ERR_INPUT; exit(Dyarr_Invalid_Input); }
    memmove((char*)a->data + pos * a->single_size,
            (char*)a->data + (pos + 1) * a->single_size,
            (a->current_size - pos - 1) * a->single_size);
    a->current_size--;
    return Dyarr_Normal;
}
```

**清空(clear)** 是把数组打回原形——`current_size = 0`,再 `realloc` 到只剩一个槽位的余量,方便后续复用:

```c
DyarrStatus dyarr_clear(DynamicArray* a) {
    if (!a) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    a->current_size = 0;
    void* p = realloc(a->data, a->single_size);
    if (!p) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    a->data         = p;
    a->total_usable = 1;
    return Dyarr_Normal;
}
```

**销毁(destroy)** 是彻底退场,顺序很重要——**先释放数据块,再释放结构体外壳**,反了就是内存泄漏:

```c
DyarrStatus dyarr_destroy(DynamicArray* a) {
    if (!a) { SHOW_ERR_FREE; exit(Dyarr_Invalid_Free); }
    free(a->data);
    free(a);
    return Dyarr_Normal;
}
```

## 函数指针:打印和比较凭什么要传进来

到这里你可能发现一个尴尬:我们的容器装的是 `void*`,它**根本不知道**自己装的是 int 还是结构体。那"怎么打印一个元素"、"两个元素算不算相等"、"谁该排在前面"——这些问题容器自己一个都答不了。答案就是:**让调用方把答案以函数指针的形式传进来。**

### 函数指针 typedef 长什么样

打印回调的别名这样写,意思是"接收一个元素地址、不返回的函数":

```c
typedef void (*PrintFunc)(void*);
```

这个语法第一眼有点劝退。它的读法是:`PrintFunc` 是一个类型,这个类型的值是"指向 `void(void*)` 这种函数的指针"。下面这段小程序就能直观看出它怎么用——函数名本质上就是个指针,既可以 `pf(&x)` 直接调用,也可以 `(*pf)(&x)` 解引用再调用,效果一模一样:

```c
#include <stdio.h>

typedef void (*PrintFunc)(void*);

static void print_int(void* p) { printf("%d ", *(int*)p); }

int main(void) {
    PrintFunc pf = print_int;
    int x = 42;
    pf(&x);              /* 用 () 直接调用 */
    (*pf)(&x);           /* 解引用再调用,效果完全一样 */
    printf("\n");
    return 0;
}
```

真实输出:

```text
42 42
```

有了这个别名,打印全部元素的函数就特别干净——容器只负责"遍历到每个元素的地址",至于怎么打印,全交给用户传进来的 `pf`:

```c
DyarrStatus dyarr_print_all(DynamicArray* a, PrintFunc pf) {
    if (!a || !pf) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    printf("[ ");
    for (size_t i = 0; i < a->current_size; i++) {
        pf((char*)a->data + i * a->single_size);
    }
    printf("]\n");
    return Dyarr_Normal;
}
```

同样的套路再套一个 `ChangeFunc`(就地改一个元素)就有了 `dyarr_for_each`,套一个 `LocateFunc`(判等)就有了查找,套一个 `CompareFunc`(比较大小)就有了排序。一个 `typedef`,一整套 API 就活了。

### 笔记里一个会误导人的类型判断

笔记里 push 函数有一段想"挡住类型不对的入参",写的是:

```c
if (sizeof(data) != dyarr->single_size) { ... }   /* 看似合理,其实永远挡不住 */
```

这里 `data` 是个 `void*` 指针,`sizeof(data)` 取的是**指针本身的大小**,跟元素是 int 还是 double 没有半毛钱关系。本机实测一把你就懂了:

```c
#include <stdio.h>

int main(void) {
    int  v = 7;
    void* data = &v;
    printf("sizeof(void*) = %zu\n", sizeof(void*));
    printf("sizeof(data)  = %zu   <-- 这跟元素是 int 还是 double 没有任何关系\n",
           sizeof(data));
    return 0;
}
```

真实输出(64 位机上):

```text
sizeof(void*) = 8
sizeof(data)  = 8   <-- 这跟元素是 int 还是 double 没有任何关系
```

正确的做法是**让调用方显式把 `single_size` 当参数传进来**,函数里拿它跟 `a->single_size` 比——这正是整理版 `dyarr_push_back` 第三段判空在做的事。C 没有运行时类型反射,元素大小只能由调用方负责声明,这一条是这个泛型方案的宿命。

## 查找与排序:回调驱动的两套语义

### 查找:判等回调 + 位置数组

判等这件事容器干不了,所以 `LocateFunc` 接收"容器里的元素"和"用户要找的 key"两个指针,相等返回非 0。配套一个 `DyarrFind` 枚举把"找到/没找到"做成语义化的返回值,比直接返回 0/-1 可读得多:

```c
typedef int  (*LocateFunc)(void*, void*);   /* 相等返回非 0 */
typedef enum { Dyarr_Find = 1, Dyarr_NotFind = -1 } DyarrFind;

DyarrFind dyarr_contains(DynamicArray* a, void* key, LocateFunc eq) {
    if (!a || !eq || !key) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    for (size_t i = 0; i < a->current_size; i++) {
        if (eq((char*)a->data + i * a->single_size, key)) {
            return Dyarr_Find;
        }
    }
    return Dyarr_NotFind;
}

long dyarr_index_of(DynamicArray* a, void* key, LocateFunc eq) {
    if (!a || !eq || !key) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    for (size_t i = 0; i < a->current_size; i++) {
        if (eq((char*)a->data + i * a->single_size, key)) {
            return (long)i;
        }
    }
    return (long)Dyarr_NotFind;
}
```

笔记里还设计了一个"返回所有匹配位置"的玩法:用一个只记 `int` 位置的辅助动态数组,遍历主数组时把每个命中位置 push 进去。思路和 `index_of` 同构,只是把"return i"换成了"往位置数组里 push i",这里不重复贴了。

### 排序:冒泡 + 交换函数

笔记老老实实承认只实现了冒泡排序(原话是"quicksort 太累了 lol")。冒泡的骨架大家都熟,这里的关键是**交换两个元素也要靠 `memcpy` 三段式**(因为不知道元素多大,只能按 `single_size` 整块搬):

```c
DyarrStatus dyarr_sort(DynamicArray* a, CompareFunc cmp) {
    if (!a || !cmp) { SHOW_ERR_NULL; exit(Dyarr_Null_Input); }
    char* tmp = malloc(a->single_size);
    if (!tmp) { SHOW_ERR_MALLOC; exit(Dyarr_Err_Malloc); }
    for (size_t i = 0; i + 1 < a->current_size; i++) {
        for (size_t j = 0; j + i + 1 < a->current_size; j++) {
            char* pj  = (char*)a->data + j * a->single_size;
            char* pj1 = (char*)a->data + (j + 1) * a->single_size;
            if (cmp(pj, pj1) > 0) {
                memcpy(tmp, pj, a->single_size);
                memcpy(pj, pj1, a->single_size);
                memcpy(pj1, tmp, a->single_size);
            }
        }
    }
    free(tmp);
    return Dyarr_Normal;
}
```

> 一个小优化点:笔记里 sort 每次比较都现场 `malloc` 一个 `swapbit` 当临时空间,内层循环跑完才 `free`,N² 次比较就是 N² 次 malloc。整理版把 `tmp` 提到函数最外层只 malloc 一次,性能直接好一个数量级——这种"在热点里反复申请堆"的坑,只有真跑大数据量时才暴露得出来。

## 跑一遍:把全套 API 串起来

光看分块代码不过瘾,我们写个 driver 把所有 API 串起来跑一遍,看真实输出。完整的 `dynarr.h` / `dynarr.c` / `main.c` 三件套就是上面这些函数的集合,driver 长这样:

```c
#include "dynarr.h"

static void print_int(void* p) { printf("%d ", *(int*)p); }
static int  eq_int(void* a, void* b) { return *(int*)a == *(int*)b; }
static int  cmp_int(void* a, void* b) {
    int x = *(int*)a, y = *(int*)b;
    return (x > y) - (x < y);
}
static void inc_int(void* p) { (*(int*)p)++; }

int main(void) {
    int data1 = 10;

    printf("== 1. init + push_back 单个 ==\n");
    DynamicArray* a = dyarr_init(10, sizeof(int));
    dyarr_push_back(a, &data1, sizeof(int));
    dyarr_print_all(a, print_int);

    printf("\n== 2. 从静态数组升级 ==\n");
    int src[10] = {5, 3, 9, 1, 7, 2, 8, 4, 6, 0};
    DynamicArray* b = dyarr_from_static(src, sizeof(int), 10);
    dyarr_print_all(b, print_int);

    printf("\n== 3. 复制构造 ==\n");
    DynamicArray* c = dyarr_copy(b);
    dyarr_print_all(c, print_int);

    printf("\n== 4. 在 pos=1 插入 100 ==\n");
    int v = 100;
    printf("before: "); dyarr_print_all(c, print_int);
    dyarr_insert(c, &v, 1);
    printf("after:  "); dyarr_print_all(c, print_int);

    printf("\n== 5. 删除 pos=1 ==\n");
    printf("before: "); dyarr_print_all(c, print_int);
    dyarr_delete_at(c, 1);
    printf("after:  "); dyarr_print_all(c, print_int);

    printf("\n== 6. for_each 全体 +1 ==\n");
    printf("before: "); dyarr_print_all(c, print_int);
    dyarr_for_each(c, inc_int);
    printf("after:  "); dyarr_print_all(c, print_int);

    printf("\n== 7. 查找:contains / index_of ==\n");
    int key = 6;
    printf("b contains %d? %s\n", key,
           dyarr_contains(b, &key, eq_int) == Dyarr_Find ? "yes" : "no");
    printf("first index of %d in b = %ld\n", key, dyarr_index_of(b, &key, eq_int));

    printf("\n== 8. 冒泡排序 b ==\n");
    printf("before: "); dyarr_print_all(b, print_int);
    dyarr_sort(b, cmp_int);
    printf("after:  "); dyarr_print_all(b, print_int);

    printf("\n== 9. 扩容压力:连续 push_back ==\n");
    for (int i = 0; i < 20; i++) {
        int x = 1000 + i;
        dyarr_push_back(b, &x, sizeof(int));
    }
    printf("after push loop: current_size=%zu total_usable=%zu\n",
           b->current_size, b->total_usable);

    printf("\n== 10. clear / destroy ==\n");
    dyarr_clear(a);
    printf("after clear a: current_size=%zu total_usable=%zu\n",
           a->current_size, a->total_usable);
    dyarr_destroy(a);
    dyarr_destroy(b);
    dyarr_destroy(c);
    return 0;
}
```

编译命令和真实输出如下(`gcc -std=c11 -Wall -Wextra dynarr.c main.c -o demo`,valgrind 全程零错误):

```text
== 1. init + push_back 单个 ==
[ 10 ]

== 2. 从静态数组升级 ==
[ 5 3 9 1 7 2 8 4 6 0 ]

== 3. 复制构造 ==
[ 5 3 9 1 7 2 8 4 6 0 ]

== 4. 在 pos=1 插入 100 ==
before: [ 5 3 9 1 7 2 8 4 6 0 ]
after:  [ 5 100 3 9 1 7 2 8 4 6 0 ]

== 5. 删除 pos=1 ==
before: [ 5 100 3 9 1 7 2 8 4 6 0 ]
after:  [ 5 3 9 1 7 2 8 4 6 0 ]

== 6. for_each 全体 +1 ==
before: [ 5 3 9 1 7 2 8 4 6 0 ]
after:  [ 6 4 10 2 8 3 9 5 7 1 ]

== 7. 查找:contains / index_of ==
b contains 6? yes
first index of 6 in b = 8

== 8. 冒泡排序 b ==
before: [ 5 3 9 1 7 2 8 4 6 0 ]
after:  [ 0 1 2 3 4 5 6 7 8 9 ]

== 9. 扩容压力:连续 push_back ==
after push loop: current_size=30 total_usable=30

== 10. clear / destroy ==
after clear a: current_size=0 total_usable=1
```

注意第 9 步:连续 push 20 个,b 的 `total_usable` 从 15 一路翻倍到 30,扩容机制确实在工作。第 6 步也值得看一眼——`for_each` 配合一个 `inc_int` 回调,一次性把所有元素加了 1,这就是函数指针的威力:容器只管遍历,业务逻辑全在调用方那一行回调里。

## 常见踩坑

笔记里踩过的、整理时改掉的坑,集中列一下,每一条都是真会炸的:

- **`if (!a && !b)` 当判空用** —— 这是要两个都空才报错,任意一个空都会漏判。判空永远是 `||`,写反了等于没判。
- **`memcpy` 用在重叠区** —— insert/delete 要后移/前移一段和自身重叠的数据,必须 `memmove`,`memcpy` 在重叠区是 UB,Valgrind 会直接报 `Source and destination overlap`。
- **`realloc` 不接返回值** —— `realloc` 可能整块搬家,旧指针作废。永远写成 `p = realloc(old, n); old = p;`,先存临时变量再覆盖。
- **`pos < 0` 判 `size_t`** —— `size_t` 是无符号的,`pos < 0` 永远是假,这种判断会被编译器警告"comparison is always false"。要么改成 `pos > current_size`,要么用带符号类型。
- **`sizeof(指针)` 当类型判断** —— `sizeof(void*)` 取的是指针大小,跟元素类型无关,挡不住任何错。元素类型信息只能由调用方显式传 `single_size`。
- **缩容不截断 `current_size`** —— resize 缩到比现有元素少时,必须把 `current_size` 也压下去,否则它会指向已归还的内存。
- **destroy 顺序反了** —— 先 `free(外壳)` 再 `free(data)` 就是泄漏,因为 free 完外壳你再也拿不到 `data` 指针。永远先 data 后外壳。
- **sort 在内层循环 malloc** —— N² 次比较 N² 次堆申请,大数据量直接卡死。临时空间提到循环外只申请一次。

## 小结

一个能装任何类型的动态数组,在 C 里就是这么一套东西:

- **三把钥匙**:连续内存块 `data`、当前个数 `current_size`、容量 `total_usable`,外加一个 `single_size` 把"类型"编码成运行时参数。
- **工厂函数三件套**:从零造、从静态数组升级、从同类复制,任何一种都不能直接托管入参指针,必须自己开空间再 memcpy。
- **resize 是 realloc 的封装**,缩容要截断、扩容靠 `dyarr_ensure` 兜底。
- **增删都是 memmove/memcpy 的组合**:push 拷到尾部、insert 先后移再拷入、delete 前移覆盖、pop 直接 `--`。
- **凡容器自己答不了的问题(怎么打印、怎么判等、怎么比较),全交给函数指针回调**——一个 `typedef` 一套 API。
- **错误码状态机 + 报错宏**:出错既要打印人话也要带码退出,别只写一句 `return`。

笔记最值得肯定的一点是:它把"一个数据结构库到底要暴露什么"这件事,从一个模糊的需求拆成了一组清清楚楚的函数签名和内存操作。而整理这一遍最大的收获,是发现"能跑的草稿"和"经得起 `-Wall` 和 valgrind 的代码"之间,隔着的就是那几处 `&&`/`||`、`memcpy`/`memmove`、缺 `return`、错位的偏移量——这些只有真编译真跑才暴露得出来。

## 练习

1. 把笔记里规划但本篇没展开的 push/insert 变体补齐:`dyarr_push_back_n`(尾插 N 个相同值)、`dyarr_push_back_array`(尾插一个静态数组)、`dyarr_append`(把另一个动态数组拼到末尾)。每个都写个 driver 验证输出。
2. 给 `dyarr_delete_at` 加一个"删除后若 `current_size <= total_usable/4` 就自动缩容一半"的策略(笔记里叫它自适应 shrink),注意别缩到比默认容量还小。
3. 把冒泡排序换成快排或归并,比较两者在 10000 个随机 int 上的耗时。体会一下回调比较函数(`CompareFunc`)在换算法时完全不用改的好处。
4. 给所有会 `exit` 的错误路径改成"返回错误码、不退出",在 driver 里检查返回值并打印错误类型。想想这套状态机比起直接 `exit` 在库设计上有什么好处和代价。

## 参考资源

- 本仓库姊妹篇:[用 C 造一个泛型容器:从 void* 到自己的 vector](0-generic-containers-in-c.md) —— 同一套 `void* + dataSize + 回调` 思路在 `CCSTDC_Vector` 上的落地。
- [cppreference: `realloc`](https://en.cppreference.com/w/c/memory/realloc) —— realloc 搬家行为与失败语义的标准描述。
- [cppreference: `memmove`](https://en.cppreference.com/w/c/string/byte/memmove) —— 为什么重叠区必须用 memmove。
- ISO/IEC 9899:2011 §6.5.3.4(`sizeof`)、§7.22.3(`malloc`/`realloc`/`free`)。

---

*整理自作者笔记,按 C-Journey 写作规范重写;所有输出本机实测捕获。*
