---
title: "工程化毕业项目:把一个库从「能编」推到「可信」"
description: "这是工程化阶段的收官章,把前面 15 章一道道立起来的防线——头文件契约、target 语义、链接器、测试、sanitizer、clang-tidy、gcov、CI——拧成一股绳,落在两个活教材上跑一遍完整流水线。活教材之一是 examples/stage4-cmake-lib:同一份源编出静态库 + 动态库、带 install/export/find_package 的最小库工程,我们把它端到端跑通——装库、消费者 find_package(Mathlib) 链上、跑出 ml_add(2,3)=5。之二是 projects/clib-utilities:add_library + CTest + Unity 5 用例的真实项目,把它过一遍六道门:① CMake Debug/Release 多配置(Ch5),看 flags.make 怎么从 -g 切到 -O3 -DNDEBUG;② CTest 2/2 Passed(Ch7),Unity 5 用例 0 Failures;③ sanitizer(Ch10),ASan+UBSan——这一步诚实记下 clib 在 sanitizer 下 dynamic_array_unity FAILED,UBSan 抓到 CCDynamicArray.c:203 函数指针类型不匹配、ASan 抓到 eraseSingle heap-buffer-overflow,而这正是 sanitizer 门存在的意义,也说明 CI 当前 sanitize job 只覆盖 examples/ 子项目、clib 暂不在硬门内;④ clang-tidy(Ch12),clang_tidy_check.py 对 examples 退出 0;⑤ gcov(Ch13),CCDynamicArray.c 42.45% 的行覆盖 baseline;⑥ install/export/find_package(Ch6),stage4-cmake-lib 全链路。每步只串联命令和真输出、不重讲原理。最后散文收口工程化阶段「可信度脊柱」的定位,呼应阶段 0 工具链体检 → 阶段 1-3 语言/指针/数据结构 → 阶段 4 工程化 → 阶段 5 系统编程的教程主线。全 gcc16.1.1 + clang22.1.6 真跑,贴真实退出码和输出。"
chapter: 4
order: 16
tags:
  - host
  - engineering
  - build
  - cmake
  - testing
  - toolchain
  - linker
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4·第 1 章:头文件契约(整个工程化阶段的声音锚点 + 契约层起点)"
  - "阶段 4·第 5 章:CMake 工程化(target 语义、Debug/Release 多配置)"
  - "阶段 4·第 6 章:静态库、动态库与链接顺序(install/export/find_package 全链路)"
  - "阶段 4·第 7 章:测试不再是 printf(Unity + CTest,5 条用例的来历)"
  - "阶段 4·第 10 章:ASan+UBSan 深入(sanitize 这道门)"
  - "阶段 4·第 12 章:静态分析门(clang-tidy)"
  - "阶段 4·第 13 章:覆盖率门(gcov/lcov,42.45% 的来历)"
  - "阶段 4·第 15 章:把质量门拼成流水线(ci.yml 六道门的全景)"
related:
  - "阶段 4·第 15 章:把质量门拼成流水线(本章把它那六道门一个个真跑一遍)"
  - "阶段 0·第 1 章:工具链体检(gcc/clang 双跑纪律,整条流水线的底层)"
  - "阶段 5·第 1 章:文件 IO(可信的库是系统编程的积木)"
---

# 工程化毕业项目:把一个库从「能编」推到「可信」

## 引言:门一道道立完了,该把它们一次跑给一个真项目看

前面 15 章我们做的事,可以用一句话概括——**一道一道给 C 工程立防线**。第 1 章钉头文件契约(include guard、ODR、`static inline`),第 5 章立 target 语义和 PRIVATE/PUBLIC/INTERFACE 三态,第 6 章把链接器的 `undefined reference`、库顺序、`RPATH`/`RUNPATH` 摸到底,第 7 章把测试从 `printf` 演示迁成 Unity 断言 + CTest 红绿,第 10 章复现 ASan+UBSan 抓运行时 UB,第 12 章立 clang-tidy 静态分析门,第 13 章用 gcov 把「测试盖到了哪」量化成数字,第 15 章把它们拼进 ci.yml 的六道 job 挂到每次 push 上。可每一章都是「为了讲这件事、临时搭一个最小例子」——例子跑完就丢,没有哪个真实项目**从头到尾把这六道门挨个过一遍**。

