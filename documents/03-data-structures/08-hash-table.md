---
title: "哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找"
description: "阶段3·第8章。BST(第7章)查找是 O(log n),这一章用「哈希函数把 key 直接映射到桶」实现平均 O(1) 的查找/插入/删除——拿空间换时间,字典/Map 的内核。先讲链地址法的结构:一个「桶数组」(指针数组),每个桶挂一条单链表(直接复用第1章的手艺);哈希函数对整数 key 用 `key % 桶数`,不同 key 落同桶就是「哈希冲突」,冲突的节点全挂进同一条链表。真跑 hash_basic.c:桶数 7,插 `10 17 24 31` 四个数(都 % 7 == 3,全挤进 bucket[3] 挂成一条 4 节点链表——这就是「退化成链表」的实锤),再插 `5 11` 分散到别的桶,打印每个桶的链表看清冲突长什么样。再真跑 hash_search.c:search(24) 命中、search(99) 落空(先哈希定位桶再在桶链表里找)。负载因子 = 元素数/桶数,太大就退化;真跑 hash_rehash.c 演示负载因子超 0.75 自动扩容 7→17 桶并 rehash 所有 key(复用节点不重新 malloc),负载因子从 1.14 降到 0.47。坑:哈希函数选不好一堆 key 挤同桶退化成链表 O(n)、C 的 `%` 对负 key 返回负数要先转正、扩容必须 rehash 所有 key(哈希函数依赖桶数)。释放要逐个桶的链表 free(先存 next 再 free,呼应第1章)+桶数组本身是固定数组不用单独 free。所有代码块即真跑版,gcc16+clang22 双跑 -std=c11 -Wall -Wextra,free 用 ASan+UBSan 复核退出码 0 无泄漏。"
chapter: 3
order: 8
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第1章:单链表(链地址法的桶链表就是它)"
  - "第 10 章:数组(桶数组)、阶段2·第6/7章:动态内存"
related:
  - "阶段3·第7章:BST(O(log n) 查找对照)、第11章:二分查找(O(log n))、第12章:大 O(O(1) 平均)"
---

# 哈希表:链地址法、哈希函数、冲突与 O(1) 平均查找

## 引言:从 O(log n) 到平均 O(1)

上一章我们写了 BST(第7章),它的查找/插入/删除是 O(log n)——靠每次比较把候选范围砍一半,在一百万个 key 里找东西大约只要 20 次比较。这已经比数组的 O(n) 顺序查找快了不知道多少倍,可你仔细想:每次比较都得「沿着树往下一层层走」,这是因为 BST 的 key 是**有序存放**的,你得靠比较来定位。那有没有可能更狠一点——**根本不比较,直接算出 key 该放在哪**?哈希表就是这个思路:用一个**哈希函数**(hash function)把 key 直接映射到一个数组的下标(叫**桶** bucket),你要找 `key=24`,算一下 `hash(24)` 得到「它在 bucket[3]」,直接跳到 bucket[3] 去拿——理想情况下一步到位,这就是平均 O(1) 的来历。字典、Map、集合、数据库的索引、缓存(LRU)、符号表……背后全是哈希表,它是计算机科学里最值钱的一个数据结构,拿一点点额外空间换来查找速度从 O(log n) 干到 O(1)。

但 O(1) 是**平均**情况,它的前提是「哈希函数把 key 均匀地撒到各个桶里」。现实是 key 的取值往往有规律,不同的 key 完全可能被哈希函数算到**同一个桶**——这叫**哈希冲突**(collision)。比如我们这一章用的哈希函数是 `key % 7`(key 除以 7 取余),那 `10`、`17`、`24`、`31` 这四个 key 的余数全是 3,全落 bucket[3]——一个桶塞了 4 个 key,这时候再想找其中一个,就只能在这个桶里一个一个比了。冲突一旦严重,哈希表就退化成它本来要打败的东西:一条链表,查找回到 O(n)。所以哈希表的全部工程难度,都集中在「怎么处理冲突」和「怎么让 key 分布均匀」这两件事上。

## 链地址法:一个桶数组,每个桶挂一条链表

