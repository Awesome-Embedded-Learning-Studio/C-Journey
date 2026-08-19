---
title: "阶段 4 Project 参考实现"
description: "阶段 4 综合项目（clog 迷你日志库）的完整参考实现：分层任务逐步讲解，每步标注知识点链接，含头文件契约、spy sink 测试、CMake 三态与 INTERFACE 宏、sanitizer/clang-tidy/格式/覆盖率四道门、以及 mutex 线程安全与 TSan 对照的真实运行输出。"
chapter: 4
order: 5
tags:
  - host
  - engineering
  - testing
  - cmake
difficulty: advanced
reading_time_minutes: 50
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 Project 题面"
related:
  - "阶段 4 各章"
---

# 阶段 4 Project 参考实现

> 全部输出在 WSL Arch（gcc 16.1.1 / clang 22.1.8）真实运行得到。参考实现只是**一种**过关方式；你的实现不一样、验收标准对得上，就都是对的。注意本项目沿用本仓 `.clang-format`（LLVM 基底、4 空格缩进、左贴指针、转换后带空格如 `(size_t) n`），照抄时别改掉。

## 核心任务（L2）：能跑起来的日志库 {#pj-core}

**思路**：四级枚举 + 全局当前级别 + 函数指针 sink；`clog_log` 用 `vsnprintf` 渲染进 512 字节栈缓冲，超长打截断标记。头文件契约：guard、`static inline` 纪律、以及「默认打印到哪」做成可替换 sink。

**先答三道 L1 判断题（notes.md）**：① 需要——头文件可能被同一个翻译单元间接包含多次（你的 `.c` 和 `.h` 各自 include 它），没有 guard 第二遍包含时类型重定义；② `static inline`——裸 `inline` 是 C99 的雷，链接期 `undefined reference`；③ `printf` 能写进库实现（`.c` 里随便用），但「默认打到哪里」写死成 stdout 会让库的消费者无法换输出目的地，做成函数指针 sink（第 8 章的 mock 手艺顺手就来了）才谈得上测试与复用。→ 知识点：[第 1 章：头文件契约](/04-engineering/01-header-contracts)、[第 8 章](/04-engineering/08-mock-and-isolation)（sink 本质是依赖钩子）

**`include/clog.h`**——对外契约全部家当：枚举、sink 函数指针类型、四个函数。→ 知识点：[第 1 章](/04-engineering/01-header-contracts)（头文件只放声明）

```c
#ifndef CLOG_H
#define CLOG_H

typedef enum {
    CLOG_LEVEL_ERROR = 0,
    CLOG_LEVEL_WARN = 1,
    CLOG_LEVEL_INFO = 2,
    CLOG_LEVEL_DEBUG = 3,
} clog_level_t;

typedef void (*clog_sink_fn)(clog_level_t level, const char* message,
                             void* userdata);

void clog_set_level(clog_level_t level);
clog_level_t clog_get_level(void);
void clog_set_sink(clog_sink_fn sink, void* userdata); /* NULL 恢复默认 */
void clog_log(clog_level_t level, const char* fmt, ...);

#endif
```

**`src/clog.c`**——默认级别 INFO、默认 sink 打印 `[LEVEL] 消息`；`level > g_level` 直接过滤；`vsnprintf` 渲染 + 截断标记。→ 知识点：[第 3 章](/04-engineering/03-error-handling)（返回码/约定）、[第 8 章](/04-engineering/08-mock-and-isolation)（`g_sink` 函数指针）

