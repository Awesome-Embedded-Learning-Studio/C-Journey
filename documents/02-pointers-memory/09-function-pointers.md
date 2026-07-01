---
title: "函数指针：把函数存进变量、当参数传、用 qsort 排序"
description: "这一章啃下 C 的函数指针——把「函数」本身当成一种可寻址的东西,存进变量、当参数传、按需挑一个调。先真跑 int (*fp)(int,int) = add 的声明,验证 fp(3,4) 和 (*fp)(3,4) 两种调用方式都得 7,并打印 add、&add、fp 三者地址完全相同(函数名 add 自动退化成函数指针、&add 与它同地址,§6.7.6.3p8)。再把函数指针当参数传:apply(int (*op)(int,int), a, b) 让调用者决定「在里面调哪个函数」——传 add 得 7、传 sub 得 -1,这就是「回调」的本质。接着上标准库 qsort(void* base, size_t n, size_t size, int (*compar)(const void*, const void*)),自己写比较函数 cmp 排 int 数组 {5,2,8,1,9,3} 得 1 2 3 5 8 9(§7.22.5.2,qsort 用函数指针 + void* 实现对任意类型的泛型排序)。最后真跑函数指针数组当「转移表」:Op ops[] = {add,sub,mul,divi} 配 sym[] = {+,-,*,/},对 a=10,b=4 遍历四则运算得 14/6/40/2,实现一个简易计算器的核心分发逻辑。读法(int (*fp)(int,int) 里 *fp 必须加括号,否则 int *fp(int,int) 是「返回 int* 的函数」)只点到为止、详细留给第 10 章右左法则。全 gcc16+clang22 真跑。"
chapter: 2
order: 9
tags:
  - host
  - pointers
  - function
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段2·第1章：指针是什么（指针装地址）"
  - "阶段2·第3章：用指针改调用者的变量（指针作参数）"
  - "阶段2·第8章：多级指针与指针数组（int* 之外还有别的指针类型）"
  - "第 8 章：函数（函数签名、函数调用）"
related:
  - "阶段2·第10章：复杂声明与 typedef（读 int (*fp)(int) 这类声明、用 typedef 简化函数指针）"
  - "第 16 章：GitHub Actions（qsort 是泛型回调的经典样板）"
---

> 🟡 状态:待审核(2026-06-30)

# 函数指针：把函数存进变量、当参数传、用 qsort 排序

## 引言：函数也有地址

前面 8 章我们用指针存了各种各样的地址——`int` 变量的地址（第 1 章）、数组元素的地址（第 2 章）、指针的地址（第 8 章）。但有一个东西一直没说破:**函数本身也住在内存里**。编译好的程序加载到进程后,函数的机器指令躺在代码段(`.text`,第 4 章看汇编那段见过),那块代码的第一个字节也有地址——这个地址就叫「函数的地址」,你拿 `&` 去取函数名就能拿到它。于是自然就有「**函数指针**」这种东西:一个专门存「函数地址」的指针变量。存它干嘛?核心就一句——**在运行期决定「这一步到底调哪个函数」**:同样是「对两个数做点运算」,这一刻调 `add`、下一刻调 `sub`,而调用代码一模一样、只是换了个指针。这一招叫「回调」(callback),也是 C 里实现「策略模式」「泛型算法」(标准库 `qsort`)、「转移表」(简易计算器)的统一武器。这一章我们把函数指针从声明到实战真跑一遍,读法那套「右左法则」留给第 10 章展开,本章先把它用起来。

## 函数指针的声明:那个括号不能省

函数指针的声明长这样:

```c
int (*fp)(int, int);
```

读法是:`fp` 是一个「指针」(`*fp` 外层的括号在告诉你 `fp` 先和 `*` 结合),它指向的是「一个接收两个 `int` 参数、返回 `int` 的函数」。`int` 在最左边是「被指函数的返回类型」,右边 `(int, int)` 是「被指函数的参数列表」。这里那个括号 `(*fp)` 是**生死攸关**的——你要是手滑写成:

```c
int *fp(int, int);   /* 这是「返回 int* 的函数」的声明,不是函数指针! */
```

那就完全不是函数指针了:因为 `()` 的优先级高于 `*`,`fp` 先和 `(int, int)` 结合成「函数」,返回类型是 `int*`(第 10 章右左法则会系统讲怎么读这种声明)。两种写法只差一对括号,含义天差地别——这是函数指针的第一个、也是最容易翻车的坑,声明里 `*fp` 必须用括号包住。本章先不纠结读法,先把它**用起来**;真要读复杂声明(`int (*fp)(int)`、`int (*a[5])(int)`、`void (*signal(int, void(*)(int)))(int)` 这种怪物)留到第 10 章。

