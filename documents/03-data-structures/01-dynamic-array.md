---
title: "动态数组(一):一块会长大的连续内存"
description: "手搓系列的起点。材料是真实工程 projects/journey_your_pack/pack1_vector(C23 + CMake,gcc/clang 下 ASan+UBSan 默认开):一块连续内存配 current_count_/capacity_ 两个计数器,一条 count<=cap 不变式贯穿全章;create 的两笔分配(结构体与缓冲各一笔)、push/get 的最小用法真跑、insert/remove 的重叠搬移为什么必须 memmove 而非 memcpy、elem_index_addr 的 char* 指针算术(标准没给 void* 定义加减);越界契约的三种流派(安静返回/返回 NULL/assert)与咱们容器的选择;宏家族里 jyp_back_vector 空数组的 count-1 无符号回绕恰好被越界守卫接住、返回 NULL 而非崩溃;ctest 四组合(gcc/clang × sanitizer 开关)全部通过。扩容的机关与翻倍的数学留给第 2 章,realloc 的真语义留给第 3 章,所有权与分配器留给第 4 章。"
chapter: 3
order: 1
tags:
  - host
  - data-structures
  - memory
difficulty: intermediate
reading_time_minutes: 12
platform: host
c_standard: [23]
prerequisites:
  - "第 0 章:动态数组,为什么 append 一百万次也装得下(概念与四册地图)"
  - "指针与内存·第 6 章:malloc/free 基础(分配契约与失败路径)"
  - "指针与内存·第 7 章:动态内存的坑(ASan 报告怎么读)"
  - "指针与内存·第 11 章:void* 与字节操作(memcpy/memmove、指针算术)"
related:
  - "第 2 章:动态数组(二),装满了就搬家(扩容与翻倍的数学)"
  - "第 3 章:动态数组(三),realloc 的两副面孔"
  - "第 4 章:动态数组(四),谁拥有,谁善后"
  - "第 5 章:单链表(另一种世界观:节点各自 malloc)"
---

# 手搓第一个容器:一块会长大的连续内存

指针与内存那一程走完,咱们手里已经有了一批零件:malloc/realloc 的分配契约、指针算术、struct 的内存布局。数据结构这一程的第一件事,就是把这些零件攒成一台能开的机器:一个属于咱们自己的容器。

靶子选动态数组,因为它离咱们已有的东西最近。普通数组的墙,咱们学数组那一章就撞过:`int a[10];` 一写下去,这块内存就只能装十个 int,多一个都塞不下,越界写就是未定义行为。可现实里的数据量往往是运行期才知道的:一个文件有多少行、一次请求带多少参数、用户要点多少次添加,编译期根本算不出来。动态数组的思路朴素得很:准备一块 malloc 来的连续内存,装满了就换一块更大的,把旧内容搬过去。它本质上还是数组,下标访问还是一次指针算术;变的只有一件事,大小不再写死在源码里,存在字段里,随时能长。

本章代码全部来自真实工程 [projects/journey_your_pack/pack1_vector](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/tree/main/projects/journey_your_pack/pack1_vector),CMake 工程,C23,根 `CMakeLists.txt` 里写死了 gcc/clang 下 ASan+UBSan 默认全开。这份材料不在纸上谈兵:每一行都在两个编译器、两套 sanitizer 底下真跑过,本章贴出的每一段输出,都是笔者在这台 WSL2(Arch 滚动版)机器上现场抓的(快照 2026-08-31:gcc 16.1.1、clang 22.1.8、cmake 4.4.2;地址类输出每次运行都会变,ASLR 使然,但里面的关系稳定)。

动手前把工程拉起来验明正身:

```text
$ cmake -S . -B build/gcc-asan && cmake --build build/gcc-asan -j
$ ctest --test-dir build/gcc-asan
1/2 Test #1: allocation .......................  Passed    0.01 sec
2/2 Test #2: vector ...........................  Passed    0.01 sec

100% tests passed out of 2
```

这一步咱们前后跑了四组:gcc 与 clang、sanitizer 开与关,四组全部通过。地基是干净的,可以动工了。

## 一块内存和几个字段

容器的心脏是 [jyp_vector.h](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/projects/journey_your_pack/pack1_vector/include/jyp/jyp_vector.h) 里这个结构体,注释都是原作者的口气,咱们原样保留:

```c
typedef struct {
    void* data_begin_;     // 我们的起点是？
    size_t elem_size_;     // 你这元素多大啊？
    size_t current_count_; // 现在我们有多少个？
    size_t capacity_;      // 哦，容量多大呢？

    JYPMemoryAllocatePolicy policy; // resize策略
} JYPVector;
```

