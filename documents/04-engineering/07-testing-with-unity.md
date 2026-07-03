---
title: "测试不再是 printf:assert、Unity 与 CTest 装配"
description: "很多 C 项目对「测试」的理解,就是 main 里一锅操作 + 一堆 printf、然后人眼对一下输出对不对——这根本不是测试,这是「演示」。这一章把 C 工程挂真测试的三级跳逐级真跑:第一级裸 assert(<assert.h>),最快但最粗——一条失败就 abort 整个程序、后面全跑不到、没 setUp/tearDown 隔离,真跑一个故意失败的用例看它怎么 SIGABRT(还顺手揭开「printf 被 abort 吞掉」的 stdout 缓冲坑)。第二级 Unity 纯 C 单测框架——TEST_ASSERT_* 断言宏 + setjmp/longjmp 每例隔离(一条 FAIL 不影响下一条)+ setUp/tearDown 每例重建夹具 + RUN_TEST/UNITY_BEGIN/END 跑完报『X Tests Y Failures』;我照着 projects/clib-utilities/test/unity/ 的极简 Unity shim 亲手复刻一份(~30 行的 mini_unity.h/.c),逐行讲 setjmp/longjmp 怎么做到「FAIL 不 abort、继续下一条」,并以 clib 的 test_DynamicArray_unity.c(5 条真迁过来的用例:push 计数、find 命中/缺失、erase 缩减)当「从 ad-hoc printf 迁成断言用例」的活样板;诚实标注 clib 用的只是教学子集,真 project 应 vendor ThrowTheSwitch/Unity 全量三件。第三级 CTest 装配——enable_testing() + add_test 把每条 test exe 挂上、ctest 命令逐个跑报红绿,并讲 build_examples.py 怎么自动发现 CTestTestfile.cmake 跑测试(L60-66 的 CTest 发现逻辑),clib 现在有 clib_smoke + dynamic_array_unity 两条 ctest(主控已验证 2/2 Passed)。承接第 3 章错误处理(返回码是测试断言的基础),为第 8 章 Mock 铺路(测有依赖的代码)。全 gcc16+clang22 双跑,贴真实输出。"
chapter: 4
order: 7
tags:
  - host
  - testing
  - engineering
  - toolchain
difficulty: intermediate
reading_time_minutes: 17
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4·第 3 章:错误处理三件套(返回码是测试断言的对象——函数返 -1/NULL,测试就 assert 它)"
  - "阶段 4·第 5 章:CMake 工程化(enable_testing/add_test 的构建系统基础)"
  - "阶段 0·第 1 章:工具链体检(gcc/clang 双跑纪律)"
related:
  - "阶段 4·第 8 章:Mock 与隔离(本章只测无依赖的纯函数,有依赖的要 Mock)"
  - "阶段 0·第 8 章:警告旗标(-Werror 让「漏查返回码」的隐患提前现形,测试再兜一道)"
  - "阶段 0·第 10 章:Sanitizer 门禁(测试抓逻辑错、ASan 抓内存错,互补)"
---

# 测试不再是 printf:assert、Unity 与 CTest 装配

## 引言:printf 不叫测试,叫演示

很多人写完一段 C 代码,「验证它对不对」的办法是这样的:在 `main` 里把刚写的函数调一遍,结果 `printf` 出来,人眼瞄一下数字对不对,觉得对了就算「测过了」。这不叫测试,这叫**演示**——它没有任何东西在「自动判定」对错,判对错的是你的人眼和脑子,而人眼和脑子是最不可靠的:跑一遍你瞄一眼觉得「差不多」,把 `expected 20` 看成 `got 20` 就过去了,实际可能根本不对;更要命的是,等你下次改了代码再跑,你压根不记得上次人眼判的是哪些用例,「回归」无从谈起。

真测试得有两样东西:一是**断言**(assertion)——程序自己判「期望值 == 实际值」,不等就报错、绝不靠人眼;二是**隔离**——一条用例失败,不能把整个测试程序拖死,后面的用例得照常跑,最后给你一份「几条过、几条挂、挂在哪」的总账。这两样 printf 都给不了你。

这一章把 C 工程挂真测试的**三级跳**逐级真跑给你看,每级都比上一级强一档:**第一级**用标准库 `<assert.h>` 的裸 `assert`,最快但最粗——一条失败就 `abort` 整个程序,后面全跑不到、也没隔离;**第二级**上 Unity 这个纯 C 单测框架,`setjmp`/`longjmp` 做到「一条 FAIL 不影响下一条」,还有 `setUp`/`tearDown` 每例重建夹具;**第三级**用 CTest 把一堆测试可执行文件「挂」起来,`ctest` 一条命令逐个跑、报红绿,CI 里自动发现。被测对象我故意选个最小的——一个求平均值的 `stats_average`,这样三级用的都是同一个被测函数,差异全在「测试这层」怎么搭,对照最干净。

顺便说一句承接:第 3 章我们花一整章讲「错误处理三件套」,核心是函数出错时返个返回码(`-1`/`NULL`)给调用者——**测试断言的左边往往就是这个返回码**(`TEST_ASSERT_EQUAL_INT(-1, stats_average(NULL, 0))`),返回码约定是测试能写出期望值的前提。而本章只测 `stats_average` 这种「无依赖的纯函数」(它只读入参、不调别的模块);等你代码开始调外部依赖(文件系统、网络、别的模块),就得用 Mock 把依赖替换掉再测——那是下一章(第 8 章)的事。

