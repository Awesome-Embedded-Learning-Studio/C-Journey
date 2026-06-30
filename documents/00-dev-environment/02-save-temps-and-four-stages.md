---
title: "编译四阶段全景：用 -save-temps 一次看穿 .i/.s/.o"
description: "课程第 0 章第二篇：把 gcc 的黑盒撬开成预处理/编译/汇编/链接四个阶段，用 -E/-S/-c 分别停在每一站、用 -save-temps 一把生成全部中间产物。真跑出 .i 的 stdio.h 膨胀、.s 的汇编、.o 不能直接执行、AT&T 与 Intel 两种汇编语法，并把『编译』这个词的误用一次性纠过来。"
chapter: 0
order: 2
tags:
  - host
  - toolchain
  - build
difficulty: beginner
reading_time_minutes: 15
platform: host
c_standard: [11]
prerequisites:
  - "第 1 章：工具链体检"
related:
  - "第 3 章：预处理深入：宏展开、头文件膨胀与条件编译"
  - "第 4 章：编译阶段看汇编：段与 ABI 预览"
---

# 编译四阶段全景：用 -save-temps 一次看穿 .i/.s/.o

## 引言：`gcc hello.c -o hello` 其实是个四段黑盒

上一章我们已经能亲手敲出 `gcc hello.c -o hello`，看着它吐出一个能跑的可执行文件。但如果你跟我一样，曾经被一个 `undefined reference`（链接找不到符号）或者一个宏没展开的诡异 bug 卡过半天，你就会意识到一个事实：**`gcc hello.c -o hello` 这一行命令，背后其实串了四个完全不同的阶段**，而我们平时把它们一锅端地叫成「编译」。

这一章我们要做的，就是把这个黑盒撬开。gcc 给每个阶段都留了一个「半路下车看一眼」的开关，让我们能让流水线**停在任意一站**，把那一站的中间产物摆出来看。等你能对着 `.i`、`.s`、`.o` 这几个文件说清楚它们各自是哪一站的产物、各自长什么样，后面所有的排查——宏展开错了、链接缺符号了、汇编看不懂了——才有了抓手。

> 先把术语纠过来：**严格意义上的「编译（compile）」只是四阶段里的第二步**（把预处理后的代码翻译成汇编）。但从 `gcc hello.c -o hello` 整段来看，gcc 替我们把预处理、编译、汇编、链接全干了。本书里我会尽量用「翻译/构建」指代整段、用「编译」专指第二步，你在别处看到「编译」时也要心里有数它到底指哪一段——术语混用是排查方向全错的头号原因。

## 核心概念：四阶段、四产物、四个停靠开关

我们先看整条流水线长什么样：

```text
hello.c ──[预处理]──▶ hello.i ──[编译]──▶ hello.s ──[汇编]──▶ hello.o ──[链接]──▶ 可执行
              gcc -E             gcc -S           gcc -c            gcc / ld
```

四站、四个产物、四个能让你停下来的开关：

| 阶段 | 干什么 | 产物 | 停靠开关 |
|---|---|---|---|
| 预处理 | 文本替换：展开宏、塞头文件、处理 `#ifdef` | `.i`（预处理后的 C 源码） | `-E` |
| 编译 | 把 C 翻译成汇编 | `.s`（汇编文本） | `-S` |
| 汇编 | 把汇编翻成机器码 | `.o`（可重定位目标文件） | `-c` |
| 链接 | 把 `.o` 和库拼到一起、填好地址 | 可执行文件 | （默认，或显式 `gcc *.o`） |

有一点要先讲清楚、免得你把它们当成 C 标准的规定：**C 标准（ISO/IEC 9899 §5.1.1.2「翻译阶段」）确实把从源码到程序的概念过程分成若干阶段，预处理是其中明确的一环**；但 `.i`/`.s`/`.o` 这些**具体的文件产物、以及 `-E`/`-S`/`-c`/`-save-temps` 这些开关，是 gcc 的实现细节，不是标准规定的**。换个编译器，产物和开关可能不一样——但「翻译是分阶段的、预处理是独立一环」这个概念，是标准给的。

## 真跑：让流水线在每一站都停一下

口说无凭，我们用一个带宏的小程序当靶子（全程显式钉 `-std=c11`，理由见第 1 章）：

```c
#include <stdio.h>

#define GREET "hello from C"

int main(void) {
    printf("%s\n", GREET);
    return 0;
}
```

### 第一站：`-E` 停在预处理后

`gcc -E` 让 gcc 做完预处理就停下，把结果（`.i` 文件）吐出来：

```text
$ gcc -std=c11 -E hello.c -o hello.i
$ wc -l hello.i
565 hello.i
```