```c
#include "clog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static clog_level_t g_level = CLOG_LEVEL_INFO;
static clog_sink_fn g_sink = NULL;
static void* g_userdata = NULL;

static const char* level_name(clog_level_t level) {
    switch (level) {
    case CLOG_LEVEL_ERROR:
        return "ERROR";
    case CLOG_LEVEL_WARN:
        return "WARN";
    case CLOG_LEVEL_INFO:
        return "INFO";
    case CLOG_LEVEL_DEBUG:
        return "DEBUG";
    default:
        return "?";
    }
}

static void default_sink(clog_level_t level, const char* message,
                         void* userdata) {
    (void) userdata;
    fprintf(stdout, "[%s] %s\n", level_name(level), message);
}

void clog_set_level(clog_level_t level) {
    g_level = level;
}

clog_level_t clog_get_level(void) {
    return g_level;
}

void clog_set_sink(clog_sink_fn sink, void* userdata) {
    g_sink = sink;
    g_userdata = userdata;
}

void clog_log(clog_level_t level, const char* fmt, ...) {
    if (level > g_level) {
        return; /* 级别过滤 */
    }
    char buf[512];
    va_list ap;
    int need = 0;
    va_start(ap, fmt);
    if (fmt == NULL) {
        snprintf(buf, sizeof(buf), "(null)");
        need = (int) strlen(buf);
    } else {
        need = vsnprintf(buf, sizeof(buf), fmt, ap);
    }
    va_end(ap);
    if (need >= (int) sizeof(buf)) {
        strcpy(buf + sizeof(buf) - 4, "..."); /* 截断标记 */
    }

    clog_sink_fn sink = (g_sink != NULL) ? g_sink : default_sink;
    sink(level, buf, g_userdata);
}
```

**`demo.c`**——分级、过滤、可插 sink 三件事一次演完。→ 知识点：[第 8 章](/04-engineering/08-mock-and-isolation)（userdata 透传）

```c
static void counting_sink(clog_level_t level, const char* message,
                          void* userdata) {
    int* count = (int*) userdata;
    (void) level;
    (void) message;
    (*count)++;
}

int main(void) {
    clog_log(CLOG_LEVEL_ERROR, "disk full");
    clog_log(CLOG_LEVEL_WARN, "low memory");
    clog_log(CLOG_LEVEL_INFO, "connected to %s:%d", "localhost", 8080);
    clog_log(CLOG_LEVEL_DEBUG, "packet dump"); /* 默认 INFO 下被过滤 */

    printf("--- 开到 DEBUG ---\n");
    clog_set_level(CLOG_LEVEL_DEBUG);
    clog_log(CLOG_LEVEL_DEBUG, "packet dump");

    printf("--- 自定义计数 sink ---\n");
    int count = 0;
    clog_set_sink(counting_sink, &count);
    clog_log(CLOG_LEVEL_INFO, "hello sink");
    clog_log(CLOG_LEVEL_WARN, "warning sink");
    printf("sink called %d times\n", count);

    clog_set_sink(NULL, NULL); /* 恢复默认 */
    clog_log(CLOG_LEVEL_ERROR, "bye");
    return 0;
}
```

**`Makefile`**——变量 + 模式规则 + `-MMD -MP` + `.PHONY`。→ 知识点：[第 4 章：make 深处](/04-engineering/04-make-deep)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP -Iinclude

demo: demo.o src/clog.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

