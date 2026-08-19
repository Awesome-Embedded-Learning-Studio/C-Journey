---
title: "阶段 4 Lab 实验参考"
description: "阶段 4 Lab（给 statslib 配齐全套质量门）的实验参考：六步加 L5 挑战的逐步解答，每步标注知识点链接，含「ctest 全绿 → sanitizer 抓到真 UAF → 修复全绿」的完整复盘。所有输出在 WSL Arch（gcc 16.1.1 / clang 22.1.8）真实运行得到。"
chapter: 4
order: 3
tags:
  - host
  - engineering
  - testing
  - build
difficulty: intermediate
reading_time_minutes: 40
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 Lab 题面"
related:
  - "阶段 4 各章"
---

# 阶段 4 Lab 实验参考

> 所有输出在 WSL Arch（gcc 16.1.1 / clang 22.1.8）真实运行得到。建议卡住时先看「思路」逐步对照。本实验埋雷处有个诚实细节：编译器在步骤 2 就会用 `-Wuse-after-free` 提醒那颗雷的存在——这是线索不是判决，真正的判决在步骤 5。

## 步骤 1：工具链体检与头文件契约（L1）{#lab-1}

**思路**：五个版本号一次摸清；三问是第 1 章三条契约的「知不知道」。

1. 工具链版本（真实输出见下）。→ 知识点：[阶段 0 第 1 章：工具链体检](/00-dev-environment/01-toolchain-health-check)
2. 三问答：① guard 三行是 `#ifndef STATSLIB_H` / `#define STATSLIB_H` / 正文 / `#endif`；② ODR 一句话——每个非 inline 全局符号全工程只有一处定义，声明到处放、定义只一份；`int stats_mean(...);` 是**声明**（不分配存储、不生成代码），放头文件给各 `.c` 共享引用同一个唯一定义，恰恰是**遵守** ODR 的写法；③ `static inline`——裸 `inline` 不提供外部符号，调用方会撞 `undefined reference`。→ 知识点：[第 1 章：头文件契约](/04-engineering/01-header-contracts)（三契约）

**验证输出**：

```text
$ gcc --version | head -1
gcc (GCC) 16.1.1 20260728
$ clang --version | head -1
clang version 22.1.8
$ cmake --version | head -1
cmake version 4.4.2
$ gdb --version | head -1
GNU gdb (GDB) 17.2
$ valgrind --version
valgrind-3.25.1
```

## 步骤 2：Makefile 头依赖（L2）{#lab-2}

**思路**：`-MMD -MP` 进 `CFLAGS`、`DEPS := $(wildcard *.d)` + `-include` 喂回 make。注意一个当场就现身的诚实细节：**gcc 在编译期就用 `-Wuse-after-free` 提醒了 `stats_range` 里那颗雷**——`pointer 'tmp' used after 'free'`，指向 `statslib.c:43`。这是三道防线（编译器警告 → sanitizer → gdb）里第一道在说话，把它留在日志里，步骤 5 会回来对账。

1. 干净构建三件套命令跑通，demo 打印 `mean=25 range=30`。→ 知识点：[第 4 章：make 深处](/04-engineering/04-make-deep)（`-MMD -MP` 一对写死、`-include` 减号吞文件不存在错误）
2. `touch statslib.h` 后两个 `.o` 全重编——`statslib.d` 里写着 `statslib.o: statslib.c statslib.h`。→ 知识点：[第 4 章](/04-engineering/04-make-deep)「-MMD」一节

**验证输出**：

