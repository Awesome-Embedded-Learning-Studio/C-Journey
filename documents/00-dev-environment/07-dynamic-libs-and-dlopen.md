---
title: "动态库与 dlopen：位置无关代码与运行时加载"
description: "第 5 章那个一直留着 U 的 printf@GLIBC，这一章给它收尾。编一个 .so 动态库，用 dlopen/dlsym 在运行期加载并调用里面的函数，看清动态库和静态库在『链接期 vs 运行期』上的本质差别。顺手拆三个真跑出来的现实：相对路径让 dlopen 找不到库、本机 glibc 2.43 其实不需要 -ldl、不加 -fPIC 在这台 gcc16 上竟没报错——以及为什么 -fPIC 仍是写 .so 的正解。"
chapter: 0
order: 7
tags:
  - host
  - toolchain
  - linker
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "第 6 章：链接与静态库"
related:
  - "第 5 章：目标文件与符号"
---

# 动态库与 dlopen：位置无关代码与运行时加载

## 引言：`printf@GLIBC` 那个一直没填的 `U`

第 5 章链接完 `prog` 之后，我们留意到一个怪现象：`counter`、`visible_fn` 这些都拿到了真实地址，唯独 `printf` 在可执行文件里**仍然是 `U`**，挂着个 `@GLIBC_2.2.5` 的尾巴。当时我说「它来自动态库，等运行期再填」——这一章就把这句话拆透。

静态库（`.a`，第 6 章那种）是**链接期**就把需要的代码抽进可执行文件，编完就自包含了；动态库（`.so`）不一样，它**链接期只在你可执行文件里记一句「我需要 libxxx.so 里的 printf」**，真正的代码不在你程序里，而是**程序启动时由动态链接器去加载 `.so`、再把符号地址填上**。这就是 `printf` 留 `U` 的原因——它的地址要到运行期、动态链接器加载 glibc 时才确定。

更灵活的是，你甚至可以**在程序跑起来之后，自己用 `dlopen` 临时加载一个库、用 `dlsym` 从里面取符号来调**——这一章我们就把这套从编译 `.so` 到运行期 `dlopen` 的链路完整走一遍。和前几章一样先划线：**动态库怎么加载、`-fPIC`、`dlopen` 的行为，全是工具链/动态链接器/POSIX 的实现现实，ISO C 不管**；语言层面唯一沾边的是 `dlsym` 那个指针转换，后面会引标准条款。

## 编一个动态库：`-fPIC -shared`

动态库的代码有个硬要求：**位置无关（position-independent）**。因为 `.so` 加载到进程里时，每次的虚拟地址都可能不一样（ASLR、被不同进程共享同一段物理内存），它里面的指令不能写死「跳到地址 0x401234」，得用「相对于当前指令的偏移」来表达。让 gcc 产出这种代码的开关是 `-fPIC`，产出 `.so` 的开关是 `-shared`。

我们拿第 6 章那对老朋友接着用：

```c
/* add.c */ int add(int a, int b) { return a + b; }
```
```c
/* mul.c */ int mul(int a, int b) { return a * b; }
```
```text
$ gcc -std=c11 -fPIC -shared -o libmymath.so add.c mul.c
$ file libmymath.so | cut -d, -f1-2
libmymath.so: ELF 64-bit LSB shared object, x86-64
```

`file` 说它是 **shared object**——和第 1 章那个 `pie executable`（可执行）、第 5 章那个 `relocatable`（目标文件）是 ELF 的不同种类，一眼能分清。

这里我得**当场说实话**：按很多老资料的讲法，「编 `.so` 不加 `-fPIC` 链接器会直接拒绝」。可我在自己这台 gcc 16.1.1 / x86-64 上试了，**这个简单 `.so` 不显式加 `-fPIC` 也编出来了、没报错**：

```text
$ gcc -std=c11 -shared -o libmymath_nopic.so add.c mul.c
$                      ← 静默成功,没报 "recompile with -fPIC"
```

