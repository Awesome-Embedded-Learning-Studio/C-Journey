---
title: "用 C 造一个泛型容器：从 void* 到自己的 vector"
description: "C 没有模板，靠 void* + 元素大小 + 回调函数照样能造出泛型 vector；拆解扩容、插入删除、生命周期，并照着真实代码看清设计取舍与粗糙处。"
chapter: 3
order: 0
tags:
  - host
  - data-structures
  - generics
  - memory
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 2：指针、内存布局与位运算"
related:
  - "CMake 与模块化工程（阶段 4）"
  - "环形缓冲区（阶段 3）"
---

# 用 C 造一个泛型容器：从 void* 到自己的 vector

## 引言

写过 C++ 的人大概都会想念 `std::vector`——push 一个元素它自己长，存什么类型都行，用完自动回收。可 C 没有模板，连"存 int 的数组"和"存字符串的数组"都得各写一遍，写到第三个的时候你一定会想：**有没有办法造一个"什么都能装"的容器？** 答案是有的，而且这正是 C 数据结构库的灵魂——靠 `void*` 加上"元素大小"再加几个回调函数，我们就能搓出一个泛型 vector。这一章我们就拆开 [tiny-c-stdlib](../../projects/tiny-c-stdlib/) 里的 `CCSTDC_Vector`，看清它是怎么做到"装啥都行"的，顺便照着这份真实代码练一练"读别人的库并挑刺"的功夫。

## 核心思想：void* + dataSize + 回调

C 想要泛型，绕来绕去就三件法宝。我们先把这个底座立起来，后面的增删改查都是在它上面变戏法。

第一件是 `void*`，一个"不知道指向什么类型"的指针。容器内部拿一块连续内存，对外只说"我这里有数据"，但不规定数据是什么类型。问题是，`void*` 不能解引用、也不能做指针算术——编译器不知道一个元素占几个字节，自然算不出第 i 个在哪。这就引出第二件法宝：**`dataSize`，记下每个元素占多少字节**。有了它，第 i 个元素的地址就是 `(char*)coreData + i * dataSize`。注意这里先把 `void*` 转成 `char*`，因为 `char` 恰好是一个字节，`char*` 的算术就是"按字节算"，这是 C 里做底层内存运算的通用套路。

我们对着真实结构体看就清楚了：

```c
typedef struct _CCSTDC_Vector {
    void* coreData;    // 一块连续内存，所有元素挨个排
    size_t dataSize;   // 每个元素多大（字节）
    size_t curSize;    // 当前存了几个
    size_t capicity;   // 容量：最多能装几个
} CCSTDC_Vector;
```

第三件法宝是**回调函数**。容器只管存，但它不知道元素是 int 还是 struct，那要打印、要比较的时候怎么办？把"怎么打印""怎么判等"这件事交给用户，用函数指针传进来：

```c
typedef void (*Printer)(void*);                  // 给我一个元素，我来打印
typedef CCSTDC_Bool (*CCSTDC_Comparator)(void*, void*);  // 给我两个元素，我返回相等与否
```

这三件法宝一凑，一个"装啥都行"的容器就立住了。后面的 `PrintList`、`FindTarget` 全是拿着 `Printer` / `Comparator` 回调去遍历，容器自己一个元素都不用认识。

## 生命周期：init → 用 → destroy

C 没有 RAII，内存得自己管，所以每个容器的第一课都是"成对出现的 init 和 destroy"。`CCSTDC_Vector_EmptyInit` 干了两件事——先 `malloc` 出结构体本身，再 `malloc` 出装元素的那块 `coreData`：

```c
CCSTDC_Vector* getter = malloc(sizeof(CCSTDC_Vector));
void* getCoreData = malloc(dataSize * wishing_capicity);
getter->coreData = getCoreData;
getter->dataSize = dataSize;
getter->curSize  = 0;
getter->capicity = wishing_capicity;
```

用完之后，`CCSTDC_Vector_DestroyWhole` 得把这两块都还回去——**先 free 里面的 `coreData`，再 free 外面的结构体**。顺序不能反，否则你先 free 了结构体，就再也拿不到 `coreData` 的指针了，那块内存直接泄漏。这种"先内后外"的释放顺序，是手动内存管理的肌肉记忆。

## 扩容：realloc 让它自己长

vector 最舒服的特性是"push 的时候自动变大"。看 `PushBack` 里的关键几行：

```c
if (vector->curSize == vector->capicity - 1) {
    CCSTDC_Vector_Resize(vector, 2 * vector->capicity);   // 装满了，容量翻倍
}
char* pushPtr = (char*)vector->coreData + datasize * vector->curSize;
memcpy(pushPtr, data, datasize);                          // 把新元素拷到末尾
vector->curSize++;
```

