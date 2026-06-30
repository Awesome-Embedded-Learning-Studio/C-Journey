---
title: "控制流：if/for/while/switch 与那个 fall-through 坑"
description: "这一章过 C 的控制流：if/else、for、while、do-while、switch、break、continue、goto。重点真跑两个最容易出 bug 的地方——switch 忘了写 break 导致的「贯穿」（fall-through，case 2 命中后一路执行进 case 3、default，真跑给你看「二/三/默认」全打出来）；以及 break（跳出整个循环）和 continue（只跳过本轮、进下一轮）的区别（真跑一个跳过偶数、超过 7 就停的循环，得 1+3+5+7=16）。顺带说清 gcc 的 -Wimplicit-fallthrough 怎么提醒你「这里可能是不小心的贯穿」，以及 goto 那个老掉牙的争议。"
chapter: 1
order: 7
tags:
  - host
  - control-flow
difficulty: beginner
reading_time_minutes: 11
platform: host
c_standard: [11, 99]
prerequisites:
  - "第 5 章：运算符基础（关系/逻辑运算符构成条件）"
related:
  - "第 5 章：运算符（条件表达式里用的 && || 等）"
---

> 🟡 状态：待审核（2026-06-30）

# 控制流：if/for/while/switch 与那个 fall-through 坑

## 引言：C 的控制流就那么几样

C 的控制流语句不多：分支 `if`/`else`/`switch`、循环 `for`/`while`/`do-while`、跳转 `break`/`continue`/`goto`/`return`。这一章我们快速过一遍，重点讲两个新人最容易栽的地方——**switch 的 fall-through（忘写 break 贯穿）**和 **break 与 continue 的区别**，每一步真跑。

## `if` / `else`

`if` 没什么神秘的：`if (条件) {...} else if (...) {...} else {...}`，条件为非 0 就进对应分支。几个要点：条件表达式只要是「非 0」就算真（所以 `if (p)` 等价 `if (p != NULL)`、`if (count)` 等价 `if (count != 0)`，这是 C 里很常见的简写）；`else` 和最近的、还没配对的 `if` 结合（「悬空 else」问题，所以嵌套 `if` 时用大括号明确归属最稳，别让缩进骗了你的眼睛）。

## 循环：`for` / `while` / `do-while`

三种循环。`for (init; cond; incr)` 把初始化、条件、递增写在一条里，最适合「已知次数」的循环（比如遍历数组）；`while (cond)` 是「当条件真就继续」，适合「不知道要跑几轮、看条件」的场景；`do { ... } while (cond);` 和 `while` 的区别是**它先跑一轮、再判断**，所以循环体至少执行一次。三者可以互相改写，按场景挑顺手的。

循环里两个跳转要分清：`break` 跳出**整个**循环（不再继续）、`continue` 只跳过**本轮**剩下的部分、直接进下一轮判断。真跑一个两个都用上的：

```c
#include <stdio.h>

int main(void) {
    int sum = 0;
    for (int i = 1; i <= 10; i++) {
        if (i % 2 == 0)
            continue; /* 跳过偶数,直接进下一轮 */
        if (i > 7)
            break; /* 超过 7 就跳出整个循环 */
        sum += i;
    }
    printf("sum = %d (只累加 1,3,5,7)\n", sum);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall loopcontrol.c -o lc && ./lc
sum = 16 (只累加 1,3,5,7)
```

跟着 `i` 从 1 走：`i=1` 奇数、`>7` 否，累加（sum=1）；`i=2` 偶数，`continue` 跳过；`i=3` 累加（sum=4）；`i=4` 跳；`i=5` 累加（sum=9）；`i=6` 跳；`i=7` 累加（sum=16）；`i=8` 偶数先 `continue` 跳过……`i=9` 奇数但 `>7`，`break` 跳出。最终 `sum=16`（=1+3+5+7）。`continue` 让你「跳过本轮的剩余语句」、`break` 让你「直接走人」，一个温和一个干脆，别用混。

## `switch` 与 fall-through：忘写 break 的经典坑

`switch` 根据「一个整数表达式」的值，跳到匹配的 `case` 标签处开始执行。它的坑在于：**匹配到某个 `case` 后，会从那里一直往下执行，遇到 `break` 才停——如果你忘了写 `break`，它就会「贯穿」（fall through）进下一个 `case`**，哪怕那个 `case` 的值并不匹配。真跑一个故意忘写 `break` 的：