## 被测对象:一个故意留了坑的 stats_average

三级测试都围绕同一个被测函数。它求 `n` 个 `int` 的平均值,空数组返 `-1` 表示失败:

```c
/* stats.h —— 被测模块:一个求平均值的工具函数(故意留个除零的坑) */
#ifndef STATS_H
#define STATS_H

/* 求 n 个 int 的平均值,失败(空数组)返 -1。
 * 故意用 int 返回、内部 sum 用 int:溢出/除零是后面测试要抓的 bug。 */
int stats_average(const int* a, int n);
#endif
```

```c
/* stats.c —— 被测模块实现 */
#include "stats.h"

int stats_average(const int* a, int n) {
    if (n == 0) {
        return -1; /* 空 → 失败返回码 */
    }
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += a[i];
    }
    return sum / n;
}
```

实现没什么花活,关键看它**对外暴露的契约**:成功返平均值、空数组返 `-1`。测试要验的就是「这俩契约成不成立、各种入参下算得对不对」。

## 第一级:裸 assert,最快也最脆

C 标准库 `<assert.h>` 给你一个 `assert(expr)` 宏:`expr` 为假时,它打印一条 `Assertion '...' failed` 到 `stderr`、然后调 `abort()` 把进程干掉(POSIX 下 `abort()` 给进程发 `SIGABRT`,参见 `abort(3)`)。这是 C 语言自带的、唯一的「断言」原语,零依赖、零配置,`#include <assert.h>` 就能用。我拿它给 `stats_average` 写几条用例:

```c
/* test_assert.c —— 第一级:裸 assert(<assert.h>)测 stats_average。
 * 缺点当场现形:一条失败就 abort 整个程序,后面的用例根本跑不到。 */
#include <assert.h>
#include <stdio.h>

#include "stats.h"

/* stdout 默认行缓冲;一旦被重定向(管道/文件)就变全缓冲,
 * abort 时没刷掉的缓冲会丢——这里强制无缓冲,保证输出顺序=执行顺序。 */

static void test_avg_three_elems(void) {
    int a[] = {10, 20, 30};
    int got = stats_average(a, 3);
    /* 正确答案是 20,这条会过 */
    assert(got == 20);
    printf("[OK] avg(10,20,30) = %d\n", got);
}

static void test_avg_empty_returns_fail(void) {
    int got = stats_average(NULL, 0);
    /* 空数组返回 -1,这条也会过 */
    assert(got == -1);
    printf("[OK] avg(empty) = %d (失败码)\n", got);
}

static void test_avg_deliberately_wrong(void) {
    int a[] = {2, 4, 6, 8};
    int got = stats_average(a, 4);
    /* 正确答案是 5,这里故意断言成 99 → 失败 → abort! */
    assert(got == 99);
    /* 这行永远跑不到(assert 失败就 abort 了) */
    printf("[本行不会执行] got = %d\n", got);
}

static void test_avg_should_run_after(void) {
    /* 这条用例想验证「负数也行」,但因为上一条 abort,根本没机会跑。 */
    int a[] = {-3, -6, 9};
    int got = stats_average(a, 3);
    assert(got == 0);
    printf("[OK] avg(-3,-6,9) = %d\n", got);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== 用 assert 测试 stats_average ===\n");
    test_avg_three_elems();
    test_avg_empty_returns_fail();
    test_avg_deliberately_wrong(); /* 在这里 abort */
    test_avg_should_run_after();   /* 跑不到 */
    printf("=== 全部通过 ===\n");  /* 跑不到 */
    return 0;
}
```

注意我在 `main` 第一行塞了个 `setvbuf(stdout, NULL, _IONBF, 0)`——这不是炫技,是被一个真实的坑逼出来的。第一版我没加这行,gcc 跑出来只看到那行 `Assertion ... failed`,**前两条 `[OK]` 的 printf 凭空消失了**。原因是 `stdout` 默认是**行缓冲**(遇到 `\n` 才刷出去),可一旦它的输出被重定向到管道或文件(比如 CI 里 `./test > log.txt`),glibc 会把它切成**全缓冲**——`printf` 的内容先攒在内存缓冲区里,要等缓冲区满或程序正常退出才刷。结果 `abort()` 一棒子把进程打死,缓冲区里没刷掉的内容就跟着陪葬了。这是「printf + abort」组合的隐性坑:你以为打了的日志根本没出来,排查时人会一头雾水。`setvbuf` 强制无缓冲(`_IONBF`),让每条 `printf` 立刻落盘,输出顺序才等于执行顺序。这个坑我们在第 8 章讲 Mock、第 9 章讲多线程时还会再撞见,先记个底。gcc 和 clang 双跑一遍:

```text
$ gcc -std=c11 -Wall -Wextra stats.c test_assert.c -o test_assert_gcc
$ ./test_assert_gcc; echo "[exit=$?]"
=== 用 assert 测试 stats_average ===
[OK] avg(10,20,30) = 20
[OK] avg(empty) = -1 (失败码)
test_assert_gcc: test_assert.c:30: test_avg_deliberately_wrong: Assertion `got == 99' failed.
[exit=134]
```

```text
$ clang -std=c11 -Wall -Wextra stats.c test_assert.c -o test_assert_clang
$ ./test_assert_clang; echo "[exit=$?]"
=== 用 assert 测试 stats_average ===
[OK] avg(10,20,30) = 20
[OK] avg(empty) = -1 (失败码)
test_assert_clang: test_assert.c:30: void test_avg_deliberately_wrong(void): Assertion `got == 99' failed.
[exit=134]
```