## 赋值与调用:函数名退化、两种调用等价

声明完指针,要让它指向一个真实的函数,这就轮到第 10 章见过的老戏法登场——**函数名会自动「退化」成函数指针**(和数组名退化成指针是一个道理,§6.7.6.3p8 明确说「函数声明符里的函数名,在表达式中会被转换成指向那个函数的指针」)。所以 `fp = add;`(直接用函数名)和 `fp = &add;`(显式取地址)是**完全等价**的,标准在那一节就规定了这件事。调用也一样:既可以直接 `fp(3, 4)`(把指针当函数名用),也可以显式 `(*fp)(3, 4)`(先解引用再调),§6.5.2.2 的函数调用语义允许这两种写法。真跑给你看:

```c
#include <stdio.h>

static int add(int a, int b) {
    return a + b;
}

int main(void) {
    int (*fp)(int, int) = add; /* fp 指向「两个 int 入参、返回 int」的函数 */

    printf("fp(3, 4)    = %d\n", fp(3, 4));      /* 直接用指针当函数名调 */
    printf("(*fp)(3, 4) = %d\n", (*fp)(3, 4));   /* 显式解引用再调,老写法 */

    int x = 7;
    printf("&x    = %p\n", (void*)&x);
    printf("add   = %p\n", (void*)add);   /* 函数名退化成函数指针 */
    printf("&add  = %p\n", (void*)&add);  /* 显式取地址,和 add 同地址 */
    printf("fp    = %p\n", (void*)fp);    /* fp 里存的也是同一个地址 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall fp_basic.c -o fp && ./fp
fp(3, 4)    = 7
(*fp)(3, 4) = 7
&x    = 0x7ffc112f600c
add   = 0x5b083e1b8149
&add  = 0x5b083e1b8149
fp    = 0x5b083e1b8149
```

两种调用都得 `7`——`fp(3,4)` 和 `(*fp)(3,4)` 是一回事,工程里现在几乎都写前者(干净)。再看地址那几行:普通变量 `&x` 落在 `0x7ffc...`(栈区),而 `add`、`&add`、`fp` 三者打印出来是**同一个** `0x5b083e1b8149`——这就是函数的代码在 `.text` 段的地址(注意它在 `0x5b...`,和栈的 `0x7ffc...` 是不同的内存区,因为代码段和栈本来就分开放)。`add` 和 `&add` 同地址,正是「函数名自动退化成函数指针」的实锤;`fp` 也等于它们,因为 `fp = add` 把那个地址存了进来。(这三行地址每次运行都会变——操作系统对代码段也有地址随机化,但 `add`、`&add`、`fp` 三者一定相等。)

## 函数指针作参数:这就是「回调」

把「函数地址」存进变量只是第一步,真正有用的是把它**当参数传给另一个函数**——让被调用的函数「在合适的时机回头调你传进来的那个函数」。这套模式叫「回调」(callback),在 C 里无处不在。看一个最小的例子:`apply` 接收一个运算函数指针 `op` 和两个数,返回 `op(a, b)`;调用者决定 `op` 是 `add` 还是 `sub`:

```c
#include <stdio.h>

static int add(int a, int b) {
    return a + b;
}

static int sub(int a, int b) {
    return a - b;
}

/* op 是「函数指针」参数:指向 (int,int)->int 的函数 */
static int apply(int (*op)(int, int), int a, int b) {
    return op(a, b);
}

int main(void) {
    printf("apply(add, 3, 4) = %d\n", apply(add, 3, 4));
    printf("apply(sub, 3, 4) = %d\n", apply(sub, 3, 4));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall callback.c -o cb && ./cb
apply(add, 3, 4) = 7
apply(sub, 3, 4) = -1
```

`apply` 的参数 `int (*op)(int, int)` 就是一个函数指针;函数体里 `op(a, b)` 拿这个指针去调它指向的函数。调用方这边,`apply(add, 3, 4)` 把 `add` 退化成的函数指针传进去、得 `7`,`apply(sub, 3, 4)` 换成 `sub` 传进去、得 `-1`——**`apply` 的代码一行没改,行为却因为传入的函数不同而不同**。这就是回调的全部魔力:把「做什么」从「怎么做」里剥离出来,`apply` 只负责「拿到两个数、把它们喂给某个函数、返回结果」,至于那个函数是加是减还是别的什么,由调用方决定。这种「把策略当参数传」的能力,是 C 实现「泛型算法」「事件处理」「排序比较器」的统一钥匙——下面马上看到的 `qsort` 就是它最经典的应用。

## 标准库 qsort:函数指针 + void* 的泛型排序