这一章就是来补这一刀的。它不立新门、不教新原理,只做一件事:拿本仓两个真项目当活教材,把它们**依次推过六道门**,贴每一步的真实命令、真实退出码、真实输出。两个活教材分工不同——

`examples/stage4-cmake-lib` 是第 6 章为讲 install/export/find_package 而造的**最小库工程**:同一份 `src/mathlib.c` 编出静态库 `mathlib`(.a)和动态库 `mathlib_shared`(.so)两个 target,带 `install(TARGETS ... EXPORT MathlibTargets)` 把库、头、生成的 Config/Targets/ConfigVersion 文件全装走。它个头小到一眼看穿、又五脏俱全,正适合拿来跑完整流水线的「构建→安装→消费」这一头。

`projects/clib-utilities` 是本仓**真整改过的项目**(十几个 `.c`/`.h`、跨三个模块目录、带 `pthread`/`dl` 系统依赖),`CMakeLists.txt` 里 `add_library(clib_utils STATIC ...)` 配 `enable_testing()` + 两条 `add_test`(smoke + Unity),是真有「测试体」的工程。它个头够大、能撑起 ctest/sanitizer/gcov 这些需要真实测试负载的门。

先把本机工具链钉一眼(整套流水线的地基):`gcc 16.1.1` / `clang 22.1.6` / `cmake 4.3.4` / `ctest`(随 cmake)/ `gcov`(随 gcc)/ `clang-tidy 22.1.6` 全在;`lcov` 本机未装(覆盖率那条线本机走 gcov 原生、CI 上才用 lcov,第 13 章交代过)。下面六步,每步只贴命令和真输出、引一下原理在哪一章讲过,不重讲。

## ① CMake Debug/Release 多配置:看 flags.make 怎么切

对应第 5 章。CMake 的多配置靠 `CMAKE_BUILD_TYPE` 这一个变量切:配 `Debug` 它给 `-g`、配 `Release` 它给 `-O3 -DNDEBUG`(顺带 `NDEBUG` 把 `assert` 静默掉,第 5 章真跑过)。拿 `clib-utilities` 当被配对象,配两次、各看一眼 CMake 生成的 `flags.make`——这文件记着每个 target 真正用的编译旗标,是验证「多配置到底干了啥」最直接的证据。

```text
$ cd projects/clib-utilities
$ cmake -B /tmp/cj/p4ch16/clib-d -DCMAKE_BUILD_TYPE=Debug
-- Build files have been written to: /tmp/cj/p4ch16/clib-d
$ cmake -B /tmp/cj/p4ch16/clib-r -DCMAKE_BUILD_TYPE=Release
-- Build files have been written to: /tmp/cj/p4ch16/clib-r
$ grep C_FLAGS /tmp/cj/p4ch16/clib-d/CMakeFiles/clib_utils.dir/flags.make
C_FLAGS = -g -Wall -Wextra
$ grep C_FLAGS /tmp/cj/p4ch16/clib-r/CMakeFiles/clib_utils.dir/flags.make
C_FLAGS = -O3 -DNDEBUG -Wall -Wextra
```

`Debug` 那行就一个 `-g`(调试信息),`Release` 那行换成 `-O3`(优化开满)+ `-DNDEBUG`(关 `assert`)——`-Wall -Wextra` 是项目自己在 `target_compile_options` 里挂的、跟构建类型无关、两边都在。两个配置都能 `cmake --build` 过、退出码 0。这一步本身不抓 bug,它只验证一件事:**项目的构建系统是健康的、能按标准姿势在两种配置下都被 CMake 接管**,这是后面所有门的前提——构建系统不健康,后面的测试、sanitizer、覆盖率全都挂不上去。

## ② CTest:2/2 Passed,Unity 5 用例 0 Failures

对应第 7 章。`clib-utilities` 的 `CMakeLists.txt` 里 `enable_testing()` 之后挂了两条 `add_test`:`clib_smoke`(原有手写测试合集)和 `dynamic_array_unity`(第 7 章把 ad-hoc `printf` 测试迁成的 Unity 5 用例)。用 Debug 那份构建,跑 `ctest --output-on-failure`:

```text
$ cmake --build /tmp/cj/p4ch16/clib-d
[100%] Built target test_dynamic_array_unity
$ cd /tmp/cj/p4ch16/clib-d && ctest --output-on-failure
Test project /tmp/cj/p4ch16/clib-d
    Start 1: clib_smoke
1/2 Test #1: clib_smoke .......................   Passed    0.00 sec
    Start 2: dynamic_array_unity
2/2 Test #2: dynamic_array_unity ..............   Passed    0.00 sec

100% tests passed, 0 tests failed out of 2
Total Test time (real) =   0.01 sec
```

`2/2 Passed`。把 `dynamic_array_unity` 那条单独拎出来跑,看 Unity 框架自己的输出:

```text
$ ./test_dynamic_array_unity
  test_pushSingle_then_iterate_counts_one ... PASS
  test_pushMulti_then_iterate_counts_three ... PASS
  test_find_present_returns_nonneg_index ... PASS
  test_find_absent_returns_notfound ... PASS
  test_eraseSingle_shrinks_by_one ... PASS

5 Tests 0 Failures
```

五条用例——push 计数、pushMulti 计数、find 命中、find 缺失、erase 缩减——每条只测一个行为、`setjmp`/`longjmp` 保证一条 FAIL 不拖死全家(这套机制第 7 章逐行拆过)。`5 Tests 0 Failures` 退出码 0,ctest 那一格变绿。这一步说明:**项目有测试体、测试体能跑、测试体在断言「期望 vs 实际」而不是靠人眼看 printf**——这是从「演示」跨到「测试」那道分水岭(第 7 章的主题)。但「测试都过了」不等于「代码没 bug」——下一道门就要诚实地把这话戳破。

## ③ Sanitizer:ASan+UBSan,这一步当场抓到 clib 两个真 bug

对应第 10 章。本仓 `.github/workflows/ci.yml` 的 `sanitize` job 干的事,就是用 clang 带 `-fsanitize=address,undefined` 把 examples/ 下的 CMake 子项目再编一遍、跑一遍——这一步我在本地复现。但**先把一个诚实事实摆前面**:CI 的 sanitize job **当前只覆盖 `examples/` 一级子项目**(ci.yml 注释明说「目前仅覆盖 CMake 子项目(SC1-4)」),`projects/clib-utilities` **暂不在 sanitizer 硬门内**。这一章我特意把 clib 也拿来过一遍 sanitizer,结果它**当场炸了**——这正好是这道门存在的意义,值得把输出原样贴出来当活教材。

先按 CI 的姿势对 `examples/stage4-cmake-lib` 跑(对应 CI 那条 sanitize job 的实际作用域):

```text
$ cd examples/stage4-cmake-lib
$ CC=clang CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  LDFLAGS="-fsanitize=address,undefined" \
  cmake -B /tmp/cj/p4ch16/stage4-asan -DCMAKE_BUILD_TYPE=Debug
$ cmake --build /tmp/cj/p4ch16/stage4-asan
[100%] Built target mathlib_shared
$ ls /tmp/cj/p4ch16/stage4-asan/libmathlib.*
/tmp/cj/p4ch16/stage4-asan/libmathlib.a
/tmp/cj/p4ch16/stage4-asan/libmathlib.so.1
```

静态库 `.a` 和带 SOVERSION 的 `.so.1` 都编出来了,退出码 0——examples 子项目在 ASan+UBSan 下干净。再顺手把 CI 的 `build-examples` job(不带 sanitizer、gcc/clang 矩阵跑 `build_examples.py`)也复现一遍:

```text
$ CC=gcc python3 scripts/build_examples.py
✅ examples 全部构建通过。
$ CC=clang python3 scripts/build_examples.py
✅ examples 全部构建通过。
```

两边退出码都是 0,examples 失败 0、projects 报告失败 0。

现在**把 clib 也拿来过 sanitizer**(这一步 CI 当前没做、本章特意做,用意等下说):

