---
title: "动态数组(四):谁拥有,谁善后"
description: "手搓动态数组四部曲的收官之册。分配器可换装:JYPMemoryAllocator 三函数指针 + SetNewGlobalAllocator,测试里换上计数分配器断言 create 两笔 alloc、三次 push 一笔 realloc、release 两笔 free,一笔不多一笔不少;原作者亲踩的坑原样交代:reallocate_raw 忘写 return,非 void 函数漏 return 被读返回值是未定义行为,gcc -Wall -Wextra 的 -Wreturn-type 警告真输出在案。元素是字节:存 char* 时槽位与字符串两层地址,release_with+dtor 的 free_str 要 *(char**)elem 解一层,把 free 直接当 dtor 等于对着缓冲区中间调 free;漏 dtor 的 LSan 翻车现场(17 字节 / 3 块,正好 hello+jyp+vector);clone 浅拷贝的后事归属。结尾诚实拆 OOM 边界:ulimit -v 256Mi 实测 push 到 128Mi 满员、要 256Mi 失败,exit 139 无遗言;realloc 失败旧块活着的探针证据;tmp 模式与 new_cap*elem_size_ 溢出守卫留作练习。四组合(gcc/clang × sanitizer)全部通过。"
chapter: 3
order: 4
tags:
  - host
  - data-structures
  - memory
difficulty: intermediate
reading_time_minutes: 11
platform: host
c_standard: [23]
prerequisites:
  - "第 1 章:动态数组(一),一块会长大的连续内存"
  - "第 3 章:动态数组(三),realloc 的两副面孔(失败语义)"
  - "指针与内存·第 7 章:动态内存的坑(LSan/free 只认开头)"
  - "指针与内存·第 9 章:函数指针(分配器与 dtor 全靠它)"
related:
  - "第 5 章:单链表(另一个极端:每个节点单独 malloc)"
  - "练习 3.5:动态数组(补 tmp 模式、写溢出守卫、换增长策略)"
  - "深水篇预告:arena 与对象池(把这章的分配器口子走下去)"
---

# 谁拥有,谁善后

前三章咱们把动态数组的里里外外看了个遍:字段与不变式、增长的数学、realloc 的合同。收官这一章处理三件一直悬着的事:内存从哪儿来(答案是一套可以换装的分配器)、装的东西里有指针时拆房子谁办后事(所有权的显式约定)、以及内存不够的那一天(失败路径的诚实拆解)。这三件事有个共同的主题:malloc 给的每一样东西,都得有人负责到底。

## 房子从哪儿来:可以换装的分配器

咱们前面一直按下的伏笔现在收:create 和 reserve 里的 `allocate_elements`、`reallocate`、`release`,都落在 common 层的薄封装上。三个函数指针组成一个分配器:

```c
typedef void* (*JYPMemoryAllocateFn)(const size_t size);
typedef void* (*JYPMemoryDeAllocateFn)(void* buffer);
typedef void* (*JYPMemoryReAllocateFn)(void* buffer, const size_t new_size);

typedef struct {
    JYPMemoryDeAllocateFn deallocator;
    JYPMemoryAllocateFn allocator;
    JYPMemoryReAllocateFn reallocator;
} JYPMemoryAllocator;
```

默认实现就是 malloc 家族,`SetNewGlobalAllocator` 换全局默认,传 NULL 换回默认。为什么要留这个口子?上一章数 realloc 次数时咱们已经尝到甜头了:测试里换上计数分配器,断言是 create 两笔 alloc(结构体加缓冲)、三个 push 一笔 realloc、release 两笔 free,一笔不多一笔不少。再往后,同一扇门里可以换成 arena、换成内存池,容器代码一行不改。这一层先埋到这里,深水篇讲分配器时咱们从这扇门进去。

这一层笔者还摔过一跤,值得原样交代。`reallocate_raw` 现在的样子:

```c
static void* default_c_realloc(void* ptr, const size_t new_size) {
    return realloc(ptr, new_size);
}
```

当年那一行里,笔者少打过 return。非 void 函数漏了 return 还被调用方读返回值,标准定为未定义行为:寄存器里恰好剩什么就返回什么,拿到垃圾指针的人还接着往下用,炸在哪一步全看运气。编译器其实提醒过,警告开着就有:

