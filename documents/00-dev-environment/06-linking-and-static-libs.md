---
title: "链接与静态库：undefined reference 到底是谁的锅"
description: "第 5 章我们看见链接器把 main.o 里的 U(visible_fn) 填成了真实地址——这一章正面拆『链接』这一步。亲手制造并诊断 undefined reference、multiple definition，用 ar rcs 打包一个静态库 libmymath.a，并复现那个把人坑哭的『库顺序陷阱』：为什么 gcc -lmymath app.o 会报 undefined reference，而 app.o -lmymath 就没事。全程真链接真报错。"
chapter: 0
order: 6
tags:
  - host
  - toolchain
  - linker
  - build
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "第 5 章：目标文件与符号"
related:
  - "第 7 章：动态库与 dlopen"
  - "第 11 章：make 入门"
---

# 链接与静态库：undefined reference 到底是谁的锅

## 引言：编译都过了，偏偏卡在最后一步

你一定见过这种场景：每个 `.c` 单独 `gcc -c` 都好好的，一到最后链接那一步，蹦出一屏幕 `undefined reference to 'xxx'`，或者 `multiple definition of 'main'`。新手这时候通常的反应是疯狂改 `.c` 里的代码——其实代码没毛病，**毛病在「链接」这一步**：编译器只管一个一个 `.c`（翻译单元）翻成 `.o`，它看不见别的文件；真正把散落的 `.o` 拼到一起、核对符号的，是**链接器**（`ld`，通常 `gcc` 在背后替你调）。

第 5 章我们已经从符号表的视角看过链接器「把 `U` 填成真实地址」的过程。这一章我们换个角度，**正面制造几种最经典的链接错误**，看链接器到底在抱怨什么、为什么。然后把几个 `.o` 打包成一个**静态库**（`.a`），顺手拆掉那个坑过无数人的「库顺序陷阱」。

先划清那条线（和前两章一致）：**链接器怎么扫描符号、按什么顺序、报什么错，全是 `ld`/`gcc` 的实现行为，ISO C 不规定**。语言层面对应的概念是「外部定义」——ISO C §6.9 要求每个外部链接的标识符在整个程序里**恰好有一次外部定义**，违反这条是未定义行为（§6.9¶5）。`multiple definition` 这个链接错误，本质就是链接器在实现层帮你拦下了这个 UB；而它**拦不拦、怎么拦**，标准没说，是工具链的现实。

## 链接器到底干什么：符号解析 + 重定位

一句话：链接器干**两件事**。

1. **符号解析（symbol resolution）**：把每个 `.o` 里那些 `U`（未定义）符号，去别的 `.o` 或库里找到对应的**定义**，对上号。对不上 → `undefined reference`；一个符号被对上好几次（同名强定义撞车）→ `multiple definition`。
2. **重定位（relocation）**：符号都对上之后，把每个 `.o` 合并进可执行文件、确定最终地址，再按第 5 章那张重定位表，把每条「待填地址」的指令回填成真实地址。

这一章我们主要盯第一件——**符号解析**，因为绝大多数链接错误都出在这。

## 复现一：`undefined reference`——你忘了给我定义

最直白的情形：`app.c` 调了 `add` 和 `mul`，但你**没把它们的来源（定义它们的 `.o` 或库）交给链接器**。先造个最小的多文件工程：

```c
/* add.c */
int add(int a, int b) { return a + b; }
```
```c
/* mul.c */
int mul(int a, int b) { return a * b; }
```
```c
/* app.c —— 只声明、不定义,要用 add 和 mul */
#include <stdio.h>
int add(int, int);
int mul(int, int);
int main(void) {
    printf("add=%d mul=%d\n", add(2, 3), mul(2, 3));
    return 0;
}
```

把 `app.c` 编成 `app.o`，**只链它自己**，不给 `add`/`mul` 的来源：