两个编译器行为一致,都精准地停在了第三条。读一下这份输出:前两条用例(`avg(10,20,30)=20`、`avg(empty)=-1`)过了,各自打了 `[OK]`;第三条 `test_avg_deliberately_wrong` 里我**故意**把期望值写成 `99`(实际是 `5`),`assert(got == 99)` 判假、触发 `abort()`——前两条的 `[OK]` 全打出来了(`setvbuf` 的功劳),第三条之后那条 `test_avg_should_run_after` 和最后的 `=== 全部通过 ===` **根本没机会执行**,进程已经死了。退出码 `134` = `128 + 6`,那个 `6` 就是 `SIGABRT` 的信号编号(POSIX 下 `kill -l` 能查到 `ABRT = 6`)。

这就是裸 `assert` 的全部真相,它的缺点摆在台面上:**一条失败,整个测试程序当场 `abort`,后面所有用例全部跳过**。你写了四条用例,第三条挂了,第四条哪怕有个更严重的 bug,你这轮根本看不见——得等修好第三条、重跑,第四条才有机会暴露。它也没有「夹具」的概念:每条用例之前要不要建个东西、之后要不要拆,`assert` 不管,你得自己手动调。`assert` 适合那种「快速验一下某个不变量」的场合,不适合当一套测试套件的主力——主力得能让每条用例独立跑完、互不影响。

> 还有个工程纪律要提醒:`assert` 的语义是「**调试期**断言」,ISO C 规定定义了宏 `NDEBUG` 之后,`assert` 展开成空操作(参见 ISO/IEC 9899:2011 §7.2)。也就是说你在 Release 构建(典型如 CMake 的 `CMAKE_BUILD_TYPE=Release`,自动带 `-DNDEBUG`)里,所有 `assert` **全部静默失效**——第 5 章我们讲多配置时顺带跑过这个现象。所以**永远别用 `assert` 干运行期必须检查的错误处理**(比如「指针非 NULL 才能用」这种,Release 下检查就没了、空指针直接解引用崩),那种得用 `if` + 返回码(第 3 章的主题)。`assert` 只该放「这里若不成立,说明代码本身有逻辑 bug、整个程序没必要继续」的那种内部不变量。

## 第二级:Unity,纯 C 单测框架,FAIL 不 abort

第二级要解决的就是 `assert` 的两个死穴:**一条失败不能拖死全家**、**每条用例要有干净的夹具**。这套机制 C 标准库没有,得上单测框架。C++ 世界有 Google Test,纯 C 世界事实标准是 **Unity**(ThrowTheSwitch/Unity,GitHub 上 ~3k 星)——它全是 `.c`/`.h`,没有依赖,你把它的源文件和你的测试一起编就行,跨平台。

Unity 的核心机制听起来有点「黑魔法」,但其实就是 C 标准库的 `setjmp`/`longjmp`(参见 `setjmp(3)`)。`setjmp(buf)` 第一次调用返 0,顺便把「当前的栈现场」(寄存器、栈指针之类)存进 `buf`;之后任何时候调 `longjmp(buf, n)`,执行流会**瞬移**回 `setjmp` 那一行,但这次 `setjmp` 的返回值变成了 `n`(非 0)。这套机制常被诟病「像 goto 但更危险」,但在测试框架里它恰好是「一条用例 FAIL 就跳出、但跳到框架手里、不影响下一条」的完美工具。我照着 `projects/clib-utilities/test/unity/` 那套极简 shim(它本身也是 Unity 全量版的子集),亲手复刻一份教学版,只有 ~30 行,核心全在这:

```c
/* mini_unity.h —— 极简 Unity-风格测试框架(教学子集,~30 行)。
 * 真 project 应 vendor ThrowTheSwitch/Unity 的三文件全量版(~2000 行):
 *   https://github.com/ThrowTheSwitch/Unity
 * 这里只留核心:TEST_ASSERT_* 断言宏 + setUp/tearDown + RUN_TEST/UNITY_BEGIN/END。
 * 关键机制:失败用 setjmp/longjmp 隔离——一条 FAIL 不影响下一条跑。 */
#ifndef MINI_UNITY_H
#define MINI_UNITY_H
#include <setjmp.h>
#include <stdio.h>

void setUp(void);    /* 每条用例前跑(由测试文件定义) */
void tearDown(void); /* 每条用例后跑 */

extern jmp_buf Unityjmp_buf;
extern int UnityFailCount;
extern int UnityPassCount;

void unity_run_test(void (*test)(void), const char* name);

#define UNITY_BEGIN()       \
    do {                    \
        UnityFailCount = 0; \
        UnityPassCount = 0; \
    } while (0)
#define UNITY_END()                                                                       \
    (printf("\n%d Tests %d Failures\n", UnityPassCount + UnityFailCount, UnityFailCount), \
     (UnityFailCount > 0 ? 1 : 0))
#define RUN_TEST(test) unity_run_test(test, #test)

#define TEST_ASSERT_EQUAL_INT(exp, act)                                                    \
    do {                                                                                   \
        if ((exp) != (act)) {                                                              \
            printf("  %s:%d FAIL: expected %d, got %d\n", __FILE__, __LINE__, (int) (exp), \
                   (int) (act));                                                           \
            longjmp(Unityjmp_buf, 1);                                                      \
        }                                                                                  \
    } while (0)

#define TEST_ASSERT_TRUE(x)                                                       \
    do {                                                                          \
        if (!(x)) {                                                               \
            printf("  %s:%d FAIL: expected TRUE (%s)\n", __FILE__, __LINE__, #x); \
            longjmp(Unityjmp_buf, 1);                                             \
        }                                                                         \
    } while (0)
#endif
```

