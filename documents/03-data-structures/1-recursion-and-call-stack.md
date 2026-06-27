---
title: "递归与调用栈:函数调自己的本质,是系统在帮你压栈"
description: "从 LIFO 栈、系统函数调用栈与栈帧三要素讲清递归的底层机制;用真实的 objdump、栈溢出 SIGSEGV、高斯求和输出,把递归从玄学变成本能,并说清什么时候该用、什么时候别用。"
chapter: 3
order: 1
tags:
  - host
  - algorithm
  - data-structures
  - function
difficulty: beginner
reading_time_minutes: 12
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 1:C 语言基础(函数)"
  - "Chapter 2:指针、内存布局与位运算"
related:
  - "Chapter 3:C 中的泛型容器"
---

# 递归与调用栈:函数调自己的本质,是系统在帮你压栈

## 引言

递归写着是真爽——几行代码就能把"遍历一棵树""求一个和"写得优雅又高复用。但说实话,包括笔者自己在内,很多人并不能算"会用"递归:要么绕进递归里出不来、搞不清当前在第几层;要么写出来的递归直接把栈干爆。根上的原因通常是——**没搞懂递归的本质就是函数调用,而函数调用的本质是系统调用栈**。这一章我们就从栈讲起,把递归从玄学变成本能。

> 本文所有代码都是纯 C,所有输出都在本机实测捕获。

## 先搞懂"栈":一个被限制的线性结构

栈不是新东西,它就是一个**被限制操作位置**的数组或链表:只能从同一端插入(压栈 `push`)和删除(弹栈 `pop`)。这个限制带来的特性叫**后进先出(LIFO)**——最后压进去的,必须最先弹出来。

```text
push 1 →  push 2 →  push 3 →  pop → 3
        ┌───┐
   top  │ 3 │  ← 只能操作这一端
        │ 2 │
        │ 1 │
        └───┘
```

至于栈怎么用数组或链表实现,这里不展开(本阶段后续会专门讲)。记住 LIFO 就够往下看了——因为**系统管理函数调用,用的就是这么一个栈**。

## 系统函数调用栈

来看一段再简单不过的 C:从 `main` 进去,调 `getAddOf2Int`,它再调 `getModFrom2Int`。

```c
#include <stdio.h>

int getModFrom2Int(int a, int b) { return a % b; }

int getAddOf2Int(int a, int b) {
    int resFromMod = getModFrom2Int(a, b);
    return resFromMod + b;
}

int main(void) {
    int a = 3, b = 2;
    int c = getAddOf2Int(a, b);
    printf("c = %d\n", c);
    return 0;
}
```

跑一下:

```text
$ gcc -g -O0 callchain.c -o callchain && ./callchain
c = 3
```

执行顺序一眼能看出来:`main` → `getAddOf2Int` → `getModFrom2Int` → 逐级返回。问题是——**系统怎么记得"该回到哪、局部变量放哪"?** 答案就是每调用一个函数,系统就在调用栈上**压一个栈帧(frame)**,函数返回时弹掉。一个栈帧通常装三样东西:

1. **函数参数**(如 `getAddOf2Int` 的 `a`、`b`);
2. **局部变量**(如 `main` 里的 `a b c`、`getAddOf2Int` 里的 `resFromMod`);
3. **返回地址**——CPU 跳进被调函数后,回来时该接着执行哪条指令。

口说无凭,上汇编。`objdump -d` 反汇编,能看到每处函数调用都是一条 `call` 指令:

```text
$ objdump -d callchain | grep -E "call.*<get(Mod|Add)"
    1166:   e8 ce ff ff ff      call   1139 <getModFrom2Int>
    1198:   e8 b1 ff ff ff      call   114e <getAddOf2Int>
```

`call` 干两件事:把**返回地址压栈**,再跳到目标函数;函数末尾的 `ret` 则把返回地址弹出来、跳回去。所以"函数调用"在机器层面,就是**往栈上压一帧、再弹一帧**。

## 递归:函数调自己

既然函数调用的本质是压栈,那**函数调自己**当然也成立——每调一次压一帧。先看个反例:一个没有出口、永远调自己的递归。

```c
#include <stdio.h>
void recur(int depth) {
    printf("depth=%d\n", depth);
    recur(depth + 1);   /* 永不停止 */
}
int main(void) { recur(1); return 0; }
```

跑起来你会发现它打印到某个深度后**直接崩**:

```text
$ gcc -O0 infrecur.c -o infrecur && ./infrecur
depth=399240
depth=399241
depth=399242
Segmentation fault (core dumped)      ← 栈溢出,退出码 139
```

原因很直白:每层调用都压一个栈帧,没有出口就一路压下去,直到**撞穿系统给程序的栈空间上限**,触发 `SIGSEGV`。所以递归的第一条铁律:**必须有终止条件(出口)**。

