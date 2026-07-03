---
title: "静态库、动态库与链接顺序:亲手造一次 undefined reference"
description: "阶段 0 第 6 章讲过链接器是什么、阶段 0 第 7 章讲过 dlopen 怎么运行期加载——这一章夹在中间,专讲『链接期』本身:符号解析怎么把一个 undefined reference 真正诊断清楚、静态库那个坑哭无数人的顺序陷阱为什么是『左到右单趟扫描』、装好一个 .so 之后程序运行期到底去哪找它(RPATH/RUNPATH/$ORIGIN)、以及 -fvisibility=hidden 怎么让一个库只导出该导出的符号。最后拿本仓 examples/stage4-cmake-lib 当活教材,把 install/export/find_package 的全链路端到端真跑一遍:用 CMake 装它、消费者 find_package(Mathlib) 链上跑通,并诚实记录 CMake 默认给可执行埋的绝对路径 RUNPATH。全 gcc16+clang22 真跑,贴真实输出。"
chapter: 4
order: 6
tags:
  - host
  - engineering
  - toolchain
  - linker
  - cmake
difficulty: intermediate
reading_time_minutes: 19
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 6 章:链接与静态库(undefined reference/multiple definition/ar rcs 入门)"
  - "阶段 0·第 7 章:动态库与 dlopen(.so/-fPIC/dlopen,本章只讲链接期不碰运行期加载)"
  - "第 1 章:头文件契约(extern 声明、ODR,本章是它的链接器落地)"
  - "第 5 章:CMake 工程化(target 语义、PUBLIC/PRIVATE,本章 install/export 那段要用)"
related:
  - "阶段 0·第 6 章:链接与静态库(本章是它的工程化深化,讲透顺序陷阱与运行期查找)"
  - "阶段 0·第 7 章:动态库与 dlopen(运行期加载归它,链接期归本章)"
  - "第 5 章:CMake 工程化(install/export/find_package 那段是它的延伸)"
---

# 静态库、动态库与链接顺序:亲手造一次 undefined reference

## 引言:链接期到底管什么,跟运行期怎么分

前置阅读是阶段 0 第 6 章和第 7 章——那里讲过链接器 `ld` 是谁(`gcc` 在背后替你调)、`undefined reference` 长什么样、`dlopen` 怎么在程序跑起来之后临时加载一个 `.so`。这一章不再重复这些入门,默认你已经会;我们专攻工程化阶段真会咬人的四件事:**符号解析怎么诊断 `undefined reference`、静态库的顺序陷阱为什么是单趟扫描、动态库装好之后运行期去哪找(`RPATH`/`RUNPATH`/`$ORIGIN`)、以及 `-fvisibility=hidden` 怎么收敛一个库的导出符号**。最后把这四件事接到真实工程上——拿本仓库 `examples/stage4-cmake-lib` 那个能产 `.a`+`.so`+`install`+`export` 的活教材,把 CMake 的 install→export→`find_package` 全链路端到端跑通,顺便诚实记录 CMake 默认给可执行埋的那条 RUNPATH 是绝对路径、迁机器会跟着走。

先划清那条线(和阶段 0 一致):**链接器怎么扫符号、按什么顺序、`RPATH`/`RUNPATH` 的语义、`-fvisibility` 的行为,全是 `ld`/`gcc`/ELF 的实现现实,ISO C 不管**。语言层面唯一沾边的,是 §6.9 那条「每个外部链接标识符恰有一次外部定义」(违反是 UB,§6.9¶5)——`undefined reference` 和 `multiple definition` 这两个链接错误,本质都是链接器在实现层帮你拦下了符号没着落或多重定义的情况。至于它**怎么扫、扫几趟、去哪找库**,标准没说,是工具链的现实,我们真跑来看。

## 符号解析:`undefined reference` 到底在抱怨什么

链接器把一堆 `.o` 拼成可执行文件时,核心活儿之一是**符号解析**:每个 `.o` 里那些没定义的符号(阶段 0 第 5 章用 `nm` 看见的 `U` 大写),要去别的 `.o` 或库里找定义、对上号;对不上,链接器就甩 `undefined reference` 拒绝生成可执行文件。先造一个最小的多文件工程,亲手复现一次,把诊断的套路焊死。

```c
/* add.c */
int add(int a, int b) {
    return a + b;
}
```

```c
/* mul.c */
int mul(int a, int b) {
    return a * b;
}
```

```c
/* app.c —— 只声明、不定义,要调用 add 和 mul */
#include <stdio.h>

int add(int, int);
int mul(int, int);

int main(void) {
    int s = add(2, 3);
    int p = mul(2, 3);
    printf("sum=%d prod=%d\n", s, p);
    return 0;
}
```

先逐个 `gcc -c` 把三个 `.c` 编成 `.o`(每个翻译单元都过——编译器只看单文件,不知道 `add`/`mul` 到底有没有人提供),然后**故意只链 `app.o`**、不给 `add.o`/`mul.o`:

```text
$ gcc -std=c11 -Wall -Wextra -c app.c -o app.o
$ gcc -std=c11 -Wall -Wextra -c add.c -o add.o
$ gcc -std=c11 -Wall -Wextra -c mul.c -o mul.o
$ gcc app.o -o app_missing
/usr/bin/ld: app.o: in function `main':
app.c:(.text+0x13): undefined reference to `add'
/usr/bin/ld: app.c:(.text+0x25): undefined reference to `mul'
collect2: error: ld returned 1 exit status
```

报错里有两条金贵的线索:**`app.o: in function 'main'`** 告诉你是哪个 `.o`、哪个函数里引用了这个没着落的符号;**`app.c:(.text+0x13)`** 告诉你引用点在 `app.c` 的 `main` 里大约什么偏移处。顺藤摸瓜——`app.c` 声明了 `add`/`mul` 却没提供它们的来源(`add.o`/`mul.o` 或库),链接器自然找不到。补上它们就通了:

```text
$ gcc app.o add.o mul.o -o app_ok && ./app_ok
sum=5 prod=6
```

clang 复核(报错措辞略有不同,但同一种病同一种治):

```text
$ clang -std=c11 -Wall -Wextra -c app.c -o app_clang.o
$ clang app_clang.o -o app_missing_clang
/usr/bin/ld: app_clang.o: in function `main':
app.c:(.text+0x1a): undefined reference to `add'
/usr/bin/ld: app.c:(.text+0x2c): undefined reference to `mul'
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```

诊断 `undefined reference` 的肌肉记忆就是这两步:先读报错里「在哪个 `.o` 的哪个函数里引用的」,再问自己「这个符号的定义在哪个 `.o` 或库里,我有没有把它交给链接器」。绝大多数情况是漏链了 `.o`、漏链了库、或者库的顺序排错了——下面这一条。

## 静态库的顺序陷阱:为什么 `-lfoo -lbar` 换个顺序就炸

把几个 `.o` 用 `ar rcs` 打包成静态库 `libxxx.a`,链接时用 `-lxxx -L<dir>` 取用,这套阶段 0 第 6 章讲过。这里要钉死的是**静态库之间、库与对象之间的命令行顺序**——它能让同样的代码一个顺序能跑、换个顺序就 `undefined reference`,而报错信息和上一节「漏链 `.o`」**一模一样**,极易误判成代码写错了。根源是链接器扫命令行的方式:**从左到右,只单趟,扫过的不回头**。

造两个有依赖关系的库来复现——上层 `foo_compute` 调用底层 `bar_value`,分别打成 `libfoo.a` 和 `libbar.a`:

```c
/* bar.c —— 底层:bar_value() */
int bar_value(void) {
    return 7;
}
```

```c
/* foo.c —— 上层:foo_compute() 依赖 bar_value() */
int bar_value(void);

int foo_compute(void) {
    return bar_value() * 3 + 1;
}
```

```c
/* driver.c —— 调用 foo_compute() */
#include <stdio.h>

int foo_compute(void);

int main(void) {
    printf("foo_compute() = %d\n", foo_compute());
    return 0;
}
```

打包好两个库,看顺序怎么决定生死:

```text
$ gcc -std=c11 -Wall -Wextra -c foo.c -o foo.o
$ gcc -std=c11 -Wall -Wextra -c bar.c -o bar.o
$ ar rcs libfoo.a foo.o
$ ar rcs libbar.a bar.o

$ gcc -std=c11 -Wall -Wextra driver.c -L. -lfoo -lbar -o driver_ok && ./driver_ok
foo_compute() = 22

$ gcc -std=c11 -Wall -Wextra driver.c -L. -lbar -lfoo -o driver_bad
/usr/bin/ld: ./libfoo.a(foo.o): in function `foo_compute':
foo.c:(.text+0x5): undefined reference to `bar_value'
collect2: error: ld returned 1 exit status
```

同样是 `driver.c` + `-lfoo -lbar`,顺序反了就炸。原理是链接器**从左到右单趟扫描**,处理到一个静态库时,它只问一句:「我现在手头累积的、还没着落的符号里,有没有正好是这库里能提供的?有,就抽出对应成员来满足;没有,就当这库暂时没用、一个成员都不抽,继续往下扫」。

正确顺序里,先扫 `driver.c`(转成 `.o`)→ 累积缺 `foo_compute`;扫到 `libfoo.a` → 它能提供 `foo_compute`,抽出 `foo.o`,但 `foo.o` 自己又登记了「缺 `bar_value`」;扫到 `libbar.a` → 它能提供 `bar_value`,抽出 `bar.o`,符号全对上。错误顺序里,先扫 `libbar.a` → 这时手头一个未解析符号都没有(驱动文件还没扫到),链接器认为「这库暂时没用」,一个成员都不抽;等扫到 `libfoo.a` 抽出 `foo.o`、登记了缺 `bar_value`,可 `libbar.a` **已经扫过去了、不会回头**——于是 `undefined reference to 'bar_value'`。注意报错指向 `foo.o:foo_compute`,不是 `bar_value` 本身没定义——`bar_value` 明明在 `libbar.a` 里,只是被单趟扫描错过了。

把这两条肌肉记忆焊死。其一,**对象(`.o`/`.c`)在命令行里要放在库的前面**,因为对象的符号需求会被累积、等后面的库来满足;库放最前,那时还没人需要、全被跳过。其二,**库之间也要按依赖排**:A 库依赖 B 库的符号,就 `-lA -lB`(被依赖的放右边)。万一库之间相互依赖、怎么排都漏(循环依赖),`-Wl,--start-group ... -Wl,--end-group` 让链接器在组内反复扫几趟,代价是链接变慢:

```text
$ gcc -std=c11 -Wall driver.c -L. -Wl,--start-group -lbar -lfoo -Wl,--end-group -o driver_group && ./driver_group
foo_compute() = 22
```