读关键几行。`TEST_ASSERT_EQUAL_INT(exp, act)` 这个宏干的事:先比 `exp == act`,不等就 `printf` 一行带文件名、行号、期望值、实际值的失败信息,然后**立刻 `longjmp(Unityjmp_buf, 1)`**——这就是「跳出当前用例、回到框架手里」的那一跳。注意它不是 `abort`,进程没死,执行流跳回了之前 `setjmp` 的地方。那个 `setjmp` 在哪?在框架的 `unity_run_test` 里:

```c
/* mini_unity.c —— 极简 Unity-风格框架实现。
 * 核心是 setjmp/longjmp:setjmp 先存现场、跑用例;一旦 longjmp 跳回,
 * 说明用例里某条断言 FAIL 了——我们捕获、记一笔,继续跑下一条。 */
#include "mini_unity.h"

jmp_buf Unityjmp_buf;
int UnityFailCount = 0;
int UnityPassCount = 0;

void unity_run_test(void (*test)(void), const char* name) {
    printf("  %s ... ", name);
    fflush(stdout);
    if (setjmp(Unityjmp_buf) == 0) {
        /* 正常路径:跑 setUp → 用例 → tearDown,没 longjmp 就算过 */
        setUp();
        test();
        tearDown();
        printf("PASS\n");
        UnityPassCount++;
    } else {
        /* longjmp 跳到这里:用例 FAIL 了(原因已在断言宏里 print)。
         * 关键:不 abort 整个程序,继续跑下一条用例。 */
        tearDown();
        UnityFailCount++;
    }
}
```

`unity_run_test` 是「跑一条用例」的调度器。它先 `setjmp(Unityjmp_buf)`——这次返 0,走 `if` 分支:依次跑 `setUp()`、用例函数 `test()`、`tearDown()`,三者都顺利返回(没 `longjmp`),就算这条用例**通过**,打 `PASS`、`UnityPassCount++`。如果用例里某条断言 FAIL 触发了 `longjmp`,执行流**瞬移**回 `setjmp` 那行、但这次返回值是 `1`(我们 `longjmp` 时传的 `1`),于是走 `else` 分支:失败原因已经在断言宏里 `printf` 过了,这里只调 `tearDown()` 收尾、`UnityFailCount++`——**然后 `unity_run_test` 正常返回**,`main` 里的下一条 `RUN_TEST` 照常调度。这就是「FAIL 不拖死全家」的全部秘密:`longjmp` 替代了 `abort`,把「用例失败」从「杀进程」降级成「跳回框架、记一笔、继续下一条」。

`setUp`/`tearDown` 是「夹具」:框架保证**每条用例之前调 `setUp`、之后调 `tearDown`**,你在这俩函数里建/拆测试环境。`stats_average` 无状态,我这俩留空;但要是测一个有状态的对象(比如动态数组),`setUp` 里 `create` 一个空数组、`tearDown` 里 `free` 掉,每条用例拿到的就是一个**全新的、干净的**数组——上一条用例的 push/erase 不会污染下一条。这就是「隔离」的工程含义,不光是「失败不拖死」,还有「状态不串味」。现在拿这套 mini_unity 测同一个 `stats_average`,用例和第一级一模一样(包括那条故意的错):

```c
/* test_unity.c —— 第二级:用极简 Unity 框架测同一个 stats_average。
 * 对照 test_assert.c:这里即使有 FAIL,后面的用例照样跑、最后报总账。 */
#include "mini_unity.h"
#include "stats.h"

/* 夹具:这里被测函数无状态,setUp/tearDown 空实现即可;
 * 真有状态的被测对象(如 clib 的 DynamicArray),setUp 建、tearDown 拆。 */
void setUp(void) {}
void tearDown(void) {}

static void test_avg_three_elems(void) {
    int a[] = {10, 20, 30};
    TEST_ASSERT_EQUAL_INT(20, stats_average(a, 3));
}

static void test_avg_empty_returns_fail(void) {
    int a[] = {10, 20, 30};
    TEST_ASSERT_EQUAL_INT(-1, stats_average(a, 3));
}

static void test_avg_deliberately_wrong(void) {
    int a[] = {2, 4, 6, 8};
    /* 正确答案是 5,故意断言成 99 → 这条 FAIL,但不会 abort 程序 */
    TEST_ASSERT_EQUAL_INT(99, stats_average(a, 4));
}

static void test_avg_should_run_after(void) {
    /* 对照 assert 版:这条在上一条 FAIL 之后照样能跑、照样能过 */
    int a[] = {-3, -6, 9};
    TEST_ASSERT_EQUAL_INT(0, stats_average(a, 3));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_avg_three_elems);
    RUN_TEST(test_avg_empty_returns_fail);
    RUN_TEST(test_avg_deliberately_wrong);
    RUN_TEST(test_avg_should_run_after);
    return UNITY_END();
}
```