处理冲突有好几种办法(开放寻址、再哈希……),最直观也最经典的是**链地址法**(separate chaining):桶本身是一个**指针数组**,每个桶 `buckets[b]` 存的是「一条单链表的头指针」;凡是哈希到同一个桶的 key,都挂进那条链表里。这样冲突就不是问题了——来一个 key 算出它在 bucket[3],bucket[3] 那条链表里可能有零个、一个、好几个节点,顺着链表找一遍就行。这正是第1章单链表手艺的直接复用:节点是 `malloc` 来的、靠 `next` 串起来、释放时要先存 `next` 再 `free`(那个 use-after-free 的坑第1章用 ASan 真跑过,这里一模一样)。

结构体长这样:一个固定大小的「桶数组」(每个元素是个 `Node*`,初始全 `NULL`),外加一个 `size` 记录已经存了多少个元素(后面算负载因子要用):

```c
#include <stdio.h>
#include <stdlib.h>

#define N_BUCKETS 7

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct {
    Node* buckets[N_BUCKETS]; /* 桶数组:每个桶是一条链表的头 */
    size_t size;              /* 已存元素个数 */
} HashTable;
```

`Node` 和第1章的单链表节点是一个模子——`int key` 存数据、`struct Node* next` 指向下一个节点(自引用必须写全名 `struct Node*`,不能图省事写 `Node*`,因为 typedef 别名此刻还没生效,这个坑第1章用 gcc/clang 双跑报过 `unknown type name` 了)。`buckets[N_BUCKETS]` 是个指针数组,每个槽位存一个 `Node*`,也就是「这条桶链表的头」;表空的时候所有桶都是 `NULL`。注意这个桶数组本身是**结构体里的固定数组**,它在栈上(或随结构体一起分配),不需要单独 `malloc`、也不需要单独 `free`——真正要从堆上申请、最后要 `free` 的是链表里的那些**节点**,别搞反了。

## 哈希函数:key % 桶数,以及负数的那个坑

哈希函数的工作是把任意一个 key 压成一个「桶下标」——对整数 key,最朴素的办法就是取模:`hash(key) = key % N_BUCKETS`。`%` 是 C 的取余运算符(阶段1·Ch5),`10 % 7` 得 3、`17 % 7` 得 3、`5 % 7` 得 5,余数一定落在 `[0, 7)` 区间,正好当 7 个桶的下标用。这一行就是整个哈希表「不用比较直接定位」的灵魂:

```c
static unsigned hash(int key) {
    return (unsigned) (key % N_BUCKETS); /* 整数 key:简单取模 */
}
```

这里有个**新手一定会撞的坑**,等我们的 key 涉及负数时才会爆:ISO/IEC 9899:2011 §6.5.5p6 规定,`a % b` 的符号**和被除数 `a` 同号**——也就是说 `-10 % 7` 在 C 里得 `-3`,不是 `3`。如果你直接拿这个 `-3` 当桶下标,`buckets[-3]` 就是数组负下标越界,直接 UB(读一块完全不相关的内存)。所以我们写成「先取模、再 if 负数就加一个桶数」的修法(后面 rehash 版会这么写)。本章前面几个 demo 的 key 全是正整数,`key % 7` 永远非负,可以暂时不管;但你心里要清楚——**只要 key 可能是负的,取模后就必须判负修正**,这是 C 取余运算符的固有特性,C++ 的 `%` 也是同一套规则。顺带一提,桶数为什么喜欢选**质数**(7、17、101)?这是经验法则:当 key 有规律(比如全是 7 的倍数)时,模一个质数能让余数分布更散;模一个合数(比如 8)则可能让一堆 key 挤进少数几个桶。本章为了演示冲突,故意用了小的质数 7。

## 插入:头插,冲突就往链表前面挂

插入的逻辑分两步:先哈希定位桶,再往那条桶链表的**头部**挂新节点。为什么头插而不是尾插?因为头插是 O(1)(第1章讲过,新节点 `next` 指旧头、返回新头,两步固定动作),尾插要走到链尾是 O(链表长度)——哈希表本来就图个快,插入当然选 O(1) 的头插。冲突的 key 全往同一条链表的头部追加,后插的在最前面:

```c
/* 头插:冲突就往链表前面挂 */
static void insert(HashTable* t, int key) {
    unsigned b = hash(key);
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return; /* malloc 失败,原表不动(阶段2·Ch6) */
    }
    n->key = key;
    n->next = t->buckets[b]; /* 接到旧头前面 */
    t->buckets[b] = n;       /* 新节点成为新头 */
    t->size++;
}
```

