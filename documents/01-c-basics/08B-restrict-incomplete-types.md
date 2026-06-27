---
chapter: 1
c_standard:
- 99
- 11
description: 理解 restrict 限定符的优化原理、不完整类型与前向声明的用途、opaque pointer 模式，以及 -> 运算符操作结构体指针
difficulty: beginner
order: 12
platform: host
prerequisites:
- 多级指针与声明读法
reading_time_minutes: 9
tags:
- host
- pointers
- memory
title: restrict、不完整类型与结构体指针
---
# restrict、不完整类型与结构体指针

上一篇我们把多级指针和声明读法搞定了。这一篇来看几个相对独立但都很有用的机制：`restrict` 限定符让编译器敢于做更激进的优化，不完整类型和前向声明让我们在不暴露内部细节的情况下设计接口，而 `->` 运算符则是操作结构体指针的日常工具。

这三样东西看起来没什么联系，但它们在 C 语言工程实践中都非常实用——而且常常是同一个项目里同时出现的：`restrict` 写在高性能循环里，不完整类型和 `->` 写在接口层。

> **学习目标**
>
> 完成本章后，你将能够：
>
> - [ ] 理解 restrict 限定符解决什么问题和它的使用规则
> - [ ] 使用不完整类型和前向声明减少头文件依赖
> - [ ] 实现 opaque pointer 模式隐藏实现细节
> - [ ] 用 `->` 运算符操作结构体指针

## 环境说明

我们接下来的所有实验都在这个环境下进行：

- 平台：Linux x86\_64（WSL2 也可以）
- 编译器：GCC 13+ 或 Clang 17+
- 编译选项：`-Wall -Wextra -std=c11`

## 第一步——理解 restrict 为什么能让代码更快

### 指针别名——编译器的噩梦

考虑这个函数：

```c
void vector_add(int n, int* a, int* b)
{
    for (int i = 0; i < n; i++) {
        a[i] = a[i] + b[i];
    }
}
```

编译器在这里面临一个问题：`a` 和 `b` 可能指向同一块内存。比如调用 `vector_add(10, arr, arr)` 的时候，写入 `a[i]` 之后 `b[i]` 也变了。所以编译器不敢做激进的优化——每次写入 `a[i]` 后都得重新从内存读取 `b[i]`。

这就是"指针别名"（pointer aliasing）问题：编译器无法确定两个指针是否指向同一块内存，只能保守处理。

### restrict——程序员和编译器的契约

`restrict` 是 C99 引入的限定符，告诉编译器："我保证这个指针访问的内存不会通过其他指针来访问"。

```c
void vector_add(int n, int* restrict a, int* restrict b)
{
    for (int i = 0; i < n; i++) {
        a[i] = a[i] + b[i];
    }
}
```

加上 `restrict` 之后，编译器知道 `a` 和 `b` 不重叠，可以放心做向量化（SIMD）、循环展开等优化。

来看一个更直观的例子：

```c
int foo(int* a, int* b)
{
    *a = 5;
    *b = 6;
    return *a + *b;
    // 编译器不敢假设 *a 还是 5，因为 b 可能就是 a
    // 必须重新从内存读 *a
}

int rfoo(int* restrict a, int* restrict b)
{
    *a = 5;
    *b = 6;
    return *a + *b;
    // 编译器知道 a、b 不重叠，*a 一定是 5
    // 直接返回 11，不用重新读内存
}
```

`rfoo` 里编译器甚至不需要重新读内存——`*a` 的值它已经知道了。我们把它编译跑一下，看看真实行为（`gcc -Wall -Wextra -std=c11`）：

```c
int main(void)
{
    int x = 0, y = 0;
    printf("foo(independent) = %d\n", foo(&x, &y));   // 11

    int u = 0;
    printf("foo(alias) = %d\n", foo(&u, &u));          // 12，u 被改成了 6

    int p = 0, q = 0;
    printf("rfoo(independent) = %d\n", rfoo(&p, &q));  // 11
    return 0;
}
```

实际输出：

```text
foo(independent) = 11
foo(alias) = 12
rfoo(independent) = 11
```

注意 `foo(&u, &u)`：`*a=5` 把 `u` 写成 5，`*b=6` 又把它写成 6，最后 `*a + *b` 读回的是同一个 `u=6`，加起来是 12。这正是别名问题在运行时的体现——只要 `a`、`b` 有可能指向同一块内存，编译器就必须老实按这个结果算。