(注:`test_avg_empty_returns_fail` 这条我故意写了个会挂的——`stats_average({10,20,30}, 3)` 返 20 不是 -1——拿它当第二条 FAIL 演示输出更丰富;真仓库里这条期望值应是 `-1` 配空入参,这里为了演示多挂一条留作教材。)gcc 和 clang 双跑:

```text
$ gcc -std=c11 -Wall -Wextra stats.c mini_unity.c test_unity.c -o test_unity_gcc
$ ./test_unity_gcc; echo "[exit=$?]"
  test_avg_three_elems ... PASS
  test_avg_empty_returns_fail ...   test_unity.c:18 FAIL: expected -1, got 20
  test_avg_deliberately_wrong ...   test_unity.c:24 FAIL: expected 99, got 5
  test_avg_should_run_after ... PASS

4 Tests 2 Failures
[exit=1]
```

```text
$ clang -std=c11 -Wall -Wextra stats.c mini_unity.c test_unity.c -o test_unity_clang
$ ./test_unity_clang; echo "[exit=$?]"
  test_avg_three_elems ... PASS
  test_avg_empty_returns_fail ...   test_unity.c:18 FAIL: expected -1, got 20
  test_avg_deliberately_wrong ...   test_unity.c:24 FAIL: expected 99, got 5
  test_avg_should_run_after ... PASS

4 Tests 2 Failures
[exit=1]
```

对照第一级那份 `abort` 输出,差异一目了然:**四条用例全部跑了**,两条挂、两条过,挂的各自打了行号(`test_unity.c:18`、`test_unity.c:24`)、期望值和实际值都摆出来,最后还有行总账 `4 Tests 2 Failures`——退出码 `1`(非 0,CI 能据此判定「这轮测试挂了」)。第三条 `test_avg_deliberately_wrong` 同样 FAIL,但它**没拖死**第四条 `test_avg_should_run_after`,那条照样跑、照样 PASS。这就是单测框架相对裸 `assert` 的本质优势:**每条用例独立、失败可定位、有总账**。

### 活教材:clib-utilities 是怎么从 ad-hoc printf 迁成 Unity 的

教学子集讲完,看一眼真仓库里的活样板。`projects/clib-utilities` 原来对 `CCDynamicArray` 的测试,长这样(`test/testDynamicArray.c` 节选):一锅操作塞进一个函数、中间夹 `printf`、靠人眼判输出对不对——典型的「演示」式测试。这轮重构把它**迁成了 Unity 断言用例**,新文件 `test/test_DynamicArray_unity.c`,五条用例各测一个行为:

```c
/* test_DynamicArray_unity.c —— 把原来的 ad-hoc printf 测试迁成 Unity 断言用例。
 * 对比 test/testDynamicArray.c(老版:一锅操作+printf、靠人眼判断),
 * 这里每条用例只测一个行为、用 TEST_ASSERT_* 自动判 pass/fail。*/
#include "unity.h"
#include "CCDynamicArray.h"

static int g_iter_count;
static void countEach(void* elem, void* arg) {
    (void) elem;
    (void) arg;
    g_iter_count++;
}

static CCDynamicArray* g_arr; /* 共享夹具:setUp 建、tearDown 拆 */

void setUp(void) {
    g_arr = CCDynamicArray_createEmpty(sizeof(int));
    g_iter_count = 0;
}

void tearDown(void) {
    CCDynamicArray_Free(g_arr);
    g_arr = NULL;
}

void test_pushSingle_then_iterate_counts_one(void) {
    int v = 42;
    CCBOOL_t ok = CCDynamicArray_pushBackSingle(g_arr, &v);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(1, g_iter_count);
}

void test_pushMulti_then_iterate_counts_three(void) {
    int vals[] = {10, 20, 30};
    CCBOOL_t ok = CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(3, g_iter_count);
}

void test_find_present_returns_nonneg_index(void) {
    int vals[] = {10, 20, 30};
    CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    int key = 20;
    CCSTD_Index_t idx =
        CCDynamicArray_Find(g_arr, &key, (CCSTD_CmpFuncType) compareInt, 0, TIL_END);
    TEST_ASSERT_TRUE(idx >= 0);
}

void test_find_absent_returns_notfound(void) {
    int vals[] = {10, 20, 30};
    CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    int key = 999;
    CCSTD_Index_t idx =
        CCDynamicArray_Find(g_arr, &key, (CCSTD_CmpFuncType) compareInt, 0, TIL_END);
    TEST_ASSERT_TRUE(idx < 0);
}

void test_eraseSingle_shrinks_by_one(void) {
    int vals[] = {1, 2, 3, 4, 5};
    CCDynamicArray_pushBackMulti(g_arr, vals, 5);
    CCBOOL_t ok = CCDynamicArray_EraseSingle(g_arr, 2);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(4, g_iter_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pushSingle_then_iterate_counts_one);
    RUN_TEST(test_pushMulti_then_iterate_counts_three);
    RUN_TEST(test_find_present_returns_nonneg_index);
    RUN_TEST(test_find_absent_returns_notfound);
    RUN_TEST(test_eraseSingle_shrinks_by_one);
    return UNITY_END();
}
```

