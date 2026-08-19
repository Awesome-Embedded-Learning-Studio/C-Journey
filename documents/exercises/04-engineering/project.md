---
title: "阶段 4 Project：clog 迷你日志库"
description: "工程化阶段综合项目：做一个带分级过滤、可插拔 sink 的迷你日志库 clog——头文件契约、函数指针 mock、Unity 测试、CMake 三态、sanitizer 与 clang-tidy 门、gcov 覆盖率，终极层加线程安全与 TSan 验证。任务分四层，难度 L1~L5。"
chapter: 4
order: 4
tags:
  - host
  - engineering
  - testing
  - cmake
difficulty: advanced
reading_time_minutes: 30
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4 全部章节"
related:
  - "阶段 4 Homework"
  - "阶段 4 Lab"
---

# 阶段 4 Project：clog 迷你日志库

## 项目定位

把阶段 4 的家当全部用进一个真实的库里：`clog`——一个带分级过滤、可插拔 sink 的迷你日志库。头文件契约（第 1 章）管住对外 API，函数指针（第 8 章）做成可 mock 的 sink，返回码与格式渲染（第 3 章 + 教材外的 `vsnprintf` 手艺）管住错误路径，mini_unity + CTest（第 7 章）把测试挂起来，CMake 三态（第 5 章）管住构建，sanitizer/clang-tidy/gcov（第 10、12、13 章）守质量门，终极层加线程安全。任务分四层，一层一层往上盖；卡住了看[参考实现](project-solutions)，它按层组织，可以只读你卡住的那层。

## 任务分层

### 核心任务（L2）：能跑起来的日志库 {#pj-core}

实现 `clog` 核心：四个级别（ERROR < WARN < INFO < DEBUG）、全局当前级别（默认 INFO，级别高于当前级别的日志被过滤掉）、sink 函数指针（默认 sink 打印 `[LEVEL] 消息\n` 到 stdout，`clog_set_sink(fn, userdata)` 可换，传 `NULL` 恢复默认）、`clog_log(level, fmt, ...)` 用 `vsnprintf` 渲染进栈上缓冲。文件布局 `include/clog.h` + `src/clog.c` + `demo.c`，配一个 Makefile（变量 + 模式规则 + `.PHONY`）。

先答三道 L1 判断题（写进 `notes.md`）：① 这个头文件需要 include guard 吗？为什么「需要」这个答案与「头文件被包含几次」有关？② 头里若放内联小函数要写 `static inline` 还是裸 `inline`？③ 判断题：`printf` 能不能直接写进库实现里（能），但为什么「默认打印到哪」这件事要做成可替换的 sink 而不是写死？

**验收标准**：`make` 全绿；demo 演示：INFO 级别下 ERROR/WARN/INFO 三条打得出来、DEBUG 被过滤；`clog_set_level(DEBUG)` 后 DEBUG 也出来；装一个只计数的自定义 sink，验证被调了。贴出 `make` 与一次完整 demo 的输出。

[参考实现 →](project-solutions#pj-core)

### 进阶任务（L3）：Unity 测试 + CTest {#pj-adv}

给 clog 挂真测试。写 `mini_unity.h/.c`（教材第 7 章的教学版）和 `test_clog.c`，五条用例，每条只测一个行为：① `clog_set_level`/`clog_get_level` 读写一致；② 级别高于当前级别时 sink 不被调（装 spy sink 计数）；③ 级别等于当前级别时 sink 被调且渲染正确（spy sink 把消息 `strcmp` 断言成期望字符串——注意 spy sink 的缓冲要在 `setUp` 里清零）；④ `clog_log(INFO, NULL, ...)` 不崩、sink 照常被调一次；⑤ `clog_set_sink(NULL, NULL)` 恢复默认后 `clog_get_sink` 相关状态正确。CMake 里 `enable_testing()` + `add_test`，`ctest --output-on-failure` 全绿。

**验收标准**：贴出 ctest 输出与 Unity 的 `5 Tests 0 Failures`；说明 spy sink 用到了哪一章的哪种 mock 手法、它和「真写 stdout」的测试各缺什么（mock 只验交互、不验真实打印）。

[参考实现 →](project-solutions#pj-adv)

### 再进阶任务（L4）：把门装上 {#pj-gates}

五件事，全绿才算过。① CMake 工程化：`add_library(clog STATIC ...)`，公开头 `include/` 标 PUBLIC、`-Wall -Wextra` 标 PRIVATE、`c_std_11` 标 PUBLIC、挂一个库自己不用但消费者能拿到的 `INTERFACE` 宏 `CLOG_LIB_VERSION=1`（从测试可执行的 `flags.make` 里 `grep` 出来贴证据）。② sanitizer 门：CI 的 flags（clang + `-fsanitize=address,undefined -fno-omit-frame-pointer -g`）重编重跑 ctest，零报告、退出 0。③ 静态分析门：配 `compile_commands.json`（`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`），对 `src/clog.c` 跑 `clang-tidy -p`，贴输出——干净的，如果有 finding 就修到干净。④ 格式门：`clang-format --dry-run --Werror` 查 `include/*.h src/*.c`，退出码 0。⑤ 覆盖率门：`--coverage -g -O0` 构建跑 ctest，`gcov -b src/clog.c` 贴四个数字；找出 `#####` 的死分支（提示：级别过滤的边界、sink 为 NULL 时的路径），补一两条用例，重跑贴涨起来的数字。

**验收标准**：五份证据（`flags.make` grep、sanitizer ctest 全绿、clang-tidy 输出、格式门退出码、前后两份 gcov 数字）。

[参考实现 →](project-solutions#pj-gates)

### 终极挑战（L5）：线程安全 + TSan 验证 {#pj-l5}

两件挑战。① **线程安全**（教材外补充：第 9 章只演示了多线程 gdb，`pthread_mutex` 等同步原语是阶段 5 的内容，这里提前用最小的一把锁）：`clog` 的全局状态（当前级别 + sink 指针）被多个线程同时读写会产生数据竞争——给 `clog_set_level`/`clog_set_sink`/`clog_log` 加一把 `pthread_mutex_t`。写一个 4 线程程序，每线程打 10000 条日志，用 `gcc -fsanitize=thread` 编译运行：贴出「零 data race 报告」的完整退出；再**故意把锁去掉**跑一次 TSan，贴出那份 data race 报告做对照（证明锁不是装饰）。② **spy sink 内容断言**：spy sink 把每条消息连同 `userdata` 透传记录下来，多线程各写各的名字，断言「收到的消息总数 == 4×10000、每个名字的消息条数 == 10000、所有消息都以期望格式开头」。

**验收标准**：贴出加锁版 TSan 零报告、去锁版 data race 报告、spy 统计断言输出；说出「TSan 抓的是竞争存在与否、不是结果对错」这句教材结论在本题哪里得到印证。

[参考实现 →](project-solutions#pj-l5)

## 提交物清单

项目目录（`include/`、`src/`、`tests/`、`Makefile`、`CMakeLists.txt`）+ 各层终端记录 + 200 字以内小结：说说这个项目里哪一处让你对「质量门是可信度脊柱」体会最深。