```text
$ make clean && make && ./demo
rm -f demo *.o *.d
gcc -std=c11 -Wall -Wextra -MMD -MP -c demo.c -o demo.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c statslib.c -o statslib.o
statslib.c: In function 'stats_range':
statslib.c:43:16: warning: pointer 'tmp' used after 'free' [-Wuse-after-free=]
   43 |     guard = tmp[0];
      |             ~~~^~~
statslib.c:38:5: note: call to 'free' here
   38 |     free(tmp);
      |     ^~~~~~~~~
gcc -std=c11 -Wall -Wextra -MMD -MP -o demo demo.o statslib.o
mean=25 range=30
$ touch statslib.h && make
gcc -std=c11 -Wall -Wextra -MMD -MP -c demo.c -o demo.o
gcc -std=c11 -Wall -Wextra -MMD -MP -c statslib.c -o statslib.o
gcc -std=c11 -Wall -Wextra -MMD -MP -o demo demo.o statslib.o
```

Makefile 全文（验收标准里那句「-MMD 替你记得」的完整版）：

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP

demo: demo.o statslib.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

DEPS := $(wildcard *.d)
-include $(DEPS)

.PHONY: clean
clean:
	rm -f demo *.o *.d
```

## 步骤 3：CMake 三态与多配置（L3）{#lab-3}

**思路**：`stats` 库的 `target_include_directories` 标 PUBLIC、`-Wall -Wextra` 标 PRIVATE、`c_std_11` 标 PUBLIC；两个 build 目录各配一个 `CMAKE_BUILD_TYPE`。

1. Debug `C_FLAGS = -g -Wall -Wextra`、Release `C_FLAGS = -O3 -DNDEBUG -Wall -Wextra`——`-Wall -Wextra` 是项目自己 PRIVATE 挂的、两边都在；`-g` 和 `-O3 -DNDEBUG` 是 CMake 按构建类型选的。→ 知识点：[第 5 章：CMake 工程化](/04-engineering/05-cmake-engineering)「多配置」一节
2. `NDEBUG` 由 Release 的 `-DNDEBUG` 带进来，把 `assert` 展开成空操作：Debug 版 `assert` 触发退出 134（128+SIGABRT），Release 版静默通过退出 0。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)、[阶段 0 第 9 章](/00-dev-environment/10-standards-and-optimization)

**验证输出**：

```text
$ grep '^C_FLAGS' build-dbg/CMakeFiles/stats.dir/flags.make
C_FLAGS = -g -Wall -Wextra
$ grep '^C_FLAGS' build-rel/CMakeFiles/stats.dir/flags.make
C_FLAGS = -O3 -DNDEBUG -Wall -Wextra
$ ./build-dbg/demo 2; echo "exit=$?"
demo: .../s3/demo.c:15: main: Assertion `x == 1 && "x 只能是 1"' failed.
exit=134
$ ./build-rel/demo 2; echo "exit=$?"
mean=25 range=30
exit=0
```

## 步骤 4：Unity 测试 + CTest（L3）{#lab-4}

**思路**：复刻教材第 7 章 ~30 行 mini_unity（`TEST_ASSERT_EQUAL_INT`/`TEST_ASSERT_TRUE` + `setjmp`/`longjmp` + `UNITY_BEGIN/END`），五条用例每个行为一条；CMake 里 `enable_testing()` 写在前、`add_test` 在后。

1. ctest `1/1 Passed`、退出 0；Unity 自己的总账 `5 Tests 0 Failures`。→ 知识点：[第 7 章：测试不再是 printf](/04-engineering/07-testing-with-unity)（第二级 Unity + 第三级 CTest）
2. `longjmp` 那一跳为什么「一条 FAIL 不拖死全家」：断言宏失败时 `longjmp(Unityjmp_buf, 1)` 跳回 `unity_run_test` 里 `setjmp` 的 else 分支——记一笔失败、`tearDown` 收尾、**函数正常返回**，`main` 里的下一条 `RUN_TEST` 照常调度。→ 知识点：同上

**验证输出**：

```text
$ ctest --test-dir build --output-on-failure
Test project /tmp/cj-ex4-lab/s4/build
    Start 1: stats_unity
1/1 Test #1: stats_unity ......................   Passed    0.00 sec
100% tests passed out of 1
Total Test time (real) =   0.00 sec
ctest exit=0
$ ./build/test_stats
  test_mean_normal ... PASS
  test_mean_empty ... PASS
  test_range_normal ... PASS
  test_range_single ... PASS
  test_range_empty ... PASS

5 Tests 0 Failures
```