```c
#include <stdio.h>

int main(void) {
    int x = 2;
    printf("switch(%d),case 都没写 break:\n", x);
    switch (x) {
    case 1:
        printf("  一\n");
    case 2:
        printf("  二\n"); /* 命中这里,然后一路贯穿下去 */
    case 3:
        printf("  三\n");
    default:
        printf("  默认\n");
    }
    return 0;
}
```

```text
$ gcc -std=c11 -Wall switchfall.c -o sf && ./sf
switch(2),case 都没写 break:
  二
  三
  默认
```

`x=2`，switch 跳到 `case 2` 打印「二」——**然后没有 break，它停不下来**，一路执行进 `case 3` 打「三」、再进 `default` 打「默认」。明明只匹配了 `case 2`，却打出了三条。这就是 fall-through，是 switch 最经典的 bug 来源：你以为只执行匹配的那段，结果它「漏」进了下面所有分支。

绝大多数情况下，你想要的是「每个 case 执行完就跳出去」，所以**每个 case 末尾都要老老实实写 `break`**（最后一个 case/default 写不写 break 效果一样，但加上更安全、改代码时不会忘）。偶尔你确实想利用 fall-through（让几个 case 共享同一段代码），那是刻意为之，gcc 给了个开关帮你区分「不小心的贯穿」和「故意的」：`-Wimplicit-fallthrough` 会在「没写 break 的贯穿」处警告你，而如果你确实想贯穿，加个注释 `/* fall through */`（或 `__attribute__((fallthrough))`）就告诉编译器「我知道、故意的」，警告就消了。所以工程里开 `-Wimplicit-fallthrough`，能让 fall-through 这个坑至少在编译期被揪出来。

顺带几个 switch 的要点：`switch` 只能对**整型**（含 `char`、`enum`）switch，不能对浮点或字符串；`case` 后面必须是**常量表达式**（编译期已知的值，不能是变量）；没匹配的会走 `default`（没 `default` 就啥也不执行、直接出 switch）。

## `goto`：能用，但请克制

最后说 `goto`。它能无条件跳到一个标签（`goto cleanup;`），历史上被骂得很惨（Dijkstra 那篇「Go To 有害」），因为滥用会让控制流变成一团乱麻。但 C 里 `goto` 有一个**正当且常见**的用途：**集中错误清理**。C 没有异常、没有析构，函数里多处分配资源/获取锁、任意一处失败都要释放前面已获取的，用 `goto` 跳到函数末尾的清理段是最干净的写法（Linux 内核里到处都是这套）。所以记法是：**`goto` 只往后跳、只跳到清理段，不要拿它当循环用、不要乱跳**——这样它就是好工具，不是 spaghetti。

## 小结

C 的控制流里，`if` 条件「非 0 即真」（`if (p)` ≡ `if (p != NULL)`）；三种循环 `for`（已知次数）、`while`（看条件）、`do-while`（至少跑一次）可互相改写。循环里 `break` 跳出整个循环、`continue` 只跳过本轮（真跑一个跳偶数、超 7 停的循环得 1+3+5+7=16，看清两者区别）。`switch` 最经典的坑是 **fall-through**：匹配 `case` 后没有 `break` 会一路贯穿进下面的分支（真跑 `switch(2)` 打出「二/三/默认」三条），所以每个 `case` 末尾都要写 `break`，工程里开 `-Wimplicit-fallthrough` 让编译器揪出不小心的贯穿（故意贯穿加 `/* fall through */` 注释）。switch 只对整型、`case` 必须是常量。`goto` 能用但请克制——只在「往后跳到清理段」这种错误清理场景用，别当循环使。下一章我们看 C 的函数。

## 参考资源

- ISO/IEC 9899:2011 §6.8.4（选择语句：`if` §6.8.4.1、`switch` §6.8.4.2）、§6.8.5（迭代语句：`while`/`do`/`for`）、§6.8.6（跳转：`goto`/`continue`/`break`/`return`）
- GCC 手册：`-Wimplicit-fallthrough`（switch 贯穿警告，及 `/* fall through */` / `__attribute__((fallthrough))` 注解）
- 第 5 章：运算符（条件里用的关系/逻辑运算符）