顺序反成 `-lbar -lfoo`,用 group 包起来也救回来了。但 group 是兜底不是正解——能排出线性顺序就别用,因为它逼链接器多趟扫,大工程链接时间明显变长。

最后再强调一遍那个最容易误判的点:静态库顺序陷阱报的 `undefined reference`,和「漏链了 `.o`/漏链了库」的报错**字面一模一样**。碰到 `undefined reference`,第一反应别去改源码,先回去**盯着链接命令行看库和对象的相对顺序**——这是新手和老兵的分水岭。

## 装好 .so 之后,运行期去哪找:`RPATH`/`RUNPATH`/`$ORIGIN`

静态库在**链接期**就把代码抽进可执行文件了(阶段 0 第 6 章);动态库 `.so` 不一样——链接期只在可执行文件里记一句「我要 `libxxx.so`」,真正的代码要等**程序启动时由动态链接器加载进来**(阶段 0 第 7 章那个一直留 `U` 的 `printf@GLIBC` 就是这么填上地址的)。这节要解决的问题是:**程序启动那一刻,动态链接器去文件系统的哪些地方找 `libxxx.so`?**

造一个最小动态库 + 消费者,把它装到子目录 `./libs/`,看运行期查找的行为:

```c
/* libthing.c —— 动态库实现 */
int thing_value(void) {
    return 42;
}
```

```c
/* use_thing.c —— 链接动态库 */
#include <stdio.h>

int thing_value(void);

int main(void) {
    printf("thing_value() = %d\n", thing_value());
    return 0;
}
```

```text
$ mkdir -p ./libs
$ gcc -std=c11 -Wall -Wextra -fPIC -shared -o ./libs/libthing.so libthing.c
```

编可执行文件、链接 `libthing.so`,链接期靠 `-L./libs` 找到库、链接没问题;但**故意不设任何运行期查找路径**:

```text
$ gcc -std=c11 -Wall -Wextra -L./libs use_thing.c -lthing -o use_thing_norpath
$                       ← 链接过了:-L./libs 在链接期管用

$ readelf -d use_thing_norpath | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libthing.so]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
                       ← 只有 NEEDED(声明要哪些 .so),没有 RPATH/RUNPATH

$ ./use_thing_norpath
./use_thing_norpath: error while loading shared libraries: libthing.so: cannot open shared object file: No such file or directory
```

链接期和运行期是两套查找逻辑——`-L./libs` 只在链接期生效;程序启动时动态链接器去哪找 `.so`,由另一套规则管(系统目录 `/lib`、`/usr/lib`、`/usr/local/lib` + 环境变量 `LD_LIBRARY_PATH` + 可执行文件自己带的 `RPATH`/`RUNPATH`)。这台机器上 `libthing.so` 谁的系统目录里都没有,可执行又没带 `RPATH`,于是 `cannot open shared object file`、退出码 127。临时救场靠 `LD_LIBRARY_PATH`:

```text
$ LD_LIBRARY_PATH=./libs ./use_thing_norpath
thing_value() = 42
```

但 `LD_LIBRARY_PATH` 是环境变量、要用户每次设,根本不该当正式手段。正解是**在链接期把运行期查找路径烤进可执行文件**,用 `-Wl,-rpath,<路径>`(`-Wl,` 是把后面的选项透传给链接器 `ld`,逗号分隔):

```text
$ gcc -std=c11 -Wall -Wextra -L./libs -Wl,-rpath,\$ORIGIN/libs use_thing.c -lthing -o use_thing_rpath
$ readelf -d use_thing_rpath | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libthing.so]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN/libs]

$ ./use_thing_rpath
thing_value() = 42
```

这里我得当场说实话——一开始我写成 `-Wl,-rpath,$ORIGIN`(不带 `/libs`),`readelf -d` 显示 `Library runpath: [$ORIGIN]`、字面值没错,可运行照样 `cannot open shared object file`。原因很简单:**`$ORIGIN` 在动态链接器那里展开成「可执行文件自己所在目录」**,也就是 `.`;可我的 `libthing.so` 装在 `./libs/` 里、不在 `.` 里——`$ORIGIN` 指向的目录里压根没有这个 `.so`。写成 `$ORIGIN/libs` 才对上。这条坑特别阴:报错和「完全没设 RPATH」一模一样,你会以为是 `$ORIGIN` 没生效,其实是路径拼错了。

`$ORIGIN` 这个写法是动态链接器认识的特殊字面量(不是 shell 变量、不会被 shell 展开——所以我命令行里给 `$ORIGIN` 加反斜杠转义、防 zsh 把它当环境变量展开),它展开成可执行文件**自己的绝对路径所在目录**。好处是可移植:把整个目录(可执行 + `libs/libthing.so`)原封不动搬到别的机器、别的路径,只要相对结构不变,`$ORIGIN/libs` 永远指对地方——比写死绝对路径 `/tmp/cj/p4ch6/libs` 强得多,后者迁了机器或挪了目录就失效。