这文件值得逐段读,它是「从 printf 迁成断言」的标准动作示范。先看**夹具**:`setUp` 建一个空数组 `g_arr`、`tearDown` `Free` 掉它——每条用例拿到的都是一个**全新的、空的**数组,上一条用例 push 进去的东西不会串进下一条。这就是上面说的「隔离」的工程含义,从「失败不拖死」升级到「状态不串味」。再看**用例粒度**:老版一个函数里又 push、又 iterate、又 find、又 erase,全糊在一起;迁过来之后**每条用例只测一个行为**——`test_pushSingle_then_iterate_counts_one` 只验「push 一个,iterate 计数得 1」;`test_find_present_returns_nonneg_index` 只验「找存在的元素,返回非负下标」;`test_eraseSingle_shrinks_by_one` 只验「erase 一个,长度减一」。这种「单一行为」粒度的好处是:**哪条挂了,立刻知道是哪个行为坏了**,不用再在「一锅操作」里反查。

断言风格也注意一下:`TEST_ASSERT_TRUE(ok)` 验布尔(这里 `ok` 是 `pushBack` 的返回码,呼应第 3 章的「返回码是断言对象」)、`TEST_ASSERT_EQUAL_INT(3, g_iter_count)` 验整数相等。`clib` 用的 `unity.h`/`unity.c` 在 `test/unity/` 目录下,**也是极简 shim**(和我上面那份 `mini_unity` 同构,只是宏名、变量名稍异),核心 `unity_run_test` 同样靠 `setjmp`/`longjmp` 隔离:

```c
/* unity.c(clib 的极简 Unity shim,test/unity/unity.c) */
#include "unity.h"

jmp_buf unity_jmp_buf;
int unity_fail_count = 0;
int unity_pass_count = 0;

void unity_run_test(void (*test)(void), const char* name) {
    printf("  %s ... ", name);
    fflush(stdout);
    if (setjmp(unity_jmp_buf) == 0) {
        setUp();
        test();
        tearDown();
        printf("PASS\n");
        unity_pass_count++;
    } else {
        /* FAIL 已在断言宏里 print 了原因,这里只标记+收尾 */
        tearDown();
        unity_fail_count++;
    }
}
```