原因不难理解：现代 gcc 在 x86-64 上默认就倾向产位置无关代码（可执行文件默认都是 PIE 了），`add`/`mul` 这种不碰全局地址的简单函数，加不加 `-fPIC` 产出的码没区别，自然不报错。但**别因此就觉得 `-fPIC` 可有可无**——一旦你的库里有「取全局变量地址」「跳进一张非 PIC 的跳转表」这类代码、或者换到老一点的工具链、换到某些对 PIC 要求更严的架构，不加 `-fPIC` 立刻就是 `relocation R_X86_64_PC32 against ... can not be used when making a shared object; recompile with -fPIC`。所以规矩照旧：**编 `.so` 永远显式带 `-fPIC`**，别赌自己代码正好是 PIC 友好的那种。

## 运行期加载：`dlopen` / `dlsym` / `dlclose`

现在是这一章的主角。我们写一个 `loader`，它启动时并没有链 `add` 这个符号，而是**跑起来之后自己去 `dlopen` 打开 `libmymath.so`、`dlsym` 把 `add` 捞出来调**：

```c
/* loader.c */
#include <stdio.h>
#include <dlfcn.h>

typedef int (*binop_fn)(int, int);

int main(void) {
    void *h = dlopen("./libmymath.so", RTLD_NOW);   /* 注意:相对路径 */
    if (!h) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    binop_fn add = (binop_fn) dlsym(h, "add");      /* void* 转 函数指针 */
    if (!add) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        return 1;
    }
    printf("add(2,3) via dlopen = %d\n", add(2, 3));
    dlclose(h);
    return 0;
}
```

`dlopen` 返回一个**不透明句柄** `void*`，`dlsym(h, "add")` 用**字符串名字**去库里找符号，找到后也以 `void*` 返回。这里有个绕不开的标准细节：`dlsym` 返回的 `void*` 是个**对象指针**，我们要把它当**函数指针**用（`(binop_fn) dlsym(...)`）。ISO C 只保证**对象指针和 `void*` 之间能安全往返**（§6.3.2.3¶4），但**对象指针↔函数指针的转换，标准并没有给出保证**——它落在实现定义那一档。**POSIX 在 `dlsym` 的条款里额外拍板：返回值可以转成函数指针来用。** 所以这行转换在 Linux/macOS 上是安全的，但严格按 ISO C 它不是定义良好的——这是 C 和 POSIX 之间一条真实的缝，知道它在哪就行。

编它要链 `dl`（`dlopen` 那一族函数在 `libdl`），跑一下：

```text
$ gcc -std=c11 -Wall -Wextra loader.c -o loader -ldl
$ ./loader
add(2,3) via dlopen = 5
```

`-Wall -Wextra` 一声不吭，跑出 `5`。注意整套流程的关键：`loader` 编译链接的时候，`add` 这个符号**根本不在场**——没有 `add.o`、没有 `-lmymath`，链接器也不知道 `add` 长什么样。它是在程序跑起来、`dlopen` 执行的那一刻，才动态地把 `libmymath.so` 映射进来、`dlsym` 查到 `add` 的地址。这就是「运行时加载」的全部含义，也是写插件系统、按需加载模块的基础。

## 两个真坑，一个已经不坑了

**先说相对路径这个真坑。** 我上面 `dlopen` 写的是 `"./libmymath.so"`——带斜杠的相对/绝对路径，`dlopen` 会**按字面去文件系统找**，不做任何库搜索。在库所在目录跑没事，**换个工作目录再跑就炸了**：

```text
$ cd /tmp && /tmp/cj/ch7/loader
dlopen failed: ./libmymath.so: cannot open shared object file: No such file or directory
```

因为从 `/tmp` 看，「`./libmymath.so`」就是 `/tmp/libmymath.so`，那儿当然没有。这坑特别阴——你在自己机器上测得好好的，打包发布后用户的工作目录千奇百怪，程序一到他们那就不定时崩。**正解是用绝对路径**（`dlopen("/abs/path/libmymath.so", ...)`），或者把库装到系统目录（`/usr/local/lib` 等，`dlopen("libmymath.so", ...)` 不带斜杠时才会走库搜索路径），或者设 `LD_LIBRARY_PATH`。一句话：**`dlopen` 的路径别写相对的，除非你百分之百确定进程的 cwd**。