DEPS := $(wildcard *.d src/*.d)
-include $(DEPS)

.PHONY: clean
clean:
	rm -f demo *.o src/*.o *.d src/*.d
```

**验证输出**：

```text
$ make && ./demo
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -c demo.c -o demo.o
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -c src/clog.c -o src/clog.o
gcc -std=c11 -Wall -Wextra -MMD -MP -Iinclude -o demo demo.o src/clog.o
[ERROR] disk full
[WARN] low memory
[INFO] connected to localhost:8080
--- 开到 DEBUG ---
[DEBUG] packet dump
--- 自定义计数 sink ---
sink called 2 times
[ERROR] bye
```

验收对照：INFO 级别下 ERROR/WARN/INFO 三条出得来、DEBUG 被过滤（`packet dump` 只在开到 DEBUG 后出现）；自定义 sink 被调 2 次；恢复默认后 `bye` 照常打印。

## 进阶任务（L3）：Unity 测试 + CTest {#pj-adv}

**思路**：spy sink 是第 8 章「函数指针表 mock」的直接应用——它不真写 stdout，只记「被调了几次、消息是什么」，测试断言的是**交互**而不是屏幕输出；代价是 mock 只验交互、不验真实打印（真实打印那条路有 demo 兜着，这就是「mock 测多、集成测少」）。mini_unity 照第 7 章教学版复刻（`setjmp`/`longjmp` 隔离 + `setUp`/`tearDown` 清 spy 状态）。

**`tests/test_clog.c`**（五条用例，每条一个行为）→ 知识点：[第 7 章：测试不再是 printf](/04-engineering/07-testing-with-unity)、[第 8 章](/04-engineering/08-mock-and-isolation)

```c
static void spy_sink(clog_level_t level, const char* message, void* userdata) {
    (void) userdata;
    g_calls++;
    g_last_level = level;
    snprintf(g_msg, sizeof(g_msg), "%s", message);
}

void setUp(void) {
    g_calls = 0;
    g_msg[0] = '\0';
    g_last_level = CLOG_LEVEL_ERROR;
    clog_set_level(CLOG_LEVEL_INFO);
    clog_set_sink(spy_sink, NULL);
}

void tearDown(void) {
    clog_set_sink(NULL, NULL);
}

static void test_level_get_set(void) {
    clog_set_level(CLOG_LEVEL_DEBUG);
    TEST_ASSERT_EQUAL_INT(CLOG_LEVEL_DEBUG, (int) clog_get_level());
    clog_set_level(CLOG_LEVEL_WARN);
    TEST_ASSERT_EQUAL_INT(CLOG_LEVEL_WARN, (int) clog_get_level());
}

static void test_filter_blocks_debug(void) {
    clog_log(CLOG_LEVEL_DEBUG, "should not appear");
    TEST_ASSERT_EQUAL_INT(0, g_calls); /* 高于当前级别的被过滤 */
}

static void test_message_rendered(void) {
    clog_log(CLOG_LEVEL_INFO, "hello %d", 42);
    TEST_ASSERT_EQUAL_INT(1, g_calls);
    TEST_ASSERT_EQUAL_INT(CLOG_LEVEL_INFO, (int) g_last_level);
    TEST_ASSERT_EQUAL_INT(0, strcmp(g_msg, "hello 42"));
}

static void test_null_fmt_safe(void) {
    clog_log(CLOG_LEVEL_INFO, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_calls);
    TEST_ASSERT_EQUAL_INT(0, strcmp(g_msg, "(null)"));
}