要诚实标注一件事:**clib 用的是教学子集**,只有 `TEST_ASSERT_EQUAL_INT`/`TEST_ASSERT_TRUE`/`TEST_ASSERT_FALSE` 三个断言宏、`~20` 行实现。**真 project 应 vendor 真正的 ThrowTheSwitch/Unity 全量版**(<https://github.com/ThrowTheSwitch/Unity>,三个文件 `unity.c`/`unity.h`/`unity_internals.h`、~2000 行),它有几十个断言宏(`TEST_ASSERT_EQUAL_STRING`、`TEST_ASSERT_EQUAL_MEMORY`、`TEST_ASSERT_FLOAT_WITHIN`、`TEST_ASSERT_EQUAL_INT_ARRAY` 之类)、有 `TEST_IGNORE`(跳过某条用例)、有更完善的失败信息、还配 CMake 集成脚本和 Ruby 生成器。clib 的 shim 是为了「教学可读、无外部依赖」刻意留的小子集,够 `CCDynamicArray` 这种纯逻辑测试用;你自己的工程要正经测字符串/浮点/数组,直接 vendor 全量三件更划算。

## 第三级:CTest 装配,把测试「挂」起来跑

到第二级,你已经能写一条「跑完报 X Tests Y Failures」的测试可执行了。但真工程不止一个测试可执行——`clib` 有 `test_runner`(smoke)、`test_dynamic_array_unity`(Unity)两个,以后还会更多。总不能每次手动一条条 `./test_xxx` 跑吧?这就到了第三级:**CTest**。

CTest 是 CMake 自带的测试调度器(不是独立工具,CMake 装上就有)。它的工作模式特别朴素:你在 `CMakeLists.txt` 里用 `enable_testing()` 开启、用 `add_test(NAME 名字 COMMAND 可执行)` 把一条测试「登记」进 CTest;configure/build 完之后,`build/` 目录里会生成一个 `CTestTestfile.cmake`(测试清单),然后你在命令行敲 `ctest`,它就照着清单**逐个跑、每个看退出码、退出码 0 算过、非 0 算挂、最后报红绿**。注意这个判定逻辑:**CTest 判 pass/fail 的唯一依据是测试可执行的退出码**——退出码 0 = 过、非 0 = 挂(这就是为什么上面 Unity 的 `UNITY_END()` 在有失败时返 `1`:让 CTest 知道「这轮挂了」;而 `assert` 版 `abort` 退出码 134,CTest 也判挂)。所以「让 CTest 认你的测试」的契约就一条:**测试可执行 exit 0 表全过、exit 非 0 表有挂**,Unity/GTest/Criterion 这些框架都遵守这条契约。

我把 `stats` 的演示也挂上 CMake + CTest,`CMakeLists.txt` 长这样(被测编静态库、Unity shim 编静态库、两条测试各自链接它们、逐个 `add_test`):

```cmake
# CMakeLists.txt —— 第三级:用 CMake + CTest 把测试「挂」起来。
# 这是最小演示工程:一个静态库(stats)+ 两个测试可执行,逐个 add_test。
cmake_minimum_required(VERSION 3.23)
project(stats_demo LANGUAGES C)

# 被测代码编成静态库(测试和它链接)
add_library(stats_lib STATIC stats.c)
target_include_directories(stats_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(stats_lib PUBLIC c_std_11)

# Unity shim 单独编(两条测试都依赖它)
add_library(mini_unity STATIC mini_unity.c)
target_include_directories(mini_unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- 启用 CTest(必须在 add_test 之前) ----
enable_testing()

# 测试 1:全过的 Unity 测试 → ctest 绿灯
add_executable(test_pass test_pass.c)
target_link_libraries(test_pass PRIVATE stats_lib mini_unity)
add_test(NAME stats_pass COMMAND test_pass)

# 测试 2:带一条故意 FAIL 的 Unity 测试 → ctest 红灯(演示隔离+总账)
add_executable(test_with_fail test_unity.c)
target_link_libraries(test_with_fail PRIVATE stats_lib mini_unity)
add_test(NAME stats_with_fail COMMAND test_with_fail)
```

读关键几行。`enable_testing()` 是开关,**必须写在所有 `add_test` 之前**(否则 `add_test` 被忽略、不报错也不生效,这是个静默坑);习惯放 `project()` 之后、靠前的位置。`add_library(stats_lib STATIC stats.c)` 把被测编成静态库,这样两个测试可执行都链接它,避免重复编译(也呼应第 5 章 target-centric 的思路)。`add_test(NAME stats_pass COMMAND test_pass)` 是登记动作:`NAME` 是这条测试在 CTest 里的名字(待会儿输出里能看到)、`COMMAND` 是真跑的那个可执行(这里是 `test_pass`,CMake 知道它就是上面 `add_executable` 出来的那个)。我故意挂了两条测试:一条 `test_pass`(全过、退 0、ctest 绿灯)、一条 `test_with_fail`(带那条故意的 FAIL、退 1、ctest 红灯)——这样 ctest 输出能同时演示「过」和「挂」两种状态。configure + build + ctest:

```text
$ cmake -S . -B build -G "Unix Makefiles"
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cj/p4ch7/build
$ cmake --build build
[ 50%] Linking C static library libmini_unity.a
[ 50%] Built target mini_unity
[ 62%] Building C object CMakeFiles/test_pass.dir/test_pass.c.o
[ 75%] Linking C executable test_pass
[ 75%] Built target test_pass
[ 87%] Building C object CMakeFiles/test_with_fail.dir/test_unity.c.o
[100%] Linking C executable test_with_fail
[100%] Built target test_with_fail
$ ctest --test-dir build --output-on-failure
Test project /tmp/cj/p4ch7/build
    Start 1: stats_pass
1/2 Test #1: stats_pass .......................   Passed    0.00 sec
    Start 2: stats_with_fail
2/2 Test #2: stats_with_fail ..................***Failed    0.00 sec
  test_avg_three_elems ... PASS
  test_avg_empty_returns_fail ...   test_unity.c:18 FAIL: expected -1, got 20
  test_avg_deliberately_wrong ...   test_unity.c:24 FAIL: expected 99, got 5
  test_avg_should_run_after ... PASS

4 Tests 2 Failures


50% tests passed, 1 tests failed out of 2

Total Test time (real) =   0.00 sec

The following tests FAILED:
          2 - stats_with_fail (Failed)
Errors while running CTest
```

逐行读这份 ctest 输出。`1/2 Test #1: stats_pass ... Passed`——第一条测试(绿灯),名字 `stats_pass` 就是我们 `add_test(NAME ...)` 给的。`2/2 Test #2: stats_with_fail ... ***Failed`——第二条(红灯),`***Failed` 那个星号是 CTest 的视觉标记。我加了 `--output-on-failure`,所以挂的那条**把它的 stdout 全打出来了**——`test_avg_three_elems ... PASS`、`test_unity.c:18 FAIL: expected -1, got 20`、`4 Tests 2 Failures`,这正是那条 `test_with_fail` 可执行自己跑出来的 Unity 输出。最后一行总账 `50% tests passed, 1 tests failed out of 2`,还贴心列了 `The following tests FAILED: 2 - stats_with_fail (Failed)`——CI 里盯着这一段就知道哪条挂了。

`--output-on-failure` 这旗标务必记住:**默认 ctest 只打 `Passed`/`***Failed`,不显示失败详情**;不加这旗标,挂的测试只给你一个名字,你得自己再进去跑一遍才看得到为什么挂。CI 里加上它,挂的当场就把 Unity 的 `FAIL: expected X, got Y` 吐出来,省一轮排查。还有几个常用的:`ctest -R stats_pass` 只跑名字匹配正则 `stats_pass` 的测试(选择性跑)、`ctest --rerun-failed --output-on-failure` 只重跑挂的、`ctest -j8` 并行 8 个(大测试套加速)。

### build_examples.py 怎么自动发现并跑 CTest

真仓库里你不用手动敲 `ctest`,CI 的核心脚本 `scripts/build_examples.py` 会替你跑。它的逻辑很直白:对每个子项目 configure + build 之后,**检查 build 目录里有没有 `CTestTestfile.cmake`**,有就调 `ctest` 跑一遍。读它的关键几行(`scripts/build_examples.py` L60-66):

```python
    if (bdir / "CTestTestfile.cmake").exists() and CTEST:
        r = run([CTEST, "--test-dir", str(bdir), "--output-on-failure"])
        if r.returncode != 0:
            print(f"[{tag}] TEST FAIL       {rel}")
            if fatal:
                print(r.stdout[-1500:])
            return False
```

读这段。`bdir` 是子项目的 build 目录,`CTestTestfile.cmake` 是 CMake 在 configure 时(因为你写了 `enable_testing()`)生成的测试清单文件——**它的存在就是「这个子项目挂了 CTest 测试」的信号**。脚本检查它存在、且系统装了 `ctest`(`CTEST` 是 `shutil.which("ctest")` 的结果),就调 `ctest --test-dir bdir --output-on-failure` 把这个子项目的测试全跑一遍。退出码非 0 就标 `TEST FAIL`、按 `fatal` 决定要不要让整个 CI 挂掉。这就是「`enable_testing()` + `add_test` → 自动进 CI」的完整闭环:**你只要在 CMakeLists 里写对了那两行,build_examples.py 自动发现、自动跑、自动报**。

这套闭环在 `clib-utilities` 上已经跑通了。它的 `CMakeLists.txt` 里挂了两条测试(读 `projects/clib-utilities/CMakeLists.txt` L37-57):一条 `clib_smoke`(老的 `test_runner`,smoke 级「能编能跑退 0」)、一条 `dynamic_array_unity`(就是我们上面逐行读的 `test_DynamicArray_unity.c`,5 条 Unity 用例)。我在本机现跑一遍 clib 的 ctest,确认这两条都绿:

```text
$ cmake -S projects/clib-utilities -B /tmp/clib_verify -G "Unix Makefiles"
$ cmake --build /tmp/clib_verify
$ ctest --test-dir /tmp/clib_verify --output-on-failure
Test project /tmp/clib_verify
    Start 1: clib_smoke
1/2 Test #1: clib_smoke .......................   Passed    0.00 sec
    Start 2: dynamic_array_unity
2/2 Test #2: dynamic_array_unity ..............   Passed    0.00 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.00 sec
```

`2/2` 全 Passed——`clib_smoke` 和 `dynamic_array_unity` 两条测试都绿。这就是「主控已验证 2/2 Passed」的来路:`projects/clib-utilities` 现在是 CI 硬门(`build_examples.py` 里 `KNOWN_LEGACY` 不含它,挂了会让 CI 红),它的 Unity 迁移已经稳定、ctest 干净跑通。你以后给 clib 加新模块,照着 `test_DynamicArray_unity.c` 的样板写一条 `test_XXX_unity.c`、在 CMakeLists 里 `add_executable` + `add_test`,build_examples.py 自动就把它纳入 CI——这就是测试三级跳在真工程里的落地姿势。

## 小结

这一章把 C 工程挂真测试的三级跳钉死了。**第一级**裸 `assert`(`<assert.h>`),零依赖最快,但一条失败就 `abort` 整个程序、后面全跑不到、也没夹具——它适合「快速验不变量」,不适合当测试套件主力;顺带揭了 `stdout` 全缓冲遇上 `abort` 会丢日志的坑(`setvbuf` 强制无缓冲救场),以及 `NDEBUG` 下 `assert` 静默失效(别拿它干运行期错误处理,那种用 `if`+返回码,第 3 章主题)。**第二级** Unity 纯 C 单测框架,核心是 `setjmp`/`longjmp` 把「用例失败」从「杀进程」降级成「跳回框架、记一笔、继续下一条」——`setUp`/`tearDown` 保证每条用例状态隔离、`TEST_ASSERT_*` 自动判期望==实际、`UNITY_BEGIN/END` 跑完报总账;我照着 clib 的极简 shim 复刻了一份 ~30 行的 `mini_unity`,逐行讲透了 `longjmp` 那一跳,并以 clib 的 `test_DynamicArray_unity.c`(5 条从 ad-hoc printf 迁过来的用例)当「单一行为粒度 + 夹具隔离」的活样板。**第三级** CTest,`enable_testing()` + `add_test` 把测试可执行挂进清单、`ctest` 逐个跑看退出码报红绿(判 pass/fail 的契约就一条:exit 0 过、非 0 挂);`build_examples.py` 自动发现 `CTestTestfile.cmake` 跑测试(L60-66),clib 的 `clib_smoke` + `dynamic_array_unity` 两条已验证 2/2 Passed。三级不是「选一个」,是工程里层层叠加:`assert` 留给快速调试不变量,Unity 写正式用例,CTest 把它们编排进 CI——下一章我们开始碰「有依赖的代码怎么测」(Mock),这套测试基建就是它的地基。

## 参考资源

- **ThrowTheSwitch/Unity**(<https://github.com/ThrowTheSwitch/Unity>,GitHub ~3k 星):纯 C 单测框架的工业标准,三文件全量版 `unity.c`/`unity.h`/`unity_internals.h`,几十个断言宏 + CMake 集成。本仓库 `projects/clib-utilities/test/unity/` 是它的极简教学子集。
- **ISO/IEC 9899:2011 §7.2**(`assert`):`assert` 宏的语义、`NDEBUG` 下展开为空操作的定义。
- **`setjmp(3)` / `longjmp(3)`**(man 页):非局部跳转的原语,Unity 隔离机制的底层。注意它对自动变量的 volatile 要求(本仓库教学子集未涉及,真 Unity 全量版处理了)。
- **CMake 官方文档**:`enable_testing()` / `add_test()` / `ctest(1)` 的命令行旗标(`--output-on-failure`、`-R`、`-j`)。
- **本仓库 `scripts/build_examples.py` L60-66**:CTest 自动发现逻辑——`CTestTestfile.cmake` 存在即跑,是「`add_test` 进 CI」的闭环实现。
- **第 8 章·Mock 与隔离**:本章只测无依赖的纯函数,有依赖的(文件/网络/其他模块)要 Mock 替换再测。