**565 行。** 我们的 `hello.c` 满打满算才 8 行，预处理后怎么变成 565 行了？因为 `#include <stdio.h>` 把整个标准 IO 头文件**原样塞了进来**——所有的 `printf` 声明、类型定义、编译器内部标注，全展开进了这个 `.i`。这就是「预处理只做文本替换」最直观的体现：它不懂 C 语法，就是把头文件内容和宏定义机械地拼进去。

我们看一眼 `.i` 的末尾，确认宏也被替换了：

```text
$ tail -3 hello.i
    printf("%s\n", "hello from C");
    return 0;
}
```

看到了吗——源码里的 `GREET` 已经不见了，`printf("%s\n", GREET)` 变成了 `printf("%s\n", "hello from C")`。**宏在预处理这一站就被替换成了它的展开内容**，等到了下一站（编译），编译器根本不知道 `GREET` 曾经存在过。

> 顺手一个真跑出来的细节：`.i` 的行数会**随 `-std` 变化**。同一个 `hello.c`，默认的 `gnu23` 下 `.i` 有 **846 行**，钉到 `c11` 是 **565 行**——因为默认的 GNU 扩展会拉进来更多头文件内容。所以别把「预处理后多少行」当成一个固定数，它取决于你的标准和平台。

### 第二站：`-S` 停在编译后（汇编）

`gcc -S` 让 gcc 做完编译就停，产物是汇编文本 `.s`：

```text
$ gcc -std=c11 -S hello.c -o hello.s
$ wc -l hello.s
28 hello.s
```

28 行汇编。我们看一眼 `main` 函数体（去掉那些以 `.` 开头的、给汇编器/调试器看的伪指令）：

```asm
main:
	pushq	%rbp
	movq	%rsp, %rbp
	leaq	.LC0(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT
	movl	$0, %eax
```

先别慌，不用全看懂，这是第 4 章的主场。这里只要建立两个直觉：第一，**C 代码到了这一站变成了汇编指令**（`pushq`、`movq`、`call` 这些）；第二，有个挺有意思的事——我们源码写的是 `printf`，汇编里却变成了 `call puts@PLT`。**gcc 发现 `printf("%s\n", "hello from C")` 这种「只打一个字符串加换行」的模式，悄悄把它优化成了更快的 `puts`**。这就是为什么「看汇编」有时能解释一些源码层面的迷惑：编译器替你做的优化，只有在汇编这一站才看得见。

### 第三站：`-c` 停在汇编后（目标文件）

`gcc -c` 让 gcc 做完汇编就停，产物是目标文件 `.o`：

```text
$ gcc -std=c11 -c hello.c -o hello.o
$ file hello.o
hello.o: ELF 64-bit LSB relocatable
```

注意 `file` 给的关键词是 **relocatable（可重定位）**，对比一下最终可执行文件：

```text
$ gcc -std=c11 hello.o -o hello
$ file hello
hello: ELF 64-bit LSB pie executable, x86-64
$ ./hello
hello from C
```

可执行文件是 **pie executable**。两者的区别正是第四站「链接」干的事：`.o` 里的地址还是「待定（可重定位）」的，链接这一站才把地址填实、把 `puts` 到底该跳到 libc 的哪个位置确定下来，产物才能跑。这也是下一个坑的由来。

## `.o` 不能直接跑：它还只是「可重定位」

新手常以为 `.o` 既然是「编出来了」，应该能直接执行。我们试一下：

```text
$ chmod +x hello.o     # 先给它执行权限，否则内核连看都不看
$ ./hello.o
zsh: exec format error: ./hello.o
（退出码 126）
```

`exec format error`——内核拒绝执行。因为 `.o` 是**可重定位目标文件，地址没填实**，它的格式（`relocatable`）根本不是内核能加载运行的可执行格式。**`.o` 必须经过链接、变成 `executable` 才能跑。** 这个区分后面讲链接（第 6 章）时会反复用到。

## `-save-temps` 会弄脏你的目录

如果你嫌一个个 `-E`/`-S`/`-c` 太麻烦，gcc 还有个「一把全停」的开关 `-save-temps`——让它在正常编译的同时，把所有中间产物都留下来：

```text
$ gcc -std=c11 -save-temps hello.c -o saveexe
$ ls -1 saveexe saveexe-hello.i saveexe-hello.s saveexe-hello.o
saveexe
saveexe-hello.i
saveexe-hello.o
saveexe-hello.s
$ ./saveexe
hello from C
```

一条命令，既编出了可执行文件 `saveexe`，又把 `.i`/`.s`/`.o` 全留下了——方便是真方便。但你看那些中间产物的名字：`saveexe-hello.i`、`saveexe-hello.s`、`saveexe-hello.o`——**它们是用「`-o` 的名字 + 源文件名」拼出来的复合名，直接落在你当前目录里**。