```text
$ cd projects/clib-utilities
$ CC=clang CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  LDFLAGS="-fsanitize=address,undefined" \
  cmake -B /tmp/cj/p4ch16/clib-asan -DCMAKE_BUILD_TYPE=Debug
$ cmake --build /tmp/cj/p4ch16/clib-asan
$ cd /tmp/cj/p4ch16/clib-asan && ctest --output-on-failure
Test project /tmp/cj/p4ch16/clib-asan
    Start 1: clib_smoke
1/2 Test #1: clib_smoke .......................   Passed    0.02 sec
    Start 2: dynamic_array_unity
2/2 Test #2: dynamic_array_unity ..............***Failed    0.01 sec
  test_pushSingle_then_iterate_counts_one ... PASS
  test_pushMulti_then_iterate_counts_three ... PASS
  test_find_present_returns_nonneg_index ...
.../CCDynamicArray.c:203:7: runtime error: call to function (unknown)
  through pointer to incorrect function type 'enum _CCSTD_CmpRes (*)(void *, void *)'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior .../CCDynamicArray.c:203:7
  PASS
  test_find_absent_returns_notfound ... PASS
  test_eraseSingle_shrinks_by_one ...
==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...0054
READ of size 12 at 0x...0054 thread T0
    #0 .../test_dynamic_array_unity+0x13af8c
    ...
0x...0054 is located 0 bytes after 20-byte region [0x...0040,0x...0054)
==ABORTING

50% tests passed, 1 tests failed out of 2
The following tests FAILED:
         2 - dynamic_array_unity (Failed)
ctest_exit=8
```

ctest 退出码 8(非 0),`dynamic_array_unity` 这条 FAILED。sanitizer 在 clib 里**当场抓到两个真 bug**——

**UBSan 抓的**:`CCDynamicArray.c:203` 调用一个「类型对不上」的函数指针。测试代码里把 `compareInt` 强转成 `CCSTD_CmpFuncType` 传进 `CCDynamicArray_Find`,而 `compareInt` 的真实签名跟 `CCSTD_CmpFuncType` 不完全一致,C 标准(ISO/IEC 9899:2011 §6.5.2.2)把「通过类型不兼容的函数指针调用」列为 UB——平时跑没事是因为 ABI 凑巧对得上,UBSan 把这层「凑巧」拆穿了。修法是统一 `compareInt` 的签名让它跟 `CCSTD_CmpFuncType` 严格一致、去掉那个强转。

**ASan 抓的**:`test_eraseSingle_shrinks_by_one` 触发 `heap-buffer-overflow`,`READ of size 12`、`0 bytes after 20-byte region`——`EraseSingle` 在缩容后读到了已释放/越界的 12 字节(正好是三个 `int` 的大小)。这是 `EraseSingle` 内部挪元素的循环边界算错了,多读了一格。这条 bug 平时跑也「看起来过了」(读到的字节恰好没被别人覆盖、assert 没炸),ASan 一埋红区就当场现形。

这一步是整条流水线**最值钱的一刀**。它说明两件事:其一,**「ctest 2/2 Passed」和「代码没内存 bug」是两回事**——第 ② 步那五条用例全绿,可 ASan 一开就抓到 heap 越界;其二,**CI 当前的 sanitize 门只覆盖 examples、clib 在硬门外**,这两个 bug 才得以暂时「潜伏」——这正呼应第 15 章讲的「硬门 vs 报告模式」那条工程现实:`build_examples` 内部对 `projects/` 走的是 `KNOWN_LEGACY` 报告模式,clib 的整改还在路上,sanitizer 这道门等 clib 把 finding 还完再收紧成硬门。这里我特意把它跑出来贴给你看,是想让收官章诚实交代「我们立了六道门、其中一道还没完全合上」——而不是假装万事大吉。

## ④ clang-tidy:examples 全过、退出 0

对应第 12 章。这道门管「能编过、但写得不对」的语义毛病:双下划线保留标识符、隐式 narrowing、缺括号这类编译器警告一声不吭的坑。本仓 `scripts/clang_tidy_check.py` 给每个 CMake 子项目配 `compile_commands.json` 再跑 `clang-tidy -p`,CI 的 `static-analysis` job 就是它。

```text
$ python3 scripts/clang_tidy_check.py
[OK]   examples/stage4-cmake-lib/src/mathlib.c

✅ clang-tidy 全部通过。
$ echo $?
0
```

退出码 0,`examples/stage4-cmake-lib/src/mathlib.c` 这一个文件(目前 `clang_tidy_check` 只扫 `examples/` 一级子目录下的源)过了 `bugprone-*`/`performance-*`/`readability-*` 三族检查。第 12 章已经把 `projects/clib-utilities` 现存的 4 个真 finding(`CCSTDLib_FetchError.c:44` 的 narrowing、`__CCThread_TrampolineArg` 的保留标识符、`CCThread.c:117` 的 int→ptr、两处缺括号)逐条列过修法——这道门跟 sanitizer 一样,clib 还在整改、暂未纳入硬门。区别在于:sanitizer 是运行时插桩(得把程序跑起来才抓得到),clang-tidy 是编译时静态分析(不用跑就抓)——两者互补,一个查「这代码跑起来会出 UB 吗」、一个查「这代码写得对吗」。