static void test_sink_reset(void) {
    clog_set_sink(NULL, NULL); /* 恢复默认,spy 失效 */
    g_calls = 0;
    clog_log(CLOG_LEVEL_INFO, "x");
    TEST_ASSERT_EQUAL_INT(0, g_calls);
}
```

**`CMakeLists.txt`**——`enable_testing()` 在 `add_test` 之前（第 7 章的静默坑），库三态照第 5 章。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)

```cmake
add_library(clog STATIC src/clog.c)
target_include_directories(clog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(clog PRIVATE -Wall -Wextra)
target_compile_features(clog PUBLIC c_std_11)
target_compile_definitions(clog INTERFACE CLOG_LIB_VERSION=1)

add_library(mini_unity STATIC tests/mini_unity.c)
target_include_directories(mini_unity
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()

add_executable(test_clog tests/test_clog.c)
target_link_libraries(test_clog PRIVATE clog mini_unity)
add_test(NAME clog_unity COMMAND test_clog)
```

**验证输出**：

```text
$ ctest --test-dir build --output-on-failure
Test project /tmp/cj-ex4-proj/build
    Start 1: clog_unity
1/1 Test #1: clog_unity .......................   Passed    0.00 sec
100% tests passed out of 1
ctest exit=0
```

## 再进阶任务（L4）：把门装上 {#pj-gates}

**思路**：五份证据逐一过关。INTERFACE 宏的证据在测试可执行的 `flags.make` 里；clang-tidy 用本仓 `.clang-tidy` 三族配置 + `-p compile_commands.json`；格式门先 `-i` 归一化再 `--dry-run --Werror`；覆盖率两轮对比。

1. INTERFACE 宏：`grep CLOG build/CMakeFiles/test_clog.dir/flags.make` → `C_DEFINES = -DCLOG_LIB_VERSION=1`——库自己的 `.c` 从来没用过它，它专门传给消费者。→ 知识点：[第 5 章](/04-engineering/05-cmake-engineering)（INTERFACE 传染给上游）
2. sanitizer 门：CI flags 重编重跑 ctest，`1/1 Passed`、退出 0。→ 知识点：[第 10 章：ASan+UBSan 深入](/04-engineering/10-sanitizer-deep)
3. 静态分析门：clang-tidy 报告「34 warnings generated. Suppressed 34 warnings (34 in non-user code)」——34 条全在系统头里被 `HeaderFilterRegex` 压掉，`src/clog.c` 零 finding。→ 知识点：[第 12 章：静态分析门](/04-engineering/12-static-analysis)
4. 格式门：`clang-format --dry-run --Werror` 退出 0。→ 知识点：[阶段 0 第 17 章](/00-dev-environment/18-format-and-quality-gate)
5. 覆盖率门：baseline 五条用例 `Lines 76.32% of 38`、`Taken 61.54% of 13`——死的全是 `level_name` 的 ERROR/WARN/DEBUG/default 四个 case（默认 sink 只有 demo 用、测试全走 spy，`level_name` 基本没被执行）。补三条用例（ERROR 边界过滤、超长截断、恢复默认 sink 走全五个 case 含非法级别 99）后 `Lines 100.00%`、`Taken 100.00%`。→ 知识点：[第 13 章：覆盖率门](/04-engineering/13-coverage)（分支覆盖比行覆盖狠；补用例→数字涨）

**验证输出**：

```text
$ grep CLOG build/CMakeFiles/test_clog.dir/flags.make
C_DEFINES = -DCLOG_LIB_VERSION=1
$ CC=clang CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  LDFLAGS="-fsanitize=address,undefined" cmake -B build-asan && cmake --build build-asan
$ ctest --test-dir build-asan --output-on-failure
1/1 Test #1: clog_unity .......................   Passed    0.07 sec
100% tests passed out of 1
ctest exit=0
$ clang-tidy -p build-tidy src/clog.c
34 warnings generated.
Suppressed 34 warnings (34 in non-user code).
$ clang-format --dry-run --Werror include/clog.h src/clog.c tests/*.c tests/*.h demo.c
$ echo $?
0
$ gcov -b clog.c.gcno    # baseline:5 条用例
Lines executed:76.32% of 38
Branches executed:100.00% of 13
Taken at least once:61.54% of 13
Calls executed:100.00% of 3
$ grep -E '#####' clog.c.gcov | head -5
    #####:   13:    case CLOG_LEVEL_ERROR:
    #####:   15:    case CLOG_LEVEL_WARN:
    #####:   19:    case CLOG_LEVEL_DEBUG:
    #####:   21:    default:
$ gcov -b clog.c.gcno    # 补 3 条用例后
Lines executed:100.00% of 38
Branches executed:100.00% of 13
Taken at least once:100.00% of 13
Calls executed:100.00% of 3
```

补的那条「走全五个 case」的用例长这样（注意非法级别要先 `clog_set_level` 抬到 99，否则 `level > g_level` 会被过滤掉、`default` 分支永远走不到）：

```c
static void test_default_sink_levels(void) {
    clog_set_sink(NULL, NULL); /* 恢复默认 sink,走 level_name 全部分支 */
    clog_set_level(CLOG_LEVEL_DEBUG);
    clog_log(CLOG_LEVEL_ERROR, "err line");
    clog_log(CLOG_LEVEL_WARN, "warn line");
    clog_log(CLOG_LEVEL_INFO, "info line");
    clog_log(CLOG_LEVEL_DEBUG, "debug line");
    clog_set_level((clog_level_t) 99);
    clog_log((clog_level_t) 99, "unknown"); /* 非法级别 → default 分支 */
    TEST_ASSERT_EQUAL_INT(99, (int) clog_get_level());
}
```

## 终极挑战（L5）：线程安全 + TSan 验证 {#pj-l5}

**思路**：教材外补充——`pthread_mutex_t` 等同步原语是阶段 5 的内容，第 9 章只演示了多线程 gdb；这里提前用最小的一把锁。全局状态（`g_level`/`g_sink`/`g_userdata`）的所有读写都放进锁里；`clog_log` **持锁调用 sink**——spy sink 的计数因此也是串行化的，测试里不需要额外的原子操作。

**加锁后的 `clog_log`/setter**（相对核心层的全部改动）：

```c
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void clog_set_level(clog_level_t level) {
    pthread_mutex_lock(&g_lock);
    g_level = level;
    pthread_mutex_unlock(&g_lock);
}

void clog_set_sink(clog_sink_fn sink, void* userdata) {
    pthread_mutex_lock(&g_lock);
    g_sink = sink;
    g_userdata = userdata;
    pthread_mutex_unlock(&g_lock);
}

void clog_log(clog_level_t level, const char* fmt, ...) {
    pthread_mutex_lock(&g_lock);
    if (level > g_level) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    /* ... 渲染进 buf(与核心层相同,略) ... */
    clog_sink_fn sink = (g_sink != NULL) ? g_sink : default_sink;
    sink(level, buf, g_userdata); /* 持锁调用 sink:spy 计数也安全 */
    pthread_mutex_unlock(&g_lock);
}
```

**多线程内容断言（`mt_test.c` 要点）**：4 个线程各打 10000 条、消息以本线程字母开头；sink 按 `message[0]` 分桶计数，主线程 join 后断言每桶 10000、总数 40000、坏格式 0。→ 知识点：[第 8 章](/04-engineering/08-mock-and-isolation)（spy 断言交互）、教材外补充（mutex）

**验证输出**：

```text
$ gcc -std=c11 -Wall -Wextra -g -pthread -fsanitize=thread -fno-omit-frame-pointer \
      -Iinclude src/clog.c mt_test.c -o mt_locked
$ ./mt_locked; echo "exit=$?"
thread A: 10000 messages
thread B: 10000 messages
thread C: 10000 messages
thread D: 10000 messages
total=40000 bad=0
OK
exit=0
```

TSan 一声不吭、退出 0——加锁版零 data race。现在**把锁去掉**（`clog_nolock.c`，全局状态裸读写），再配一个并发写级别的 flipper 线程（反复 `clog_set_level`）跑对照：

```text
$ gcc -std=c11 -Wall -Wextra -g -pthread -fsanitize=thread -fno-omit-frame-pointer \
      -Iinclude src/clog.c race_demo.c -o race_locked
$ ./race_locked; echo "exit=$?"        ← 加锁版:TSan 静默
exit=0
$ gcc ... -Iinclude src/clog_nolock.c race_demo.c -o race_nolock
$ ./race_nolock
==================
WARNING: ThreadSanitizer: data race (pid=5398)
  Read of size 4 at 0x... by thread T2:
    #0 clog_log src/clog_nolock.c:46 (race_nolock+0x141a)
    #1 logger .../race_demo.c:17
  Previous write of size 4 at 0x... by thread T1:
    #0 clog_set_level src/clog_nolock.c:33 (race_nolock+0x12f5)
    #1 flipper .../race_demo.c:8
  Location is global 'g_level' of size 4 at 0x... (race_nolock+0x4080)
SUMMARY: ThreadSanitizer: data race src/clog_nolock.c:46 in clog_log
==================
exit=66
```

加锁版 TSan 零报告、去锁版当场报 `g_level` 上的 data race——锁不是装饰。「TSan 抓的是竞争存在与否、不是结果对错」在哪印证：去锁版这次跑下来**输出照样整整齐齐**（每条消息都渲染了），但竞争是事实，退出码 66 把它钉死。这句教材结论在 clog 的全局状态上再演了一遍。