```text
$ gcc -Wall -Wextra no_return.c
no_return.c: In function ‘my_realloc’:
no_return.c:4:1: warning: no return statement in function returning non-void [-Wreturn-type]
```

现在 [allocation.c](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/projects/journey_your_pack/common/src/internal/allocation.c) 里那一行 return 上头留着一行注释记着这桩旧案,笔者没舍得删:`// ← 原来这里忘了 return,返回的全是垃圾`。-Wall -Wextra 常开不是仪式感,就是替咱们盯这类不声不响的哑弹。

## 数组不知道里面住的是谁

容器眼里的元素只是 `elem_size_` 个字节,存 int 时咱们传 `&x` 进去、拿指针解出来用。存 `char*` 时麻烦来了:槽位里放的是指针本体,指针另指一块堆上的字符串,两层地址。这一层玩脱了就是泄漏和双释放的温床,咱们慢慢看。

咱们看出问题的用法,就在 example 的末段:

```c
JYPVector* sv = jyp_create_vector(sizeof(char*), 2);
const char* raws[] = {"hello", "jyp", "vector"};
for (int i = 0; i < 3; i++) {
    char* p = dup_str(raws[i]);
    jyp_push_back_vector(sv, (const void*)&p);
}
jyp_release_vector_with(sv, free_str); // 先逐个 free 字符串,再拆房子
```

字符串是 `dup_str` 里 malloc 出来的,所有权归容器里的元素。普通 `jyp_release_vector` 只拆房子(释放缓冲和结构体),字符串本体没人管,漏。所以咱们需要 `jyp_release_vector_with`,它先对每个元素调一次 dtor 再拆:

```c
void jyp_release_vector_with(JYPVector* vector, void (*dtor)(void* elem))
{
    if (dtor) {
        for (size_t i = 0; i < vector->current_count_; i++) {
            dtor(elem_index_addr(vector, i));
        }
    }
    jyp_release_vector(vector);
}
```

dtor 收到的是槽位地址,元素本体是 char*,咱们得解一层才拿到字符串,所以 `free_str` 长这样:

```c
// release_with 的 dtor:元素是 char*,先解一层地址拿到指针本体
static void free_str(void* elem) { free(*(char**)elem); }
```

这里最容易手滑的是把 free 直接当 dtor 传进去:那等于对着缓冲区中间某个地址调 free,而 free 只认 malloc 给的开头,ASan 一炸一个准。漏了 dtor 会怎样,咱们故意翻一次车给 LSan 看,同样的三行字符串,最后改用普通 release:

```text
=================================================================
==30804==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 17 byte(s) in 3 object(s) allocated from:
    #0 0x78ed2472c0c1 in malloc (/usr/lib/libasan.so.8+0x12c0c1)
    #1 0x60e5457c43d0 in main (/tmp/vec_probe/leak_demo+0x13d0)
    ......
SUMMARY: AddressSanitizer: 17 byte(s) leaked in 3 allocation(s).
```

17 字节,3 块,正好是 "hello"(6)+"jyp"(4)+"vector"(7),报告连字节数都替咱们数好了。数组本身没漏(拆干净了),漏的是元素们各自拥有的字符串。

clone 是同一个主题的变奏。`jyp_clone_vector` 新开缓冲、按字节复制元素,槽位里那份 char* 被原样抄了过去,于是两份 vector 指着同一批字符串。test 里的处理是:`sv` 那份 `release_with` 传 NULL 只拆房子,字符串移交给 `sv2`,再由 `sv2` 的 release_with 统一办后事,free 一遍,正好。谁拥有、谁负责,在 C 里没有编译器替咱们盯着,全靠这样的显式约定;您写自己的容器时,把所有权说进头文件注释,是能救未来自己的。

## 内存不够的那一天

到此为止一切顺利,是因为内存管够。咱们把虚拟内存压到 256 Mi(`ulimit -v 262144`)再往里塞,每个元素一个字节:

```text
已塞 64 Mi 个,容量 64 Mi,data=0x730131478010
已塞 128 Mi 个,容量 128 Mi,data=0x730129477010
退出码=139
```

