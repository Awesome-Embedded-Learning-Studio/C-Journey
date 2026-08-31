---
title: "动态数组(二):装满了就搬家——翻倍、摊还与 CPython 的另一个答案"
description: "本册拆扩容的机关。同样 100000 次 push,翻倍策略只调 17 次 realloc、+1 策略调 99999 次,开篇即贴这对真输出;增长轨迹驱动逐个打印容量与 data 指针,2→4→8→16→32 的曲线之外,地址列有诈(三次原地一次搬家,留给第 3 章拆);jyp_reserve_vector 逐行讲:policy 函数指针要新容量、new_cap<want 兜底、reallocate 收的是字节要乘 elem_size_;摊还 O(1) 亲手算:翻倍的累计搬运 2^k-1 序列表格逐行落位,十万个 push 总共只搬约十三万元素,+1 策略却是约五十亿次;增长策略经 jyp_set_resize_vector_policy 一行换装;对答案环节请出 CPython v3.14.0 listobject.c 的 list_resize 公式 (newsize + newsize/8 + 6) & ~3,与本机 Python 3.14.6 的 sys.getsizeof 实测曲线逐项吻合(4,8,16,24,32,40,52,64,76,92,108),讨论翻倍 50% 浪费上界与温和增长 12.5% 的取舍。"
chapter: 3
order: 2
tags:
  - host
  - data-structures
  - memory
difficulty: intermediate
reading_time_minutes: 10
platform: host
c_standard: [23]
prerequisites:
  - "第 1 章:动态数组(一),一块会长大的连续内存(字段与不变式、基本接口)"
  - "指针与内存·第 9 章:函数指针(增长策略靠它插拔)"
related:
  - "第 3 章:动态数组(三),realloc 的两副面孔(地址列的诈在那章拆)"
  - "第 15 章:算法复杂度与大 O(摊还分析的正式版)"
  - "深水篇预告:sds 动态字符串(Redis 的另一种增长取舍)"
---

# 装满了就搬家:17 次,还是 99999 次

上一章结尾咱们留了个尾巴:第三次 push 那一刻,容量 2 的房子怎么就变成了容量 4。这一章就盯着这件事看,而且它的答案比现象有意思得多。同样往一个数组里塞十万个整数,下面两行输出都来自咱们的容器,唯一差别是装满之后"下一步容量给多大":

```text
翻倍策略:100000 次 push,realloc 只调了 17 次(容量 131072)
+1 策略:100000 次 push,realloc 调了 99999 次(容量 100000)
```

一个 17 次,一个 99999 次。这个 17 是怎么来的、凭什么翻倍能把每次 push 的代价摊平成一个常数、每次只加一为什么摊不掉,咱们这一章把它算清楚。算清楚之后还有一个对答案环节:Python 的 list 就是同一个结构,CPython 选的增长曲线和咱们的翻倍不一样,两边摆在一起看,您会发现"增长策略"根本就没有唯一正确答案,只有取舍。

## 盯着搬家看

咱们写个小驱动,每推一个数就检查容量和 `data_begin_` 的地址,变了就打印:

```c
JYPVector* v = jyp_create_vector(sizeof(int), 2);
printf("起步:count=0 容量=%zu data=%p\n", v->capacity_, (void*)v->data_begin_);
size_t last_cap = v->capacity_;
for (int i = 1; i <= 20; i++) {
    jyp_push_back_vector(v, &i);
    if (v->capacity_ != last_cap) {
        printf("第 %2d 个:容量 %2zu → %2zu,data 搬到 %p\n",
               i, last_cap, v->capacity_, (void*)v->data_begin_);
        last_cap = v->capacity_;
    }
}
```

gcc 下、咱们把 sanitizer 关掉,真输出:

```text
起步:count=0 容量=2 data=0x59ec8bb40040
第  3 个:容量  2 →  4,data 搬到 0x59ec8bb40040
第  5 个:容量  4 →  8,data 搬到 0x59ec8bb41070
第  9 个:容量  8 → 16,data 搬到 0x59ec8bb41070
第 17 个:容量 16 → 32,data 搬到 0x59ec8bb41070
```