再看 `readelf -d` 输出里那个 `(RUNPATH)`——它是 ELF 动态段的 `DT_RUNPATH` 条目(标签号 `0x1d`)。还有个**更老的、语义略不同的**条目叫 `DT_RPATH`(标签号 `0xf`、显示成 `(RPATH)`),现代 gcc/ld 默认产 `RUNPATH`。它俩的关键差别:`DT_RPATH` 的查找优先级**高于** `LD_LIBRARY_PATH`(设了 RPATH 就忽略环境变量),而 `DT_RUNPATH` **只影响这个可执行文件自己直接依赖的 `.so`**,不传染给那些 `.so` 再依赖的间接 `.so`——间接依赖的查找仍会查 `LD_LIBRARY_PATH`。要强制产老式 `DT_RPATH`,加 `-Wl,--disable-new-dtags`:

```text
$ gcc -std=c11 -Wall -Wextra -L./libs -Wl,-rpath,\$ORIGIN/libs -Wl,--disable-new-dtags use_thing.c -lthing -o use_thing_rpath_old
$ readelf -d use_thing_rpath_old | grep -Ei 'rpath|runpath'
 0x000000000000000f (RPATH)              Library rpath: [$ORIGIN/libs]
```

日常新工程用默认的 `RUNPATH` 就好;碰到老二进制或对间接依赖查找有要求,才知道 `--disable-new-dtags` 这条退路。一句话收口这节:**装好 `.so` 不等于运行期能找到它**,要么烤进系统目录(要 root、污染全局),要么用 `-Wl,-rpath` 烤进可执行文件、并优先用 `$ORIGIN` 这种相对写法保证可移植。

## `-fvisibility=hidden`:让库只导出该导出的符号

一个库默认会把所有全局函数、全局变量都暴露成**导出符号**(动态符号表里的 `T`,别人能 `dlsym` 到、能链接)。工程一大这很危险——内部辅助函数、第三方静态依赖的符号全跟着导出,既膨胀 `.so`、又容易跟别的库撞符号(两个库都导出了同名的内部 `helper`,加载到同一进程里就打架)。`-fvisibility=hidden` 这个编译开关把默认可见性改成「隐藏」,只导出**明确标记 `default` 可见性的**那些符号。

```c
/* vislib_hidden.c —— 默认隐藏,只导出标记 default 的 */
#define EXPORT __attribute__((visibility("default")))

EXPORT int api_public(void) {
    return 11;
}

static int helper_internal(void) {
    return 22;
}

int api_public_two(void) { /* 没标 EXPORT → 隐藏 */
    return helper_internal() + 1;
}
```

对比默认编译和加 `-fvisibility=hidden` 后,`nm -D`(看动态符号表,即导出给外部的那批)的差别:

```text
$ gcc -std=c11 -Wall -Wextra -fPIC -shared -o libvis_default.so vislib_default.c
$ nm -D libvis_default.so | grep ' T ' | sort
00000000000010e9 T api_public
00000000000010ff T api_public_two
                       ← 默认:两个全局函数全导出

$ gcc -std=c11 -Wall -Wextra -fvisibility=hidden -fPIC -shared -o libvis_hidden.so vislib_hidden.c
$ nm -D libvis_hidden.so | grep ' T ' | sort
00000000000010e9 T api_public
                       ← 只剩标了 EXPORT 的 api_public
```

加了 `-fvisibility=hidden`,`api_public_two` 从动态符号表里消失了——它还在 `.so` 里(代码没删),只是变成了**内部符号**,外部链接器、`dlsym` 都看不见。用普通 `nm`(不带 `-D`,看全表)能看到它降级成了小写 `t`(local、不导出):

```text
$ nm libvis_hidden.so | grep -E 'api_public|helper'
00000000000010e9 T api_public
00000000000010f4 t helper_internal
00000000000010ff t api_public_two
```

clang 复核同效果:

```text
$ clang -std=c11 -Wall -Wextra -fvisibility=hidden -fPIC -shared -o libvis_clang.so vislib_hidden.c
$ nm -D libvis_clang.so | grep ' T '
00000000000010f0 T api_public
```

工程实践是**整个库默认 `-fvisibility=hidden`**(在 CMake 里设 `CMAKE_C_VISIBILITY_PRESET hidden`,gcc 命令行加 `-fvisibility=hidden`),只在公开 API 的头文件里用宏 `EXPORT`(`__attribute__((visibility("default")))`)标记要导出的那一小批——内部的辅助函数、静态链接进来的第三方代码,统统隐藏掉。这既是收缩 ABI 表面(只暴露该暴露的、改内部实现不破坏 ABI),也是防符号冲突的现实手段。

## 全链路活教材:install/export/find_package

前面四节是手敲 `gcc`/`ar`/`readelf` 的底层现实;真实工程里这套几乎全交给 CMake 管。这一节拿本仓库 `examples/stage4-cmake-lib` 那个**专门为讲 install/export 而造**的最小库工程,把「装库→消费者 `find_package` 找到它→链上跑通」端到端真跑一遍。它的 `CMakeLists.txt` 同一份源 `src/mathlib.c` 编出**静态库 `mathlib`(.a)和动态库 `mathlib_shared`(.so)两个 target**,带 `install(TARGETS ... EXPORT MathlibTargets)` 把它们连同头文件、生成的 `MathlibTargets.cmake`/`MathlibConfig.cmake`/`MathlibConfigVersion.cmake` 一起装走——这正是消费者 `find_package(Mathlib)` 要的全部家当。

第一步,配置 + 构建 + install 到一个**前缀**(用 `DESTDIR` 模拟系统安装、不污染真 `/usr/local`):