说真的，`-save-temps` 会**污染你的源码树**——你要是在项目根目录跑它，这些 `.i`/`.s`/`.o` 会散落得到处都是，甚至可能被 `git add .` 误卷进去。正确做法是**在一个专门的临时目录里跑**（本书示例都放在 `/tmp/` 下），或者用 `-save-temps=obj` 配合 `-o` 指定到构建目录。别图省事在源码树里裸跑它。

## AT&T 还是 Intel：汇编语法别读反

刚才那段汇编（`pushq %rbp`、`movq %rsp, %rbp`）是 gcc 的**默认 AT&T 语法**。但你看的很多资料、尤其是 Intel/微软系的书，用的是 **Intel 语法**，两者长得完全不一样。gcc 用 `-masm=intel` 可以切到 Intel 语法：

```text
$ gcc -std=c11 -S hello.c -o att.s              # 默认 AT&T
$ gcc -std=c11 -S -masm=intel hello.c -o intel.s  # Intel 语法
```

同样是 `main` 的开头几条，两种语法对比（都去掉了伪指令）：

```asm
// AT&T（gcc 默认）          // Intel（-masm=intel）
main:                        "main":
    pushq   %rbp                 push    rbp
    movq    %rsp, %rbp           mov     rbp, rsp
    leaq    .LC0(%rip), %rax     lea     rax, .LC0[rip]
    movq    %rax, %rdi           mov     rdi, rax
    call    puts@PLT             call    "puts"@PLT
    movl    $0, %eax             mov     eax, 0
```

四条规则，记住就能在两种语法间切换：

1. **寄存器前缀**：AT&T 带 `%`（`%rbp`），Intel 不带（`rbp`）。
2. **立即数前缀**：AT&T 带 `$`（`$0`），Intel 不带（`0`）。
3. **操作数顺序相反**：AT&T 是「源, 目的」（`movq %rsp, %rbp` = 把 rsp 放进 rbp）；Intel 是「目的, 源」（`mov rbp, rsp` = rbp 从 rsp 取值）。**这是最容易读反的一点**，顺序搞反，整段汇编的意思全反。
4. **指令带不带尺寸后缀**：AT&T 用后缀表示大小（`pushq` 的 `q`=quad/8 字节、`movl` 的 `l`=long/4 字节）；Intel 不带（`push`、`mov`），大小从寄存器名推断。

> 顺带一个诚实的小怪癖：我这台机器上的 **gcc 16 在 Intel 模式下会把符号加引号**写成 `"main":`、`call "puts"@PLT`（你看上面 Intel 那栏）。AT&T 模式下则不带引号。这是 gcc 某些版本的行为，不是汇编语法本身的规定——你换台机器/别的版本可能不带引号。**遇到对不上号的细节，别怀疑自己，先确认编译器版本和汇编语法**。

## 小结

到这一章，gcc 的黑盒就被我们撬开了。核心就一张图：四阶段、四产物、四个停靠开关——预处理出 `.i`、编译出 `.s`、汇编出 `.o`、链接出可执行，分别对应 `-E`/`-S`/`-c`；而严格意义上的「编译」只是其中第二步，从 `.c` 到可执行整段该叫「翻译/构建」，术语别混。预处理这一站你要建立的最硬直觉是「它只做文本替换、不懂 C 语法」：`#include` 把头文件原样塞进来（`hello.c` 几行 → `.i` 几百行）、宏被原样展开（`GREET` → `"hello from C"`），这是 C 标准 §5.1.1.2 翻译阶段的体现。另外记住几个实物结论：`.o` 是 `relocatable`、地址没填实、不能直接跑，必须链接成 `executable`；`-save-temps` 方便但会污染源码树，要在临时目录里跑、别在项目根裸跑；汇编 gcc 默认出 AT&T 语法（`%`/`$`/源在前），`-masm=intel` 切 Intel，两种语法的操作数顺序相反，读反了整段都看不懂。

下一章我们一头扎进第一站——预处理，亲手让宏展开的副作用、条件编译的「走了 `#else`」这些坑现形。再下一章（第 4 章），我们会把这段汇编里的 `.text`/`.rodata` 段、函数序言、参数怎么走寄存器，系统地讲清楚。

## 参考资源

- ISO/IEC 9899 §5.1.1.2（翻译阶段：预处理是标准定义的独立阶段）
- GCC 手册：`-E`/`-S`/`-c`/`-save-temps`/`-masm=` 开关说明
- 第 4 章：编译阶段看汇编（`.text`/`.data`/`.bss`/`.rodata` 段、ABI 预览）
- 第 6 章：链接与静态库（为什么 `.o` 是 relocatable、链接填的是什么）