C 标准库 `<stdlib.h>` 提供了一个通用排序函数 `qsort`(§7.22.5.2),它的签名是这套思路的集大成者:

```c
void qsort(void* base, size_t n, size_t size,
           int (*compar)(const void*, const void*));
```

四个参数里全是函数指针 + `void*`(第 11 章专门讲 `void*`)的配合:`base` 是「数组起始地址」、`n` 是元素个数、`size` 是「每个元素占几字节」——这三个凑在一起,`qsort` 就能对**任何类型**的数组操作(它通过 `void*` 拿首地址、用 `size` 自己算偏移,完全不需要知道你排的是 `int` 还是 `struct`)。但 `qsort` 自己不知道「两个元素谁该排前面」,这取决于你的业务逻辑,所以第四个参数 `compar` 是个**函数指针**——你写一个比较函数,`qsort` 在排序过程中反复调用它来比较两个元素。比较函数的约定是:**返回负数表示 `a` 排在 `b` 前、返回 0 表示相等、返回正数表示 `a` 排在 `b` 后**(和 `strcmp` 一致)。我们用 `qsort` 排一个 `int` 数组:

```c
#include <stdio.h>
#include <stdlib.h>

/* 比较函数:返回负/0/正 表示 a 在 b 前/相等/后 */
static int cmp(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    if (ia < ib) {
        return -1;
    }
    if (ia > ib) {
        return 1;
    }
    return 0;
}

int main(void) {
    int arr[] = {5, 2, 8, 1, 9, 3};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    qsort(arr, n, sizeof(int), cmp); /* 传函数指针 cmp,排升序 */

    for (size_t i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall qsort_demo.c -o qs && ./qs
1 2 3 5 8 9
```

`{5, 2, 8, 1, 9, 3}` 排完得 `1 2 3 5 8 9`。逐行拆这个比较函数 `cmp`:它的两个参数是 `const void*`(因为 `qsort` 不知道你排什么类型,只能给你「两个元素的地址」、用 `void*` 兜底),所以第一步是**把 `void*` 强转回 `const int*` 再解引用**拿到真实的整数——`int ia = *(const int*)a;`。然后比较:小于返回 `-1`、大于返回 `1`、相等返回 `0`(这种「显式返回 -1/0/1」的写法最稳妥;很多人图省事直接写 `return ia - ib;`,但在两个数符号不同或差值接近 `INT_MIN/INT_MAX` 时会**有符号溢出**,是 UB——第 3 章讲过有符号溢出这回事,这里就别图那一行了)。调用 `qsort(arr, n, sizeof(int), cmp)`:数组退化成 `int*` 自动转 `void*`、`n` 是元素个数、`sizeof(int)` 是每个元素的字节数、`cmp` 退化成函数指针。排完 `arr` 的内容就被原地改了(注意是原地排,不返回新数组)。想排**降序**怎么办?把 `cmp` 里 `ia < ib` 返回 `1`、`ia > ib` 返回 `-1` 就行——`qsort` 一行没改,只换了比较函数,行为就反过来了,这正是回调的威力。`qsort` 是 C 标准库里「用函数指针实现泛型」的最经典样板,看懂它,以后遇到任何「带回调的库函数」(线程库、GUI 事件循环、`atexit` 注册清理函数、`bsearch` 二分查找)都是一个套路。

## 函数指针数组:转移表(简易计算器)

把一堆函数指针放进一个数组,就得到一个**「转移表」**(dispatch table)——按下标(或某个编号)挑一个函数调。这是用「数据」代替「一长串 `switch-case`」的经典手法。我们来做一个简易计算器的核心:四则运算各一个函数,放进数组,对两个操作数把四种运算全跑一遍:

```c
#include <stdio.h>

static int add(int a, int b) {
    return a + b;
}

static int sub(int a, int b) {
    return a - b;
}

static int mul(int a, int b) {
    return a * b;
}

static int divi(int a, int b) {
    return a / b;
}

/* Op = 「指向 (int,int)->int 函数」的指针类型(下一章 typedef 详讲) */
typedef int (*Op)(int, int);

int main(void) {
    Op ops[] = {add, sub, mul, divi};          /* 函数指针数组:转移表 */
    const char* sym[] = {"+", "-", "*", "/"};   /* 对应的运算符 */

    int a = 10, b = 4;
    for (int i = 0; i < 4; i++) {
        printf("%d%s%d=%d\n", a, sym[i], b, ops[i](a, b));
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall dispatch.c -o dp && ./dp
10+4=14
10-4=6
10*4=40
10/4=2
```