139,段错误,一句遗言都没有。解剖一下:128 Mi 满员,reserve 要 256 Mi,realloc 失败返回了 NULL。而咱们 reserve 里写的是 `vector->data_begin_ = reallocate(...)` 直接赋值,NULL 把旧指针盖掉了:那 128 Mi 旧缓冲从此没有指针够得着,泄漏;`capacity_` 却已经被更新成 256 Mi;下一个 push 冲进去,在近似零的地址上做 memcpy,当场倒地。

失败本身是可观测、可处理的,探针给咱们留了证据:

```text
realloc 134217728B -> 268435456B 失败,返回 NULL
旧块 134217728B 还活着,还能读,记得 free
```

教科书的解法叫 tmp 模式,咱们先接住、判空、再落笔:

```c
void* tmp = reallocate(vector->data_begin_, new_cap * vector->elem_size_);
if (tmp == NULL) {
    /* 旧缓冲还活着,数据没丢:可以重试、上报,或体面地放弃 */
    return -1; /* 前提是把 reserve 的返回类型从 void 改成 int */
}
vector->data_begin_ = tmp;
vector->capacity_ = new_cap;
```

咱们这一版没做这个功课,而且所有函数都返回 void,连上报失败的通道都没留。补救的路子摆在练习里:allocation 层打日志后直接 abort,省心,牺牲的是优雅降级;reserve 改成返回错误码,诚实,代价是整条调用链跟着改;或者在文档里写明"内存不够就崩",把决定亮给使用者。三种在真实工程里都有人选,要紧的是这是个该被显式做出的决定,不该是被遗忘的默认。顺带一提,`new_cap * elem_size_` 在容量巨大时会溢出 size_t,一行 `if (new_cap > SIZE_MAX / elem_size_)` 的守卫也一并留给您。

⚠️ realloc 的结果永远先进临时变量判空,别直接盖回原指针,失败时原指针就是您仅剩的家当。

## 交棒:四战收拢,换一种世界观

咱们这一章的手里现在有这样一套接口,全部在 [jyp_vector.h](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/blob/main/projects/journey_your_pack/pack1_vector/include/jyp/jyp_vector.h) 里,配着四组编译器组合全过的测试:

| 接口 | 干什么 |
| --- | --- |
| `jyp_create_vector` / `jyp_release_vector` | 盖房与拆房;用完必还 |
| `jyp_release_vector_with(v, dtor)` | 元素拥有资源时的拆房,先逐个办后事 |
| `jyp_insert_vector_at` / `jyp_remove_vector_at` | 任意下标插与删;push/pop/front/back 宏都建立在它们上面 |
| `jyp_get_vector_at` | 带边界检查的槽位指针;越界 NULL,指针会过期(第 3 章) |
| `jyp_reserve_vector` / `jyp_shrink_to_fit_vector` | 手动挡:预留与收缩 |
| `jyp_clear_vector` | 人走屋空,容量留着 |
| `jyp_clone_vector` | 浅拷贝克隆,所有权要另想 |
| `jyp_find_in_vector` | 线性查找,qsort 同款比较器 |
| `jyp_set_resize_vector_policy` | 增长策略一行换装 |

四章带得走的判断可以收拢成三句话:realloc 不能赌,地址变不变是分配器的自由,代码只认"成功后旧指针作废"这条合同;翻倍是摊薄搬运的取舍而非真理,CPython 用八分之一加 6 站在另一个平衡点上,咱们把策略做成口子;容器释放前要问一句元素里有没有归它管的东西,有就走带 dtor 的那扇门。

下一章咱们换一种世界观。动态数组不管多能装,内存始终是一整块连续的,中间插一个元素全数组跟着挪;单链表反着来,每个节点单独 malloc、用指针串起来,插删只动两根指针,代价是下标访问没了。一整块连续内存和一串散落节点,这两种形态撑起了后面几乎所有的数据结构,咱们下一章见。

练习在 [练习 3.5:动态数组](../exercises/03-data-structures/homework.md):补 tmp 模式、写溢出守卫、换增长策略再数一遍 realloc,三件事都在等您动手。完整代码 `ctest --test-dir build` 一键验。