## ⑤ gcov 覆盖率:42.45% 的 baseline

对应第 13 章。测试有了(第 ② 步 2/2 Passed),可「测过了」不等于「测够了」——五条 Unity 用例到底跑到了 `CCDynamicArray.c` 这 139 行代码的百分之几?这一步把「测试盖到了哪」量化成数字。`--coverage` 是编译+链接两用旗标(等价于 `-fprofile-arcs -ftest-coverage` 编译 + `-lgcov` 链接),编译时生 `.gcno`(计数图)、跑测试生 `.gcda`(运行计数)、`gcov` 读这俩出报告。覆盖率测量一律 `-O0`(优化会合并/重排行、数字跟源码对不上),且本机这条线**只能用 gcc**——第 13 章那个真坑:clang 22 的 `--coverage` 产的 `.gcda` 是 `B11*` 版本、gcc 16 的 gcov 只认 `B61*`、读不了报错,所以覆盖率统一以 gcc 当权威工具。

```text
$ cd projects/clib-utilities
$ cmake -B /tmp/cj/p4ch16/clib-cov \
    -DCMAKE_C_FLAGS="--coverage -g -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage"
$ cmake --build /tmp/cj/p4ch16/clib-cov
$ cd /tmp/cj/p4ch16/clib-cov && ctest --output-on-failure
100% tests passed, 0 tests failed out of 2
$ gcov -b CMakeFiles/clib_utils.dir/BasicDataStructure/Sources/CCDynamicArray.c.gcno
File '.../CCDynamicArray.c'
Lines executed:42.45% of 139
Branches executed:42.50% of 80
Taken at least once:25.00% of 80
Calls executed:30.43% of 23
Creating 'CCDynamicArray.c.gcov'
```

`Lines executed:42.45% of 139`——139 行可执行代码里只跑过了 42.45%(59 行),剩下 80 行是死的。第 13 章已经逐函数拆过为什么这么低:五条 Unity 用例只测了 `createEmpty`/`pushBack`/`Find`/`erase`/`Iterate` 这几条主路径,`clone`/`InsertSingle`/`InsertMulti` 等 7 个函数压根 `called 0`、完全没碰;并真跑了一次「补 `clone` + `InsertSingle` 两条用例」把覆盖率从 42.45% 拉到 58.99%、证明「加用例→覆盖率涨」的闭环——那套演示归第 13 章,这一章只贴当前的 baseline 数字、告诉你「这是流水线跑完之后 clib 的真实覆盖率现状」。

把第 ②③⑤ 这三步摆在一起看,结论很有意思:**同一份 clib 代码,过了 ctest(2/2 绿)、却在 sanitizer 下 FAILED(两个真 bug)、覆盖率只有 42.45%(大半代码没被测到)**——这正是为什么「一道门不够、要立六道门」的全部理由。ctest 管「测试体有没有」、sanitizer 管「测试体跑起来时内存/UB 干不干净」、gcov 管「测试体盖到了百分之几」——三个维度互相不能替代。CI 上 `coverage` job 跟 `sanitize` job 是两条独立的 workflow、各自只管自己那格,谁也不能替谁兜底。

## ⑥ install/export/find_package:把库端到端装出去

对应第 6 章。前五步都在「编 + 测」这个圈里转,这一步跨出去——把库**装出去、让一个全新写的消费者 `find_package` 找到它、链上跑通**。这是「能编」到「能给被人用」的那道坎,`examples/stage4-cmake-lib` 这个最小库工程就是第 6 章为讲它而造的。它的 `CMakeLists.txt` 同一份 `src/mathlib.c` 编出 `mathlib`(STATIC)和 `mathlib_shared`(SHARED,带 `SOVERSION 1`)两个 target,配 `install(TARGETS ... EXPORT MathlibTargets)` 把库、头、生成的 `MathlibTargets.cmake`/`MathlibConfig.cmake`/`MathlibConfigVersion.cmake` 全装走——这正是消费者 `find_package(Mathlib)` 要的全部家当。