这里出现了一行新东西:`typedef int (*Op)(int, int);`。它给「指向 `(int,int)->int` 函数的指针」这个又臭又长的类型起了个短名字 `Op`(typedef 第 10 章详讲,这里先用上)。有了 `Op`,声明函数指针数组 `Op ops[] = {add, sub, mul, divi};` 就清爽多了——`ops` 是个有 4 个元素的数组,每个元素是一个 `Op`(函数指针),分别指向 `add`/`sub`/`mul`/`divi` 四个函数。配一个同样有 4 个元素的字符串数组 `sym[]` 存运算符符号。然后循环里 `ops[i](a, b)` 就是「取第 `i` 个函数指针、拿 `a b` 去调它」,得 `14`/`6`/`40`/`2`(注意 `10/4` 整数除法得 `2`,第 4 章讲过)。这个写法的好处在于:**加一种运算只要往两个数组各加一项**,不用动 `for` 循环;换成 `switch-case` 写就得 `case '+': ... case '-': ...` 一长串、加一种运算要改两处。当分支多了(十几种、几十种),转移表比 `switch` 干净得多——这是「数据驱动」对比「控制流驱动」的胜利。真正的计算器会把 `i` 换成「根据用户输入的运算符查表」(查表用 `strcmp` 匹配 `sym[]`),找到对应的 `ops[i]` 调用,这比写一坨 `if-else if` 优雅得多。

## 小结

函数也住在内存里(编译好的机器指令落在 `.text` 段),所以函数也有地址、也能被 `&` 取出来。**函数指针**就是存「函数地址」的指针变量,声明形如 `int (*fp)(int, int)`(那个括号 `(*fp)` 不能省,否则 `int *fp(int,int)` 就成了「返回 `int*` 的函数」——`[]`/`()` 优先级高于 `*`,详细读法第 10 章右左法则展开)。函数名 `add` 在表达式里**自动退化成函数指针**(§6.7.6.3p8,和数组名退化同理),所以 `fp = add;` 和 `fp = &add;` 等价、真跑 `add`/`&add`/`fp` 三个地址完全相同(函数代码在 `.text` 段的地址);调用时 `fp(3,4)` 和 `(*fp)(3,4)` 也等价(§6.5.2.2,真跑都得 `7`),工程里写前者更干净。函数指针真正的威力在**当参数传**:把「做什么」(策略)从「怎么做」里剥离出来,`apply(op, a, b)` 不改一行代码,传 `add` 得 `7`、传 `sub` 得 `-1`,这就是「**回调**」。标准库的 `qsort`(`void qsort(void* base, size_t n, size_t size, int (*compar)(const void*, const void*))`,§7.22.5.2)把这套用到了极致——`void*` 兜底任意类型的数组、函数指针 `compar` 接收用户的比较逻辑(约定:返回负/0/正表示前后),`{5,2,8,1,9,3}` 配自己写的 `cmp` 排升序得 `1 2 3 5 8 9`(比较函数里 `void*` 要强转回 `const int*` 再解引用,显式返回 `-1/0/1` 比 `return ia-ib` 安全——后者有符号溢出是 UB)。把一堆函数指针放进数组就得到**转移表**(`Op ops[] = {add,sub,mul,divi}`,配 `typedef int (*Op)(int,int)` 简化声明),按下标选函数调,真跑对 `a=10,b=4` 遍历四则运算得 `14/6/40/2`——加运算只加数据、不动循环,是「数据驱动」取代 `switch-case` 的经典手法。下一章我们专门学「右左法则」,看怎么系统读懂 `int (*fp)(int)`、`int (*a[5])(int)` 这种层层套娃的复杂声明,以及用 `typedef` 把它们写得不那么吓人。

## 参考资源

- ISO/IEC 9899:2011 §6.7.6.3(函数声明符,函数名退化为函数指针:p8)、§6.5.2.2(函数调用,函数指针调用的两种写法)、§7.22.5.2(`qsort` 函数原型与 `compar` 的返回约定)
- K. N. King《C Programming: A Modern Approach》第 17 章·Pointers to Functions(函数指针声明、作参数、`qsort` 比较函数、函数指针数组与转移表)
- Robert C. Seacord《Effective C》第 6 章(函数指针、`qsort` 的安全比较函数写法)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 5.11 节(函数指针、`qsort` 实战)、第 5.10 节(命令行参数与回调风格)
- 阶段2·第 1 章(指针装地址)、第 3 章(指针作参数,本章回调的根基)、第 8 章(指针数组,本章函数指针数组是它的延伸)
- 第 8 章:函数(函数签名、函数调用)、阶段2·第 10 章:复杂声明与 typedef(右左法则读 `int (*fp)(int)`、用 `typedef` 简化函数指针)、阶段2·第 11 章:void\* 与字节级操作(`qsort` 的 `void*` 参数细讲)