这两步和第1章单链表的 `push_front` 是一个模子:`n->next = t->buckets[b]` 让新节点指着原来的链表头,`t->buckets[b] = n` 再把桶的头指针更新为新节点。差别只在于第1章的「头」是个 `Node* head` 变量,这里的「头」是 `buckets[b]` 这个数组槽位——但本质上都是「一个存着 `Node*` 的位置」,改它就改了链表的头。`t->size++` 顺手记一笔,后面算负载因子要用。同样别忘了 `malloc` 后查 `NULL`(§7.22.3),还有节点的 `next` 必须被赋值——这里 `n->next = t->buckets[b]` 已经把它指向了旧头(空桶时旧头是 `NULL`,正好让新节点成为链尾),所以不用单独 `n->next = NULL`。

现在把这套结构搭起来真跑,看看冲突长什么样。桶数 7,先插四个 `10 17 24 31`(它们的 `% 7` 全是 3,会全挤进 bucket[3]),再插 `5 11`(分别落 bucket[5] 和 bucket[4],分散开),然后打印每个桶的链表:

```c
#include <stdio.h>
#include <stdlib.h>

#define N_BUCKETS 7

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct {
    Node* buckets[N_BUCKETS]; /* 桶数组:每个桶是一条链表的头 */
    size_t size;              /* 已存元素个数 */
} HashTable;

static unsigned hash(int key) {
    return (unsigned) (key % N_BUCKETS); /* 整数 key:简单取模 */
}

/* 头插:冲突就往链表前面挂 */
static void insert(HashTable* t, int key) {
    unsigned b = hash(key);
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return;
    }
    n->key = key;
    n->next = t->buckets[b]; /* 接到旧头前面 */
    t->buckets[b] = n;       /* 新节点成为新头 */
    t->size++;
}

static void print_table(const HashTable* t) {
    for (unsigned b = 0; b < N_BUCKETS; b++) {
        printf("bucket[%u]:", b);
        const Node* cur = t->buckets[b];
        while (cur != NULL) {
            printf(" %d", cur->key);
            cur = cur->next;
        }
        printf("\n");
    }
}

static void free_table(HashTable* t) {
    for (unsigned b = 0; b < N_BUCKETS; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next; /* 先存 next,再 free(呼应阶段3 Ch1) */
            free(cur);
            cur = next;
        }
        t->buckets[b] = NULL;
    }
    t->size = 0;
}

int main(void) {
    HashTable t = {0}; /* 桶数组 + size 全清零 */

    /* 这四个都 % 7 == 3,全部挤进 bucket[3]——这就是哈希冲突 */
    insert(&t, 10);
    insert(&t, 17);
    insert(&t, 24);
    insert(&t, 31);
    /* 这两个落别的桶:5%7=5、11%7=4,各自独占 */
    insert(&t, 5);
    insert(&t, 11);

    print_table(&t);

    free_table(&t);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra hash_basic.c -o hb && ./hb
bucket[0]:
bucket[1]:
bucket[2]:
bucket[3]: 31 24 17 10
bucket[4]: 11
bucket[5]: 5
bucket[6]:
```

看 `bucket[3]` 那一行:`31 24 17 10`——四个 key 全挤在这一个桶里,而且顺序是反的(头插,后插的在最前:最后插的 `31` 在链表头、最先插的 `10` 在链表尾)。这就是哈希冲突最直白的样子:**一个桶挂了 4 个节点,变成了一条长度为 4 的链表**。如果这时候你想在这个桶里找 `10`,得从链表头 `31` 开始一个一个比,比 4 次才找到——这已经偏离了「O(1) 一步到位」的理想,这正是「哈希函数选不好、key 又有规律」时哈希表退化成链表的实锤(后面讲负载因子会专门治它)。反观 `bucket[4]` 和 `bucket[5]` 各自只挂了 1 个 key,找它们就是货真价实的 O(1)。

`HashTable t = {0};` 这个写法值得多说一句:它把整个结构体(包括桶数组和 `size`)一次性清零(§6.7.9p19,这种 `{0}` 是 C 给聚合类型的「全零初始化」简写),所有桶的头指针都变成 `NULL`、`size` 是 0——空表的合法初始状态。千万别漏这一步,否则 `buckets` 里是栈上的垃圾地址,`insert` 时 `n->next = t->buckets[b]` 就把新节点接到了一个随机地址上,后面遍历必崩。

## 查找:先哈希定位桶,再在桶链表里找