**再说 `-ldl`——这个坑在本机其实已经不在了。** 老资料会告诉你「`dlopen` 在 `libdl` 里，gcc 默认不链，漏了 `-ldl` 就 `undefined reference to 'dlopen'`」。我查了下本机 glibc 版本，再实测：

```text
$ ldd --version | head -1
ldd (GNU libc) 2.43
$ gcc -std=c11 loader.c -o loader_nodl        ← 故意不链 -ldl
$                                             ← 编过了,没报 undefined reference
```

**glibc 2.34 起，`libdl` 已经并入 `libc`**，本机 2.43 上不链 `-ldl` 照样能编过。但在 glibc 2.33 及更早、或 musl 等其它 libc 上，`-ldl` 仍是必需的。所以移植性最好的写法是**照旧带 `-ldl`**——在新 glibc 上它是个空壳库、链了不碍事，在老系统上它能救命，两头都对。

## 它和静态库到底差在哪

把第 6 章的静态库和这章的动态库摆一起，差别一目了然：

| 维度 | 静态库 `.a` | 动态库 `.so` |
|---|---|---|
| 代码何时进程序 | **链接期**：需要的 `.o` 被抽进可执行文件 | **运行期**：动态链接器加载 `.so` |
| 可执行文件体积 | 大（库代码拷进来了） | 小（只记「我要哪个 .so 的哪个符号」） |
| 库升级 | 要**重新链接**程序 | 换个 `.so` 文件即可（前提是 ABI 兼容） |
| 多个程序共享 | 各拷一份 | 同一段 `.so` 物理内存可被多进程共享 |
| 符号在可执行里 | 链接后 `U` 被填成真实地址 | 仍可能留 `U@GLIBC`，运行期填 |

第 5 章那个 `printf@GLIBC` 一直留 `U`，现在彻底清楚了——它是动态库符号，按上表最底行的规则，运行期才填。也正因为 `.so` 是程序跑起来才接进去的，换个库文件不用重新链接程序，这是动态库最大的好处；但这好处有个前提——新老库得 ABI 兼容（函数签名、结构体布局没变），不然换完运行期照样崩。

## 小结

一句话收口：动态库 `.so` 的代码不进可执行文件，链接期只留一句「我要哪个库的哪个符号」，启动时由动态链接器填地址——这就是 `printf@GLIBC` 一直留 `U` 的真相。编 `.so` 用 `-fPIC -shared`，`-fPIC` 是位置无关的硬要求（哪怕这台 gcc 上简单代码不加也编得过，规矩仍要带）。运行期加载靠 `dlopen/dlsym/dlclose`，`dlsym` 返回的 `void*` 要转函数指针，这步靠 POSIX 保证、ISO C 不管。最后记住两个现实里真会咬人的：`dlopen` 的路径别写相对的（换 cwd 就崩），以及 `-ldl` 在新 glibc 上非必需、老的必需——所以移植写法照带。

到这，「从 `.c` 到可执行再到运行期加载」的全链路就都打通了。下一章我们回到编译器本身，系统讲警告旗标——从 `-Wall -Wextra` 一路讲到 `-Werror -Wpedantic -Wconversion`，让编译器替你提前揪出那些会让你半夜爬起来的错。

## 参考资源

- `man dlopen` / `man dlsym` / `man dlclose`（POSIX 运行期加载接口；注意 POSIX 对 `dlsym` 转函数指针的保证）
- `man ld.so`（动态链接器/加载器：`RPATH`/`RUNPATH`、`LD_LIBRARY_PATH`、库搜索顺序）
- glibc 2.34 release notes（`libdl` 并入 `libc` 的变更说明）
- ISO/IEC 9899:2011 §6.3.2.3¶4（指针转换：对象指针与 `void*` 的往返；函数指针不在保证范围内）
- GCC 手册：`-fPIC`/`-fpic`/`-fPIE`、`-shared`