五条用例都绿——但这句「都绿」不是终点。第 2 步编译器已经用 `-Wuse-after-free` 警告过 `statslib.c:43`，ctest 却全绿。这正是下一刀要验证的命题：**绿 ≠ 干净**。

## 步骤 5：sanitizer 抓到真 bug（L4）{#lab-5}

**思路**：用 CI 的姿势（`CC=clang` + 三件套 flags）重编重跑，ASan 的「已释放即毒化」让 `free` 之后那次读当场现形；修复后回绿，顺手过格式门。

1. sanitizer 下 ctest：`test_range_normal` 刚开跑，ASan 就甩 `heap-use-after-free`——`READ of size 4`、三段栈（非法读 → `freed by thread T0 here` → `previously allocated by thread T0 here`）、退出码 8。→ 知识点：[第 10 章：ASan+UBSan 深入](/04-engineering/10-sanitizer-deep)（三段式报告读法）
2. 修复：把 free 之后的「防御性校验」整个删掉（数据刚拷进副本、校验本无意义），`*out = hi - lo` 直接返回。重跑 ctest `1/1 Passed`。→ 知识点：[第 16 章：工程化毕业项目](/04-engineering/16-capstone)（clib 的 sanitizer 发现同款收尾）
3. 格式门：先 `clang-format -i` 归一化再 `--dry-run --Werror`，退出 0——LLVM 基底、4 空格缩进、左贴指针、转换后带空格（`(size_t) n`）。→ 知识点：[阶段 0 第 17 章：格式化与质量门](/00-dev-environment/18-format-and-quality-gate)

**验证输出**：

```text
$ ctest --test-dir build-asan --output-on-failure
1/1 Test #1: stats_unity ......................***Failed    0.06 sec
  test_mean_normal ... PASS
  test_mean_empty ... PASS
  test_range_normal ... =================================================================
==4344==ERROR: AddressSanitizer: heap-use-after-free on address 0x... thread T0
READ of size 4 at 0x... thread T0
    #0 ... (test_stats+0x1933bd)
0x... is located 0 bytes inside of 16-byte region [0x...,0x...)
freed by thread T0 here:
    #1 ... (test_stats+0x193347)
previously allocated by thread T0 here:
    #1 ... (test_stats+0x192bfa)
SUMMARY: AddressSanitizer: heap-use-after-free (test_stats+0x1933bd)
50% tests passed, 1 tests failed out of 1
ctest exit=8
$ # 删掉 free 之后的防御性校验
$ cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
1/1 Test #1: stats_unity ......................   Passed    0.07 sec
100% tests passed out of 1
ctest exit=0
$ clang-format -i statslib.h statslib.c mini_unity.h mini_unity.c test_stats.c
$ clang-format --dry-run --Werror statslib.h statslib.c mini_unity.h mini_unity.c test_stats.c
$ echo $?
0
```

sanitizer 是收官章「最值钱的一刀」：五条用例全绿的程序，ASan 一开就把那颗从步骤 2 一直躺到现在的雷炸出来。埋雷代码长这样（修复就是把 `free` 之后那三行删掉）：

```c
    free(tmp);
    /* 埋的雷:free 之后又读了一次 tmp[0]「做防御性校验」——
     * 读出来的值只进 volatile、被丢弃,所以普通构建下结果不受影响;
     * 但这次读本身是 use-after-free,ASan 会当场抓住。 */
    volatile int guard;
    guard = tmp[0];
    (void) guard;
    *out = hi - lo;
```

## 步骤 6：gcov 覆盖率（L4）{#lab-6}

**思路**：`-DCMAKE_C_FLAGS="--coverage -g -O0"` + `-DCMAKE_EXE_LINKER_FLAGS="--coverage"` 注入埋点；`gcov -b` 出四个数字。baseline 五条用例的死角：`lo = tmp[i]`（测试数据全升序，lo 从不更新）、`malloc` 失败分支。