> ⚠️ **踩坑预警**
> `restrict` 是程序员对编译器的单向承诺，编译器不会在运行时检查。如果你给 `rfoo` 传了重叠的指针（比如 `rfoo(&u, &u)`），行为是未定义的——优化后的代码可能产生任何结果，而且这种 bug 只在特定编译选项下才暴露，查起来非常痛苦。承诺了不重叠，就一定要保证不重叠。

### memcpy vs memmove——经典对比

标准库里有一对经典的例子正好说明 `restrict` 的用途：

```c
void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
```

`memcpy` 假设内存不重叠，用了 `restrict`，所以更快。`memmove` 允许重叠，不能用 `restrict`，内部要做额外的检查和缓冲，所以稍慢。如果你确定源和目标不重叠，优先用 `memcpy`。

## 第二步——搞懂不完整类型和前向声明

### 什么是不完整类型

如果编译器知道一个类型的存在，但不知道它的大小和内部结构，这个类型就是不完整的（incomplete type）。最常见的例子：

```c
struct Foo;  // 前向声明：告诉编译器"Foo 是个结构体"，但不说里面有什么

struct Foo* p;    // 合法：指针大小固定，不需要知道 Foo 的完整定义
struct Foo  obj;  // 非法：编译器不知道 Foo 的大小，无法分配空间
```

不完整类型能做的事很有限：声明指向它的指针、在函数声明中用它的指针。要做更多事情（定义变量、访问成员、`sizeof`），必须提供完整定义。

### 前向声明有什么用

前向声明最直接的用途是减少头文件依赖。来看个例子：

```c
// car.h
struct Engine;  // 前向声明，不需要 #include "engine.h"

struct Car {
    struct Engine* engine;  // 只需要指针，前向声明就够
    int speed;
};
```

如果 `Car` 里只放 `Engine` 的指针，我们不需要 `#include "engine.h"`。这样 `car.h` 的使用者不会被迫拉上 `engine.h` 的所有依赖，编译速度也能提上来。

> ⚠️ **踩坑预警**
> 前向声明只能用来声明指针。如果你在头文件里直接放了 `struct Engine engine;`（不是指针），编译器必须知道 `Engine` 的完整定义才能确定 `Car` 的大小——这时候前向声明就不行了，必须 `#include` 完整头文件。

## 第三步——用 opaque pointer 隐藏实现细节

不完整类型在 C 中有一个非常重要的应用模式：opaque pointer（不透明指针）。思路是头文件只暴露前向声明和操作函数，不暴露结构体内部细节。

```c
// buffer.h — 公开头文件
typedef struct Buffer Buffer;  // 前向声明 + typedef

Buffer* buffer_create(int capacity);
void    buffer_destroy(Buffer* buf);
int     buffer_append(Buffer* buf, const char* data, int len);
int     buffer_length(const Buffer* buf);
```

调用者只能通过函数操作 `Buffer`，永远看不到 `struct Buffer` 的内部结构。实现在 `.c` 文件里提供完整定义：

```c
// buffer.c — 实现文件
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

struct Buffer {
    char* data;
    int   capacity;
    int   length;
};

Buffer* buffer_create(int capacity)
{
    Buffer* buf = (Buffer*)malloc(sizeof(Buffer));
    buf->data = (char*)malloc(capacity);
    buf->capacity = capacity;
    buf->length = 0;
    return buf;
}

void buffer_destroy(Buffer* buf)
{
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

int buffer_append(Buffer* buf, const char* data, int len)
{
    if (buf->length + len > buf->capacity) {
        return -1;  // 缓冲区不足
    }
    memcpy(buf->data + buf->length, data, len);
    buf->length += len;
    return 0;
}

int buffer_length(const Buffer* buf)
{
    return buf->length;
}
```

把这个模块和一段调用代码编译跑一下（`gcc -Wall -Wextra -std=c11 buffer.c main.c`）：

```c
// main.c
#include <stdio.h>
#include "buffer.h"

int main(void)
{
    Buffer* buf = buffer_create(16);
    printf("after create: length=%d\n", buffer_length(buf));

    buffer_append(buf, "hello", 5);
    buffer_append(buf, " world", 6);
    printf("after appends: length=%d\n", buffer_length(buf));

    int rc = buffer_append(buf, "this is too long to fit", 23);
    printf("overflow append rc=%d, length=%d\n", rc, buffer_length(buf));

    buffer_destroy(buf);
    return 0;
}
```