查找的逻辑和插入是对称的:同样先 `hash(key)` 算出 key 在哪个桶,然后顺着那条桶链表一个节点一个节点比 `cur->key == key`——命中就返回,链表走完还没找到就是「不在表里」。这套「定位 + 顺序找」的组合,就是哈希表查找的全部:

```c
/* 查找:先哈希定位桶,再在桶链表里顺 next 找 */
static bool search(const HashTable* t, int key) {
    unsigned b = hash(key);
    const Node* cur = t->buckets[b];
    while (cur != NULL) {
        if (cur->key == key) {
            return true; /* 命中 */
        }
        cur = cur->next;
    }
    return false; /* 桶链表走完没找到 */
}
```

注意 `search` 用的 `hash` 函数和 `insert` 是**同一个**——这是哈希表正确性的根基:key 插的时候算出在 bucket[3],找的时候必须还算出 bucket[3],否则永远找不到。`const Node* cur` 写成 `const`(阶段2·Ch4)是表明「查找只读、不改节点」,`cur = cur->next` 顺着链表走和第1章单链表遍历是一回事。理想情况(冲突少)下,某个桶的链表很短,这一趟 while 几乎立刻结束——这就是平均 O(1);最坏情况(一堆 key 挤同桶)下,这条链表可能很长,退化到 O(n)。

真跑一遍,查一个表里有的 `24`、再查一个表里没有的 `99`:

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define N_BUCKETS 7

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct {
    Node* buckets[N_BUCKETS];
    size_t size;
} HashTable;

static unsigned hash(int key) {
    return (unsigned) (key % N_BUCKETS);
}

static void insert(HashTable* t, int key) {
    unsigned b = hash(key);
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return;
    }
    n->key = key;
    n->next = t->buckets[b];
    t->buckets[b] = n;
    t->size++;
}

/* 查找:先哈希定位桶,再在桶链表里顺 next 找 */
static bool search(const HashTable* t, int key) {
    unsigned b = hash(key);
    const Node* cur = t->buckets[b];
    while (cur != NULL) {
        if (cur->key == key) {
            return true; /* 命中 */
        }
        cur = cur->next;
    }
    return false; /* 桶链表走完没找到 */
}

static void free_table(HashTable* t) {
    for (unsigned b = 0; b < N_BUCKETS; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next;
            free(cur);
            cur = next;
        }
        t->buckets[b] = NULL;
    }
    t->size = 0;
}