1. baseline：`Lines 92.31% of 26`、`Branches 100% of 24`、`Taken at least once 70.83% of 24`——`#####` 钉在第 23 行 `return -1`（malloc 失败）和第 32 行 `lo = tmp[i];`。→ 知识点：[第 13 章：覆盖率门](/04-engineering/13-coverage)（gcov 读法与死行）
2. 补两条用例：`stats_mean(a, 2, NULL)` 救活 `!out` 分支、降序数据 `{40,30,20,10}` 救活 `lo = tmp[i]`。重跑：`Lines 96.15%`、`Taken 83.33%`。→ 知识点：[第 13 章](/04-engineering/13-coverage)（补用例→覆盖率涨的闭环）
3. 剩下的死行只剩第 23 行 `malloc` 失败——这条普通测试根本走不到，交给附加挑战。→ 知识点：同上（错误注入分支要靠故障注入，见 L5）

**验证输出**：

```text
$ gcov -b statslib.c.gcno
Lines executed:92.31% of 26
Branches executed:100.00% of 24
Taken at least once:70.83% of 24
Calls executed:100.00% of 3
$ grep -E '#####|taken 0%' statslib.c.gcov | head -8
branch  0 taken 0% (fallthrough)
    #####:   23:        return -1;
branch  0 taken 0% (fallthrough)
    #####:   32:            lo = tmp[i];
$ # 补 stats_mean(a, 2, NULL) 与降序 {40,30,20,10} 两条用例后
$ gcov -b statslib.c.gcno
Lines executed:96.15% of 26
Branches executed:100.00% of 24
Taken at least once:83.33% of 24
Calls executed:100.00% of 3
$ grep -E '#####|taken 0%' statslib.c.gcov | head -4
branch  0 taken 0% (fallthrough)
    #####:   23:        return -1;
```

## 附加挑战（L5）：malloc 失败注入 {#lab-l5}

**思路**：教材外补充——故障注入（fault injection）思路，源自 Test Double 那套「按脚本返回」的哲学；`--wrap` 本身见第 8 章。`__wrap_malloc` 平时透传 `__real_malloc`，`arm_malloc_failure()` 之后**下一次**调用返回 NULL。为什么「只失效一次」的设计不会误伤别的 `malloc`：注入窗口只有 `arm` 到下一次调用之间，测试只在这条缝隙里调 `stats_range`；窗口外的一切调用（包括 `printf` 内部的）都走 `__real_malloc` 原路。

1. 正常路径 30 → 注入后 -1 → 再测正常 30（失效一次性）→ `stats_mean` 不碰 malloc、注入对它无效。→ 知识点：[第 8 章：Mock 与隔离](/04-engineering/08-mock-and-isolation)（--wrap 与 `__real_` 透传）、[第 3 章](/04-engineering/03-error-handling)（返回码是断言对象）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -I. statslib.c wrap_malloc.c test_fail.c \
      -Wl,--wrap,malloc -o tf_gcc && ./tf_gcc
OK
exit=0
$ clang -std=c11 -Wall -Wextra -I. statslib.c wrap_malloc.c test_fail.c \
      -Wl,--wrap,malloc -o tf_clang && ./tf_clang
OK
exit=0
```

注入器全文：

```c
static int fail_next = 0;

void arm_malloc_failure(void) {
    fail_next = 1;
}

void* __real_malloc(size_t n);

void* __wrap_malloc(size_t n) {
    if (fail_next) {
        fail_next = 0;
        return NULL;
    }
    return __real_malloc(n);
}
```

测试里 `arm_malloc_failure(); assert(stats_range(a, 4, &out) == -1);` 这一句，把步骤 6 里那条 `#####` 的 `return -1` 分支救活了——覆盖率测不到的错误路径，故障注入能测到。这正是 L5 想让你看清的最后一课：**「防御性代码」不是写在那里就完了，每一行错误处理都该有一个让它真正执行一次的测试手段**。