```text
$ cd /tmp/cj/p4ch6
$ cmake -S /home/charliechen/C-Journey/examples/stage4-cmake-lib -B mathlib_build -G Ninja
$ cmake --build mathlib_build
[1/5] Building C object CMakeFiles/mathlib_shared.dir/src/mathlib.c.o
[2/5] Building C object CMakeFiles/mathlib.dir/src/mathlib.c.o
[3/5] Linking C shared library libmathlib.so.1
[4/5] Creating library symlink libmathlib.so
[5/5] Linking C static library libmathlib.a

$ DESTDIR=/tmp/cj/p4ch6/mathlib_prefix cmake --install mathlib_build
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/libmathlib.a
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/libmathlib.so.1
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/libmathlib.so
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/include/mathlib.h
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibTargets.cmake
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibTargets-noconfig.cmake
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibConfig.cmake
-- Installing: /tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibConfigVersion.cmake
```

`install` 装出来的目录树一眼看明白:`lib/` 里是 `.a`+`.so`(含带 ABI 版本的 `libmathlib.so.1` 和指向它的 `libmathlib.so` 软链)、`include/` 里是公开头 `mathlib.h`、`lib/cmake/Mathlib/` 里是消费端 `find_package` 要的四个文件——**`MathlibConfig.cmake` 是入口**(它 `include` 了 `MathlibTargets.cmake`)、**`MathlibTargets.cmake` 声明 IMPORTED target**(把 `mathlib`/`mathlib_shared` 注册成「导入的、带好 include 路径和库位置的目标」)、**`MathlibConfigVersion.cmake` 让 `find_package(Mathlib 1.0)` 能查版本兼容性**。

那个 `BUILD_INTERFACE`/`INSTALL_INTERFACE` 生成器表达式(第 5 章讲过)此刻就在发挥作用:库的 `target_include_directories` 写成 `$<BUILD_INTERFACE:源目录/include>`(构建时指源码树)+ `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>`(install 后指 `include/`),所以导出的 IMPORTED target 自动带上 install 前缀下的 `include/`,消费者不用手写 `-I`。

第二步,写消费者工程,`find_package(Mathlib)` 找到刚装的库、链上跑通:

```cmake
# consumer/CMakeLists.txt
cmake_minimum_required(VERSION 3.23)
project(consumer LANGUAGES C)

# 指向刚 install 出的 MathlibConfig.cmake 所在目录
set(Mathlib_DIR "/tmp/cj/p4ch6/mathlib_prefix/usr/local/lib/cmake/Mathlib"
    CACHE PATH "Path to MathlibConfig.cmake")
find_package(Mathlib 1.0 REQUIRED)

add_executable(consumer main.c)
# 导出的 IMPORTED target 叫 mathlib_shared,带好 include 路径和 .so 位置
target_link_libraries(consumer PRIVATE mathlib_shared)
```

```c
/* consumer/main.c —— 用 find_package 拿到的 Mathlib 跑 */
#include <stdio.h>
#include <mathlib.h>

int main(void) {
    printf("ml_add(2,3)=%d  ml_mul(2,3)=%d\n", ml_add(2, 3), ml_mul(2, 3));
    return 0;
}
```

```text
$ cmake -S consumer -B consumer/build -G Ninja
$ cmake --build consumer/build
[1/2] Building C object CMakeFiles/consumer.dir/main.c.o
[2/2] Linking C executable consumer

$ ./consumer/build/consumer
ml_add(2,3)=5  ml_mul(2,3)=6
```

跑通了。可这里有个**当场说实话**必须交代——我一开始预期「装好的库没自带 RPATH、消费者不埋 RPATH 就会运行期找不到 `libmathlib.so`」,结果**直接跑成功了、没报错**。回去 `readelf -d` 一看可执行文件,真相浮出来了:CMake 默认在**构建期**就给可执行文件埋了一条 `RUNPATH`,指向它链接的那个 IMPORTED `.so` 的绝对位置:

```text
$ readelf -d consumer/build/consumer | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libmathlib.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [/tmp/cj/p4ch6/mathlib_prefix/usr/local/lib]
```

那条 `Library runpath: [/tmp/cj/p4ch6/mathlib_prefix/usr/local/lib]` 就是 CMake 替我埋的——它叫 **build-tree RPATH**(构建期 RPATH),是 CMake 的默认行为:链接到任何 `.so`(IMPORTED 的也算),就把那个 `.so` 的所在目录烤进可执行文件的 `DT_RUNPATH`,保证在构建树上直接跑就能找到依赖、不用手设 `LD_LIBRARY_PATH`。这条机制省心,但要心里有数:它烤的是**绝对路径**(这里指向 `/tmp/cj/p4ch6/mathlib_prefix/...`),把可执行文件原样拷到别的机器、别的目录,这条绝对 RUNPATH 就失效了。要做成可分发的可执行文件,正规做法是给消费者 target 设 `INSTALL_RPATH`(用相对的 `$ORIGIN` 写法,像上一节那样)再 `install` 它;构建期这条 RUNPATH 只是为了本地开发顺手能跑。把这两件事分清:CMake 的 build-tree RPATH 是开发期便利、绝对路径;`$ORIGIN` 那种相对 RPATH 才是分发期的可移植写法。