int main(void) {
    HashTable t = {0};

    insert(&t, 10);
    insert(&t, 17);
    insert(&t, 24);
    insert(&t, 31); /* 这四个都落 bucket[3] */
    insert(&t, 5);
    insert(&t, 11);

    /* 24 落 bucket[3],那条链表里有它 */
    int hit = 24;
    bool found_hit = search(&t, hit);
    printf("search(%d) -> %s\n", hit, found_hit ? "found" : "not found");

    /* 99 落 bucket[1](99%7=1),那条链表是空的 */
    int miss = 99;
    bool found_miss = search(&t, miss);
    printf("search(%d) -> %s\n", miss, found_miss ? "found" : "not found");

    free_table(&t);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra hash_search.c -o hs && ./hs
search(24) -> found
search(99) -> not found
```

`search(24)`:`hash(24) == 3`,跳到 bucket[3],那条链表是 `31 -> 24 -> 17 -> 10`,从头顺着比,第二个节点就是 `24`,命中。`search(99)`:`hash(99) == 1`(99 = 14×7 + 1),跳到 bucket[1]——它是个空桶(`buckets[1] == NULL`),`while (cur != NULL)` 一次都不进,直接返回 `false`。这里有个小细节:`printf` 的参数里我**先把 `search()` 的返回值存进 `found_hit`/`found_miss` 变量,再传给 printf**,而不是写成 `printf("...", search(&t, 24))`。这是因为函数实参的求值顺序在 C 里是**未指定**的(§6.5p2),如果你把好几个有副作用的函数调用塞进同一条 `printf` 参数里,不同编译器可能按不同顺序求值——拆开先存变量再打印,行为就完全确定。这是个不起眼但实战会咬人的坑,值得养成习惯。

## 负载因子与扩容:别让链表太长

前面我们看到 bucket[3] 挂了 4 个节点,找 `10` 要比 4 次。链表越长,退化越严重,那怎么衡量「链表是不是要太长了」?用**负载因子**(load factor),它就是「元素总数 / 桶数」:`load = size / n_buckets`。这个比值直观上等于「平均每个桶挂了几个节点」——负载因子 1.0 意味着平均每个桶 1 个节点(理想状态,链表长度大概 1,查找几乎 O(1));负载因子 4.0 意味着平均每个桶 4 个节点(链表普遍很长,查找开始吃力)。工程上的经验阈值大约是 **0.75**:一旦 `load > 0.75`,就**扩容**——开一个更大的桶数组,把所有 key **重新哈希**(rehash)搬到新桶里去,把负载因子压下来。

为什么要 rehash 而不是「直接把桶数组变大」?因为每个 key 落哪个桶是由 `hash(key) = key % n_buckets` 算出来的,**这个公式依赖桶数 `n_buckets`**。桶数从 7 变成 17 之后,同一个 key 算出的桶下标会变:`24 % 7 == 3`(老桶),`24 % 17 == 7`(新桶)——如果你只把桶数组变大却不重新搬,`24` 还傻傻挂在老的 bucket[3],而 `search` 会去新的 bucket[7] 找,永远找不到。所以扩容必须把每个 key 用**新桶数**重新算一遍哈希、搬到新桶,这个过程就叫 **rehash**。这是个 O(n) 的大动作(所有 key 都要搬一遍),但因为它只在偶尔的瞬间发生(摊还分析下来每次插入分摊到 O(1),第12章会正式讲),所以哈希表的平均性能仍然是 O(1)。

下面这个 demo 真跑一遍扩容:起始 7 个桶,逐个插 `{10, 17, 24, 31, 5, 11, 21, 38}` 八个 key,每插一个就检查负载因子,超过 0.75 就自动扩容到 17 个桶并 rehash。注意 rehash 时我们**复用节点、不重新 malloc**——把每个节点从老链表上摘下来、用新桶数重新算哈希、再挂到新桶;节点对象本身一个都不浪费:

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define LOAD_FACTOR_THRESHOLD 0.75 /* 负载因子超 0.75 触发扩容 */

typedef struct Node {
    int key;
    struct Node* next;
} Node;

typedef struct {
    Node* buckets[32]; /* 固定上限,简化教学;真实库会动态分配桶数组 */
    size_t n_buckets;
    size_t size;
} HashTable;

static unsigned hash_key(int key, size_t n_buckets) {
    /* 哈希函数依赖桶数:换桶数结果就变,这是 rehash 的关键 */
    int m = (int) n_buckets;
    int r = key % m; /* C 的 % 结果含符号,先转正 */
    if (r < 0) {
        r += m;
    }
    return (unsigned) r;
}

static void ht_init(HashTable* t, size_t n_buckets) {
    t->n_buckets = n_buckets;
    t->size = 0;
    for (size_t i = 0; i < n_buckets; i++) {
        t->buckets[i] = NULL;
    }
}

static void insert_no_resize(HashTable* t, int key) {
    unsigned b = hash_key(key, t->n_buckets);
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return;
    }
    n->key = key;
    n->next = t->buckets[b];
    t->buckets[b] = n;
    t->size++;
}

static double load_factor(const HashTable* t) {
    return (double) t->size / (double) t->n_buckets;
}

static void print_table(const HashTable* t) {
    for (size_t b = 0; b < t->n_buckets; b++) {
        printf("[%2zu]:", b);
        const Node* cur = t->buckets[b];
        while (cur != NULL) {
            printf(" %d", cur->key);
            cur = cur->next;
        }
        printf("\n");
    }
}

/* 扩容 + rehash:把所有 key 重新哈希到新桶数组 */
static void rehash(HashTable* t, size_t new_n_buckets) {
    size_t old_n = t->n_buckets;

    /* 新桶数组(用栈上固定 buffer 演示;真实库用 malloc/realloc 动态分配) */
    Node* new_buckets[32];
    for (size_t i = 0; i < new_n_buckets; i++) {
        new_buckets[i] = NULL;
    }

    /* 遍历每条旧链表,把节点摘下来重新哈希挂到新桶(不重新 malloc,复用节点) */
    for (size_t b = 0; b < old_n; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next; /* 先存,下面要改 cur->next */
            unsigned nb = hash_key(cur->key, new_n_buckets);
            cur->next = new_buckets[nb]; /* 头插到新桶 */
            new_buckets[nb] = cur;
            cur = next;
        }
    }

    /* 把新桶数组拷回结构体,更新桶数 */
    for (size_t i = 0; i < new_n_buckets; i++) {
        t->buckets[i] = new_buckets[i];
    }
    t->n_buckets = new_n_buckets;
}

static void insert(HashTable* t, int key) {
    insert_no_resize(t, key);
    /* 插完检查负载因子,超阈值就扩容(翻倍取质数,这里 7->17) */
    if (load_factor(t) > LOAD_FACTOR_THRESHOLD) {
        rehash(t, 17);
    }
}

static bool search(const HashTable* t, int key) {
    unsigned b = hash_key(key, t->n_buckets);
    const Node* cur = t->buckets[b];
    while (cur != NULL) {
        if (cur->key == key) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}

static void free_table(HashTable* t) {
    for (size_t b = 0; b < t->n_buckets; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next;
            free(cur);
            cur = next;
        }
        t->buckets[b] = NULL;
    }
    t->size = 0;
}

int main(void) {
    HashTable t;
    ht_init(&t, 7); /* 起始 7 个桶 */

    int keys[] = {10, 17, 24, 31, 5, 11, 21, 38};
    size_t nkeys = sizeof(keys) / sizeof(keys[0]);

    /* 逐个插入,并在负载因子超 0.75 时自动扩容到 17 桶 */
    for (size_t i = 0; i < nkeys; i++) {
        insert(&t, keys[i]);
    }

    printf("after insert: size=%zu, n_buckets=%zu, load=%.3f\n", t.size, t.n_buckets,
           load_factor(&t));
    print_table(&t);

    /* 扩容后查找仍然正确(rehash 没丢东西) */
    bool f24 = search(&t, 24);
    bool f99 = search(&t, 99);
    printf("search(24) -> %s\n", f24 ? "found" : "not found");
    printf("search(99) -> %s\n", f99 ? "found" : "not found");

    free_table(&t);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra hash_rehash.c -o hr && ./hr
after insert: size=8, n_buckets=17, load=0.471
[ 0]: 17
[ 1]:
[ 2]:
[ 3]:
[ 4]: 38 21
[ 5]: 5
[ 6]:
[ 7]: 24
[ 8]:
[ 9]:
[10]: 10
[11]: 11
[12]:
[13]:
[14]: 31
[15]:
[16]:
search(24) -> found
search(99) -> not found
```

看清楚发生了什么:8 个 key 插进 7 个桶,负载因子是 8/7 ≈ 1.14,早就超过 0.75 了,所以 `insert` 在某个时刻触发 `rehash(t, 17)`,桶数从 7 翻到 17。rehash 之后,每个 key 都用 `% 17` 重新算了一遍:`17 % 17 == 0`、`24 % 17 == 7`、`38 % 17 == 4`、`21 % 17 == 4`、`5 % 17 == 5`、`11 % 17 == 11`、`10 % 17 == 10`、`31 % 17 == 14`。负载因子从 1.14 降到了 `8/17 ≈ 0.471`,每个桶最多挂 2 个节点(`[ 4]: 38 21` 还是一对,因为 38 和 21 模 17 都是 4——这是哈希函数本身的局限,扩容只能缓解、不能消灭冲突)。`search(24)` 在扩容后仍然命中(bucket[7]),证明 rehash 一个 key 都没丢。

rehash 那段循环是整个 demo 最值得拆开看的部分:外层 `for` 遍历每一条**旧**桶链表,内层 `while` 沿着链表走;对每个节点 `cur`,先 `Node* next = cur->next` 把它的下一个存下来(因为下一步要改 `cur->next`,不存就丢了后续),然后用**新桶数** `new_n_buckets` 调 `hash_key` 算它的新家,`cur->next = new_buckets[nb]` 让它指着新桶的旧头、`new_buckets[nb] = cur` 把它挂上去——和 `insert` 的头插是一回事,只不过这次插的是「从老链表上摘下来的旧节点」,完全不 `malloc`。这套「摘下 → 重算 → 头插」的循环走完,所有节点都搬到了新桶数组,老桶数组里那些头指针此刻指着的位置已经被新桶覆盖(`t->buckets[i] = new_buckets[i]`),节点一个不漏、一个不多 malloc,干净。注意 `hash_key` 里我特意写了 `if (r < 0) { r += m; }`——就是前面讲的那个 C 取余符号坑,这里 `key` 可能来自任意场景、不保证非负,所以取模后判负修正,杜绝负下标 UB。

这里还有个**教学版的诚实交代**:我把桶数组写成 `Node* buckets[32]` 固定大小(够装 17 个桶),是为了让 demo 短一点、不引入 `realloc` 桶数组的额外复杂度。真实库(比如 glibc 的 `hsearch`、C++ 的 `std::unordered_map`)桶数组是 `malloc` 出来的、扩容时用 `realloc` 或重新申请更大的数组——那样桶数组本身也要参与生命周期管理(申请、搬、释放)。本章的重点是「链地址法 + 哈希函数 + 冲突 + rehash 的机制」,桶数组的动态化是工程细节,留给真去做项目的时候碰。

## 释放:每个桶的链表 free,桶数组本身不用动

哈希表用完要释放,但这里有个新手容易搞混的点:**到底要 free 什么**?答案是只 free 那些 `malloc` 出来的**节点**——也就是每条桶链表上的所有节点。桶数组 `buckets[N_BUCKETS]` 本身是结构体里的**固定数组**(教学版),随结构体分配、随结构体回收,不需要也不能 `free` 它(`free` 一个栈上的数组是 UB,阶段2·Ch7 讲过 free 的语义)。所以释放逻辑就是:外层 `for` 遍历每个桶,内层 `while` 沿着那条桶链表走,走一个 free 一个——和第1章单链表的 `free_list` 一模一样,关键是 `free` 当前节点之前**先把它的 `next` 存下来**,否则就是经典的 use-after-free:

```c
static void free_table(HashTable* t) {
    for (unsigned b = 0; b < N_BUCKETS; b++) {
        Node* cur = t->buckets[b];
        while (cur != NULL) {
            Node* next = cur->next; /* 先存!free 之后 cur 这块就不能再碰 */
            free(cur);
            cur = next;
        }
        t->buckets[b] = NULL; /* 顺手置空,防悬垂 */
    }
    t->size = 0;
}
```

`Node* next = cur->next;` 这一行是第1章我们用 ASan 真跑出血的教训:`free(cur)` 之后 `cur` 指着的那块堆内存已经还给系统了,再去读 `cur->next` 就是 use-after-free,gcc 能在编译期甩 `-Wuse-after-free` 警告、ASan 运行期直接 abort 报 `heap-use-after-free`。这里必须趁 `cur` 还有效先把 `next` 抄到局部变量,`free(cur)` 之后用 `next` 往后跳,完全不需要碰已释放内存。`t->buckets[b] = NULL;` 是好习惯(防悬垂),虽然在这个 demo 里马上要析构整个表、置不置空区别不大,但养成「free 完相关指针立刻置 NULL」的肌肉记忆能省掉后面无穷无尽的 UAF 噩梦(阶段2·Ch7 的那套纪律)。把这个 `free_table` 接到前面三个 demo 末尾,用 ASan 复核一遍:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer \
      hash_basic.c -o hb_asan && ./hb_asan; echo "exit=$?"
bucket[0]:
bucket[1]:
bucket[2]:
bucket[3]: 31 24 17 10
bucket[4]: 11
bucket[5]: 5
bucket[6]:
exit=0
```

退出码 `0`、ASan 一声不吭——6 个节点(bucket[3] 的 4 个 + bucket[4] 的 1 个 + bucket[5] 的 1 个)都被妥善 free 掉了,`free_table` 一个不漏。rehash 那个 demo 同样过 ASan:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer \
      hash_rehash.c -o hr_asan && ./hr_asan; echo "exit=$?"
after insert: size=8, n_buckets=17, load=0.471
[ 0]: 17
[ 1]:
[ 2]:
[ 3]:
[ 4]: 38 21
[ 5]: 5
[ 6]:
[ 7]: 24
[ 8]:
[ 9]:
[10]: 10
[11]: 11
[12]:
[13]:
[14]: 31
[15]:
[16]:
search(24) -> found
search(99) -> not found
exit=0
```

8 个节点全程只被 `malloc` 了 8 次(rehash 复用节点、不重新分配)、最后被 `free` 了 8 次,进出平衡,ASan 没报任何 `detected memory leaks`。注意 rehash 那一通搬运动作特别容易出 UAF 或丢节点——摘节点的顺序稍有差池就会把链表搞断或重复 free,ASan 这一关能把绝大多数搬运 bug 当场抓出来,所以写完哈希表(以及任何搬动节点的代码)务必过一遍 ASan 才算数。

## 小结

哈希表是用阶段3·第1章的单链表手艺搭起来的、靠「哈希函数把 key 直接映射到桶」实现平均 O(1) 查找的数据结构,它拿一点点额外空间(桶数组)换来查找速度从 BST 的 O(log n) 干到 O(1),是字典/Map/缓存/符号表的内核。**链地址法**的结构是一个桶数组(指针数组),每个桶 `buckets[b]` 存的是一条单链表的头;凡是 `hash(key)` 算到同一个桶的 key(这就是**哈希冲突**),都挂进那条链表里——节点定义、头插、遍历、释放全复用第1章,连「free 前先存 next 否则 use-after-free」那个坑都一模一样。**哈希函数**对整数 key 用 `key % 桶数`(§6.5.5),桶数宜选质数让 key 散得开;这里有个 C 取余的固有坑——`a % b` 的符号跟被除数 `a` 同号(§6.5.5p6),`-10 % 7` 得 `-3`,所以 key 可能是负数时取模后必须 `if (r < 0) r += m` 修正,否则负下标越界 UB。**插入**是头插 O(1)(新节点 `next` 指旧头、桶头指针更新为新节点),**查找**是先哈希定位桶再在桶链表里顺 `next` 比 `key`——理想(冲突少)O(1),最坏(一堆 key 挤同桶、退化成链表)O(n),所以哈希函数选不好、key 又有规律时哈希表会退化成它本来要打败的链表(我们真跑的 `bucket[3]: 31 24 17 10` 就是退化实锤)。**负载因子** `size / n_buckets` 衡量「平均每桶几个节点」,工程阈值约 0.75,超了就**扩容**——开更大的桶数组并 **rehash** 所有 key(因为 `hash(key) = key % n_buckets` 依赖桶数,桶数一变每个 key 的新位置都得重算);rehash 是 O(n) 但偶尔发生、摊还下来插入仍平均 O(1),真跑 demo 里我们复用节点(摘下→重算→头插,不重新 malloc)把 7 桶扩到 17 桶、负载因子从 1.14 降到 0.471。**释放**只 free 那些 malloc 出来的链表节点(每条桶链表逐个 free,先存 next),桶数组本身是结构体里的固定数组、随结构体回收不能单独 free。还有个不起眼的实战坑:别把 `search()` 这种有副作用的调用直接塞进 `printf` 参数列表——函数实参求值顺序未指定(§6.5p2),先存变量再打印才稳妥。这一章的哈希表是教学版(固定桶数组 + 整数 key),真实库里桶数组是动态 `malloc`/`realloc` 的、key 可以是任意类型(字符串用 djb2 之类的字符串哈希)、还可能用开放寻址代替链地址;但「哈希函数 + 桶 + 冲突处理 + 负载因子 + rehash」这套骨架是不变的。下一章我们离开「查找结构」,回到排序——冒泡、插入、选择这三种 O(n²) 的基础排序,真跑对照它们的性能。

## 参考资源

- ISO/IEC 9899:2011 §6.5.5p5/p6(乘除运算符:`%` 取余的语义、结果的符号与被除数同号)、§6.7.2.1(结构与成员声明:自引用 `struct Node*` 合法)、§6.7.9p19(聚合类型的 `{0}` 全零初始化)、§7.22.3(`malloc`/`free`,返回 NULL 表示失败)、§6.5p2(函数实参求值顺序未指定)
- Donald E. Knuth《The Art of Computer Programming》Vol. 3 Sorting and Searching 第 6.4 节 Hashing(链地址法、开放寻址、负载因子与 rehash 的经典理论分析,哈希函数的选取)
- Robert Sedgewick《Algorithms in C》第 14 章 Hashing(链地址法的 C 实现、负载因子阈值、扩容的摊还分析)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 6 章 第 6.6 节(用哈希表实现的表查找例子 `struct nlist *next` 自引用 + `hash` 函数,经典到不能再经典)
- 阶段3·第1章 单链表(链地址法的桶链表就是它,头插/free 先存 next 的坑一脉相承)、第7章 BST(O(log n) 查找对照)、第11章 二分查找(O(log n))、第12章 算法复杂度与大 O(O(1) 平均 vs O(n) 最坏、摊还分析)
- 阶段2·第6章 malloc/free(节点申请、必查 NULL)、第7章 动态内存的坑(use-after-free、ASan 抓)、第4章 const(`search` 参数 `const Node*`)
