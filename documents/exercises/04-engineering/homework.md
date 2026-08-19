---
title: "阶段 4 课后练习（Homework）"
description: "工程化与质量门阶段的课后练习：16 章每章 2 题（基础+进阶），另加 2 道跨章综合与 1 道 L5 挑战（手写带脚本队列的 mock 框架，改编自 cmocka）。难度覆盖 L1~L5，题目都做了变式处理——换场景、换推理方向，照抄教材例题抄不出答案；参考答案独立成文件、逐步解答附知识点链接。"
chapter: 4
order: 0
tags:
  - host
  - engineering
  - testing
difficulty: intermediate
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 全部章节（第 1~16 章）"
related:
  - "阶段 4 Lab：给 statslib 配齐全套质量门"
  - "阶段 4 Project：clog 日志库"
---

# 阶段 4 课后练习（Homework）

## 引言

这里的题按章组织，每章两道（基础 + 进阶），最后是两道跨章综合和一道 L5 挑战。每题标注难度档位（L1~L5，见[练习总览](/exercises/)）和涉及章节。题目都是「变式」——换场景、换推理方向，照抄教材例题抄不出答案；每道题都要真编译真跑，把输出贴下来才算完。有几章的 L1 题是「知不知道」类的判断题/填空题，答在纸上即可，不用开终端。

答案在独立的[参考答案](homework-solutions)文件里，按题号对应，每步解答带知识点链接。建议一章做完再看答案。所有代码用 `-std=c11 -Wall -Wextra` 起步（个别题目会要求 sanitizer、CMake 或别的旗标，题面会写明）。用到教材没讲的内容时，题面会明确标注「教材外补充」。

## 4.1 头文件契约

### 4.1-A {#hw-4-1-a}

难度 **L1** · 涉及[第 1 章：头文件契约](/04-engineering/01-header-contracts)

六道判断题/填空题，全部纸面作答。① 把 include guard 的三行写出来（宏名用 `MYLIB_X_H` 示意）。② 判断题：`#pragma once` 在 ISO C 标准里吗？追求可移植该用哪种 guard？③ 判断题：头文件里写 `int counter = 5;`（带初始化、不带 `extern`），被两个 `.c` 各自 `#include`，链接会出什么事？④ 用一句话复述 ODR。⑤ 填空：头文件里的内联函数要写成 `____ inline`；只写裸 `inline` 时链接期会报 `____`。⑥ 判断题：C++ 的 `inline` 语义和 C99 的裸 `inline` 一样吗？