第一步,配 + 构建 + install 到一个 DESTDIR 前缀(用 `DESTDIR` 模拟系统安装、不污染真 `/usr/local`):

```text
$ cd examples/stage4-cmake-lib
$ cmake -B /tmp/cj/p4ch16/mathlib_build
$ cmake --build /tmp/cj/p4ch16/mathlib_build
[100%] Built target mathlib_shared
$ DESTDIR=/tmp/cj/p4ch16/mathlib_prefix cmake --install /tmp/cj/p4ch16/mathlib_build
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/libmathlib.a
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/libmathlib.so.1
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/libmathlib.so
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/include/mathlib.h
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibTargets.cmake
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibConfig.cmake
-- Installing: /tmp/cj/p4ch16/mathlib_prefix/usr/local/lib/cmake/Mathlib/MathlibConfigVersion.cmake
```

装出来的目录树一眼看明白:`lib/` 里是 `.a` + `.so.1`(带 ABI 版本)+ `.so`(指向 `.so.1` 的软链);`include/` 里是公开头 `mathlib.h`;`lib/cmake/Mathlib/` 里是消费端 `find_package` 要的四个文件——`MathlibConfig.cmake` 是入口(它 `include` 了 `MathlibTargets.cmake`)、`MathlibTargets.cmake` 把 `mathlib`/`mathlib_shared` 注册成 IMPORTED target(带好 include 路径和库位置)、`MathlibConfigVersion.cmake` 让 `find_package(Mathlib 1.0)` 能查版本兼容性。那个 `BUILD_INTERFACE`/`INSTALL_INTERFACE` 生成器表达式(第 5 章讲过)此刻就在发挥作用:导出的 IMPORTED target 自动带上 install 前缀下的 `include/`,消费者不用手写 `-I`。

第二步,写消费者工程,`find_package(Mathlib)` 链上跑通:

```cmake
# consumer/CMakeLists.txt
cmake_minimum_required(VERSION 3.23)
project(consumer LANGUAGES C)
list(PREPEND CMAKE_PREFIX_PATH /tmp/cj/p4ch16/mathlib_prefix/usr/local)
find_package(Mathlib 1.0 REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE mathlib)
```

```c
/* consumer/main.c —— 用 find_package 拿到的 Mathlib 跑 */
#include <stdio.h>
#include <mathlib.h>

int main(void) {
    printf("ml_add(2,3) = %d\n", ml_add(2, 3));
    printf("ml_mul(4,5) = %d\n", ml_mul(4, 5));
    return 0;
}
```

```text
$ cd /tmp/cj/p4ch16/consumer && cmake -B build && cmake --build build
[100%] Built target consumer
$ ./build/consumer
ml_add(2,3) = 5
ml_mul(4,5) = 20
```

跑通了,`ml_add(2,3)=5`、`ml_mul(4,5)=20`。这里有个第 6 章已经诚实交代过的点值得再确认一眼:消费者默认链的是 IMPORTED target `mathlib`(静态那一个),CMake 把 `libmathlib.a` 的代码直接并进可执行——`nm` 能在 `consumer` 里看到 `T ml_add`/`T ml_mul` 符号。改成链 `mathlib_shared`(动态那个)再看 `readelf -d`:

```text
$ readelf -d build/consumer | grep -Ei 'rpath|runpath|NEEDED'
 0x0000000000000001 (NEEDED)             Shared library: [libmathlib.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [/tmp/cj/p4ch16/mathlib_prefix/usr/local/lib]
```

CMake 默认在构建期给可执行埋了一条 `RUNPATH`,指向它链接的那个 `.so` 的**绝对**位置(`/tmp/cj/p4ch16/mathlib_prefix/usr/local/lib`)——这是 build-tree RPATH,开发期省心、可直接跑;但要做成可分发的可执行,正规做法是给消费者 target 设 `INSTALL_RPATH` 用相对的 `$ORIGIN` 写法(第 6 章讲过)、再 `install` 它。这条绝对 RUNPATH 把库和可执行**绑死在同一台机器、同一个路径**上,迁机器就失效——是这套机制的便利与代价。到此 install→export→`find_package`→链上→跑通的全链路在 Ch16 这一次真跑里端到端复现了一遍,跟第 6 章的结论一致。