加个出口就正常了。高斯求和 `1+2+...+n`,递归定义是 `f(n) = n + f(n-1)`,`f(0) = 0`:

```c
#include <stdio.h>
long gaussian(int n) { return n == 0 ? 0 : n + gaussian(n - 1); }
int main(void) { printf("sum(1..10) = %ld\n", gaussian(10)); return 0; }
```

```text
$ gcc gauss.c -o gauss && ./gauss
sum(1..10) = 55
```

`n == 0` 就是出口。调用栈的展开过程:压 `gaussian(10)` → `gaussian(9)` → … → `gaussian(0)`,到 0 开始逐层弹回、把和累加起来。

> 一个有意思的小把戏:把"打印"放在递归调用**之前**还是**之后**,结果完全不同。放在之前是正序,放在之后是倒序——因为"之后"意味着先一路压到底、再一边弹栈一边打印:

```c
void printReverse(const int* a, int n) {   /* 先递归再打印 → 倒序 */
    if (n == 0) return;
    printReverse(a, n - 1);
    printf("%d ", a[n - 1]);
}
```

```text
$ ./arrprint      /* 同一份代码里 printReverse 与 printForward 的实测输出 */
4 3 2 1           ← printReverse(倒序)
1 2 3 4           ← printForward(正序,打印在前)
```

调换两行的顺序就能反转遍历方向——这正是栈 LIFO 的妙处。

## 递归 = 系统在帮你维护一个栈

想透这一点,递归就不神秘了:**任何递归都能改写成"用一个显式栈 + 循环"**。递归不过是把"该处理什么"这个任务,交给了系统的调用栈去记住;你完全可以自己拿一个数组当栈、用 `while` 循环手动压弹,效果等价(而且省去了函数调用的开销、也不会栈溢出)。

这个等价性在后面学**树和图的遍历**时会反复用到:深度优先遍历既能写成递归,也能写成显式栈——它们本质是同一个东西。

## 性能代价,以及什么时候用

递归写得爽,但有代价:

- **时间**:每次函数调用都要压栈、弹栈,有开销;而且像朴素递归算斐波那契 `fib(n)=fib(n-1)+fib(n-2)` 会**重复计算大量子问题**,复杂度爆炸(指数级)。
- **空间**:递归深度就是栈帧数量,占 `O(深度)` 的栈空间;太深就栈溢出(上面那个 `399242` 层就炸了)。

所以**什么时候该用递归**?一句话:**问题本身是"自相似"的、或天然是非线性结构(树、图)时,用递归最自然**。线性表的简单遍历,循环往往更清晰、更省;但树的遍历、分治、回溯,递归几乎是本能写法。遇到重复子问题(如 fib),加**记忆化**或改动态规划,别裸递归。

## 常见踩坑

- **忘了出口 / 出口写错**:必炸栈。写递归第一件事先确定终止条件,且参数必须**朝出口收敛**(每层让问题规模变小)。
- **深递归栈溢出**:系统栈通常就几 MB(本例 ~40 万层 int 栈帧就炸)。超深递归改循环 + 显式栈。
- **朴素递归重复计算**:`fib` 那种,加记忆化数组或改 DP。
- **把"打印在前/后"搞反**:遍历顺序全错,记住 LIFO。

## 小结

递归的本质就一句:**函数调自己,系统在调用栈上替你压栈;有出口、能收敛,就是合法递归。** 把"栈帧 = 参数 + 局部变量 + 返回地址""LIFO 决定遍历顺序""递归 ⇄ 显式栈等价"这三点吃透,后面树和图的递归写法你会觉得理所当然。

## 练习

1. 把 `gaussian` 改成**尾递归**(带一个累加参数 `gaussian(int n, long acc)`),想想它和原版在栈帧占用上有什么区别。
2. 写一个递归求字符串长度(模拟 `strlen`),终止条件是遇到 `'\0'`。
3. 用递归实现**二分查找**(数组已排序),注意终止条件和区间收敛。
4. 把练习 1 的尾递归改写成 `while` 循环 + 显式参数,体会"递归 ⇄ 迭代"的等价。
5. 写朴素递归 `fib(40)`,计时;再加记忆化,对比耗时(体会重复子问题的代价)。

## 参考资源

- *C 语言数据结构与算法* 相关教材的"栈"与"递归"章节。
- `man objdump` / `gcc -S` —— 自己看汇编,理解 `call`/`ret` 与栈帧。
- [recursion — Wikipedia](https://en.wikipedia.org/wiki/Recursion_(computer_science))

---
*整理自作者笔记,按 C-Journey 写作规范重写为纯 C;所有输出本机实测捕获。*