拿房子打比方:`data_begin_` 是房子地址,`capacity_` 是房间数,`current_count_` 是已经住进去的人数,`elem_size_` 是每间房占多少字节。比喻搭完就拆,换精确的说法:房子是堆上一块连续内存,`capacity_ * elem_size_` 是它的总字节数;下标 i 的元素住在 `data_begin_` 往后 `i * elem_size_` 字节的位置,访问永远是一次指针算术,这一点和普通数组毫无区别。区别在别处:普通数组的房间数编译期定死,咱们的房间数存在字段里,运行期随时可以换大房子,这就是"动态"二字的全部含义。

这几个数之间有一条咱们全程都要守的不变式:`current_count_ <= capacity_`。push 写的就是下标 `current_count_` 那一格,这一步安全与否,等价于 `current_count_ < capacity_` 成立与否。后面每个函数动手前,您都可以在心里过一遍这条。往后四章咱们还会反复回来看这条不变式:扩容是它在快被打破时的自救,清空是把它一边归零,收缩是把它俩拉近。

还有个设计值得您多看一眼:这个结构体是"裸"的,字段全公开,配套的 `jyp_vector_size(v)` 这类宏就是直接摸字段(头文件注释说得直白:哪天结构体转 opaque,它们是第一批下岗的)。小容器这样做是 C 的常态,读写一目了然,代价是没有任何东西拦着咱们手滑改字段,契约全写在头文件注释里。另一种世界观是把结构体藏到不透明指针后面,外部只见函数不见字段,本仓 `projects/clib-utilities` 的 CCDynamicArray 走的就是那条路。两种都对,咱们选裸的:字段少、看得清,先把机器本身看明白,等您写自己的库时再决定藏不藏。

## 盖房子,住人:create 与最小用法

```c
JYPVector* jyp_create_vector(size_t elem_size, size_t initial_capacity)
{
    JYPVector* vec = ALLOCATE_ELEMS(JYPVector, 1);
    vec->data_begin_ = allocate_elements(elem_size, initial_capacity);
    vec->elem_size_ = elem_size;
    vec->capacity_ = initial_capacity;
    vec->current_count_ = 0;
    vec->policy = p;
    return vec;
}
```

这里有两笔独立的分配:一笔给结构体本体(`ALLOCATE_ELEMS(JYPVector, 1)` 展开就是 `sizeof(JYPVector)` 乘 1),一笔给数据缓冲区。返回的是堆上那个结构体的地址,之后所有函数都拿它当句柄。分配走的是 common 层的薄封装,那一层有故事,咱们留到第 4 章单独拆。

最小用法咱们跑起来,example 程序的开头这么写:

```c
JYPVector* v = jyp_create_vector(sizeof(int), 2);
int vals[] = {10, 20, 30};
for (int i = 0; i < 3; i++) {
    jyp_push_back_vector(v, &vals[i]); // 第三次坐不下了,自动扩容 2→4
}
```

`jyp_push_back_vector` 是个宏,展开就是 `jyp_insert_vector_at(v, data, v->current_count_)`:尾部追加等价于"插在下标 count 上"。咱们直接看真输出:

```text
== 基本款:push / insert / pop ==
push x3      [3] = 10 20 30    (容量 4)
insert 99@1  [4] = 10 99 20 30    (容量 4)
pop_back 拿到 30
pop_back     [3] = 10 99 20    (容量 4)
remove@0 拿到 10
remove@0     [2] = 99 20    (容量 4)
```

咱们逐行看。容量 2 的房子住三个人,第三次 push 自动换成了容量 4 的房子,这趟搬家怎么发生的、贵不贵,是第 2 章的主场,这里咱们先记住现象。`insert 99@1` 把 99 插到下标 1,后面的 20 30 整体右移一格,数量从 3 变 4,容量没变(还没满,不搬)。`pop_back` 和 `remove@0` 都把被删的值经 `out` 指针带了出来,想留全尸的拿去用,不想要的传 NULL 让它就地消失。

## 插队与退场:memmove 的不可妥协

中间插入是整个容器里最"重"的一笔,咱们分两段看。守卫和容量:

```c
void jyp_insert_vector_at(JYPVector* vector, const void* data, size_t index)
{
    if (index > vector->current_count_) { // == count 合法(push_back 就靠它),> count 免谈
        return;
    }

    jyp_reserve_vector(vector, vector->current_count_ + 1);
```

`jyp_reserve_vector` 那一行保证"装得下再动手",装不下它会先长,长的机关咱们留到第 2 章拆。然后是搬移本身:

```c
    // [index, count) 右移一格到 [index+1, count+1) —— 源和目的地重叠,必须 memmove
    memmove(elem_index_addr(vector, index + 1),
            elem_index_addr(vector, index),
            (vector->current_count_ - index) * vector->elem_size_);

    memcpy(elem_index_addr(vector, index), data, vector->elem_size_); // 真正把元素放进去
    vector->current_count_++;
}
```