顺带说一句那个 `mathlib_shared` 的 `SOVERSION 1`——它是 `set_target_properties(... PROPERTIES SOVERSION 1)` 设的,效果就是 install 出来的 `.so` 带版本后缀 `libmathlib.so.1`、外加一条指向它的 `libmathlib.so` 软链(`.so.1` 是带 ABI 版本的真实文件、`.so` 是链接期要的别名)。`SOVERSION` 是给动态库标 ABI 兼容版本的:不兼容的改动要 bump 版本号,消费者靠 `libmathlib.so.1` 这个名字锁住「我这版是跟 `.so.1` 兼容的」。这套 SONAME/SOVERSION 机制是动态库版本管理的标准做法,`readelf -d` 里能看到对应的 `SONAME` 条目。

## 退一步:`find_package` 到底有几种找法

刚才消费者那行 `find_package(Mathlib 1.0 REQUIRED)` 一行就把 `MathlibConfig.cmake` 摸出来了,顺滑得像变魔术。但等到你换一个库——比如想用系统的 zlib——`find_package(ZLIB)` 又是另一套动作,因为它找的根本不是「`XXXConfig.cmake`」那种文件。`find_package` 其实有**两套找包模式**,先把这条分水岭钉死,后面看任何 `find_package` 都不会糊。

**Module 模式**找的是 CMake **自带的**、叫 `Find<PackageName>.cmake` 的脚本。这些脚本是 CMake 发行版里写好的「找包配方」,专门给一批系统常见库(zlib、curl、OpenSSL、Python、Threads……)用,搜 `CMAKE_MODULE_PATH` 环境变量和 CMake 自己安装目录下的 `Modules/`。两件事可以当场验:一是 CMake 自带了多少这种配方,二是 Module 模式找包成功后会按约定留哪几个变量。先看第一件,CMake 装好就带了一条「列出所有自带模块」的子命令:

```text
$ cmake --help-module-list | grep -E '^Find'
FindALSA
FindASPELL
FindAVIFile
FindArmadillo
FindBISON
FindBLAS
FindBZip2
FindBacktrace
FindBoost
...
```

这台机器上(CMake 4.3)`grep -E '^Find'` 出来一共 **163 条**——也就是说 CMake 开箱就认得 163 个常见库,`FindZLIB`/`FindCURL`/`FindPNG`/`FindJPEG`/`FindPython3`/`FindThreads`/`FindOpenGL` 全在里面。一个库的公开头 + 库被这 163 个之一覆盖到,你一行 `find_package(<P>)` 就拿到了,不用自己写配方。Module 模式找包成功后,这批脚本**按约定**留一组变量:`<P>_FOUND`(找到没)、`<P>_INCLUDE_DIRS`(头文件在哪)、`<P>_LIBRARIES`(库在哪),有的还会留 `<P>_VERSION_STRING`。这条「`<P>_FOUND`/`<P>_INCLUDE_DIRS`/`<P>_LIBRARIES`」三件套约定是 Module 模式的指纹——注意它给的是**裸字符串变量**,不是带名字空间的 IMPORTED target。

最干净的验证是写一个**只有 `find_package` 的极小工程**,真跑一次,把这几个变量打印出来。这台机器上 zlib 是装好的(`/usr/lib/libz.so` + `/usr/include/zlib.h`,版本 1.3.2),所以 `find_package(ZLIB)` 在 Module 模式下应该一抓一个准:

```cmake
# zprobe/CMakeLists.txt —— 只为看 find_package(ZLIB) 留了哪些变量
cmake_minimum_required(VERSION 3.15)
project(zprobe LANGUAGES C)

find_package(ZLIB)

message(STATUS "ZLIB_FOUND = ${ZLIB_FOUND}")
message(STATUS "ZLIB_INCLUDE_DIRS = ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES = ${ZLIB_LIBRARIES}")
message(STATUS "ZLIB_VERSION_STRING = ${ZLIB_VERSION_STRING}")
```

```text
$ cmake -S zprobe -B zprobe/build -G Ninja
-- The C compiler identification is GNU 16.1.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Found ZLIB: /usr/lib/libz.so (found version "1.3.2")
-- ZLIB_FOUND = TRUE
-- ZLIB_INCLUDE_DIRS = /usr/include
-- ZLIB_LIBRARIES = /usr/lib/libz.so
-- ZLIB_VERSION_STRING = 1.3.2
-- Configuring done (0.4s)
```

那行 `Found ZLIB: /usr/lib/libz.so (found version "1.3.2")` 就是 CMake 自带的 `FindZLIB.cmake` 在 Module 模式下跑出来的——它定位了头和库、读了 `zlib.h` 里的版本宏、按约定填好了 `ZLIB_FOUND/ZLIB_INCLUDE_DIRS/ZLIB_LIBRARIES/ZLIB_VERSION_STRING` 这一组变量。调用方拿这几个字符串变量,自己去 `target_include_directories(... PRIVATE ${ZLIB_INCLUDE_DIRS})` + `target_link_libraries(... PRIVATE ${ZLIB_LIBRARIES})` 链上,活就完了。这就是 Module 模式:配方 CMake 给你写好了,产物是一组裸变量。

找不到时它说什么,也得亲眼见一次——换个**这台机器上没装**的库来试,Java 的 JNI 头(`JAVA_INCLUDE_PATH`)压根不存在,正好复现 Module 模式「找不到」的真实措辞:

```cmake
# jniprobe/CMakeLists.txt —— 这台机器没装 JDK,演示 Module 模式「找不到」
cmake_minimum_required(VERSION 3.15)
project(jniprobe LANGUAGES C)

find_package(JNI)

message(STATUS "JNI_FOUND = ${JNI_FOUND}")
message(STATUS "JNI_INCLUDE_DIRS = ${JNI_INCLUDE_DIRS}")
message(STATUS "JNI_LIBRARIES = ${JNI_LIBRARIES}")
```

```text
$ cmake -S jniprobe -B jniprobe/build -G Ninja
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/sbin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Could NOT find JNI (missing: JAVA_INCLUDE_PATH JAVA_INCLUDE_PATH2 AWT JVM)
-- JNI_FOUND = FALSE
-- JNI_INCLUDE_DIRS = JAVA_INCLUDE_PATH-NOTFOUND;JAVA_INCLUDE_PATH2-NOTFOUND;JAVA_AWT_INCLUDE_PATH-NOTFOUND
-- JNI_LIBRARIES =
-- Configuring done (0.2s)
```

`Could NOT find JNI (missing: ...)` 这行就是 Module 模式找不到时 CMake 替你打的诊断——括号里 `missing:` 列出 `FindJNI.cmake` 想要、但没捞着的每一个具体变量(`JAVA_INCLUDE_PATH` 等)。注意那个 `<X>-NOTFOUND` 后缀:`find_path`/`find_library` 没命中时不会留空,而是塞一个字面量 `JAVA_INCLUDE_PATH-NOTFOUND` 进变量,调用方一眼能看出「这个变量是失败的占位符」而不是「碰巧是空字符串」。Module 模式的成败两端都按这套约定走。

这里我得当场拆一个**特别能坑人的坑**:你可能看过有教程用 `cmake -P` 直接跑一个 `.cmake` 脚本去演示 `find_package`(包括早期笔记我也这么写过),在那篇里它跑出 `Could NOT find ZLIB`。可同样这台机器、同样的 zlib 装得好好的,上面的 `project()` 配置却能找到——`cmake -P` 怎么就找不到了?真跑一遍当场打脸:

```text
$ cmake -P probe_zlib.cmake
-- probing via FindZLIB.cmake ...
-- Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
-- ZLIB_FOUND = FALSE
-- ZLIB_INCLUDE_DIRS =
-- ZLIB_LIBRARIES = ZLIB_LIBRARY-NOTFOUND
-- ZLIB_VERSION_STRING =
```

zlib 明明装着(`/usr/lib/libz.so`、`/usr/include/zlib.h` 都在),`cmake -P` 却报 `Could NOT find ZLIB`,而带 `project(... LANGUAGES C)` 的正常配置却报 `Found ZLIB`。原因在 `cmake -P` 这个**脚本模式(script mode)**:`-P` 是「跑一个 CMake 脚本、不做任何工程配置」的玩法,它**不初始化编译器/平台上下文**,而 `find_package` 底层那套 `find_library`/`find_path` 的搜索路径和 hints 里有不少是靠 `project()` 探测出来的(比如编译器目标三元组、系统默认库目录的判定),脚本模式下这些 hints 残缺,系统库就抓不到。所以要演示 `find_package` 的真实行为,**正经 `project()` 配置才是诚实的场子**;`cmake -P` 那种演示法子会给你一个虚假的「找不到」,把你和读者都带沟里去。这条坑我以前也踩过,这里写出来给自己也给后来人提个醒。

**Config 模式**走的是另一条路:它不指望 CMake 自带配方,而是去找**库自己 install 出来的** `<PackageName>Config.cmake`(或小写 `<lower>-config.cmake`)。这种配置文件**不是 CMake 自带的**,而是「懂得自我描述的库」在 `install(EXPORT ...)` 阶段一起装出来的——我们前面那套 `MathlibConfig.cmake`/`MathlibTargets.cmake` 就是 Config 模式要找的东西。Config 模式找到后,留下的是一个(或几个)**IMPORTED target**:一个跨工程边界、带好 include 路径和库位置的「导入目标」,直接塞给 `target_link_libraries` 就能用,不用手抓 `_INCLUDE_DIRS`/`_LIBRARIES` 变量。一句话收口两套模式的差别:Module 模式靠 CMake 自带的 `FindXXX.cmake` 找系统常见库、留一组裸变量;Config 模式靠库自己装的 `XXXConfig.cmake` 找任何「懂得自我描述」的库、留一个 IMPORTED target。本仓库 `Mathlib` 走的是 Config 模式,系统 zlib 走的是 Module 模式——同一个 `find_package`,底下两条路,认准它找的是 `Find` 还是 `Config` 就知道走哪条。

那为什么前面消费者写的是 `target_link_libraries(consumer PRIVATE mathlib_shared)`——一个**没带名字空间**的裸 target 名?翻一下 `examples/stage4-cmake-lib` 的 `install(EXPORT ...)` 就明白了:它**没写 `NAMESPACE`**,所以导出文件 `MathlibTargets.cmake` 里登记的 IMPORTED target 就叫原始名 `mathlib`/`mathlib_shared`,消费者拿来直接链。这套「裸名」在一个孤立 demo 里没问题,可真到了多库工程里就埋雷:你要是同时 `find_package` 了两个库、它们各自的 export 集里都造了一个叫 `common` 的 target,两个 `common` 在同一个 CMake 作用域里直接撞车、报「重定义」。这正是 `install(EXPORT ... NAMESPACE <P>::)` 存在的理由——给导出的 IMPORTED target 统一加一个「`包名::`」前缀,把命名空间隔开,消费者就只能用 `Mathlib::mathlib_shared` 这种带前缀的名字去链,想撞都撞不起来。

