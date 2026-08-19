---
title: "阶段 4 课后练习参考答案（Homework）"
description: "工程化与质量门阶段课后练习的逐题详细解答：每道题给出解题思路、逐步解答（每步标注知识点链接）与真实验证输出（gcc 16.1.1 / clang 22.1.8 / WSL Arch 实跑）。含三处与教材表述不一致的真实结果如实交代：ABI 反例的 stack smashing、volatile 只保住被标变量、libc.a 起名撞系统库。"
chapter: 4
order: 1
tags:
  - host
  - engineering
  - testing
difficulty: intermediate
reading_time_minutes: 60
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 课后练习（Homework）"
related:
  - "阶段 4 各章"
---

# 阶段 4 课后练习参考答案（Homework）

> 所有命令与输出在 WSL Arch（gcc 16.1.1 / clang 22.1.8）下真实运行得到。UB 类题目的输出「只是这台机器这次的选择」，换编译器/优化级别可能不同——这正是每道题要你体会的东西。三处真实结果和「想当然」不一致的地方，正文里都当场交代了。

## 4.1-A {#hw-4-1-a}

**难度 L1** · 题面见 [homework](homework#hw-4-1-a)

**思路**：纸面判断题，全是第 1 章三条契约的「知不知道」。

1. 三行：`#ifndef MYLIB_X_H`、`#define MYLIB_X_H`、正文、`#endif`。→ 知识点：[第 1 章：头文件契约](/04-engineering/01-header-contracts)「契约一：include guard」一节
2. `#pragma once` **不在** ISO C 标准里，是编译器扩展；追求可移植用 `#ifndef/#define/#endif`。→ 知识点：同上
3. 头文件里放带初始化的变量定义，谁 `#include` 谁多一份定义，n 个翻译单元就 n 重定义，链接器报 `multiple definition`。→ 知识点：[第 1 章](/04-engineering/01-header-contracts)「契约二：ODR」一节（头文件只放声明）
4. ODR：一个非 `inline` 的全局变量或函数，整个程序里只能有**一处定义**；声明可以到处放（头文件），定义只能一份（某个 `.c`）。→ 知识点：同上
5. 填 `static`；裸 `inline` 链接期报 `undefined reference to 'xxx'`（C99 的裸 inline 不提供外部符号）。→ 知识点：[第 1 章](/04-engineering/01-header-contracts)「契约三：裸 C99 inline」一节
6. **不一样**——C++ 的 `inline` 自动提供外部定义，C99 的裸 `inline` 没有；这是 C++ 转 C 的人最容易踩的坑。→ 知识点：同上

## 4.1-B {#hw-4-1-b}

**难度 L3** · 题面见 [homework](homework#hw-4-1-b)

**思路**：`a.h` 先被包含时定义了 `COMMON_H`，`b.h` 的 `#ifndef` 判定为「已定义」、整段跳过——`struct Color` 从未被声明过，报错和「没写这个头」几乎一样。

1. `main.c` 同时包含两头的完整版本编译，`struct Color` 变成不完整类型。→ 知识点：[第 1 章](/04-engineering/01-header-contracts)「契约一」一节（guard 宏名要全工程唯一）
2. 把两个 guard 宏改成各自唯一的 `A_H`/`B_H`，重编跑出正确结果。→ 知识点：同上（撞名的修法）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. main.c -o collide
main.c: In function 'main':
main.c:8:12: error: variable 'c' has initializer but incomplete type
    8 |     struct Color c = {255, 0, 128};
      |            ^~~~~
main.c:8:18: error: storage size of 'c' isn't known
    8 |     struct Color c = {255, 0, 128};
      |                  ^
---- 修复后 ----
$ gcc -std=c11 -Wall -Wextra -I. main_fix.c -o collide_fix && ./collide_fix
v=(3,4) c=(255,0,128)
exit=0
```

关键代码（撞名版本）：

```c
/* a.h 和 b.h 的 guard 宏名都是 COMMON_H —— 撞名! */
#ifndef COMMON_H
#define COMMON_H
struct Vec2 {
    int x;
    int y;
};
#endif
```

为什么「整段被跳过」：`#include` 是纯文本插入，`a.h` 插入时 `COMMON_H` 还没定义，于是 `#define` 了它、`struct Vec2` 生效；轮到 `b.h` 插入时 `COMMON_H` 已存在，`#ifndef` 判假、整个 `b.h` 正文被预处理器删掉，`struct Color` 就从来没在这个翻译单元里出现过。诊断时记住这个特征：报错说类型「incomplete / isn't known」，而不是「redefinition」——那多半不是没包含，而是被撞名的 guard 吞了。

## 4.2-A {#hw-4-2-a}

**难度 L2** · 题面见 [homework](homework#hw-4-2-a)

**思路**：头里 `typedef struct IntStack IntStack_t;` 制造不完整类型，字段藏进 `.c`；消费者只能走 API。栈和 ringbuffer 同构：数组 + 一个游标，push/pop 都在 `sp` 上。

1. 头文件只有前向声明 + 五个原型；`.c` 里 `struct IntStack` 藏着 `data/cap/sp`，`create` 两次 malloc + 失败回滚、`destroy` 先 `free(data)` 再 `free(s)`。→ 知识点：[第 2 章：API 设计与不透明类型](/04-engineering/02-api-and-opaque-types)「不透明类型是什么」一节
2. 消费者 `s->sp` 会撞 `invalid use of incomplete typedef`；`IntStack local;` 会撞 `storage size of 'local' isn't known`——编译器不知道类型多大，既不能解引用也不能在栈上分配。→ 知识点：[第 2 章](/04-engineering/02-api-and-opaque-types)「消费者想偷摸字段」与「代价」两节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. main_ok.c intstack.c -o ok_gcc && ./ok_gcc
size = 3
pop 30
pop 20
pop 10
exit=0
$ clang -std=c11 -Wall -Wextra -I. main_ok.c intstack.c -o ok_clang && ./ok_clang
size = 3
pop 30
pop 20
pop 10
$ gcc -std=c11 -Wall -Wextra -I. -c main_illegal.c -o illegal.o
main_illegal.c:5:6: error: invalid use of incomplete typedef 'IntStack_t' {aka 'struct IntStack'}
    5 |     s->sp = 99;       /* 想偷摸字段 —— 不完整类型 */
      |      ^~
main_illegal.c:6:16: error: storage size of 'local' isn't known
    6 |     IntStack_t local; /* 想栈上分配 —— 不知道多大 */
      |                ^~~~~
```

注意栈弹出来的顺序是 `30 20 10`——LIFO，和教材 ringbuffer 的 FIFO `10 20 30` 正好相反，这就是「换方向」的题眼。

## 4.2-B {#hw-4-2-b}

**难度 L4** · 题面见 [homework](homework#hw-4-2-b)

**思路**：旧 `client.o` 里编进去的是「`cfg.port` 在偏移 0、`cfg.verbose` 在偏移 4」，新实现却按「version 在 0、port 在 4、verbose 在 8」写 12 字节。结果不是读错值，而是——**如实交代**：本机 Arch 的 gcc 默认开栈保护，新实现往旧消费者那个只有 8 字节的栈对象里写了 12 字节，直接触发 `stack smashing detected`，比「读到错值」更狠。

1. 版本 1 全量编译跑出 `port=8080 verbose=1`；把消费者单独编成 `client.o`（对着 v1 头）。→ 知识点：[第 2 章](/04-engineering/02-api-and-opaque-types)「收益二：ABI 稳定」一节（旧 `.o` 里编进的是字段偏移）
2. 头改成 v2（`port` 前插 `version`）、只重编实现、链旧 `client.o`：栈对象 8 字节、`config_init` 写 12 字节 → 栈金丝雀被踩 → `stack smashing detected`。重编消费者后一切正常。→ 知识点：同上（不透明类型改字段不破消费者，摊开的 struct 一改就炸）、[第 1 章](/04-engineering/01-header-contracts)（编译器只看单个翻译单元，链接期不拦这种错）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. naive.c client.c -o v1 && ./v1
port=8080 verbose=1
$ gcc -std=c11 -Wall -Wextra -I. -c client.c -o client.o     # 消费者:旧头
$ gcc -std=c11 -Wall -Wextra -I. -c naive2.c -o naive2.o     # 实现:v2 头
$ gcc client.o naive2.o -o stale && ./stale
*** stack smashing detected ***: terminated
Aborted
$ gcc -std=c11 -Wall -Wextra -I. naive2.c client.c -o fresh && ./fresh
port=9090 verbose=0
```

「旧的 .o 里编进去的是什么」：编译 `client.c` 时，`cfg.port` 被翻译成「栈偏移 0 读 4 字节」、`cfg.verbose` 是「偏移 4 读 4 字节」——**偏移是编译期算死的**。新实现按新布局写 `version@0 port@4 verbose@8`，其中偏移 8 的写入越过了旧对象的边界。链接器只管符号对得上（`config_init` 这个名字两边一致），**根本不知道也不管两个翻译单元对 struct 布局的理解不一致**——所以这种错链接期不拦、运行期才炸。而 opaque 那套打法里消费者 `.o` 压根没有字段偏移，第 2 章演示过加字段照样链、照样跑。

## 4.3-A {#hw-4-3-a}

**难度 L2** · 题面见 [homework](homework#hw-4-3-a)

**思路**：errno 铁律——只在「确认失败之后」查它问为什么，永远不拿它判断成功；成功调用不动 errno，残留值是常态。

1. 正确姿势：失败返回 -1、调用方先看 `rc`，失败才读 errno（EINVAL=22）。→ 知识点：[第 3 章：错误处理三件套](/04-engineering/03-error-handling)「风格②：errno 约定」一节
2. 错误姿势：成功调用后查 `errno == 0`——先清零时「碰巧成立」，但这是运气；不预先清零时，上一次失败的 EINVAL 残留着，成功调用被判成失败。→ 知识点：同上（成功调用后 errno 的值是「未指定」的）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra errno_kelvin.c -o ek_gcc && ./ek_gcc
[正确姿势] rc=-1 => 失败, errno=22 (Invalid argument)
[错误姿势] 清零后 errno==0,判定成功: k=298.15 (这次碰巧对)
[错误姿势] errno=22 (Invalid argument)——明明成功了却判定失败!
[正确姿势] rc=0 => 成功
exit=0
```

第三行就是教材「残留坑」在温度场景的复现：`to_kelvin(-300)` 失败设了 EINVAL，紧接着的成功调用没动 errno，`if (errno == 0)` 就把成功判成了失败。正确姿势永远是 `rc` 定成败、errno 只补充「为什么」。

## 4.3-B {#hw-4-3-b}

**难度 L3** · 题面见 [homework](homework#hw-4-3-b)

**思路**：context 那套（堆上 `desc` + `__func__` 记位置 + 两次 `vsnprintf` 探长再拷贝）直接从教材思路复刻；解析本身用 `strtoll` + `errno == ERANGE` + `int` 范围双保险。

1. `error_ctx` 四个函数（new/set/clear/free）和教材 `context_demo.c` 同构，`set` 里 `va_copy` 留给第二次 `vsnprintf`。→ 知识点：[第 3 章](/04-engineering/03-error-handling)「风格③：context 对象」一节
2. `parse_int` 区分两种错：非法字符（`end == s || *end != '\0'`，码 1）与越界（`errno == ERANGE || v > INT_MAX || v < INT_MIN`，码 2）；这里 `errno` 的用法符合铁律——调用前清零、只在 `strtoll` 失败后信它。→ 知识点：[第 3 章](/04-engineering/03-error-handling)（errno 铁律 + context 的组合拳）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra errctx.c parse_int.c -o pi_gcc && ./pi_gcc
[成功] '123' -> 123
[失败] code=1, 位置=parse_int, 说明=非法字符:'12a3'
[失败] code=2, 位置=parse_int, 说明=越界:'99999999999999' -> 99999999999999
exit=0
$ clang -std=c11 -Wall -Wextra errctx.c parse_int.c -o pi_clang && ./pi_clang
[成功] '123' -> 123
[失败] code=1, 位置=parse_int, 说明=非法字符:'12a3'
[失败] code=2, 位置=parse_int, 说明=越界:'99999999999999' -> 99999999999999
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined errctx.c parse_int.c -o pi_asan && ./pi_asan
[成功] '123' -> 123
[失败] code=1, 位置=parse_int, 说明=非法字符:'12a3'
[失败] code=2, 位置=parse_int, 说明=越界:'99999999999999' -> 99999999999999
exit=0
```

核心解析函数：

```c
static int parse_int(const char* s, int* out, error_ctx* err) {
    char* end = NULL;
    errno = 0; /* 铁律:调用前清零,只信「失败之后」的 errno */
    long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0') {
        error_ctx_set(err, 1, __func__, "非法字符:'%s'", s);
        return -1;
    }
    if (errno == ERANGE || v > INT_MAX || v < INT_MIN) {
        error_ctx_set(err, 2, __func__, "越界:'%s' -> %lld", s, v);
        return -1;
    }
    *out = (int) v;
    return 0;
}
```

`99999999999999` 没让 `strtoll` 报 ERANGE（它在 `long long` 范围内），是 `v > INT_MAX` 那道防线抓的——所以「双保险」不是重复代码，是两层不同的判断。ASan/UBSan 全程干净，说明反复 `malloc`/`free` 的 `desc` 堆管理没漏没越界。

## 4.4-A {#hw-4-4-a}

**难度 L1** · 题面见 [homework](homework#hw-4-4-a)

**思路**：纸面判断题，第 4 章的「知不知道」。

1. **修改时间**（mtime），不是内容。→ 知识点：[第 4 章：make 深处](/04-engineering/04-make-deep)「第一道坑」一节
2. 三个词：`'xxx' is up to date.`（make 觉得啥都不用干，程序却还是旧行为）。→ 知识点：同上
3. `main.o: main.c mod.h`（目标 `.o` 依赖它自己的 `.c` 加它 `#include` 的所有项目头）。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-MMD」一节
4. 头文件被删/改名时，make 不会因为「找不到这个 prerequisite」罢工——`-MP` 给每个头补一条空 phantom 规则。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-MP 的 phantom 规则」一节
5. 对，TAB 是语法；空格会报 `missing separator.  Stop.`。→ 知识点：[阶段 0 第 11 章](/00-dev-environment/12-make-basics)
6. `fatal error: genhdr.h: No such file or directory`（编译那条比生成器先跑）。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-jN：并行构建与那道阴险的竞态」一节

## 4.4-B {#hw-4-4-b}

**难度 L3** · 题面见 [homework](homework#hw-4-4-b)

**思路**：多目录的关键全在教材那两句话——「`.d` 跟着 `.o` 走」「`.d` 里的路径是编译命令里 `.o` 的路径」。`build/%.o: src/%.c` 配上 `| build`（order-only，教材提过竖线语法）自动建目录，`$(wildcard build/*.d)` 收集依赖。

1. Makefile 全文（见下）：`-Iinclude` 放进 `CFLAGS`，`build/%.o` 模式规则带 `-MMD -MP`，`-include build/*.d`。→ 知识点：[第 4 章](/04-engineering/04-make-deep)（`-MMD -MP` 一对写死；`| build` 是教材提过的 order-only prerequisite）
2. `touch util.h` 触发两个 `.o` 全重编——因为 `build/main.d` 里写着 `build/main.o: src/main.c include/util.h`。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-MMD」一节
3. 删掉 `util.h` 后报的是**编译器的** `fatal error: util.h: No such file or directory`，不是 make 的 `No rule to make target`——`-MP` 的空规则让 make 不因「prerequisite 消失」罢工，把战场交给了真正的消费者（编译器）。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-MP」一节

**验证输出**：

```text
$ make clean && make && ./build/app
mkdir -p build
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -c src/main.c -o build/main.o
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -c src/util.c -o build/util.o
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -o build/app build/main.o build/util.o
util says: v1
$ cat build/main.d
build/main.o: src/main.c include/util.h
include/util.h:
$ touch include/util.h && make
gcc ... -c src/main.c -o build/main.o
gcc ... -c src/util.c -o build/util.o
gcc ... -o build/app build/main.o build/util.o
$ # 改 GREETING v2 再 make → 同样重编 → ./build/app 输出 v2
$ rm include/util.h && make
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -c src/main.c -o build/main.o
src/main.c:1:10: fatal error: util.h: No such file or directory
    1 | #include "util.h"
      |          ^~~~~~~~~~~~
compilation terminated.
make: *** [Makefile:11: build/main.o] Error 1
```

Makefile 全文：

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP -Iinclude

BIN = build/app
OBJS = build/main.o build/util.o

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

DEPS := $(wildcard build/*.d)
-include $(DEPS)

.PHONY: clean
clean:
	rm -rf build
```

## 4.5-A {#hw-4-5-a}

**难度 L2** · 题面见 [homework](homework#hw-4-5-a)

**思路**：判断标准就一条（教材原话）——**消费者的编译/链接会不会因为缺了这个依赖而失败**。会，就 PUBLIC；不会，就 PRIVATE；只给消费者、自己不用，INTERFACE。

1. ① 公开头目录 → **PUBLIC**（消费者要 `#include` 得到）；② 私有头目录 → **PRIVATE**（只有自己 `.c` 用）；③ 警告旗标 → **PRIVATE**（警告是开发期自己的事，传染给消费者会让别人一链接你就被你的 warning 烦死）；④ 公开头露出 `pthread_mutex_t` → 依赖挂 **PUBLIC**（消费者 `#include` 你的头就必须能链 pthread，否则链接失败——对照第 2 章 `CCMutex.h` 用 `void*` 把类型藏住，正是因为藏住了才不用传染）；⑤ 库自己不用、只给消费者的宏 → **INTERFACE**。→ 知识点：[第 5 章：CMake 工程化](/04-engineering/05-cmake-engineering)「传播三态」一节
2. 消费者侧 `target_link_libraries(demo PRIVATE greeter)` 里的 PRIVATE 是「demo 把 greeter 当内部实现细节、没有第三方再链接 demo」；库侧的 PRIVATE 是「这个属性只给我自己编用」。同一个词、两处位置、管的是两个不同 target 的传播方向。→ 知识点：同上

## 4.5-B {#hw-4-5-b}

**难度 L3** · 题面见 [homework](homework#hw-4-5-b)

**思路**：① GLOB 只在 configure 那一次扫目录，加文件不 reconfigure 就不会进构建系统；② `CMAKE_BUILD_TYPE` 替单配置生成器选旗标，`NDEBUG` 把 `assert` 关成空操作。

1. `beta.c` 躺在磁盘上、`main.c` 也调了它，但生成的 Makefile 里根本没有 `beta.c.o` → `undefined reference to 'beta'`；重新 configure 后新文件进源表、构建通过。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)「GLOB_RECURSE 的坑」一节
2. Debug 的 `C_FLAGS = -g`、Release 的 `C_FLAGS = -O3 -DNDEBUG`；Debug 版 `assert` 触发退出 134（128+SIGABRT），Release 版静默通过退出 0。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)「多配置」一节、[阶段 0 第 9 章](/00-dev-environment/10-standards-and-optimization)
3. 多配置生成器（VS/Xcode/Ninja Multi-Config）不认 `CMAKE_BUILD_TYPE`，要用 `cmake --build build --config Release` 在构建期切。→ 知识点：同上（单配置 vs 多配置生成器）

**验证输出**：

```text
$ cmake --build build            # 只 build,没 reconfigure
[ 33%] Building C object CMakeFiles/demo.dir/main.c.o
[ 66%] Linking C executable demo
/usr/bin/ld: CMakeFiles/demo.dir/main.c.o: in function `main':
main.c:(.text+0xa): undefined reference to `beta'
collect2: error: ld returned 1 exit status
$ cmake -B build && cmake --build build && ./build/demo
alpha=1 beta=99
$ grep '^C_FLAGS' build-dbg/CMakeFiles/demo.dir/flags.make
C_FLAGS = -g
$ grep '^C_FLAGS' build-rel/CMakeFiles/demo.dir/flags.make
C_FLAGS = -O3 -DNDEBUG
$ ./build-dbg/demo 7; echo "exit=$?"
demo: .../demo.c:10: main: Assertion `x % 2 == 0 && "奇数不该进来"' failed.
exit=134
$ ./build-rel/demo 7; echo "exit=$?"
x=7 ran to completion
exit=0
```

## 4.6-A {#hw-4-6-a}

**难度 L2** · 题面见 [homework](homework#hw-4-6-a)

**思路**：链接器从左到右**单趟**扫描，扫过的不回头；对象在前、库在后、被依赖的靠右。三库链 `octa→quad→twice` 的正确顺序是 `-locta -lquad -ltwice`。

1. 正确顺序：driver.o 累积缺 `octa` → `libocta.a` 供上并登记缺 `quad` → `libquad.a` 供上并登记缺 `twice` → `libtwice.a` 收尾。→ 知识点：[第 6 章：静态库、动态库与链接顺序](/04-engineering/06-libs-and-linking)「静态库的顺序陷阱」一节
2. 错误顺序 `-ltwice -lquad -locta`：扫 `libtwice.a` 时还没人缺 `twice`，一个成员都不抽；扫到 `libocta.a` 抽出 `octa.o` 时发现缺 `quad`，可 `libquad.a` 已经扫过了——报 `undefined reference to 'quad'`，注意它点名的是 `libocta.a(octa.o)`，不是「quad 没定义」。→ 知识点：同上（报错字面和「漏链」一模一样，先看命令行顺序）
3. `--start-group` 让组内反复扫，救回来但链接变慢——兜底不是正解。→ 知识点：同上

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra driver.c -L. -locta -lquad -ltwice -o ok && ./ok
octa(5) = 40
exit=0
$ gcc -std=c11 -Wall -Wextra driver.c -L. -ltwice -lquad -locta -o bad
/usr/bin/ld: ./libocta.a(octa.o): in function `octa':
octa.c:(.text+0x11): undefined reference to `quad'
collect2: error: ld returned 1 exit status
$ gcc -std=c11 -Wall -Wextra driver.c -L. -Wl,--start-group -ltwice -lquad -locta -Wl,--end-group -o grouped && ./grouped
octa(5) = 40
```

顺带交代一个本人在出题时亲手踩的真坑：第一版我把底层库起名叫 `libc.a`，结果 `-lc` 先匹配到自己的库、系统 libc 被顶掉，`printf`/`__libc_start_main` 全部 `undefined reference`——**别把库起成 `libc.a`**，这是「起名撞系统库」的活案例。

## 4.6-B {#hw-4-6-b}

**难度 L3** · 题面见 [homework](homework#hw-4-6-b)

**思路**：① `$ORIGIN` 展开成「可执行文件自己所在目录」，`.so` 装在 `./libs/` 就必须写 `$ORIGIN/libs`，写 `$ORIGIN` 等于没设；② `-fvisibility=hidden` 把默认可见性改成隐藏，只有标了 `default` 的导出。

1. 漏 `/libs` 的版本：`readelf -d` 里 RUNPATH 是 `[$ORIGIN]`、字面值看起来没毛病，运行照样 `cannot open shared object file`（退出 127）——报错和「完全没设 RPATH」一模一样；改成 `\$ORIGIN/libs` 后正常跑出 42。→ 知识点：[第 6 章](/04-engineering/06-libs-and-linking)「RPATH/RUNPATH/`$ORIGIN`」一节（`$ORIGIN` 指可执行所在目录、路径要拼对）
2. 默认编译 `nm -D` 导出 `core_pub` + `core_secret` 两个 `T`；加 `-fvisibility=hidden` 后只剩 `core_pub`，普通 `nm` 显示 `core_secret` 降级成小写 `t`（local，不导出），`static` 的 helper 本来就是 `t`。→ 知识点：[第 6 章](/04-engineering/06-libs-and-linking)「-fvisibility=hidden」一节

**验证输出**：

```text
$ readelf -d use_bad | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libcore.so]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN]
$ ./use_bad; echo "exit=$?"
./use_bad: error while loading shared libraries: libcore.so: cannot open shared object file: No such file or directory
exit=127
$ gcc ... -L./libs -Wl,-rpath,\$ORIGIN/libs use_core.c -lcore -o use_ok && ./use_ok
core_value() = 42
$ nm -D libvis_default.so | grep ' T ' | sort
00000000000010e9 T core_pub
00000000000010ff T core_secret
$ nm -D libvis_hidden.so | grep ' T ' | sort
00000000000010e9 T core_pub
$ nm libvis_hidden.so | grep -E 'core_pub|core_helper|core_secret'
00000000000010f4 t core_helper
00000000000010e9 T core_pub
00000000000010ff t core_secret
```

## 4.7-A {#hw-4-7-a}

**难度 L2** · 题面见 [homework](homework#hw-4-7-a)

**思路**：重定向后 stdout 变全缓冲，`abort()` 一棒子把没刷掉的缓冲全陪葬；`setvbuf` 强制无缓冲后输出顺序才等于执行顺序。退出码 134 = 128 + SIGABRT(6)。

1. 不设缓冲 + 重定向：`out.log` 是**空的**（连 `=== assert 测试 ===` 都丢了），只有 stderr 上那条 `Assertion ... failed` 活着。→ 知识点：[第 7 章：测试不再是 printf](/04-engineering/07-testing-with-unity)「第一级：裸 assert」一节（stdout 全缓冲遇 abort 丢日志的坑）
2. `setvbuf(stdout, NULL, _IONBF, 0)` 后直接跑：前两条 `[OK]` 全打出来，第三条断言失败 `abort`，第四条和「全部通过」永远到不了，退出 134。→ 知识点：同上（一条失败拖死全家、没夹具——两个死穴各对应哪一行）

**验证输出**：

```text
$ ./t1_gcc > out.log 2> err.log; echo "exit=$?"
exit=134
$ cat out.log            ← 空的!缓冲全丢
$ cat err.log
t1_gcc: test_assert.c:23: test_sum_wrong: Assertion `got == 99' failed.
$ ./t2_gcc; echo "exit=$?"
=== assert 测试 ===
[OK] sum(10,20,30) = 60
[OK] sum(empty) = -1
t2_gcc: test_assert2.c:23: test_sum_wrong: Assertion `got == 99' failed.
exit=134
```

## 4.7-B {#hw-4-7-b}

**难度 L3** · 题面见 [homework](homework#hw-4-7-b)

**思路**：`add_test` 写在 `enable_testing()` 之前会被**静默忽略**——不报错、不生效，ctest 只给你一句「No tests were found」。CTest 判 pass/fail 的唯一依据是退出码：0 过、非 0 挂。

1. 先不写 `enable_testing()`：ctest 报 `No tests were found!!!`（注意它**不报错**，退出码还是 0——这就是「静默坑」）。→ 知识点：[第 7 章](/04-engineering/07-testing-with-unity)「第三级：CTest」一节（`enable_testing()` 必须写在所有 `add_test` 之前）
2. 补上 `enable_testing()` 并**重新 configure**（改了 CMakeLists 必须 reconfigure）：`good` 绿、`bad` 红（`t_fail` 返回 1），ctest 退出码 8；`ctest -R good` 只跑绿的那条；修好 `t_fail` 后 2/2 全绿退出 0。→ 知识点：同上（退出码契约 + `-R` 过滤）

**验证输出**：

```text
$ ctest --test-dir build --output-on-failure
Test project /tmp/cj-ex4-hw4/47b/build
No tests were found!!!
ctest exit=0
$ # 补上 enable_testing() 再 configure + build
$ ctest --test-dir build --output-on-failure
    Start 1: good
1/2 Test #1: good .............................   Passed    0.00 sec
    Start 2: bad
2/2 Test #2: bad ..............................***Failed    0.00 sec
50% tests passed, 1 tests failed out of 2
The following tests FAILED:
	  2 - bad (Failed)
ctest exit=8
$ ctest --test-dir build -R good
1/1 Test #1: good .............................   Passed    0.00 sec
100% tests passed out of 1
$ # 修好 t_fail 再 build
$ ctest --test-dir build --output-on-failure
100% tests passed out of 2
ctest exit=0
```

## 4.8-A {#hw-4-8-a}

**难度 L2** · 题面见 [homework](homework#hw-4-8-a)

**思路**：依赖抽成函数指针 `g_sensor_fn`（默认指真实实现），测试时换成带脚本的 mock——记调用次数、按序返回 20/30/40。产品代码为了可测性让了一步：多了全局指针和 setter。

1. `avg_temp(3)` 三次采样拿到 20/30/40，平均 30；`call_count == 3`；再调一次 `avg_temp(1)` 计数涨到 4（脚本取模循环）。→ 知识点：[第 8 章：Mock 与隔离](/04-engineering/08-mock-and-isolation)「第一招：函数指针表 mock」一节
2. `sensor_set_fn(NULL)` 恢复真实实现，读数回到 25——测试卫生，别让 mock 残留污染下一个测试。→ 知识点：同上

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. sensor.c test_sensor.c -o ts_gcc && ./ts_gcc
avg = 30, rc = 0
call_count = 3
after avg_temp(1): call_count = 4
restored: avg = 25
OK
$ clang -std=c11 -Wall -Wextra -I. sensor.c test_sensor.c -o ts_clang && ./ts_clang
avg = 30, rc = 0
call_count = 3
after avg_temp(1): call_count = 4
restored: avg = 25
OK
$ gcc ... -fsanitize=address,undefined ... && ./ts_asan
avg = 30, rc = 0
call_count = 3
after avg_temp(1): call_count = 4
restored: avg = 25
OK
```

mock 桩核心：

```c
static int mock_sensor(int* out) {
    int v = script[call_count % script_len];
    call_count++;
    if (out) {
        *out = v;
    }
    return v;
}
```

## 4.8-B {#hw-4-8-b}

**难度 L3** · 题面见 [homework](homework#hw-4-8-b)

**思路**：产品代码一行不改（照题面往 **fd 1** 打日志），链接期 `-Wl,--wrap,write` 只改写**本工程目标文件**里对 `write` 的引用；动态链接的 `printf` 在 libc 内部调用 `write`，**根本不经过** `--wrap` 这个链接期符号——所以测试自己照常 `printf`、与截获的 `write` 互不干扰（实测 `last_fd=1、last_len=12`，断言全过，`printf` 照常打屏）。

1. `__wrap_write` 签名必须和真实 `write` 完全一致（`ssize_t (int, const void*, size_t)`）：记下被调次数、最后一次的 `len`，返回 count 假装成功、**不真写**。→ 知识点：[第 8 章](/04-engineering/08-mock-and-isolation)「第二招：链接期 --wrap」一节
2. `objdump` 显示 `logger_emit` 里 `call 11a7 <__wrap_write>`——调用目标被链接器改写，不是 libc 的 `write`；`nm` 佐证可执行文件里只有 `__wrap_write` 没有 `write`。→ 知识点：[第 8 章](/04-engineering/08-mock-and-isolation)（--wrap 是链接期魔法）、[阶段 0 第 5 章](/00-dev-environment/06-object-files-and-symbols)（nm 看符号）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. logger.c test_logger.c -Wl,--wrap,write -o tw_gcc && ./tw_gcc
test output                          ← 测试自己的 printf 照常工作(libc 内部调用不经过 --wrap)
got = 12
wrap_call_count = 1, last_fd = 1, last_len = 12
OK
$ clang -std=c11 -Wall -Wextra -I. logger.c test_logger.c -Wl,--wrap,write -o tw_clang && ./tw_clang
got = 12
wrap_call_count = 1, last_fd = 1, last_len = 12
OK
$ objdump -d tw_gcc | sed -n '/<logger_emit>:/,/ret/p'
...
        mov    $0x1,%edi           ← 产品代码原样的 write(1, ...)
        call   ... <__wrap_write>   ← 调用目标被链接器改写
$ nm tw_gcc | grep -E 'logger_emit|wrap'
000000000000... T __wrap_write
000000000000... T logger_emit
000000000000... b wrap_call_count
```

`--wrap` 的代价：读 `logger.c` 的人看不出 `write` 被换过，只有去翻链接选项才知道——显式（函数指针）vs 隐式（--wrap）的取舍，就是教材「三种手段怎么选」那张表。

## 4.9-A {#hw-4-9-a}

**难度 L2** · 题面见 [homework](homework#hw-4-9-a)

**思路**：gdb batch 模式（`-q -batch -ex ...`）把交互式命令脚本化，适合做题和复现。崩溃后第一件事 `thread apply all bt` 看全线程栈，因为多线程 bug 的根因常在别的线程。

1. `worker_good` 纯忙循环不碰缓冲 → 崩溃唯一确定在 `worker_bad`：Thread 3 命中 SIGSEGV、停在 `shared_buf[i] = i * round;`；gdb 自动切到 Thread 3（`info threads` 里带 `*` 那行）。→ 知识点：[第 9 章：gdb 实战](/04-engineering/09-gdb-multi-thread)「多线程栈」一节
2. `thread apply all bt` 三个线程各居其位：Thread 3 事故现场、Thread 2 忙循环、Thread 1 卡在 `pthread_join`；`info locals` 给 `i=0, round=3`，`print shared_buf` 是 `0x0`——free 后置 NULL 再写，链条完整。→ 知识点：同上（`thread apply all bt` 是多线程崩溃后第一件事）

**验证输出**：

```text
$ ./mtcrash; echo "exit=$?"
Segmentation fault
exit=139
$ gdb -q -batch -ex run -ex "thread apply all bt" -ex "info threads" \
      -ex "info locals" -ex "print shared_buf" --args ./mtcrash
[New Thread 0x7ffff7bff6c0 (LWP 1660)]
[New Thread 0x7ffff73fe6c0 (LWP 1661)]

Thread 3 "mtcrash" received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x7ffff73fe6c0 (LWP 1661)]
worker_bad (arg=0x0) at mtcrash.c:32
32	                shared_buf[i] = i * round;

Thread 3 (Thread 0x7ffff73fe6c0 (LWP 1661) "mtcrash"):
#0  worker_bad (arg=0x0) at mtcrash.c:32
Thread 2 (Thread 0x7ffff7bff6c0 (LWP 1660) "mtcrash"):
#0  0x00005555555551c7 in worker_good (arg=0x0) at mtcrash.c:16
Thread 1 (Thread 0x7ffff7fa8740 (LWP 1657) "mtcrash"):
#4  0x0000555555555334 in main () at mtcrash.c:53
  Id   Target Id                                     Frame
  1    Thread 0x7ffff7fa8740 (LWP 1657) "mtcrash" 0x00007ffff7ca07f2 in ?? () from /usr/lib/libc.so.6
  2    Thread 0x7ffff7bff6c0 (LWP 1660) "mtcrash" 0x00005555555551c7 in worker_good (arg=0x0) at mtcrash.c:16
* 3    Thread 0x7ffff73fe6c0 (LWP 1661) "mtcrash" worker_bad (arg=0x0) at mtcrash.c:32
i = 0
round = 3
$1 = (int *) 0x0
```

直接跑只有一句 `Segmentation fault` + 139，batch 一趟就把「哪个线程、哪一行、指针什么值」全挖出来了。

## 4.9-B {#hw-4-9-b}

**难度 L4** · 题面见 [homework](homework#hw-4-9-b)

**思路**：`-O2` 把 `start=12` 整条链在编译期折叠：`collatz_steps` 被内联、`steps/doubled/result` 都被算出常量，运行时内存里根本没留位置，gdb 只能回 `<optimized out>`。

1. `-O0` 版：断点停稳，`steps=9 doubled=18`、`info locals` 五个变量全在。→ 知识点：[第 9 章](/04-engineering/09-gdb-multi-thread)「-O2 的变量失踪」一节、[阶段 0 第 9 章](/00-dev-environment/10-standards-and-optimization)
2. `-O2` 版：同一断点 `print steps` → `<optimized out>`（本机这次 `doubled/result` 被优化掉、`start` 反而幸存——见 `info locals` 里的 `start = 12`）；`nm` 对比：`-O0` 有 `t collatz_steps`，`-O2` **符号整个消失**（被内联）。→ 知识点：同上
3. `volatile` 兜底要如实读输出：标了 `volatile` 的 `steps` 回来了（`$1 = 9`），但 `doubled/result` 依然 `<optimized out>`、`collatz_steps` 仍被内联——**volatile 只保住被标的那一个变量**，不是「保住全场」，这是本题相对教材结论的一处诚实细化。→ 知识点：同上（volatile 的代价与边界）

**验证输出**：

```text
$ gdb -q -batch -ex "break collatz.c:18" -ex run -ex "print steps" \
      -ex "print doubled" -ex "info locals" --args ./collatz_O0
Breakpoint 1, main () at collatz.c:18
18	    return 0;
$1 = 9
$2 = 18
start = 12
steps = 9
doubled = 18
result = 19
$ gdb -q -batch -ex "break collatz.c:18" -ex run -ex "print steps" \
      -ex "print doubled" -ex "info locals" --args ./collatz_O2
Breakpoint 1, main () at collatz.c:18
18	    return 0;
$1 = <optimized out>
$2 = <optimized out>
start = 12
steps = <optimized out>
doubled = <optimized out>
result = <optimized out>
$ nm collatz_O0 | grep collatz
0000000000001139 t collatz_steps
$ nm collatz_O2 | grep collatz || echo "(O2 无符号:被内联)"
(O2 无符号:被内联)
$ gdb -q -batch -ex "break collatz_v.c:18" -ex run -ex "print steps" \
      -ex "print doubled" -ex "info locals" --args ./collatz_v_O2
Breakpoint 1, main () at collatz_v.c:18
18	    return 0;
$1 = 9
$2 = <optimized out>
start = 12
steps = 9
doubled = <optimized out>
result = <optimized out>
```

## 4.10-A {#hw-4-10-a}

**难度 L2** · 题面见 [homework](homework#hw-4-10-a)

**思路**：同一套 CI flags 下，UBSan 默认 recover（报完继续跑、退出 0），ASan 报完即停（退出非 0）；clang 和 gcc 对「越界」这种双管辖区错误的表现不一样——这就是 CI 教训的来源。

1. clang 编 `oob2`：UBSan 的 bounds 检查**截胡**，报 `index 8 out of bounds for type 'int[4]'`，报完继续、退出 0；`uaf2` 归 ASan 管：`heap-use-after-free` 三段栈（非法读在 :9、free 在 :8、malloc 在 :6），退出 1；`ovf2` 两条 UBSan（有符号溢出 + 移位越界）都报、退出 0。→ 知识点：[第 10 章：ASan+UBSan 深入](/04-engineering/10-sanitizer-deep)「用 CI 的那套 flags」一节
2. gcc 编同一个 `oob2`：UBSan 报完**没有短路**，ASan 的 `stack-buffer-overflow` 照样登场、`ABORTING`、退出 1——**同样一套 flags，两个编译器的输出不一样**。→ 知识点：同上（sanitizer 行为依赖编译器实现；别拿「我本地 gcc 跑过没报」当「CI 也一定过」）

**验证输出**（clang 三个 + gcc 对照）：

```text
$ clang -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g oob2.c -o oob_c
$ ./oob_c
oob2.c:6:5: runtime error: index 8 out of bounds for type 'int[4]'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior oob2.c:6:5
a[0] = 0
exit=0
$ ./uaf_c
=================================================================
==1781==ERROR: AddressSanitizer: heap-use-after-free on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 ... in main /tmp/cj-ex4-hw5/410a/uaf2.c:9
freed by thread T0 here:
    #1 ... in main /tmp/cj-ex4-hw5/410a/uaf2.c:8
previously allocated by thread T0 here:
    #1 ... in main /tmp/cj-ex4-hw5/410a/uaf2.c:6
SUMMARY: AddressSanitizer: heap-use-after-free /tmp/cj-ex4-hw5/410a/uaf2.c:9 in main
exit=1
$ ./ovf_c
ovf2.c:7:15: warning: shift count >= width of type [-Wshift-count-overflow]
ovf2.c:6:15: runtime error: signed integer overflow: 2147483647 + 100 cannot be represented in type 'int'
ovf2.c:7:15: runtime error: shift exponent 32 is too large for 32-bit type 'int'
-2147483549 -300736192
exit=0
$ gcc -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -g oob2.c -o oob_g
$ ./oob_g
oob2.c:6:6: runtime error: index 8 out of bounds for type 'int [4]'
oob2.c:6:10: runtime error: store to address 0x... with insufficient space for an object of type 'int'
=================================================================
==1798==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x... thread T0
WRITE of size 4 at 0x... thread T0
    #0 ... in main /tmp/cj-ex4-hw5/410a/oob2.c:6
Address 0x... is located in stack of thread T0 at offset 96 in frame
  This frame has 2 object(s):
    [48, 52) 'i' (line 5)
    [64, 80) 'a' (line 4) <== Memory access at offset 96 overflows this variable
Shadow bytes around the buggy address:
=>0x...: f1 f1 f1 f1 f1 f1 04 f2 00 00 f3 f3[f3]f3 f3 f3
exit=1
```

## 4.10-B {#hw-4-10-b}

**难度 L4** · 题面见 [homework](homework#hw-4-10-b)

**思路**：use-after-scope 是新版 gcc/clang 上 `-fsanitize=address` 的默认行为，标志是 shadow byte `f8`；三个旋钮用 `ASAN_OPTIONS=help=1` 让 ASan 自报；LSan 报泄漏会让退出码非 0。

1. 出块后读 `local`：`stack-use-after-scope`，shadow 行 `f1 f1 f1 f1[f8]f8 f3 f3`——`f8` = Stack use after scope。→ 知识点：[第 10 章](/04-engineering/10-sanitizer-deep)「-fsanitize-address-use-after-scope」一节
2. `help=1` 自报：`halt_on_error` 当前 `true`、`abort_on_error` 当前 `false`（严格 `abort(3)` 默认关着）、`detect_leaks` 当前 `true`。→ 知识点：同上「ASAN_OPTIONS 三件套」一节
3. `detect_leaks=1`：LSan 报 `Direct leak of 64 byte(s)`、退出 1；`=0`：一声不吭退出 0。LSan 靠 `ptrace` 扫内存，seccomp 严的容器里会启动即挂（`Unable to scan process memory`），那种环境必须 `detect_leaks=0`——本机无受限容器，这条据教材与 LSan 文档作答。→ 知识点：同上「LSan 的容器/seccomp 坑」一节

**验证输出**：

```text
$ ./s3_g
=================================================================
==1807==ERROR: AddressSanitizer: stack-use-after-scope on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 ... in main /tmp/cj-ex4-hw5/410b/scope3.c:9
  This frame has 1 object(s):
    [32, 48) 'local' (line 6) <== Memory access at offset 32 is inside this variable
SUMMARY: AddressSanitizer: stack-use-after-scope /tmp/cj-ex4-hw5/410b/scope3.c:9 in main
Shadow bytes around the buggy address:
=>0x...: f1 f1 f1 f1[f8]f8 f3 f3 00 00 00 00 00 00 00 00
exit=1
$ ASAN_OPTIONS=help=1 ./s3_g 2>&1 | grep -A1 -E "halt_on_error|abort_on_error|detect_leaks"
	halt_on_error
		- Crash the program after printing the first error report ... (Current Value: true)
	detect_leaks
		- Enable memory leak detection. (Current Value: true)
	abort_on_error
		- If set, the tool calls abort() instead of _exit() ... (Current Value: false)
$ ASAN_OPTIONS=detect_leaks=1 ./l3_g
=================================================================
==1817==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #1 ... in main /tmp/cj-ex4-hw5/410b/leak3.c:4
SUMMARY: AddressSanitizer: 64 byte(s) leaked in 1 allocation(s).
exit=1
$ ASAN_OPTIONS=detect_leaks=0 ./l3_g >/dev/null 2>&1; echo "exit=$?"
0
```

## 4.11-A {#hw-4-11-a}

**难度 L2** · 题面见 [homework](homework#hw-4-11-a)

**思路**：能力矩阵纸面题，核心是分清「地址能不能访问」（ASan）和「值从哪来」（valgrind）。

1. ① 读未初始化 → **valgrind memcheck**（ASan 的影子内存只管地址合法性，不初始化读在 C 里多为未指定行为，UBSan 也不管）；② free 后再读 → **ASan**（heap-use-after-free 是它的主场，报完三段栈）；③ 无锁自增 → **TSan**（valgrind 侧对应 helgrind/drd）；④ `INT_MAX + 1` → **UBSan**（有符号溢出 UB）。→ 知识点：[第 11 章：valgrind 与 sanitizer 的分工](/04-engineering/11-valgrind)「一张能力矩阵」一节
2. 形态差别：sanitizer 是**编译期插桩**（要 `-fsanitize` 重编重链、ASan 约慢 2 倍、能挂 CI 天天跑），valgrind 是**运行期动态二进制翻译**（不用重编、不要源码、慢 10-50 倍、只能定向复核）。→ 知识点：[第 11 章](/04-engineering/11-valgrind)「引言」一节
3. MSan 要求程序里**每一个库**（包括 glibc）都用 MSan 重编，否则未重编的库传出的未初始化字节会一路误报——工程上几乎没法全栈启用，所以「不重编就能查未初始化读」这个生态位归 valgrind。→ 知识点：同上（MSan 落地困难户）

## 4.11-B {#hw-4-11-b}

**难度 L3** · 题面见 [homework](homework#hw-4-11-b)

**思路**：① ASan 对未初始化读一声不吭（退出 0），valgrind 靠「字节有没有被写过」的元数据当场抓；本机 Arch 的 valgrind 动态链接启动即死（教材的环境坑），按 `-static` 绕法后 memcheck 正常工作。② TSan 抓的是「竞争存在与否」，不是「结果对错」。

1. ASan 跑 uninit：`decide returned 0`、退出 0，一个字不报。valgrind 动态链接：`Fatal error at startup: a function redirection which is mandatory ...`（本机 Arch 把 ld-linux 符号 strip 了，教材「环境坑」一节就是这个）；`-static` 绕过后：`Conditional jump or move depends on uninitialised value(s)` 精确到 `uninit.c:5`，`--error-exitcode=99` 退出 99。→ 知识点：[第 11 章](/04-engineering/11-valgrind)「实验一」与「环境坑」两节
2. TSan：`WARNING: ThreadSanitizer: data race`，直指 `race.c:9`、`Location is global 'counter'`，退出 66——哪怕这次 counter 恰好打出了期望值，竞争存在就是存在。→ 知识点：[第 11 章](/04-engineering/11-valgrind)「实验三」一节

**验证输出**：

```text
$ gcc -g -std=c11 -Wall -Wextra -fsanitize=address,undefined \
      -fno-omit-frame-pointer uninit.c -o uninit_asan
$ ./uninit_asan; echo "exit=$?"
decide returned 0
exit=0
$ gcc -g -std=c11 -Wall -Wextra uninit.c -o uninit_dyn
$ valgrind --tool=memcheck ./uninit_dyn
==2059== Memcheck, a memory error detector
valgrind:  Fatal error at startup: a function redirection
valgrind:  which is mandatory for this platform-tool combination
exit=1
$ gcc -g -std=c11 -Wall -Wextra -static uninit.c -o uninit_static
$ valgrind --tool=memcheck --leak-check=no --error-exitcode=99 ./uninit_static
==2066== Conditional jump or move depends on uninitialised value(s)
==2066==    at 0x402F86: decide (uninit.c:5)
==2066==    by 0x402FB7: main (uninit.c:13)
==2066== Conditional jump or move depends on uninitialised value(s)
==2066==    at 0x41197A: free (in /tmp/cj-ex4-hw6/411b/uninit_static)
==2066==    by 0x402FDF: main (uninit.c:15)
decide returned 0
exit=99
$ gcc -g -std=c11 -Wall -Wextra -pthread -fsanitize=thread \
      -fno-omit-frame-pointer race.c -o race_tsan
$ ./race_tsan; echo "exit=$?"
==================
WARNING: ThreadSanitizer: data race (pid=2075)
  Read of size 4 at 0x... by thread T2:
    #0 worker /tmp/cj-ex4-hw6/411c/race.c:9 (race_tsan+0x11f9)
  Previous write of size 4 at 0x... by thread T1:
    #0 worker /tmp/cj-ex4-hw6/411c/race.c:9 (race_tsan+0x1211)
  Location is global 'counter' of size 4 at 0x... (race_tsan+0x4064)
SUMMARY: ThreadSanitizer: data race /tmp/cj-ex4-hw6/411c/race.c:9 in worker
==================
counter = 1000000 (期望 1000000)
exit=66
```

注意最后一行：counter 这次**恰好**打出了 1000000，但 TSan 照样报 race、退出 66——「结果对是运气，竞争存在是事实」。

## 4.12-A {#hw-4-12-a}

**难度 L1** · 题面见 [homework](homework#hw-4-12-a)

**思路**：纸面判断题，静态分析的定位。

1. **编译时**静态分析，不运行程序。→ 知识点：[第 12 章：静态分析门](/04-engineering/12-static-analysis)「引言」一节
2. 不抓——编译器完全不管；违反 **ISO C11 §7.1.3**（Reserved identifiers）。→ 知识点：[第 12 章](/04-engineering/12-static-analysis)「第三类：reserved identifier」一节
3. `implementation-defined`（ISO §6.3.1.3：有符号整数窄化的结果由实现定义，不是 UB）。→ 知识点：同上「第一类：narrowing conversion」一节
4. clang-tidy 强在风格与标准符合性（reserved-identifier、缺括号、narrowing），cppcheck 强在内存/指针类真 bug（空指针解引用、泄漏、越界）、卖点是少误报。→ 知识点：[第 12 章](/04-engineering/12-static-analysis)「cppcheck」一节
5. 每个源文件的编译命令（include 路径、`-std`、宏定义），一般用 CMake 的 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 生成。→ 知识点：同上「clang_tidy_check.py」一节

## 4.12-B {#hw-4-12-b}

**难度 L3** · 题面见 [homework](homework#hw-4-12-b)

**思路**：先让编译器说话，再让 clang-tidy 说话，两边错位互补正是这道题要看的。注意题面的诚实预告：gcc 对 dangling else **是**会报警的（`-Wdangling-else` 收在 `-Wall`），对另两类一声不吭。

1. gcc `-Wall -Wextra -Wpedantic` 只报一条 `suggest explicit braces to avoid ambiguous 'else'`——reserved identifier 和 narrowing 全静默。→ 知识点：[第 12 章](/04-engineering/12-static-analysis)「三类 finding 的对照」一节
2. clang-tidy（`--checks` 用本仓三族配置）报 6 条：`bugprone-reserved-identifier` ×2、`readability-braces-around-statements` ×3（含 dangling else 那处）、`bugprone-narrowing-conversions` ×1（措辞是 `implementation-defined`）。→ 知识点：同上
3. 修法：`__HiddenCfg` 改名 `HiddenCfg`、if/else 补花括号、`int small = (int) big;` 显式窄化。重跑 clang-tidy 只剩 14 条全被系统头抑制的输出，编译运行 `0 1 1`。三处修法里，**显式 `(int)` 窄化**对应教材说的「legacy 妥协」——最彻底是把字段类型也改对，但 ABI 影响面大，先用 cast 兜住、留下 TODO。→ 知识点：同上「clib-utilities 的活样板」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -Wpedantic -c findings.c -o f.o
findings.c: In function 'pick':
findings.c:9:8: warning: suggest explicit braces to avoid ambiguous 'else' [-Wdangling-else]
    9 |     if (flag)
      |        ^
$ clang-tidy --checks="-*,bugprone-*,performance-*,readability-*,..." findings.c -- -std=c11
findings.c:4:16: warning: declaration uses identifier '__HiddenCfg', which is a reserved identifier [bugprone-reserved-identifier]
findings.c:6:3: warning: declaration uses identifier '__HiddenCfg', which is a reserved identifier [bugprone-reserved-identifier]
findings.c:9:14: warning: statement should be inside braces [readability-braces-around-statements]
findings.c:10:19: warning: statement should be inside braces [readability-braces-around-statements]
findings.c:12:13: warning: statement should be inside braces [readability-braces-around-statements]
findings.c:19:17: warning: narrowing conversion from 'long' to signed type 'int' is implementation-defined [bugprone-narrowing-conversions]
$ clang-tidy --checks="..." findings_fixed.c -- -std=c11
14 warnings generated.
Suppressed 14 warnings (14 in non-user code).
$ gcc -std=c11 -Wall -Wextra -Wpedantic findings_fixed.c -o ff && ./ff
0 1 1
```

## 4.13-A {#hw-4-13-a}

**难度 L2** · 题面见 [homework](homework#hw-4-13-a)

**思路**：只测 `grade_level(95)` 时，两个 `if` 的 false 路、`return "B"` 和 `return "C"` 全是死的；补 70/30 两条用例后全活。字符串内容比较用 `strcmp`——直接 `==` 比的是指针地址（那条 -Waddress 警告就是编译器在提醒）。

1. 第一版：`Lines executed:50.00% of 6`、`Branches executed:50.00% of 4`、`Taken at least once:25.00% of 4`——`.gcov` 里第 8/9/11 行顶着 `#####`。→ 知识点：[第 13 章：覆盖率门](/04-engineering/13-coverage)「gcov」与「行覆盖 vs 分支覆盖」两节
2. 补两条用例后：三行数字全 100%，`grade_level called 3`。→ 知识点：同上（补用例→覆盖率涨的闭环）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra --coverage -g -O0 grade.c test_grade1.c -o t1 && ./t1
$ gcov -b grade.c
File 'grade.c'
Lines executed:50.00% of 6
Branches executed:50.00% of 4
Taken at least once:25.00% of 4
$ cat grade.c.gcov
function grade_level called 1 returned 100% blocks executed 50%
        1:    4:const char* grade_level(int score) {
branch  0 taken 100% (fallthrough)
branch  1 taken 0%
        1:    6:        return "A";
    #####:    8:    if (score >= 60) {
branch  0 never executed (fallthrough)
branch  1 never executed
    #####:    9:        return "B";
    #####:   11:    return "C";
$ # 补上 grade_level(70)=="B" 与 grade_level(30)=="C" 后
$ gcov -b grade.c
Lines executed:100.00% of 6
Branches executed:100.00% of 4
Taken at least once:100.00% of 4
```

「行覆盖 100% 也不代表分支覆盖 100%」在哪亲眼看到：第一版的行覆盖是 50%，看上去已经「测了一半」；分支那边 `Taken at least once` 只有 25%——四个分支走向只走过一路，另外三路是死的。行覆盖会粉饰「happy path 一跑到底」的测试，分支覆盖把这层粉饰揭掉。

## 4.13-B {#hw-4-13-b}

**难度 L3** · 题面见 [homework](homework#hw-4-13-b)

**思路**：三轮测试分别命中 `||` 短路的三条路：`a==NULL` 单独为真（轮 2）、`a!=NULL && n<=0`（轮 3）——教材留的那个练习，答案就是「补一条非 NULL 但 n=0」。

1. 轮 1（只测正常路径）：`Taken at least once:66.67% of 6`；轮 2（加 NULL）：`83.33%`；轮 3（加 n=0）：`100.00%`。→ 知识点：[第 13 章](/04-engineering/13-coverage)「分支覆盖」一节
2. 第三轮 `.gcov` 里 branch 明细：`branch 0 taken 75% (fallthrough)` 是 `a==NULL` 假、`branch 1 taken 25%` 是真（4 次调用里 1 次 NULL）；`branch 2 taken 33%` 是 `n<=0` 真（那条「非 NULL 但 n=0」）、`branch 3 taken 67%` 是假——两条新用例各命中一路。→ 知识点：同上（读 .gcov 的 branch 行）

**验证输出**：

```text
===== 轮次 1(只测 {10,20,30} 两次) =====
Lines executed:85.71% of 7
Branches executed:100.00% of 6
Taken at least once:66.67% of 6
===== 轮次 2(+stats_average(NULL, 0) == -1) =====
Lines executed:100.00% of 7
Branches executed:100.00% of 6
Taken at least once:83.33% of 6
===== 轮次 3(+stats_average(a, 0) == -1,非 NULL 但 n=0) =====
Lines executed:100.00% of 7
Branches executed:100.00% of 6
Taken at least once:100.00% of 6
$ cat stats.c.gcov   # 轮次 3
function stats_average called 4 returned 100% blocks executed 100%
        4:    6:    if (a == NULL || n <= 0) {
branch  0 taken 75% (fallthrough)
branch  1 taken 25%
branch  2 taken 33% (fallthrough)
branch  3 taken 67%
        2:    7:        return -1;
        8:   10:    for (int i = 0; i < n; i++) {
branch  0 taken 75%
branch  1 taken 25% (fallthrough)
        2:   13:    return (int) (sum / n);
```

## 4.14-A {#hw-4-14-a}

**难度 L2** · 题面见 [homework](homework#hw-4-14-a)

**思路**：`clock()` 量的是 CPU 时间不是墙钟——sleep 时几乎不走，纯烧 CPU 时和墙钟趋同。量化第一性选择：先分清你要量的是墙钟还是 CPU。

1. sleep 组：`clock()=0.000027`、`monotonic=1.000183`、`cpu=0.000023`——`clock()` 和进程 CPU 时间几乎都是 0，墙钟老老实实 1 秒。→ 知识点：[第 14 章：性能剖析](/04-engineering/14-profiling)「为什么别用 clock()」一节
2. burn 组：三个读数都是 0.59 秒——纯 CPU-bound 时三者趋同。由此：「我的程序慢」要先量化是哪一种慢（CPU-bound 还是 I/O-bound），方向完全不同。→ 知识点：[第 14 章](/04-engineering/14-profiling)「第一件：慢，要量化哪一种时间」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -O2 clocks.c -o clocks && ./clocks
sleep(1)  : clock()=0.000027 s  monotonic=1.000183 s  cpu=0.000023 s
burn(300M): clock()=0.591606 s  monotonic=0.591606 s  cpu=0.591606 s
```

## 4.14-B {#hw-4-14-b}

**难度 L4** · 题面见 [homework](homework#hw-4-14-b)

**思路**：迭代数必须 argv 驱动——写死成常量 `-O2` 会整段折叠、机器码里没循环可测；`volatile` 累加防死代码消除。50:5 的比例在 call graph 里就是 90.9% : 9.1%（50/55 与 5/55）。

1. `-O2 -fno-inline -pg`：flat 里 `leaf_accumulate` self 100%、**calls 55**（插桩精确计数）；call graph 里 worker_heavy children 7.78s 占 90.9%、worker_light 0.78s 占 9.1%——比例 ≈ 50:5。→ 知识点：[第 14 章](/04-engineering/14-profiling)「gprof 是怎么工作的」与「真跑」两节
2. 去掉 `-fno-inline`：flat profile 只剩一行 `100.00% ... main`，三个函数被内联、符号消失，profile 假报「main 慢」。`-fno-inline` 只用于剖析看清结构，发布版必须还给它内联。→ 知识点：[第 14 章](/04-engineering/14-profiling)「真坑：gprof 被 inline 骗了」一节

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -O2 -fno-inline -pg work_tree.c -o wt_pg
$ ./wt_pg 80000000
heavy=479999994000000000 light=47999999400000000 (iters=80000000)
$ gprof -b ./wt_pg gmon.out
Flat profile:
Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls   s/call   s/call  name
100.00      8.56     8.56       55     0.16     0.16  leaf_accumulate
  0.00      8.56     0.00        1     0.00     7.78  worker_heavy
  0.00      8.56     0.00        1     0.00     0.78  worker_light

			Call graph
                0.78    0.00       5/55          worker_light [4]
                7.78    0.00      50/55          worker_heavy [3]
[1]    100.0    8.56    0.00      55         leaf_accumulate [1]
-----------------------------------------------
                0.00    7.78       1/1           main [2]
[3]     90.9    0.00    7.78       1         worker_heavy [3]
                7.78    0.00      50/55          leaf_accumulate [1]
-----------------------------------------------
                0.00    0.78       1/1           main [2]
[4]      9.1    0.00    0.78       1         worker_light [4]
                0.78    0.00       5/55          leaf_accumulate [1]
$ gcc -std=c11 -Wall -Wextra -O2 -pg work_tree.c -o wt_inline && ./wt_inline 80000000
$ gprof -b ./wt_inline gmon.out
Flat profile:
Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  Ts/call  Ts/call  name
100.00      8.69     8.69                             main
```

## 4.15-A {#hw-4-15-a}

**难度 L2** · 题面见 [homework](homework#hw-4-15-a)

**思路**：硬门 = 脚本退出码非 0 就断 CI；报告 = 只生数字不挡合并。六行表加三个追问。

1. 六道门表：`build-examples` → `build_examples.py` 退出码 → 硬门（gcc/clang 矩阵双跑）；`sanitize` → 同一个脚本 + sanitizer flags 再编 → 硬门；`docs` → `validate_frontmatter.py` + markdownlint action 的退出码 → 硬门；`format-check` → `clang-format --dry-run --Werror` 退出码 → 硬门；`static-analysis` → `clang_tidy_check.py`（stdout 有 `warning:` 即失败）→ 硬门；`coverage` → lcov 只打摘要 → **报告**。→ 知识点：[第 15 章：把质量门拼成流水线](/04-engineering/15-ci-pipeline)「逐 job 概览」一节
2. `coverage` 是报告：最后一步 `lcov --summary` 只**打印**覆盖率、不做阈值断言，哪怕 5% 也退出 0；但它里面「编译+`ctest --output-on-failure`」那一步是硬的（测试挂了 `&&` 链断、CI 红）。→ 知识点：同上「coverage」一节
3. `KNOWN_LEGACY`：`build_examples.py` 给明知编不过的老工程（Keil/bochs/VS/mplayer）留报告出口子，失败只打日志不断 CI；新代码和整改完的走硬门。`concurrency.cancel-in-progress`：同分支新推送取消上一轮在跑的旧 CI，省排队、状态始终对应最新一次。→ 知识点：同上「KNOWN_LEGACY 双模式」与「concurrency」两节
4. 加新门三步：写 `scripts/xxx_check.py`（裁决逻辑写脚本里、`sys.exit(0/1)`）→ ci.yml 抄一个 job（checkout + 装工具 + 跑脚本）→ 想清楚硬门还是报告。→ 知识点：同上「怎么加一道新门」一节

## 4.15-B {#hw-4-15-b}

**难度 L3** · 题面见 [homework](homework#hw-4-15-b)

**思路**：三道门各跑各的命令、各自 `rc` 判红绿、汇总 `fail` 由脚本 `exit` 交出去——「什么叫失败」全写在脚本里，这正是教材说的「裁决逻辑写在脚本里」。

1. 第一轮 `test1.c` 故意断言 `mod_double(21) == 44`：门 1 编译绿，门 2 测试 `test exit=134`（abort）红，门 3 sanitizer 同样红，`汇总 fail=1`、脚本退出 1。→ 知识点：[第 15 章](/04-engineering/15-ci-pipeline)「本地复现三道硬门」一节、[第 7 章](/04-engineering/07-testing-with-unity)（退出码是 CI 判红绿的唯一依据）
2. 第二轮换 `test2.c`：三道门全绿、`汇总 fail=0`、退出 0。→ 知识点：同上

**验证输出**：

```text
$ ./gate.sh
==> gate 1: 编译门 (gcc -Wall -Wextra -Werror)
    compile exit=0
==> gate 2: 测试门 (跑测试可执行, 看退出码)
    link exit=0
testbin: test1.c:7: main: Assertion `mod_double(21) == 44' failed.
    test exit=134
==> gate 3: sanitizer 门 (ASan+UBSan 重编再跑)
    sanitize build exit=0
testbin_asan: test1.c:7: main: Assertion `mod_double(21) == 44' failed.
    sanitize run exit=134
==== 汇总 fail=1 ====
gate exit=1
$ TEST_C=test2.c ./gate.sh
==> gate 1: 编译门 (gcc -Wall -Wextra -Werror)
    compile exit=0
==> gate 2: 测试门 (跑测试可执行, 看退出码)
    link exit=0
all pass
    test exit=0
==> gate 3: sanitizer 门 (ASan+UBSan 重编再跑)
    sanitize build exit=0
all pass
    sanitize run exit=0
==== 汇总 fail=0 ====
gate exit=0
```

## 4.16-A {#hw-4-16-a}

**难度 L1** · 题面见 [homework](homework#hw-4-16-a)

**思路**：收官章的「知不知道」，全是第 16 章真跑过的事实。

1. UBSan 抓的是 `CCDynamicArray.c:203` **函数指针类型不匹配的调用**（`compareInt` 强转签名不符，ISO §6.5.2.2 UB）；ASan 抓的是 `eraseSingle` 的 **heap-buffer-overflow**（缩容后读到 `0 bytes after 20-byte region`）。→ 知识点：[第 16 章：工程化毕业项目](/04-engineering/16-capstone)「③ Sanitizer」一节
2. **不能**——ctest 那五条用例全绿，ASan 一开当场抓到两个真 bug。「测试全绿」和「没有内存 bug」是两回事，中间那道缝就是 sanitizer 门存在的意义。→ 知识点：同上
3. 行覆盖 **42.45%**，139 行里约跑了 59 行。→ 知识点：[第 13 章](/04-engineering/13-coverage)
4. **没有**——CI 的 sanitize 与 clang-tidy 目前只覆盖 `examples/` 一级子项目，clib 在硬门外、legacy 整改中（修齐再纳入）。→ 知识点：[第 16 章](/04-engineering/16-capstone)（诚实交代「六道门里有一道还没合上」）
5. **绝对路径**（CMake 默认埋的 build-tree RPATH 指向安装前缀）；分发时换成相对的 `$ORIGIN` 写法、设 `INSTALL_RPATH`。→ 知识点：[第 6 章](/04-engineering/06-libs-and-linking)（build-tree RPATH 是开发期便利）

## 4.16-B {#hw-4-16-b}

**难度 L3** · 题面见 [homework](homework#hw-4-16-b)

**思路**：`arr_erase` 挪元素循环写 `i < a->len`，最后一轮 `i == len-1` 读 `data[len]`——容量 4、元素 4 时正好读到分配区外一字节。普通构建下读的是邻接内存的脏字节、写入 `data[3]` 后被 `len--` 藏掉，测试全绿；ASan 埋的红区当场点名。

1. 第一阶段普通构建：ctest `100% tests passed`、Unity `4 Tests 0 Failures`。→ 知识点：[第 7 章](/04-engineering/07-testing-with-unity)（ctest 全绿 ≠ 没 bug，这句话先记着）
2. 第二阶段 CI sanitizer flags 重编：`heap-buffer-overflow`、`READ of size 4`、`0 bytes after 16-byte region`、点名 `arr_erase`——和教材 16 章 clib `EraseSingle` 是同一种病：**缩容时越界读**。→ 知识点：[第 16 章](/04-engineering/16-capstone)「③ Sanitizer」一节、[第 10 章](/04-engineering/10-sanitizer-deep)
3. 第三阶段把循环改成 `i + 1 < a->len`，sanitizer 下 ctest 全绿退出 0。→ 知识点：同上

**验证输出**：

```text
$ ctest --test-dir build --output-on-failure
1/1 Test #1: arr_unity ........................   Passed    0.00 sec
100% tests passed out of 1
$ CC=clang CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  LDFLAGS="-fsanitize=address,undefined" cmake -B build-asan && cmake --build build-asan
$ ctest --test-dir build-asan --output-on-failure
1/1 Test #1: arr_unity ........................***Failed    0.08 sec
  test_push_three ... PASS
  test_erase_middle ... =================================================================
==2716==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 ... (test_arr+0x194c6a)
0x... is located 0 bytes after 16-byte region [0x...,0x...)
allocated by thread T0 here:
    #1 ... (test_arr+0x1941ae)
SUMMARY: AddressSanitizer: heap-buffer-overflow (test_arr+0x194c6a)
50% tests passed, 1 tests failed out of 1
ctest exit=8
$ # 修掉循环边界再 build
$ ctest --test-dir build-asan --output-on-failure
1/1 Test #1: arr_unity ........................   Passed    0.09 sec
100% tests passed out of 1
ctest exit=0
```

为什么普通构建「看起来没事」：`data[4]` 那 4 字节仍在进程的堆区里（不是保护区），读得动、值不崩；读来的脏字节被写进 `data[3]`、随后 `len--` 把 `data[3]` 移出视线。UB 最阴险的地方就在这里——**它能静默运行到某天布局一变才炸**，ASan 的红区就是把「某天」提前到今天。

## 4.C-1 {#hw-4-c-1}

**难度 L3** · 题面见 [homework](homework#hw-4-c-1)

**思路**：不透明句柄（第 2 章）+ context 错误（第 3 章）+ mini_unity（第 7 章）+ gcov（第 13 章）四章拧一股绳。队列用环形容器（head/tail 模 cap），元素是堆上 dup 的字符串。

1. `strqueue.h` 只有 `typedef struct StrQueue StrQueue_t;` 加五个原型，字段藏 `.c`；`sq_push` 满返 0 并往 ctx 写「队列已满:cap=%zu」，`sq_pop` 空返 0 写「队列为空」。→ 知识点：[第 2 章](/04-engineering/02-api-and-opaque-types)、[第 3 章](/04-engineering/03-error-handling)
2. 四条 Unity 用例 FIFO 顺序/满报错/空报错/出队缩容，`4 Tests 0 Failures`，ASan 干净。→ 知识点：[第 7 章](/04-engineering/07-testing-with-unity)
3. gcov：`Lines 78.79% of 66`、`Branches 100% of 30`、`Taken 60.00% of 30`、`Calls 42.86% of 7`——死的全是「参数非法（码 1）/分配失败（码 3）/出队缓冲不够（码 5）」这些错误注入分支。工程权衡：这些分支要么靠故障注入（Lab 的 L5 就是干这个的）、要么留给「防御性代码可测性低」的旧账，硬补普通用例性价比低。→ 知识点：[第 13 章](/04-engineering/13-coverage)

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra strqueue.c errctx.c mini_unity.c test_strqueue.c -o t && ./t
  test_fifo_order ... PASS
  test_push_full_reports ... PASS
  test_pop_empty_reports ... PASS
  test_pop_shrinks ... PASS

4 Tests 0 Failures
exit=0
$ gcc ... -fsanitize=address,undefined ... -o t_asan && ./t_asan
  test_fifo_order ... PASS
  test_push_full_reports ... PASS
  test_pop_empty_reports ... PASS
  test_pop_shrinks ... PASS

4 Tests 0 Failures
$ gcov -b strqueue.c
Lines executed:78.79% of 66
Branches executed:100.00% of 30
Taken at least once:60.00% of 30
Calls executed:42.86% of 7
function dup_str called 8 returned 100% blocks executed 75%
function sq_create called 4 returned 100% blocks executed 62%
function sq_push called 9 returned 100% blocks executed 69%
function sq_pop called 4 returned 100% blocks executed 67%
```

`sq_pop` 的实现要点（「空/不够返 0、错误进 ctx」）：

```c
int sq_pop(StrQueue_t* q, char* out, size_t out_cap, ErrCtx_t* err) {
    if (!q || !out) {
        errctx_set(err, 1, __func__, "参数非法");
        return 0;
    }
    if (q->count == 0) {
        errctx_set(err, 4, __func__, "队列为空");
        return 0;
    }
    char* s = q->slots[q->head];
    size_t n = strlen(s);
    if (n + 1 > out_cap) {
        errctx_set(err, 5, __func__, "出队缓冲不够:需要 %zu,给了 %zu", n + 1,
                   out_cap);
        return 0;
    }
    memcpy(out, s, n + 1);
    q->head = (q->head + 1) % q->cap;
    q->count--;
    free(s);
    return 1;
}
```

## 4.C-2 {#hw-4-c-2}

**难度 L4** · 题面见 [homework](homework#hw-4-c-2)

**思路**：第 5 章 target 三态 + 第 6 章 install/export/find_package + 第 15 章质量门拼装。踩过的真坑写在这里：`include(GNUInstallDirs)` 必须写在 `target_include_directories` **之前**——否则 `${CMAKE_INSTALL_INCLUDEDIR}` 还是空串，导出的 IMPORTED target 里 INTERFACE_INCLUDE_DIRECTORIES 被空值顶掉，消费者就 `fatal error: libcalc.h: No such file or directory`。

1. `add_library(calc SHARED ...)` + `SOVERSION 1` + `BUILD_INTERFACE`/`INSTALL_INTERFACE` 头路径 + `install(EXPORT ... NAMESPACE Calc::)` + `configure_package_config_file`/`write_basic_package_version_file`。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)（生成器表达式）、[第 6 章](/04-engineering/06-libs-and-linking)「全链路活教材」一节
2. `DESTDIR` 装出 7 类文件：`libcalc.so.1`、软链 `libcalc.so`、头、`CalcTargets.cmake`、`CalcTargets-noconfig.cmake`、`CalcConfig.cmake`、`CalcConfigVersion.cmake`。消费者 `find_package(Calc 1.0 REQUIRED)` + 链 `Calc::calc`，跑出 `calc_add(2,3)=5 calc_mul(4,5)=20`。→ 知识点：同上
3. `readelf -d`：RUNPATH 是**绝对路径**（CMake 默认 build-tree RPATH）；分发时用 `$ORIGIN` 相对写法设 `INSTALL_RPATH` 才可移植。→ 知识点：[第 6 章](/04-engineering/06-libs-and-linking)（build-tree RPATH 的诚实交代）

**验证输出**：

```text
$ cmake --build libcalc/build
[ 50%] Building C object CMakeFiles/calc.dir/src/libcalc.c.o
[100%] Linking C shared library libcalc.so
$ DESTDIR=$PWD/prefix cmake --install libcalc/build
-- Installing: .../prefix/usr/local/lib/libcalc.so.1
-- Installing: .../prefix/usr/local/lib/libcalc.so
-- Installing: .../prefix/usr/local/include/libcalc.h
-- Installing: .../prefix/usr/local/lib/cmake/Calc/CalcTargets.cmake
-- Installing: .../prefix/usr/local/lib/cmake/Calc/CalcTargets-noconfig.cmake
-- Installing: .../prefix/usr/local/lib/cmake/Calc/CalcConfig.cmake
-- Installing: .../prefix/usr/local/lib/cmake/Calc/CalcConfigVersion.cmake
$ cmake -S consumer -B consumer/build -DCALC_PREFIX=$PWD/prefix && cmake --build consumer/build
[ 50%] Building C object CMakeFiles/consumer.dir/main.c.o
[100%] Linking C executable consumer
$ ./consumer/build/consumer
calc_add(2,3) = 5
calc_mul(4,5) = 20
$ readelf -d consumer/build/consumer | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libcalc.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [/tmp/cj-ex4-hw9e/prefix/usr/local/lib]
```

## 4.C-3 {#hw-4-c-3}

**难度 L5** · 题面见 [homework](homework#hw-4-c-3)

**思路**：改编自 cmocka 的 `will_return`/`check_expected` 脚本队列语义（如实标注）。骨架复用第 7 章 `setjmp`/`longjmp` 隔离，队列用两个环形 FIFO（返回值脚本 + 期望参数脚本），队列空或参数不符就打印 FAIL、`longjmp` 回框架、继续下一条用例。

1. `mockfw.c` 两套队列 + `mock_take_return`/`mock_check_param`；`mock_run_test` 每例先 `mock_reset()`（新脚本）、再 `setjmp` 跑用例。→ 知识点：[第 7 章](/04-engineering/07-testing-with-unity)（setjmp/longjmp 隔离）、[第 8 章](/04-engineering/08-mock-and-isolation)（函数指针 mock 的框架化）
2. 用例①脚本 `-1,-1,7` + 三个期望参数 42 → `poll_until_ok(42,3)==7`、被调 3 次；用例②三次 `-1` → 返 -1；用例③期望写成 99 → 框架抓「参数不符」，这条 FAIL 且**不拖死**下一条。→ 知识点：[第 8 章](/04-engineering/08-mock-and-isolation)（mock 只按脚本返回、还能验参数）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. mockfw.c sensor.c test_sensor_mock.c -o tm_gcc && ./tm_gcc
  test_retry_succeeds ... PASS
  test_all_fail ... PASS
  test_param_mismatch_caught ...   FAIL: sensor_read 参数不符:期望 99, 实际 42
  test_still_runs_after_fail ... PASS

4 Tests 1 Failures
exit=1
$ clang -std=c11 -Wall -Wextra -I. mockfw.c sensor.c test_sensor_mock.c -o tm_clang && ./tm_clang
  test_retry_succeeds ... PASS
  test_all_fail ... PASS
  test_param_mismatch_caught ...   FAIL: sensor_read 参数不符:期望 99, 实际 42
  test_still_runs_after_fail ... PASS

4 Tests 1 Failures
exit=1
$ gcc ... -fsanitize=address,undefined ... -o tm_asan && ./tm_asan
  test_retry_succeeds ... PASS
  test_all_fail ... PASS
  test_param_mismatch_caught ...   FAIL: sensor_read 参数不符:期望 99, 实际 42
  test_still_runs_after_fail ... PASS

4 Tests 1 Failures
```

框架核心（两套队列 + 长跳隔离）：

```c
static mval_t ret_q[Q_CAP];
static int ret_head = 0;
static int ret_len = 0;
static mval_t exp_q[Q_CAP];
static int exp_head = 0;
static int exp_len = 0;

mval_t mock_take_return(const char* mock_name) {
    if (ret_len <= 0) {
        printf("  FAIL: %s 的返回值脚本用完了\n", mock_name);
        longjmp(MockJmp, 1);
    }
    return q_pop(ret_q, &ret_head, &ret_len);
}

void mock_check_param(const char* mock_name, mval_t got) {
    if (exp_len <= 0) {
        printf("  FAIL: %s 的期望参数脚本用完了(got=%lld)\n", mock_name, got);
        longjmp(MockJmp, 1);
    }
    mval_t want = q_pop(exp_q, &exp_head, &exp_len);
    if (want != got) {
        printf("  FAIL: %s 参数不符:期望 %lld, 实际 %lld\n", mock_name, want, got);
        longjmp(MockJmp, 1);
    }
}

void mock_run_test(void (*test)(void), const char* name) {
    printf("  %s ... ", name);
    fflush(stdout);
    mock_reset(); /* 每条用例拿到全新的脚本队列 */
    if (setjmp(MockJmp) == 0) {
        test();
        printf("PASS\n");
        MockPassCount++;
    } else {
        MockFailCount++;
    }
}
```

业务侧 `poll_until_ok` 一行没改 mock 的账，全靠函数指针 + 脚本队列：这就是 cmocka 把「桩生成 + 队列语义 + 断言 API」自动化之前的手写全貌——看懂了它，再看 cmocka 就是「省去手写的脚手架」。