```text
$ gcc -std=c11 -c add.c mul.c app.c
$ gcc app.o -o app_missing          ← 故意漏掉 add.o mul.o
/usr/bin/ld: app.o: in function `main':
app.c:(.text+0x14): undefined reference to `mul'
app.c:(.text+0x25): undefined reference to `add'
collect2: error: ld returned 1 exit status
```

链接器在第 5 章那张符号表里找不到 `add`、`mul` 的定义（它们在 `add.o`、`mul.o` 里，但你没给），于是报 `undefined reference`。**报错信息会告诉你"在哪个 `.o` 的哪个函数里、第几字节处引用的"**（`app.c:(.text+0x14)`）——这是排查的关键线索，顺藤摸瓜就能定位到是哪个调用没着落。

修法很简单：把 `add.o`、`mul.o` 一起链上：

```text
$ gcc app.o add.o mul.o -o app_ok && ./app_ok
add=5 mul=6
```

## 复现二：`multiple definition`——同名定义撞车

反过来，如果一个符号**被定义了不止一次**，链接器也会炸。最常见的就是「两个文件各写了一个 `main`」——这在拼装旧代码时特别容易发生：

```c
/* m1.c */ int main(void) { return 0; }
```
```c
/* m2.c */ int main(void) { return 0; }
```
```text
$ gcc m1.c m2.c -o dup
/usr/bin/ld: ...: in function `main':
m2.c:(.text+0x0): multiple definition of `main'; ...:m1.c:(.text+0x0): first defined here
collect2: error: ld returned 1 exit status
```

`main` 这个符号有两个强定义，链接器不知道该用哪个，直接拦下。语言层这违反了 §6.9¶5（一个外部链接标识符只能有一次外部定义），是 UB；实践中链接器把它当硬错误。

这坑本仓库整合旧代码时就撞过——旧目录里 `error.c` 和 `main.c` 各有一个 `main`，一句 `gcc *.c` 一锅端就 `multiple definition`。所以拼装多份旧代码前，先全局搜一遍 `main`、全局变量名，把重名的改掉或归并。临时绕过有 `-Wl,--allow-multiple-definition`（把后来的弱化处理），但**那是兜底，不是正解**——正解是消除重复定义。

## 把 `.o` 打包成静态库：`ar rcs` 与 `libxxx.a`

工程一大，散落一堆 `.o` 在命令行里列很烦。**静态库**就是把多个 `.o` 打包成一个归档文件 `.a`，按需取用。打包工具是 `ar`：

```text
$ ar rcs libmymath.a add.o mul.o
$ ar t libmymath.a          ← 列出库里有哪些成员
add.o
mul.o
$ nm libmymath.a | grep ' T '
0000000000000000 T add
0000000000000000 T mul
```

`ar rcs` 的三个字母：`r` = 把成员**插入/替换**进归档，`c` = **创建**归档（不存在就建），`s` = 写**符号索引**（这个关键，见下面的坑）。命名约定：静态库叫 `lib<名字>.a`（这里是 `libmymath.a`），这样链接时用 `-l<名字>`（`-lmymath`）就能找到它，再用 `-L<目录>` 告诉链接器去哪找（`-L.` 表示当前目录）。

这里要注意 `ar` 的 `s` 选项是写**符号索引**（早期要单独跑 `ranlib` 生成）。**缺索引的 `.a`，链接器在里面翻不到符号**，老的归档就会报 `archive has no index; run ranlib to add one`。现在 `ar rcs` 默认就带索引，但你要是看到这个报错，就知道是索引的事，`ranlib libmymath.a` 补一下即可。

## 库顺序陷阱：为什么 `-lm` 的位置要命

现在到了本章最该焊死的一条。同样是 `app.o` 和 `libmymath.a`，下面两种写法，一个能跑、一个报错，**区别只在 `-lmymath` 放在 `app.o` 的左边还是右边**：

```text
$ gcc app.o -L. -lmymath -o app_ok && ./app_ok      ← 对象在前、库在后:能跑
add=5 mul=6

$ gcc -L. -lmymath app.o -o app_bad                 ← 库在前、对象在后:报错!
/usr/bin/ld: app.o: in function `main':
app.c:(.text+0x14): undefined reference to `mul'
app.c:(.text+0x25): undefined reference to `add'
collect2: error: ld returned 1 exit status
```

为什么？因为**链接器是按命令行从左到右、单趟扫描的**。它处理到 `libmymath.a` 时，会问："我现在手头有没有未解析的符号，正好是这库里能提供的？" 

- 正确顺序里，`app.o` 先被处理，它登记了"我缺 `add`、`mul`"；接着处理 `libmymath.a`，库里正好有 → 抽取 `add.o`、`mul.o` 来满足。✅
- 错误顺序里，`libmymath.a` 先被处理，**这时 `app.o` 还没轮到、没人声明需要 `add`/`mul`**，所以链接器认为"这库暂时没用"，一个成员都不抽；等轮到 `app.o` 发现需要 `add`/`mul` 时，库**已经扫过去了、不会回头**——于是 `undefined reference`。

记住这条肌肉记忆：**被依赖的放右边，提供依赖的放左边；对象在前，库在后；库之间也要按依赖顺序排**（A 库依赖 B 库，就 `... -lA -lB`）。最臭名昭著的例子是数学库：`gcc foo.c -lm -o foo` 对（`-lm` 在 `foo.c` 之后），写反就可能炸。如果库之间互相依赖、顺序排不过来，`-Wl,--start-group ... -Wl,--end-group` 让链接器在组内多趟扫描（代价是变慢）。

最后这条是新手和老兵的分水岭：库顺序陷阱的报错信息（`undefined reference`）和「漏链 `.o`」的报错**一模一样**，极易误判成「我代码写错了」。**碰到 `undefined reference`，先别改代码，先看链接命令行里库和对象的相对顺序**。

## 小结

链接这一步，本质上链接器就干两件事：符号解析（把每个 `U` 配上定义）和重定位（回填地址）。最常见的两个报错你要会读：`undefined reference` 是用了某符号却没给它的定义来源（`.o` 或库），报错会指明在哪个 `.o` 的哪个函数里引用的；`multiple definition` 是同名强定义撞车，语言层这违反 §6.9¶5、算 UB，实践中链接器会硬拦，拼装旧代码时尤其高发，先全局查重名。库方面，静态库 `libxxx.a` 就是用 `ar rcs` 打包的一堆 `.o`，链接时 `-lxxx -L<dir>`，`ar rcs` 的 `s` 保证里面有符号索引。最后那个最坑人的库顺序陷阱，根源是链接器从左到右只单趟扫描，所以**对象要在前、库在后、被依赖的放右边**；而且它报的错和「漏链 `.o`」一模一样，排查时先看链接命令行的顺序、再想着改代码。

到这里，从 `.c` 到可执行的整条路（预处理 → 编译 → 汇编 → 链接）我们就从黑盒走到白盒了。下一章我们看这条路上的另一条岔路——**动态库**（`.so`）：它和静态库在「链接期 vs 运行期」上有什么本质差别，以及第 5 章那个一直留着 `U` 的 `printf@GLIBC` 到底是怎么在程序启动时被填上的。

## 参考资源

- `man ld` / `man ar` / `man ranlib`（链接器选项、归档与索引）
- `man gcc` 的 `-l` / `-L` / `-Wl,` 选项（把选项透传给链接器）
- ISO/IEC 9899:2011 §6.9 / §6.9¶5（外部定义：每个外部链接标识符恰有一次外部定义）
- Oracle *Linker and Libraries Guide*（静态库归档、链接顺序的权威讲解）