把它接到我们这个活教材上,把 `stage4-cmake-lib` 的 `install(EXPORT ...)` 那段改成带名字空间:

```cmake
install(EXPORT MathlibTargets
        NAMESPACE Mathlib::
        FILE MathlibTargets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mathlib)
```

重新 install 一次,扒开装出来的 `MathlibTargets.cmake` 看,差别就在 `add_library` 登记的那个 IMPORTED target 名字:

```cmake
# Create imported target Mathlib::mathlib
add_library(Mathlib::mathlib STATIC IMPORTED)
...
# Create imported target Mathlib::mathlib_shared
add_library(Mathlib::mathlib_shared SHARED IMPORTED)
```

加了 `NAMESPACE Mathlib::` 之后,原本的 `mathlib`/`mathlib_shared` 被改写成了 `Mathlib::mathlib`/`Mathlib::mathlib_shared`。消费侧那行 `target_link_libraries` 就得相应改成链带名字空间的 target:

```cmake
target_link_libraries(ns_consumer PRIVATE Mathlib::mathlib_shared)
```

真跑一遍,跑得通、输出对(`ml_add(2,3)=5  ml_mul(2,3)=6`),`readelf -d` 看 `RUNPATH` 也照常指向 `.so` 所在目录——说明 `Mathlib::mathlib_shared` 这个带前缀的 IMPORTED target 被 CMake 正确解析成了 `/tmp/.../lib/libmathlib.so.1`。这条 `包名::target` 的双冒号约定是 CMake 现代写法的硬规矩:**双冒号在 CMake 里是保留给 IMPORTED/ALIAS target 的**,普通 target 名不允许含 `::`,所以你看到 `Mathlib::mathlib_shared` 这种写法,一眼就能断定它一定来自 `find_package`(或别处的 IMPORTED),不可能是个本地手写的 `add_library` 名——这条视觉信号对读代码的人是实打实的实惠。

## 小结

链接期这一步,链接器干的是符号解析 + 重定位,我们专攻了符号解析这一面的四件事和它在 CMake 全链路里的落地。**`undefined reference` 的诊断**靠读报错里「在哪个 `.o` 的哪个函数里引用的」,绝大多数是漏链 `.o`/漏链库/库顺序错。**静态库顺序陷阱**的根源是链接器从左到右只单趟扫描、扫过的不回头,所以对象要在前、库在后、被依赖的放右边;它报的错和「漏链」字面一模一样,排查先看命令行顺序再想改代码。**装好 `.so` 不等于运行期能找得到它**,链接期 `-L` 只在链接期管用,运行期靠系统目录 + `LD_LIBRARY_PATH` + 可执行自带的 `RPATH`/`RUNPATH`;正解是 `-Wl,-rpath,$ORIGIN/...` 烤进可执行、用 `$ORIGIN` 保证可移植(`$ORIGIN` 指可执行所在目录,路径要拼对),`RUNPATH` 是现代默认、`RPATH` 要 `--disable-new-dtags`。**`-fvisibility=hidden`** 把库默认可见性改成隐藏、只导出标了 `default` 的符号,收缩 ABI 表面、防符号冲突。最后 CMake 的 install/export/find_package 把这一切自动化了——`install(TARGETS ... EXPORT ...)` 装库 + 生成 Targets/Config/ConfigVersion 文件,消费者 `find_package` 拿到带好 include 路径和库位置的 IMPORTED target 直接链;但 CMake 默认埋的 build-tree RPATH 是绝对路径、只管开发期本地跑,分发期的可移植要自己设 `INSTALL_RPATH` 配 `$ORIGIN`。

下一章我们离开链接器,转向工程化里另一条质量红线——把编译期、链接期、运行期的错误用一套统一的错误处理模型串起来,看怎么设计不透明类型 + 错误码 + 错误上下文,让一个库的失败路径既好诊断、又不泄漏内部细节。

## 参考资源

- `man ld`(`--start-group`/`--end-group`、`-rpath`、`--disable-new-dtags`、库顺序的权威说明)
- `man ld.so`(动态链接器/加载器:`RPATH`/`RUNPATH`、`$ORIGIN`、`LD_LIBRARY_PATH`、库搜索顺序——这套查找规则的标准出处)
- `man readelf`(看 ELF 动态段 `DT_RPATH`/`DT_RUNPATH`/`DT_NEEDED`/`DT_SONAME` 各条目)
- `man dlopen`(运行期加载接口;注意 `dlsym` 返回 `void*` 转函数指针靠 POSIX 保证,ISO C 不管——见阶段 0 第 7 章)
- GCC 手册:`-fvisibility`/`-fvisibility=hidden`、`-fPIC`、`-shared`、`-Wl,`
- CMake 文档:`install(TARGETS ... EXPORT ...)`、`CMakePackageConfigHelpers`(`write_basic_package_version_file`)、`find_package` 的 Config 模式、`BUILD_RPATH`/`INSTALL_RPATH`/`CMAKE_BUILD_RPATH_USE_ORIGIN`
- ISO/IEC 9899:2011 §6.9 / §6.9¶5(外部定义:每个外部链接标识符恰有一次外部定义;违反为 UB——`undefined reference`/`multiple definition` 是链接器在实现层拦下的现实)