## 小结

工程化阶段这 16 章,我们做的事可以一句话收口——**把一个 C 工程从「能编」一路推到「可信」,把每一层防线都铺完**。第 1 章钉头文件契约,让一堆 `.c`/`.h` 能在链接器那里和平共处;第 5 章立 target 语义,让「我的库、我的头、我的私有细节」在构建系统里有清晰的界;第 6 章把链接器的顺序陷阱和 `.so` 的运行期查找摸到底,让库能装出去、能被 `find_package` 找到;第 7 章把测试从 `printf` 演示迁成断言,让「测过了」这件事可机器判;第 10 章用 sanitizer 抓运行时 UB,让「测试没盖到的内存错」也跑不掉;第 12 章用 clang-tidy 抓编译时语义毛病,让「能编过但写得不对」现形;第 13 章用 gcov 把测试覆盖率量化,让「测够了」从感觉变数字;第 15 章把它们拼进 ci.yml 挂到每次 push 上,让人可以忘、CI 不会忘。这一章把六道门挨个真跑了一遍,贴出每一步的真实退出码和输出——其中 sanitizer 那一步还诚实记下 clib 当场炸出来的两个真 bug(UBSan 的函数指针类型不匹配、ASan 的 eraseSingle heap 越界),这是整条流水线最值钱的一刀:**它证明「ctest 全绿」和「代码没内存 bug」是两回事,也证明 sanitizer 这道门存在的意义就在于把这两者之间的缝隙补上**。

这条「可信度脊柱」不是孤立的,它接在整个 C-Journey 教程主线上。往回看,阶段 0 的工具链体检立了 gcc/clang 双跑纪律、把编译四阶段/链接/sanitizer/格式化的底层全摸过一遍——那是这一章六道门能跑起来的地基(没工具链体检,你连 `gcc -std=c11 -Wall -Wextra` 都不会下);阶段 1 的 C 语言基底、阶段 2 的指针与内存、阶段 3 的数据结构,把语言层面的正确性铺到位——`static inline` 不踩 C99 裸 inline 坑、指针不越界、数据结构不内存泄漏,是工程化门能拦住 bug 的前提(代码本身写得烂,六道门也只能帮你拦下一部分)。这一章铺的工程化脊柱,是把前几阶段那些「写得对的 C 代码」**组织成一个能被人协作、能被机器持续验证的项目**。往前看,阶段 5 的系统编程——文件 IO、`fork`/`exec`、信号、socket、共享内存——会大量调用本阶段立起来的这些库和工具链(`find_package` 找依赖、CTest 跑测试、sanitizer 抓段错误),可信的库是系统编程的积木。从此一个 C 工程不再是「一个 `.c` 从头跑到尾」,而是一个**有契约、有构建系统、有测试、有持续验证、能装出去给别人用**的活物——这,就是工程化阶段 16 章给你的全部。

## 参考资源

- **本仓活教材**:[`examples/stage4-cmake-lib/`](../../examples/stage4-cmake-lib/)(第 6 章为讲 install/export/find_package 造的最小库工程,本章第 ⑥ 步活教材)、[`projects/clib-utilities/`](../../projects/clib-utilities/)(真整改过的项目,本章第 ①–⑤ 步活教材)、[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)(六道 job 的真实配置)、[`scripts/build_examples.py`](../../scripts/build_examples.py)、[`scripts/clang_tidy_check.py`](../../scripts/clang_tidy_check.py)。
- **承接章节**:第 5 章(Debug/Release 多配置)、第 6 章(install/export/find_package 全链路)、第 7 章(Unity + CTest)、第 10 章(ASan+UBSan)、第 12 章(clang-tidy)、第 13 章(gcov/42.45% 的来历)、第 15 章(ci.yml 六道门全景)——本章是这七章的串联真跑,原理归它们、本章只贴命令和输出。
- **ISO/POSIX 条款**:ISO/IEC 9899:2011 §6.5.2.2(函数指针类型不兼容调用是 UB,本章 sanitizer 那步 UBSan 抓的就是这条);`--coverage`/`gcov` 语义见 `gcov(1)` man 页;ELF `DT_RUNPATH`(标签 `0x1d`)见 System V ABI / `readelf(1)` / `ld(1)` 的 `-rpath` 文档。