注释里那对长横线后面的六个字是本段的全部要害:源和目的地重叠。下标 index 到 count 这一段要整体右移一格,源区间和目的区间错开一个元素,大概率犬牙交错。memcpy 对重叠区间没有承诺,行为未定义;memmove 的合同才是"如同先把内容拷到临时缓冲再放到目的地",重叠无所谓。这俩长得像双胞胎,合同差着一条命,讲字节操作那一章咱们对过一次,到这里它第一次真正干活。

`elem_index_addr` 值得单独看,地址三件套收进一个函数:

```c
// 地址三件套(base, index, elem_size)收进一个函数:从此只见 (vector, index)
static inline void* elem_index_addr(const JYPVector* v, size_t index) {
    return (char*)v->data_begin_ + (index * v->elem_size_);
}
```

先转 `char*` 再做加法,这一步的讲究咱们拆开说:void 没有大小,标准没给 `void*` 定义加减,gcc 把它当扩展容忍着,`-Wpedantic` 之下就会出声提醒。转成 `char*` 之后每步一字节,乘上 `elem_size_` 落到字节地址,这是指针算术的正宗用法,也顺便解释了为什么这函数能对任意元素类型工作:它眼里的元素从来只是字节。

remove 是同一段舞步反着跳,您对照看两处对称:

```c
    // 和 insert 同一段 memmove,参数互换、往左挪
    memmove(elem_index_addr(vector, index),
            elem_index_addr(vector, index + 1),
            (vector->current_count_ - index - 1) * vector->elem_size_);
    vector->current_count_--;
```

insert 右移腾位再 memcpy 放新元素,remove 先把值经 out 拷走(传了的话)再左移盖掉空格,参数表里目的和源正好互换,搬移长度少一个元素。中间插、中间删都是 O(n):最坏在头部,全数组跟着挪,所以头部的 push_front/pop_front 宏上标着"热路径别用",咱们把主战场放在尾部。

## 越界怎么办:一份写明的契约

咱们把 get 的契约在这里一并立住:`jyp_get_vector_at(v, i)` 返回第 i 个元素槽位的地址,直接读、直接改都行;越界(下标大于等于 count)返回 NULL。insert 的守卫是同款思路但更宽:下标大于 count 直接安静返回,头文件注释的原话是"绝不乱拷",只有 `index == count` 合法,因为那正好是 push_back。

兄弟们，咱们的 C 是没有异常机制这个东西的！至少标准没有！越界怎么办永远是个显式决定:返回 NULL、安静返回、还是 assert 拍死,咱们这个容器选了前两种,把判断的责任留给调用方。您可以不认同这个选择,但要承认它被写明了。

宏家族里还藏着一个无符号数的细节。`jyp_back_vector` 取末尾元素,宏展开是 `jyp_get_vector_at(v, v->current_count_ - 1)`,空数组上 count 是 0,那 `0 - 1` 会算出什么?size_t 是无符号类型,没有负数可装:减出负数时不报错,也不返回 -1。标准规定无符号运算按 2 的 64 次方取模(ISO C §6.2.5p9),于是 0 减 1 得到 2 的 64 次方减 1,也就是 SIZE_MAX。咱们真跑打印这个值,得到 18446744073709551615,一个天文数字。这个天文数字恰好被 get 的 `index >= count` 守卫接住,返回 NULL,于是空数组取 back 拿到的是 NULL 而非崩溃。头文件注释专门为这句留了说明。无符号数的世界里没有负号,减出负数就掉到最大值那头,这个脾气咱们在移位那一章见过,这里又撞上了。

example 是给人看的,较真的断言在 [test_jyp_vector.c](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/projects/journey_your_pack/pack1_vector/test/test_jyp_vector.c) 里给编译器看,咱们抽两段:

```c
    int n = 99;
    jyp_insert_vector_at(v, &n, 1); // 中间插入:重叠挪位的高危路径
    assert(jyp_vector_size(v) == 4);
    assert(at(v, 0) == 10 && at(v, 1) == 99 && at(v, 2) == 20 && at(v, 3) == 30);

    /* (中间 pop_back、remove 各演示一遍,略) */

    jyp_insert_vector_at(v, &n, 99); // 越界插入:安静返回
    assert(jyp_vector_size(v) == 2);
    assert(jyp_get_vector_at(v, 2) == NULL); // 越界读取给 NULL
```

每个声称的行为都有一条断言盯着。这套测试咱们在四组编译器组合下各跑一遍,四组全过,后面的章节咱们持续沿用这个节奏:容器每长一个本事,test 里就多一条对应的较真。

## 交棒

这一章咱们把手搓的第一个容器用起来了:几个字段、一条不变式、增删查的接口和它们写明的契约。有一件事咱们全程只看了现象没问原理:第三次 push 那一刻,容量 2 的房子是怎么变成容量 4 的?旧房客是怎么搬进新房的?这一趟搬家值多少钱、多久发生一次,正是下一章的主场,而且它的答案里藏着一条整个数据结构课程都要用的道理。咱们下一章见。