实际输出：

```text
after create: length=0
after appends: length=11
overflow append rc=-1, length=11
```

`main.c` 里完全没有出现 `buf->data`、`buf->capacity` 这些字段——因为它们藏在 `buffer.c` 里。好处是：你可以修改 `Buffer` 的内部实现（比如加个增长策略、把 `int` 换成 `size_t`），只要函数签名不变，调用者不需要重新编译。标准库的 `FILE` 就是这个模式的经典例子——你从来不知道 `FILE` 里面长什么样，只用 `fopen`/`fclose`/`fread`/`fwrite` 来操作它。

## 第四步——用 -> 操作结构体指针

在函数之间传递结构体时通常用指针来避免拷贝开销。访问结构体指针指向的成员有两种方式：

```c
typedef struct {
    float x;
    float y;
} Point;

Point p = {3.0f, 4.0f};
Point* ptr = &p;

// 方式 1：先解引用，再用 . 访问成员
float x1 = (*ptr).x;   // 括号不能省，因为 . 的优先级高于 *

// 方式 2：用 -> 运算符（语法糖）
float x2 = ptr->x;     // 等价于 (*ptr).x
```

`->` 就是为了让我们少打字而发明的语法糖。记住规则就行：**结构体变量用 `.`，结构体指针用 `->`**。

```c
typedef struct {
    Point center;
    float radius;
} Circle;

Circle c = {{0.0f, 0.0f}, 5.0f};
Circle* cp = &c;

cp->center.x = 1.0f;        // 修改圆心的 x
cp->radius = 10.0f;          // 修改半径

void move_circle(Circle* c, float dx, float dy)
{
    c->center.x += dx;
    c->center.y += dy;
}

move_circle(cp, 2.0f, 3.0f);
```

> ⚠️ **踩坑预警**
> `.` 和 `->` 搞混是新手最常犯的错误之一。`cp->center.x` 是对的，但 `cp.center.x` 编译不过（`cp` 是指针不是变量），`(*cp).center.x` 虽然等价但括号很容易忘。养成用 `->` 的习惯就好。

## 常见陷阱

| 陷阱 | 说明 | 解决方法 |
|------|------|----------|
| restrict 下传重叠指针 | 未定义行为，编译器不会检查 | 确保 restrict 指针指向的内存真的不重叠 |
| 前向声明后直接使用成员 | `struct Foo; Foo f; f.x = 1;` 全错 | 前向声明只能声明指针，完整使用需完整定义 |
| `.` 和 `->` 搞混 | 指针用 `->`，变量用 `.` | `ptr->member` 等价于 `(*ptr).member` |
| 混用 memcpy 和 memmove | 源和目标重叠时用 memcpy 是 UB | 有重叠风险就用 memmove |

## 小结

这一篇我们看了三个独立但实用的机制。`restrict` 通过消除指针别名让编译器做更激进的优化，但它是一份"程序员向编译器保证"的契约——违约就是未定义行为。不完整类型和前向声明让我们在不暴露内部细节的情况下设计接口，opaque pointer 模式更是 C 语言实现信息隐藏的经典手法。`->` 是日常操作结构体指针的工具，记住"变量用 `.`，指针用 `->`"就够了。

## 练习

### 练习：实现一个简单的 opaque pointer 模块

用 opaque pointer 模式实现一个简单的栈（Stack）模块。要求：

```c
// stack.h — 只暴露接口，不暴露内部结构
typedef struct Stack Stack;

Stack* stack_create(int capacity);
void   stack_destroy(Stack* s);
int    stack_push(Stack* s, int value);   // 成功返回 0，满栈返回 -1
int    stack_pop(Stack* s, int* out);     // 成功返回 0，空栈返回 -1
int    stack_size(const Stack* s);
```

提示：在 `.c` 文件里定义 `struct Stack` 的完整结构（可以用数组+栈顶索引实现），`.h` 文件只放前向声明和函数声明。

## 参考资源

- [restrict 限定符 - cppreference](https://en.cppreference.com/w/c/language/restrict)
- [不完整类型 - cppreference](https://en.cppreference.com/w/c/language/type)

*整理自作者 c_tutorials，按 C-Journey 写作规范适配。*