[参考答案 →](homework-solutions#hw-4-1-a)

### 4.1-B {#hw-4-1-b}

难度 **L3** · 涉及[第 1 章：头文件契约](/04-engineering/01-header-contracts)

教材提醒过「guard 宏名要全工程唯一，撞名了又是一个坑」，但没有演示这个坑。这道题把它复现出来。写两个头 `a.h`（定义 `struct Vec2 { int x; int y; };`）和 `b.h`（定义 `struct Color { unsigned char r; unsigned char g; unsigned char b; };`），**两个头故意用同一个 guard 宏名** `COMMON_H`。`main.c` 同时 `#include` 两者、各声明一个变量并打印。**先预测**编译会报什么错，再真跑贴真实报错；然后把两个 guard 宏名改成各自唯一的 `A_H`/`B_H`，重新编译运行贴输出。解释：为什么撞名会让后包含的头「整段被跳过」？

[参考答案 →](homework-solutions#hw-4-1-b)

## 4.2 API 设计与不透明类型

### 4.2-A {#hw-4-2-a}

难度 **L2** · 涉及[第 2 章：API 设计与不透明类型](/04-engineering/02-api-and-opaque-types)

用不透明类型写一个定容 `int` 栈 `IntStack`：头里只放前向声明加五个原型（create/destroy/push/pop/size），真实字段藏进 `.c`（教材用的 ringbuffer，这里换方向做栈）。消费者 `main` 只握着句柄走 API：压 3 个、查 size、弹 3 个。gcc 和 clang 双跑贴输出。再回答两问：消费者为什么写不出 `s->sp`（编译器会说什么）？为什么 `IntStack s;`（想在栈上分配）也编不过？

[参考答案 →](homework-solutions#hw-4-2-a)

### 4.2-B {#hw-4-2-b}

难度 **L4** · 涉及[第 2 章：API 设计与不透明类型](/04-engineering/02-api-and-opaque-types)、[第 1 章](/04-engineering/01-header-contracts)

教材演示了 opaque 的正向收益（加字段不破消费者），这道题做**反向实验**：把 struct 摊在头里会怎样。写一个模块 `naive.h`：`struct Config { int port; int verbose; };` 加一条 `void config_init(struct Config* c);`，`.c` 里把 `port` 填成 8080、`verbose` 填 1。消费者 `client.c` 直接读字段打印。先把消费者编成 `client.o`；然后改头文件——在 `port` **前面**插入一个 `int version;`（版本 2 的实现把 version 填 2、port 填 9090、verbose 填 0），只重编实现、**不重编消费者**，把旧 `client.o` 和新实现链起来跑。**先预测**旧消费者打印的 port 和 verbose 是多少，再真跑——如果结果和你预测的不一样（提示：新实现往偏移 8 写了第三个字段，而旧消费者的栈上对象只有 8 字节，本机 Arch 默认开栈保护），解释真正发生了什么；重编消费者后对比贴输出。最后解释「旧的 .o 里编进去的是什么、为什么这种错链接期不拦、运行期才炸」。

[参考答案 →](homework-solutions#hw-4-2-b)

## 4.3 错误处理三件套

### 4.3-A {#hw-4-3-a}

难度 **L2** · 涉及[第 3 章：错误处理三件套](/04-engineering/03-error-handling)

教材用除法演示了 errno 的「残留坑」，这里换场景。写一个温度换算函数 `int to_kelvin(double c, double* out)`：`c < -273.15` 时设 `errno = EINVAL`、返回 -1；否则把 $c + 273.15$ 写进 `*out` 返回 0。按这个调用序列写 `main`：先调 `to_kelvin(-300.0, ...)`（失败），再用**正确姿势**（先看返回值、失败才查 errno）处理它；然后调一次 `to_kelvin(25.0, ...)`（成功），用**错误姿势**——`if (errno == 0)` 判断这次调用成功没有——打印判出来的结论。贴输出，解释错误姿势为什么不可靠（调用前清零与否都说清楚）。

[参考答案 →](homework-solutions#hw-4-3-a)

### 4.3-B {#hw-4-3-b}

难度 **L3** · 涉及[第 3 章：错误处理三件套](/04-engineering/03-error-handling)

照着教材 context 对象的思路，写一个 `int parse_int(const char* s, int* out, error_ctx* err)`：把字符串解析成 `int`，出错时往 context 里写「码 + 人话 + 出错函数名」，API 只返成功/失败。要区分两种错：非法字符（码 1，人话带出错的字符串）和越界（码 2，用 `strtoll` 的 `errno == ERANGE` 判断——注意这里 errno 的用法要符合第 3 章的铁律）。测试三个输入：`"123"`（成功）、`"12a3"`（码 1）、`"99999999999999"`（码 2，超过 `int` 范围）。用 ASan+UBSan 跑一遍确认堆管理干净。贴全部输出。

[参考答案 →](homework-solutions#hw-4-3-b)

## 4.4 make 深处

### 4.4-A {#hw-4-4-a}

难度 **L1** · 涉及[第 4 章：make 深处](/04-engineering/04-make-deep)

六道判断题/填空题，纸面作答。① 判断题：make 判断目标是否过期，比较的是文件内容还是修改时间？② 填空：`%.o: %.c` 的 prerequisite 里没有头文件，改了 `mod.h` 后 make 会说什么（三个英文单词）？③ 填空：`-MMD` 生成的 `.d` 文件第一行长什么样（照教材格式写，目标名用 `main.o`）？④ `-MP` 加的 phantom 规则是为了什么场景？⑤ 判断题：Makefile 规则命令行行首必须是 TAB，空格会报什么错？⑥ 判断题：代码生成器产出的头文件漏写进 `%.o` 的 prerequisite，串行构建侥幸能过、`-j8` 并行会当场报什么错（把那条 fatal error 的原文默写出来）？

[参考答案 →](homework-solutions#hw-4-4-a)

### 4.4-B {#hw-4-4-b}

难度 **L3** · 涉及[第 4 章：make 深处](/04-engineering/04-make-deep)

教材明确说了「多目录的完整 Makefile 不展开」，这道题把它补上——只用教材给的原则（`.d` 文件跟着 `.o` 走、路径写对就自动对）。搭一个 `src/` + `include/` 工程：`src/main.c` 和 `src/util.c` 都 `#include "util.h"`，头里定义宏 `GREETING "v1"`。手写 Makefile：模式规则编 `build/%.o`（带 `-MMD -MP -Iinclude`）、`-include` 喂 `build/*.d`、自动创建 `build/` 目录、`clean` 删整个 `build/`。验收三步：① 干净构建跑出 `v1`；② 把 `GREETING` 改成 `v2` 再 `make`，贴出重编了哪些文件；③ **删掉 `include/util.h`**（源码不动）再 `make`，贴出这次报的是什么错——解释为什么报的是编译器的「找不到头文件」而不是 make 的「No rule to make target」（这正是 `-MP` 在干活）。

[参考答案 →](homework-solutions#hw-4-4-b)

## 4.5 CMake 工程化

### 4.5-A {#hw-4-5-a}

难度 **L2** · 涉及[第 5 章：CMake 工程化](/04-engineering/05-cmake-engineering)

归类题，纸面作答。给下面五条工程事实各自挑一个关键字（PRIVATE / PUBLIC / INTERFACE）并给一句理由：① 库的公开 API 头目录；② 库内部实现才用的私有头目录；③ 开发期警告旗标 `-Wall -Wextra`（希望自己编得严、但不传染给链接我的人）；④ 库的公开头文件里露出了 `pthread_mutex_t`（参照第 2 章 `CCMutex.h` 把类型藏起来的正例，反着想：如果没藏住会怎样），依赖 `pthread` 该挂什么；⑤ 库自己的代码根本不用、但想让消费者拿到的一个宏 `GREETER_VIA_CMAKE=1`。再答一问：`target_link_libraries(demo PRIVATE greeter)` 里那个 PRIVATE 是给谁用的、和库侧的 PRIVATE 是同一件事吗？

[参考答案 →](homework-solutions#hw-4-5-a)

### 4.5-B {#hw-4-5-b}

难度 **L3** · 涉及[第 5 章：CMake 工程化](/04-engineering/05-cmake-engineering)、[阶段 0 第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)

两道实验。① **GLOB_RECURSE 的坑**：写一个 `file(GLOB_RECURSE ...)` 收源的小工程（`src/alpha.c` 定义 `int alpha(void)`，`main.c` 调用），配置构建跑通；再加一个 `src/beta.c`、改 `main.c` 同时调 `alpha()` 和 `beta()`，**只 build 不 reconfigure**，贴出链接报错；然后重新 configure 修好。解释为什么「新文件明明在磁盘上」却没被编进来。② **多配置**：写一个带 `assert(x % 2 == 0)` 的程序，分别配 Debug 和 Release 两个 build 目录，`grep` 两个 `flags.make` 里的 `C_FLAGS`，贴出来；Debug 版传奇数运行、Release 版传奇数运行，各贴退出码。解释 `NDEBUG` 对 `assert` 做了什么。加一问（不用跑）：在 Visual Studio 这种多配置生成器上，`cmake -DCMAKE_BUILD_TYPE=Release` 为什么不生效、该用什么姿势切配置？

[参考答案 →](homework-solutions#hw-4-5-b)

## 4.6 静态库、动态库与链接顺序

### 4.6-A {#hw-4-6-a}

难度 **L2** · 涉及[第 6 章：静态库、动态库与链接顺序](/04-engineering/06-libs-and-linking)

教材演示了两库顺序陷阱，这道题加到三库。写三个静态库：`libtwice.a` 提供 `int twice(int x)`，`libquad.a` 的 `quad` 调 `twice`，`libocta.a` 的 `octa` 调 `quad`，`main.c` 调 `octa(5)`（应是 40）。用 `ar rcs` 打好三个库，按正确顺序（被依赖的靠右）链接跑通；再换一个**错误顺序**链接，复现 `undefined reference`——贴报错并指出报错里点名的函数是哪个、它其实躺在哪个库里。最后用 `-Wl,--start-group ... -Wl,--end-group` 把错误顺序救回来，说明为什么这是兜底不是正解。起名时留个心眼：不要把库叫 `libc.a`——`-lc` 会先匹配到你的库而不是系统 libc，`printf` 会当场消失。

[参考答案 →](homework-solutions#hw-4-6-a)

### 4.6-B {#hw-4-6-b}

难度 **L3** · 涉及[第 6 章：静态库、动态库与链接顺序](/04-engineering/06-libs-and-linking)

两道实验。① **`$ORIGIN` 拼错坑**（教材当场说实话的那条）：造 `libcore.so` 放进 `./libs/`，链接消费者时写 `-Wl,-rpath,\$ORIGIN`（**故意漏掉 `/libs`**；注意 bash 下 `$ORIGIN` 必须转义成 `\$ORIGIN`，否则 shell 把它当变量展开成空串——教材第 6 章的转义纪律，这一问正是靠它把「拼错」做出来），`readelf -d` 看 RUNPATH、运行贴报错；再改成 `-Wl,-rpath,\$ORIGIN/libs` 修好、运行贴输出。解释为什么报错和「完全没设 RPATH」一模一样、坑在哪。② **`-fvisibility=hidden`**：写一个 `.so`，里面有公开函数 `core_pub`（标 `__attribute__((visibility("default")))`）、内部辅助 `core_helper`（`static`）和一个忘了标的 `core_secret`；分别用默认编译和加 `-fvisibility=hidden` 编译，`nm -D` 对比导出符号，贴两份输出，说出差别。

[参考答案 →](homework-solutions#hw-4-6-b)

## 4.7 测试不再是 printf

### 4.7-A {#hw-4-7-a}

难度 **L2** · 涉及[第 7 章：测试不再是 printf](/04-engineering/07-testing-with-unity)

被测函数换成求和：`int stats_sum(const int* a, int n)`（`n <= 0` 返 -1）。写四条裸 `assert` 用例，第三条故意断言成错的值。分两次跑：① **不设缓冲**，把输出重定向到文件再 cat——观察前两条 `[OK]` 是不是丢了，解释 stdout 全缓冲遇上 `abort()` 的坑；② 加 `setvbuf(stdout, NULL, _IONBF, 0)`，直接跑，观察输出顺序和执行到哪一条为止，贴退出码（是多少？这个数字怎么来的）。说出裸 `assert` 两个死穴（一条失败拖死全家、没夹具）各对应你观察到的哪一行。

[参考答案 →](homework-solutions#hw-4-7-a)

### 4.7-B {#hw-4-7-b}

难度 **L3** · 涉及[第 7 章：测试不再是 printf](/04-engineering/07-testing-with-unity)、[第 5 章](/04-engineering/05-cmake-engineering)

教材点名了一个静默坑——`enable_testing()` 必须写在所有 `add_test` 之前，否则 `add_test` 被忽略、不报错也不生效——但没有演示。这道题把它复现出来。写一个最小 CMake 工程：两个测试可执行 `t_pass`（`return 0`）和 `t_fail`（`return 1`），**先故意不写 `enable_testing()`** 就 `add_test` 两条，configure + build 后跑 `ctest --test-dir build --output-on-failure`，贴出那句「没测试」的真实输出；然后补上 `enable_testing()`（放对位置），重新跑 ctest，贴出一绿一红的输出和退出码；再演示 `ctest -R` 只跑红的那条、修好 `t_fail` 后全绿。CTest 判 pass/fail 的唯一依据是什么？用你这两条测试的退出码说清。

[参考答案 →](homework-solutions#hw-4-7-b)

## 4.8 Mock 与隔离

### 4.8-A {#hw-4-8-a}

难度 **L2** · 涉及[第 8 章：Mock 与隔离](/04-engineering/08-mock-and-isolation)

教材用 `time()` 演示函数指针表 mock，这里换场景。业务函数 `int avg_temp(int n, int* out)`：对温度传感器采样 `n` 次求平均，依赖抽成函数指针 `g_sensor_fn`（默认指向真实实现 `sensor_read_default`，返 25）。mock 版带「脚本」：按顺序返回 20、30、40，并记调用次数。测试里装上 mock、断言 `avg_temp(3)` 等于 30、调用次数等于 3；再调用一次 `avg_temp(1)` 断言次数涨到 4；最后恢复真实实现。贴输出，说出函数指针 mock 的代价（产品代码为可测性让了什么步）。

[参考答案 →](homework-solutions#hw-4-8-a)

### 4.8-B {#hw-4-8-b}

难度 **L3** · 涉及[第 8 章：Mock 与隔离](/04-engineering/08-mock-and-isolation)、[阶段 0 第 5 章：目标文件与符号](/00-dev-environment/06-object-files-and-symbols)

教材用 `--wrap=read` 演示了链接期替换，这里换方向——wrap 写。产品代码 `int logger_emit(const char* msg)`：直接调 `write(1, msg, strlen(msg))` 把一行日志写进 stdout，**一行都不为测试改动**。测试里定义 `__wrap_write`（签名必须和真实 `write` 完全一致）：记下被调次数、最后一次的 `len`，返回 `len`（假装写成功），**不真写**。断言被调 1 次、`len` 等于日志长度。编译链接加 `-Wl,--wrap,write`，跑通后用 `objdump -d` 把 `logger_emit` 里那个 `call` 的目标贴出来，证明它跳到的是 `__wrap_write` 而不是 libc 的 `write`。说出 `--wrap` 的代价（对读产品代码的人意味着什么）。

[参考答案 →](homework-solutions#hw-4-8-b)

## 4.9 gdb 实战

### 4.9-A {#hw-4-9-a}

难度 **L2** · 涉及[第 9 章：gdb 实战](/04-engineering/09-gdb-multi-thread)

写一个两线程程序：`worker_good` 只做纯 CPU 忙循环、**不碰**共享缓冲；`worker_bad` 在 round==3 时 `free` 掉全局 `shared_buf`、置 `NULL`、然后往里写（必然 SEGV，且凶手唯一确定）。用 gdb 的 **batch 模式**（`gdb -q -batch -ex ...`）跑出崩溃现场：`run` 后依次 `thread apply all bt`、`info threads`、`info locals`、`print shared_buf`。贴出 batch 输出，回答：崩在哪个线程、哪个函数哪一行？`shared_buf` 的值是多少？gdb 自动切过去的是哪个线程？说出 `thread apply all bt` 为什么是「多线程崩溃后第一件事」。

[参考答案 →](homework-solutions#hw-4-9-a)

### 4.9-B {#hw-4-9-b}

难度 **L4** · 涉及[第 9 章：gdb 实战](/04-engineering/09-gdb-multi-thread)、[阶段 0 第 9 章：标准与优化](/00-dev-environment/10-standards-and-optimization)

教材用「常量折叠的求和」演示 `-O2` 变量失踪，这里换一个更阴的：Collatz 步数。写 `static int collatz_steps(int n)`（偶数 $\frac{n}{2}$、奇数 $3n+1$，数到 1 的步数）和 `main`：`int start = 12; int steps = collatz_steps(start); int doubled = steps * 2; int result = doubled + 1;` 打印 result。分别用 `-O0` 和 `-O2` 编（都带 `-g`），各用 gdb batch 模式在打印行打断点：`print steps`、`print doubled`、`info locals`。贴两份对照输出：`-O0` 下都读得到，`-O2` 下哪个变量 `optimized out` 了？再用 `nm` 对比两个可执行文件里有没有 `collatz_steps` 符号，解释它去哪了。最后给 `steps` 加 `volatile` 重编 `-O2`，再 print 一次验证「强制留内存位置」的兜底，并说出 `volatile` 的代价。

[参考答案 →](homework-solutions#hw-4-9-b)

## 4.10 ASan+UBSan 深入

### 4.10-A {#hw-4-10-a}

难度 **L2** · 涉及[第 10 章：ASan+UBSan 深入](/04-engineering/10-sanitizer-deep)

照抄本仓 CI 的 sanitize 姿势（`CC=clang` + `CFLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g`），编三个最小含错程序：栈数组越界写（`a[8]`，数组只有 4 个）、use-after-free、`INT_MAX + 100` 与 `1 << 32`。每个跑一遍贴真实报告。回答三问：① 每个错分别是 UBSan 还是 ASan 抓的、退出码各是多少？② 为什么越界写那例 UBSan 报完程序**还继续跑**（recover 模式），而 UAF 那例报完进程就死？③ 把越界写那例换 gcc 编一遍再跑，贴输出——gcc 和 clang 报的东西有什么差别？这给了 CI 什么教训？

[参考答案 →](homework-solutions#hw-4-10-a)

### 4.10-B {#hw-4-10-b}

难度 **L4** · 涉及[第 10 章：ASan+UBSan 深入](/04-engineering/10-sanitizer-deep)

三道实验。① **作用域外用**：块作用域里声明 `int local[4] = {7, 8, 9, 10};`、把地址交给块外的指针 `p`，出块后 `printf("%d\n", *p)`。用 CI 那套 flags 编译运行，贴出报告并指出 shadow byte 里那个 `f8` 是什么含义、`-fsanitize-address-use-after-scope` 在新版 gcc/clang 上是不是默认开。② **ASAN_OPTIONS 三旋钮**：用 `ASAN_OPTIONS=help=1` 让 ASan 自报 `halt_on_error`/`abort_on_error`/`detect_leaks` 的当前值（贴真实输出）。③ **泄漏**：写一个 `malloc` 不 `free` 的小程序，`detect_leaks=1`（默认）跑贴报告和退出码，`detect_leaks=0` 跑贴退出码。说出 LSan 在什么受限环境会启动即挂、怎么处理（教材诚实标注过那条没贴伪造输出，你答原理即可）。

[参考答案 →](homework-solutions#hw-4-10-b)

## 4.11 valgrind 与 sanitizer 的分工

### 4.11-A {#hw-4-11-a}

难度 **L2** · 涉及[第 11 章：valgrind 与 sanitizer 的分工](/04-engineering/11-valgrind)

能力矩阵归类题，纸面作答。四类错误各该用谁主抓、为什么：① `malloc` 了一块没赋值就拿来判断；② `free` 之后又读；③ 两个线程无锁自增同一个变量；④ `INT_MAX + 1`。再答两问：⑤ sanitizer 和 valgrind 的根本形态差别是什么（插桩 vs 翻译、要不要重编、各多慢）？⑥ 为什么 MSan 能抓未初始化读、工程上却几乎没人用它全栈启用？

[参考答案 →](homework-solutions#hw-4-11-a)

### 4.11-B {#hw-4-11-b}

难度 **L3** · 涉及[第 11 章：valgrind 与 sanitizer 的分工](/04-engineering/11-valgrind)、[阶段 0 第 10 章：Sanitizer 门禁](/00-dev-environment/11-sanitizer-gate)

两道实验。① **ASan 抓不到的那一类**：写教材的 uninit 程序变式（`malloc` 一个 `int` 不赋值、拿去 `if (*p > 100)` 判断）。先带 `-fsanitize=address,undefined` 编译运行，贴退出码与「一声不吭」的事实；再用 valgrind 跑（**本机 Arch 的动态链接会被 strip 过的 ld-linux 卡住，按教材的 `-static` 绕法**），贴 memcheck 那句 `Conditional jump ...` 和它指的行号。如果你所在环境没装 valgrind，如实标注「未验证」，只答「valgrind 会报什么、为什么 ASan 报不了」。② **TSan 抓竞争**：两线程各做 50 万次 `counter++`，用 `gcc -fsanitize=thread` 编译运行，贴 data race 报告与退出码；解释为什么「这次 counter 恰好打出了期望值」也**不代表**程序是对的。

[参考答案 →](homework-solutions#hw-4-11-b)

## 4.12 静态分析门

### 4.12-A {#hw-4-12-a}

难度 **L1** · 涉及[第 12 章：静态分析门](/04-engineering/12-static-analysis)

五道判断题/填空题，纸面作答。① 判断题：clang-tidy 是编译时静态分析工具还是运行时工具？② 判断题：`-Wall -Wextra -Wpedantic` 抓不抓「双下划线开头的标识符」？它违反的是 ISO C 哪一条（条款号）？③ 填空：`long → int` 的隐式窄化在 ISO C 里是 `____`（undefined behavior 还是 implementation-defined）？④ cppcheck 和 clang-tidy 各强在哪一侧？⑤ `compile_commands.json` 给 clang-tidy 提供了什么、它一般怎么生成？

[参考答案 →](homework-solutions#hw-4-12-a)

### 4.12-B {#hw-4-12-b}

难度 **L3** · 涉及[第 12 章：静态分析门](/04-engineering/12-static-analysis)

写**一个** `.c` 文件，把教材的三类 finding 全装进去：① 一个双下划线开头的 typedef；② 一个缩进骗人的 dangling else（`if (flag) if (x > 0) return 1; else return 2;` 那种）；③ 一处 `long big = 1L << 40; int small = big;` 的隐式窄化。先用 `gcc -std=c11 -Wall -Wextra -Wpedantic -c` 编一遍——观察它**只对 dangling else 那处报警**（`-Wdangling-else` 收在 `-Wall` 里），对双下划线标识符和隐式窄化**一声不吭**（这正是「编译器够不着的角落」）；再用 `clang-tidy findings.c -- -std=c11` 跑，贴出全部 warning（几个 check 各报了什么？）；最后逐个修掉（改名去 `__`、补花括号、显式 `(int)` 转换），重跑 clang-tidy 贴干净输出。三处修法里哪一处按教材的说法是「legacy 妥协」？

[参考答案 →](homework-solutions#hw-4-12-b)

## 4.13 覆盖率门

### 4.13-A {#hw-4-13-a}

难度 **L2** · 涉及[第 13 章：覆盖率门](/04-engineering/13-coverage)

被测函数换成等级划分：`const char* grade_level(int score)`（`>= 90` 返 "A"、`>= 60` 返 "B"、其余 "C"）。第一版测试只断言 `strcmp(grade_level(95), "A") == 0` 这一条（注意：字符串内容比较要用 `strcmp`，直接 `==` 比的是指针地址——先想明白为什么），带 `--coverage -g -O0` 编译运行，跑 `gcov -b` 贴四个数字（Lines/Branches/Taken at least once/Calls）和 `.gcov` 里带 `#####` 的行；然后补上 `grade_level(70)` 和 `grade_level(30)` 两条用例，重新编译运行、再跑一次 `gcov -b`，贴数字。说出「行覆盖 100% 也不代表分支覆盖 100%」这句教材结论，你在本题哪个数字上亲眼看到了？

[参考答案 →](homework-solutions#hw-4-13-a)

### 4.13-B {#hw-4-13-b}

难度 **L3** · 涉及[第 13 章：覆盖率门](/04-engineering/13-coverage)

教材在讲 `stats_average` 时留了一个练习：「`Taken at least once` 还剩 1 路没单独走，是 `||` 右边 `n<=0` 那条单独短路，得再补一条『传非 NULL 但 `n=0`』的用例才彻底满」——这道题把它做完。重写教材的 `stats.c` 和测试，分三轮：① 只测 `{10,20,30}`（2 次调用）；② 加一条 `stats_average(NULL, 0) == -1`；③ 再加一条「非 NULL 但 `n=0`」的用例。每轮都带 `--coverage` 重新编跑、`gcov -b` 贴四个数字，观察 `Taken at least once` 从多少涨到多少。最后一轮把 `.gcov` 里 branch 那几行的 `taken` 百分比逐条抄下来，对照源码说清每个分支是怎么被两条新用例分别命中的。

[参考答案 →](homework-solutions#hw-4-13-b)

## 4.14 性能剖析

### 4.14-A {#hw-4-14-a}

难度 **L2** · 涉及[第 14 章：性能剖析](/04-engineering/14-profiling)

教材用 `sleep(1)` 证明了 `clock()` 量的是 CPU 时间，并在练习里留了一道「换成纯 CPU 循环」的题——这里把它做完。写一个程序，把 `sleep(1)` 和 `burn(300000000)`（`volatile` 累加防优化）各自包进 `clock()`、`CLOCK_MONOTONIC`、`CLOCK_PROCESS_CPUTIME_ID` 三种时钟里，打印两组的六个读数。贴输出，回答：I/O-bound（sleep）时三种读数关系如何、为什么？CPU-bound（burn）时呢？由此说出「先量化、再动手优化」里的「量化」第一性选择是什么。

[参考答案 →](homework-solutions#hw-4-14-a)

### 4.14-B {#hw-4-14-b}

难度 **L4** · 涉及[第 14 章：性能剖析](/04-engineering/14-profiling)

教材练习 2 的变式。写 `work_tree` 式程序（`main` → `worker_heavy`/`worker_light` → 叶子 `leaf_accumulate`），但把调用次数改成 **50 次和 5 次**（教材是 20/2），迭代数从 `argv` 进来（为什么必须 argv 驱动、写死会怎样，先答后跑）。用 `-O2 -fno-inline -pg` 编译跑 `gprof -b`，贴 flat profile 和 call graph：验证 `leaf_accumulate` 的 calls 是 55、self 100%，两个 worker 的 children 时间比例是否约等于 50:5。然后去掉 `-fno-inline` 重编重跑，贴那份「100% in main」的假报告，解释 profile 为什么被编译器骗了、`-fno-inline` 为什么只能用于剖析不能进发布。

[参考答案 →](homework-solutions#hw-4-14-b)

## 4.15 把质量门拼成流水线

### 4.15-A {#hw-4-15-a}

难度 **L2** · 涉及[第 15 章：把质量门拼成流水线](/04-engineering/15-ci-pipeline)

纸面作答。① 把本仓 ci.yml 六道 job（build-examples / sanitize / docs / format-check / static-analysis / coverage）各画一行「名字 → 裁决点（哪个脚本或命令的退出码）→ 硬门还是报告」，六行表。② `coverage` 为什么是报告？它里面哪一步其实是硬的？③ `KNOWN_LEGACY` 双模式说的是什么、`concurrency.cancel-in-progress` 解决什么问题？④ 照着教材「加新门三步法」给一个假想的 `cppcheck` 门写出三个步骤（不用真写脚本）。

[参考答案 →](homework-solutions#hw-4-15-a)

### 4.15-B {#hw-4-15-b}

难度 **L3** · 涉及[第 15 章：把质量门拼成流水线](/04-engineering/15-ci-pipeline)、[第 4 章](/04-engineering/04-make-deep)

照教材 `gate_echo.sh` 的思路写一个**你自己的** `gate.sh`，串三道本地门：① 编译门——`gcc -std=c11 -Wall -Wextra -Werror` 编一个被测模块；② 测试门——跑测试可执行、看退出码；③ sanitizer 门——`-fsanitize=address,undefined` 重编再跑。脚本靠「每道门的退出码汇总成 fail」最后 `exit $fail`。分两轮跑：第一轮测试可执行里埋一个故意挂的断言（第 ② 道门红），贴出完整输出和非 0 退出码；第二轮修掉断言，贴出 `fail=0`、退出码 0 的汇总。说明你的脚本里「什么叫失败」写在了哪里（对应教材说的「裁决逻辑写在脚本里」）。

[参考答案 →](homework-solutions#hw-4-15-b)

## 4.16 工程化毕业项目

### 4.16-A {#hw-4-16-a}

难度 **L1** · 涉及[第 16 章：工程化毕业项目](/04-engineering/16-capstone)

五道判断题/填空题，纸面作答。① 填空：收官章把 `examples/stage4-cmake-lib` 和 `projects/clib-utilities` 分别过六道门，其中第 ③ 步 sanitizer 在 clib 上当场抓到两个真 bug——UBSan 抓的是 `CCDynamicArray.c:203` 的什么、ASan 抓的是 `eraseSingle` 的什么？② 判断题：「ctest 2/2 Passed」能推出「代码没有内存 bug」吗？用 ① 的事例说明。③ 填空：`CCDynamicArray.c` 的行覆盖 baseline 是 `____%`，139 行里约跑了多少行？④ 判断题：clib 现在进了 CI 的 sanitizer 和 clang-tidy 硬门吗？为什么？⑤ 判断题：`consumer` 链 `mathlib_shared` 后 CMake 默认埋的 RUNPATH 是相对路径还是绝对路径、分发时该换什么？

[参考答案 →](homework-solutions#hw-4-16-a)

### 4.16-B {#hw-4-16-b}

难度 **L3** · 涉及[第 16 章：工程化毕业项目](/04-engineering/16-capstone)、[第 7 章](/04-engineering/07-testing-with-unity)、[第 10 章](/04-engineering/10-sanitizer-deep)

把收官章「ctest 全绿 ≠ 没 bug」的教训在小项目里亲手复现一遍。写一个定容 `int` 数组库（`arr_init`/`arr_free`/`arr_push`/`arr_erase`），`arr_erase` 挪元素的循环**故意写错边界**（读到了 `data[len]`，一字节出界）。配 mini_unity 四条用例（push 三个、erase 中间、erase 后内容正确、空数组 push/erase 边界），CMake + CTest 装配。第一阶段：ctest 全绿，贴「100% tests passed」；第二阶段：同一套代码用 CI 的 sanitizer flags 重编重跑，贴 ASan 报告（指出报错点名的函数和行号、`0 bytes after` 那句）；第三阶段：修掉循环边界，sanitizer 下重跑贴「全绿 + 退出 0」。为什么普通构建下越界读「看起来没事」？这题和教材 16 章 clib 的 `EraseSingle` 发现是同一种病，说出病名。

[参考答案 →](homework-solutions#hw-4-16-b)

## 4.C 跨章综合与挑战

### 4.C-1 {#hw-4-c-1}

难度 **L3** · 涉及[第 2 章](/04-engineering/02-api-and-opaque-types)、[第 3 章](/04-engineering/03-error-handling)、[第 7 章](/04-engineering/07-testing-with-unity)、[第 13 章](/04-engineering/13-coverage)

综合题：写一个定容字符串队列 `StrQueue`。要求把四章的手艺拧在一起：不透明类型（第 2 章）——头里只放 `typedef struct StrQueue StrQueue_t;` 和五个原型，字段藏 `.c`；context 错误对象（第 3 章）——`sq_push` 满、`sq_pop` 空时往 `error_ctx` 里写「码 + 人话」（人话要带出容量或当前长度），API 返成功/失败；mini_unity（第 7 章）——四条用例：FIFO 顺序、满时返 0 且 ctx 有错、空时 pop 返 0 且 ctx 有错、出队后 size 递减；gcov（第 13 章）——带 `--coverage` 编译跑测试，`gcov -b` 贴四个数字并指出哪条分支还是死的、为什么不补（写一句工程权衡）。贴编译、测试输出、gcov 数字三份真输出。

[参考答案 →](homework-solutions#hw-4-c-1)

### 4.C-2 {#hw-4-c-2}

难度 **L4** · 涉及[第 5 章](/04-engineering/05-cmake-engineering)、[第 6 章](/04-engineering/06-libs-and-linking)、[第 15 章](/04-engineering/15-ci-pipeline)

综合题：把一个动态库端到端「装出去、找回来、跑起来」。写 `libcalc`（`calc_add`/`calc_mul`），CMake 里 `add_library(calc SHARED ...)` 配 `SOVERSION 1`、`install(TARGETS ... EXPORT CalcTargets NAMESPACE Calc::)`、`BUILD_INTERFACE`/`INSTALL_INTERFACE` 头路径、`CMakePackageConfigHelpers` 生成 ConfigVersion 文件。用 `DESTDIR` 装到本地前缀，贴 `cmake --install` 输出（数清装了哪几类文件）。写消费者工程，`CMAKE_PREFIX_PATH` 指过去、`find_package(Calc 1.0 REQUIRED)`、链 `Calc::calc`，跑通贴输出。最后 `readelf -d` 看消费者的 RUNPATH，贴出来：它是相对还是绝对路径？按第 6 章的结论，分发时该用什么姿势改成可移植的（写出来即可，不用装第二次）？

[参考答案 →](homework-solutions#hw-4-c-2)

### 4.C-3 {#hw-4-c-3}

难度 **L5** · 涉及[第 8 章](/04-engineering/08-mock-and-isolation)、[第 7 章](/04-engineering/07-testing-with-unity)、[第 3 章](/04-engineering/03-error-handling)

挑战题（改编自 cmocka 的 `will_return` / `check_expected` 脚本队列语义，如实标注）。用第 7 章的 `setjmp`/`longjmp` 隔离手法 + 第 8 章的函数指针表，手写一个约百行的 mock 框架：一个 FIFO 队列存「脚本返回值」（`will_return(v)` 入队、mock 桩出队取回），另一个 FIFO 队列存「期望参数值」（`expect_value(v)` 入队、mock 桩出队比对）；队列空了或者参数对不上，就打印 FAIL 信息、`longjmp` 回框架、计一次失败、**继续跑下一条用例**。然后写一个业务函数 `poll_until_ok(int id, int tries)`（对依赖函数采样，非负即返回，否则重试，`tries` 次都失败返回 -1），mock 掉依赖。三条用例：① 脚本 `-1, -1, 7`、参数期望 3 个 42，断言 `poll_until_ok(42, 3) == 7` 且 mock 被调 3 次；② 三次都 `-1`，断言返回 -1；③ **故意**让期望参数写成 99，验证框架抓出「参数不符」、这条 FAIL 且不拖死下一条（贴出 `X Tests Y Failures` 的总账）。gcc 和 clang 双跑。

[参考答案 →](homework-solutions#hw-4-c-3)