容量快满的时候（这里用 `capicity - 1` 留了个哨兵位），调 `Resize` 把容量翻倍。**为什么要翻倍而不是 +1？** 因为如果每次只多一个位置，push n 个元素就要 realloc n 次、拷 n 次数据，总开销是 O(n²)；而每次翻倍，realloc 的次数是 log n，分摊到每次 push 是 O(1)。这是动态数组最经典的时间换空间取舍。

`Resize` 内部走的就是上一章讲过的 `realloc`——原地放得下就原地扩，放不下就搬个家，老数据它帮你拷好。

## 插入与删除：搬移元素

往中间插一个元素，得先把后面的整体往后挪一位，腾出空来。`Insert` 里这个循环就是在干这个：

```c
for (int i = vector->curSize; i > pos; i--) {
    memcpy((char*)coreData + i * dataSize,
           (char*)coreData + (i - 1) * dataSize,
           dataSize);
}
```

从后往前，一个一个往后拷，给 `pos` 位置让出空。**为什么从后往前？** 因为如果从前往后，你先把 `pos+1` 写进 `pos`，原来的 `pos` 就被冲掉了，连锁丢数据。从后往前挪，每个被读的值都还没被覆盖，安全。这也是手写 `memmove` 时必须想清楚的方向问题。

## 真跑一遍

光说不练假把式，我们编译运行 [test.c](../../projects/tiny-c-stdlib/CCSTDC_Tiny_Version/CCSTDC_VectorsForBlog/test.c) 的 main。它先 push 0~9，又往每个偶数位置插了一个 10，然后按值删掉所有的 10：

```text
$ gcc CCSTDC_Vector.c test.c -o vec && ./vec
10 0 10 1 10 2 10 3 10 4 10 5 10 6 10 7 10 8 10 9
0 1 2 3 4 5 6 7 8 9
```

第一行是插完 10 之后的样子，第二行是 `EraseByGivenData` 把所有 10 删掉之后的——容器确实"装啥都行"地干活了。

> **读库练手**：这份是 blog 版，留了不少可以打磨的地方，正好拿来练"审阅代码"。举两个最明显的：一是头文件里 `CCSTDC_Vector_Resize` 声明返回 `CCSTDC_Bool*`（指针），可实现里写的是 `return 1`（一个 int 当指针返回，类型对不上）；二是 `EraseByGivenData` 里搬移剩余元素用了 `memcpy`，但源和目的有重叠区域，这种场景严格说该用 `memmove`，`memcpy` 在重叠区是未定义行为。你能不能找到并修掉它们？

## 常见坑（真正的坑在后面）

> **坑 1：`void*` 直接做算术。** `coreData + i` 是不行的，编译器不知道步长。永远先转 `char*`：`(char*)coreData + i * dataSize`，按字节算才对。

> **坑 2：扩容后忘了更新指针。** `realloc` 可能搬家，返回新地址。如果不接住新地址、还用老指针，后面写数据就是写到已释放的内存里去了。

> **坑 3：重叠内存用 `memcpy`。** 源和目的有重叠时，`memcpy` 的行为是未定义的，可能丢数据。只要涉及"把一块数据往后挪"，老老实实用 `memmove`，它内部会判断方向。

> **坑 4：只 free 了结构体，忘了里面的数据区。** 容器是两层 malloc，destroy 也得两层 free。少 free 一层就是内存泄漏，valgrind 一跑便知。

## 小结

- [ ] C 泛型三件套：`void*`（不问类型）+ `dataSize`（按字节定位）+ 回调（把类型相关的操作交给用户）
- [ ] `char*` 强转是做字节级指针算术的标准手法
- [ ] 动态数组扩容选"翻倍"而非"+1"，分摊 O(1)
- [ ] 插入搬移要从后往前，重叠区拷贝用 `memmove` 不用 `memcpy`
- [ ] init 和 destroy 成对，free 时先内后外

## 练习

- [ ] 把上面点出的两个粗糙处（`Resize` 返回类型、`memcpy`→`memmove`）修掉，重新编译验证
- [ ] 照着这个套路，自己实现一个泛型栈（stack），体会 init/push/pop/destroy 的对称美
- [ ] 用 `CCSTDC_Comparator` 给这个 vector 加一个排序接口（快排 + 回调比较）

## 参考资源

- 《C 接口与实现》——David Hanson，把"用 C 造泛型容器"这件事讲到了工业级
- `man 3 memcpy` / `man 3 memmove`——注意两者对重叠区的约定差别
- GLib 的 `GArray` / `GPtrArray`——成熟的 C 泛型容器实现，可以对比设计

---
*配套示例整理自 2023–2024 学习存档。*