扩容发生在第 3、5、9、17 个,正好是 2、4、8、16 满员的下一刻,容量曲线 2→4→8→16→32,教科书一样标准。可地址那一列有诈:第 3 个"搬"到的新地址和起步地址一模一样,第 9、17 个也是原地,只有第 5 个真换了地方。搬家还分真搬和假搬?这就是 realloc 的两副面孔,值得咱们专门开一章,下一章就拆它。这一章先把容量列的故事讲完。

## 搬家的机关

扩容的机关在 `jyp_reserve_vector`,它负责保证容量够用:

```c
// 扩容只保证"至少装得下 want 个",别的不管
void jyp_reserve_vector(JYPVector* vector, size_t want)
{
    if (want <= vector->capacity_) {
        return;
    }
    size_t new_cap = vector->policy.reallocate_size_policy(vector->capacity_);
    if (new_cap < want) {
        new_cap = want; // 容量 0 起步 / 策略太保守时的兜底
    }
    vector->data_begin_ = reallocate(vector->data_begin_, new_cap * vector->elem_size_); // reallocate 要的是字节!
    vector->capacity_ = new_cap;
}
```

咱们逐行过。want 已经装得下就直接回去,这一笔不花钱。新容量不在这里写死,问 `policy` 要:它是个函数指针,装在结构体里跟咱们走。`new_cap < want` 的兜底照顾两种情况:容量 0 起步时翻倍还是 0,策略步子太小时一口气要得更多,都直接抬到 want。最后一行注释的叹号不是白打的:reallocate 收的是字节数,容量是元素个数,乘 `elem_size_` 这一步忘了就是差四倍的灾难。扩容只动 `data_begin_` 和 `capacity_`,`current_count_` 原地不动,住户还是那些住户。

默认的策略在文件顶部,三行:

```c
static size_t new_policy(size_t old_size) {
    return 2 * old_size;
}
```

两倍。为什么默认是两倍而不是加一或加四?开篇那 17 对 99999 已经把答案演了一遍,下面咱们把这笔搬运的数字亲手加一遍。

## 17 的来历:把搬运加出来

翻倍的账面是这样的:容量从 1 起步(咱们的驱动是 2 起步,数学一样,咱们按 1 算整),每次扩容要搬运的元素个数等于旧容量。于是 17 次扩容的搬运量逐次是 1、2、4、8、……、65536,把它们累计起来:

| 扩容到 | 这一趟搬 | 累计搬运 |
| --- | --- | --- |
| 2 | 1 | 1 |
| 8 | 4 | 7 |
| 32 | 16 | 31 |
| 128 | 64 | 127 |
| 2048 | 1024 | 2047 |
| 131072 | 65536 | 131071 |

规律一眼就露出来了:累计搬运永远是"扩容到"减一,2 的 k 次方减一。换句话说,装了十万个元素,从头到尾搬运的总元素数还不到十三万一千个,平均每个 push 摊到 1.3 个元素的搬运。单次 push 偶尔贵(搬全家),贵得有数,长远平均下来是常数,这就是"摊还 O(1)"的全部意思。正式的摊还分析在第 15 章,那里会给这套直觉立规矩;这里咱们靠计数器把直觉先立住。

+1 策略的账面就难看了:每次满员只长一格,每推一个新元素,咱们都得把前面的全部搬一遍,累计是 1 + 2 + 3 + ... + 99999,约五十亿次。平均每个 push 五万次搬运,二次方的坑。翻倍省下的其实是搬运,这就是它成为默认的理由。

计数器本身是咱们换上去的分配器数的,容器代码一行没动:

```c
static long g_reallocs;
static void* counting_realloc(void* p, size_t n) { g_reallocs++; return realloc(p, n); }
/* counting_alloc / counting_free 同款,各数各的 */

static size_t plus_one(size_t old) { return old + 1; }

int main(void) {
    static const JYPMemoryAllocator counting = {
        .allocator = counting_alloc,
        .deallocator = counting_free,
        .reallocator = counting_realloc,
    };
    SetNewGlobalAllocator(&counting);

    g_reallocs = 0;
    JYPVector* v = jyp_create_vector(sizeof(int), 1);
    for (int i = 0; i < 100000; i++) jyp_push_back_vector(v, &i);
    printf("翻倍策略:100000 次 push,realloc 只调了 %ld 次(容量 %zu)\n",
           g_reallocs, v->capacity_);
    jyp_release_vector(v);

    g_reallocs = 0;
    v = jyp_create_vector(sizeof(int), 1);
    const JYPMemoryAllocatePolicy p1 = { .reallocate_size_policy = plus_one };
    jyp_set_resize_vector_policy(v, &p1);
    for (int i = 0; i < 100000; i++) jyp_push_back_vector(v, &i);
    printf("+1 策略:100000 次 push,realloc 调了 %ld 次(容量 %zu)\n",
           g_reallocs, v->capacity_);
    jyp_release_vector(v);
    return 0;
}
```

注意换策略那个动作:`jyp_set_resize_vector_policy(v, &p1)` 一行就换了装。策略是函数指针,这是第 4 章分配器那套思路的另一半,咱们这个容器从第一天起就把"怎么长"做成了可替换的零件。

这个实验不需要本地环境,您点开就能玩。下面是它的自包含版:

<OnlineCompilerDemo
  title="亲手玩:换您自己的增长策略,数一遍 realloc"
  description="十万次 push,计数分配器全程替咱们数着 realloc 的次数。试着把 plus_one 改成加四或翻三倍,再把起始容量 1 改成 2,看看次数怎么变;答案和这一节的数学对得上才算真懂。"
  allow-run="true"
  run-options="-std=c11 -O2"
  sourcePath="/demos/vector_amort.c"
/>

## CPython 的另一个答案

这个游戏 Python 玩得更抠,咱们跟着看。CPython 的 list 就是动态数组,增长策略写在 `listobject.c` 的 list_resize 里(v3.14.0):

```c
/* This over-allocates proportional to the list size, making room
 * for additional growth.  The over-allocation is mild, but is
 * enough to give linear-time amortized behavior over a long
 * sequence of appends() in the presence of a poorly-performing
 * system realloc().
 * ...
 * The growth pattern is:  0, 4, 8, 16, 24, 32, 40, 52, 64, 76, ...
 */
new_allocated = ((size_t)newsize + (newsize >> 3) + 6) & ~(size_t)3;
```

翻译成人话:新容量等于"要装的数量,加上它的八分之一,再加 6,最后对齐到 4 的倍数"。注释自己把容量序列背了出来,咱们用 `sys.getsizeof` 当探针,append 一路数下去,减去 56 字节的表头、除以 8 字节一个指针,得到实测容量:

| 装到第几个 | getsizeof(B) | 换算容量 | 公式核对 |
| --- | --- | --- | --- |
| 1 | 88 | 4 | 1 + 0 + 6 = 7,对齐到 4 |
| 5 | 120 | 8 | 5 + 0 + 6 = 11,对齐到 8 |
| 9 | 184 | 16 | 9 + 1 + 6 = 16,正好 |
| 17 | 248 | 24 | 17 + 2 + 6 = 25,对齐到 24 |
| 65 | 664 | 76 | 65 + 8 + 6 = 79,对齐到 76 |
| 93 | 920 | 108 | 93 + 11 + 6 = 110,对齐到 108 |

实测曲线和源码公式逐项吻合,一个不差(本机 Python 3.14.6)。为什么 CPython 不翻倍?翻倍的浪费上界是 50%(容量最多差一倍空转),八分之一加 6 的温和增长把空转压在一成多,代价是多搬几次。注释里那句"enough to give linear-time amortized behavior"说得很清楚:它要的也是摊还线性,只是选了另一个平衡点。咱们工程里翻倍起步,同时把策略做成函数指针留着口子,这个组合比任何单一数字都更要紧。

## 交棒

这一章咱们把"装满了怎么办"算清楚了:翻倍不是随便挑的吉利数,它是把搬运摊薄的取舍;CPython 用八分之一加 6 站在另一个平衡点上,两家都成立。可是还有一件事咱们一直按着没讲:增长轨迹那一列地址,2→4 明明写着"搬到",地址却一个字都没动;4→8 说搬家,还真搬了。realloc 到底搬不搬?这个问题牵出一条您写任何 C 代码都用得上的合同,下一章专门拆。咱们下一章见。
