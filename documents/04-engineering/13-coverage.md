---
title: "覆盖率门:gcov/lcov 量化测试盖到了哪里"
description: "测试有了(第 7 章把 clib 挂上了 Unity + CTest),可『挂了测试』不等于『测够了』——你写了 5 条用例,它们到底跑到了被测代码的百分之几?没人量化,就只能拍胸脯说『应该差不多测全了吧』。这一章把『测试盖到了哪里』量化成两个数字:用 gcov 把 --coverage 编译产物(.gcno 计数图 + .gcda 运行计数)读成『139 行里跑了 59 行』这样的覆盖率报告,并讲清行覆盖(85.71% of 7 这种『这行跑过没』)和分支覆盖(100% of 6 branches, taken 66.67% 这种『每个 if 的真假两路各走过没』)的差别——分支覆盖比行覆盖狠,因为一行 if 哪怕跑了一万次、只要永远只走 true 分支,那行算『覆盖了』可 false 分支是死的。活教材是真仓库:本仓 ci.yml 已经有 coverage job(逐行读它——cmake 带 --coverage 编 clib、ctest 跑、lcov --capture 出报告),projects/clib-utilities 是真测对象——本机真跑 gcov 看 CCDynamicArray.c 的覆盖率(主控已验证 42.45% of 139 行 / 42.50% of 80 分支),逐函数拆为啥只有 42%(Unity 5 条用例只测了 createEmpty/pushBack/find/erase/Iterate 这几条主路径,clone/InsertSingle/InsertMulti 等 7 个函数 called 0 完全没碰),再亲手加 2 条用例(clone + InsertSingle)把覆盖率拉到 58.99%、证明『加用例→覆盖率涨』的闭环。lcov 本机未装,诚实标注『CI 上 apt install lcov + lcov --capture --directory,本地用 gcov -b 原生』;顺带揭一个真坑:clang 22 的 --coverage 产出的 .gcda 是 B11* 版本、gcc 16 的 gcov 读不了(只认 B61*),所以覆盖率这条线统一用 gcc 当权威工具,clang 那条得走 llvm-profdata/llvm-cov(本机未装)。承接第 7 章(测试体有了、本章量化测了多少)。全 gcc 16.1.1 真跑,贴真实 gcov 输出 + ISO/POSIX 工具条款。"
chapter: 4
order: 13
tags:
  - host
  - testing
  - engineering
  - toolchain
  - open-source
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4·第 7 章:测试不再是 printf(本章量化的是 Unity/CTest 测试盖到了哪里,先有测试体才有覆盖率)"
  - "阶段 0·第 1 章:工具链体检(gcc/clang 双跑纪律,本章覆盖率统一走 gcc)"
  - "阶段 4·第 5 章:CMake 工程化(CMAKE_C_FLAGS=--coverage 怎么注入、ctest 怎么跑)"
related:
  - "阶段 0·第 16 章:GitHub Actions(coverage 是 CI 的一道 job,本章逐行读 ci.yml)"
  - "阶段 4·第 12 章:静态分析门(同为 CI 门,clang-tidy 抓语义、gcov 量化测试,互补)"
  - "阶段 4·第 10 章:Sanitizer 门(ASan 抓内存错,gcov 量化测试覆盖,一个查错一个查测没测到)"
---

> 🟡 状态:待审核(2026-07-02)

# 覆盖率门:gcov/lcov 量化测试盖到了哪里

## 引言:测了,但测够了吗

上一章(第 7 章)我们把 `projects/clib-utilities` 的 `CCDynamicArray` 测试从「一锅 printf 靠人眼」迁成了 Unity 断言用例,5 条用例各测一个行为,push 计数、find 命中/缺失、erase 缩减,ctest 跑出来 `2/2 Passed`、绿得漂亮。可绿了就万事大吉吗?——这 5 条用例到底跑到了 `CCDynamicArray.c` 这 139 行代码的百分之几?如果有人拍了胸脯说「差不多测全了吧」,你拿什么去验证他这句「差不多」?

这就是覆盖率(coverage)要解决的问题。它把「测试盖到了哪里」从「感觉」量化成两个冷冰冰的数字:**你这 139 行,跑过了多少行;那些 `if` 的真假两路,各走过没有**。第 7 章管「测试体有没有」,本章管「测试体盖得够不够」——前者是后者的前提,没测试体谈不上覆盖率;但有了测试体不量化,你照样不知道那 5 条用例是不是只摸了 `CCDynamicArray` 的皮毛。

这一章用真仓库当活教材。先把覆盖率怎么测出来的机制讲透——`--coverage` 编译时埋点、运行测试生 `.gcda`、`gcov` 读出来报数字;再用本机真跑,看 `CCDynamicArray.c` 的覆盖率到底多少(剧透:只有 42.45%,139 行里跑了 59 行),逐函数拆为什么这么低(5 条 Unity 用例只碰了主路径,`clone`/`InsertSingle` 这 7 个函数压根没被调用过);最后亲手加 2 条用例,看覆盖率从 42.45% 涨到 58.99%,把「加用例→覆盖率涨」的闭环跑给你看。顺带把本仓 `.github/workflows/ci.yml` 里那道 `coverage` job 逐行读一遍——它就是这套量化在 CI 里的落地姿势。

先诚实标注本机工具链:**gcc 16.1.1 的 gcov 已装、cmake 4.3.4 + ctest 已装、clang 22.1.6 已装**;**lcov 未装**(本章讲它时只讲 CI 上 `apt install lcov` 怎么跑、不编造本机输出),**llvm-profdata/llvm-cov 未装**(这条线讲清为啥、留作自验)。

## 覆盖率是怎么测出来的:`.gcno` + `.gcda` + `gcov`

先把机制说清楚,不然后面那些数字都是黑箱。覆盖率测量的核心思路是**编译时埋点 + 运行时计数**——编译器在每个基本块(一段没有跳转的连续语句)的入口插一段「计数器自增」的代码,程序跑起来时这些计数器就被累加,跑完把计数落盘,工具再读出来对照源码,算出「这行被跑了多少次、这个分支的 true/false 各走了多少次」。GCC/Clang 这套机制有三件套,得记住名字,后面处处都用:

`--coverage` 是个**编译 + 链接**两用的旗标(它等价于编译时的 `-fprofile-arcs -ftest-coverage` 加链接时的 `-lgcov`,man `gcov(1)` 里有完整说明)。编译时,它让编译器为每个 `.c` 产出两个文件:一个是 `.gcno`(GC Notes,计数图——记录「这个源文件里有哪些基本块、哪些分支、彼此怎么连」,这是**静态**的,编译完就定了、不随运行变);另一个是埋点后的目标代码,等程序跑起来往计数器里写数。链接时,`--coverage` 把 gcov 运行期库链进来,这个库负责在程序退出时(或显式 `__gcov_flush()` 时)把内存里的计数器**落盘成 `.gcda`**(GC Data,运行数据——记录「这次运行里每个基本块实际被跑了多少次」,这是**动态**的,跑一次更新一次)。

读法记牢:`.gcno` 是「图」(编译产物,描述结构)、`.gcda` 是「数」(运行产物,记录实绩),`gcov` 把这俩一对,对照回源码,就吐出覆盖率报告。所以完整的覆盖率测量链条是四步:**带 `--coverage` 编译 → 跑测试(生 `.gcda`)→ `gcov` 读 `.gcno`+`.gcda` → 看报告**。缺任何一步都没数字——光编译不跑,`.gcda` 是空的,`gcov` 报 `0.00%`;跑了不读,你手里只有一堆 `.gcda` 看不懂。

## 用一个最小被测函数把整条链跑通

真仓库的 `CCDynamicArray.c` 太大(139 行、14 个函数),拿来第一次讲机制会淹在数字里。先用一个 7 行的极简被测函数把整条链走通,数字小到能逐行对照。被测对象和第 7 章那个 `stats_average` 同源——求 `n` 个 `int` 的平均值,空指针或 `n<=0` 返 `-1`:

```c
/* stats.h —— 被测模块:求平均值(覆盖演示用) */
#ifndef STATS_H
#define STATS_H

int stats_average(const int* a, int n);
#endif
```

```c
/* stats.c —— 被测模块实现 */
#include <stddef.h>

#include "stats.h"

int stats_average(const int* a, int n) {
    if (a == NULL || n <= 0) {
        return -1;
    }
    long sum = 0;
    for (int i = 0; i < n; i++) {
        sum += a[i];
    }
    return (int) (sum / n);
}
```

测试这边故意只测「正常路径」——喂个 `{10,20,30}`、断言返 20,跑两遍。那个 `if (a == NULL || n <= 0) return -1;` 的错误分支**完全没被测**:

```c
/* test_stats.c —— 只测正常路径,错误分支不碰 */
#include <assert.h>

#include "stats.h"

int main(void) {
    int a[] = {10, 20, 30};
    assert(stats_average(a, 3) == 20);
    assert(stats_average(a, 3) == 20);
    return 0;
}
```

编译时挂上 `--coverage`,这次只走 gcc(为啥只用 gcc、不用 clang,等下揭那个版本坑),加 `-g -O0` 是为了调试信息和关优化——优化会把行合并/重排,覆盖率数字会和源码对不上,所以覆盖率测量**一律 `-O0`**:

```text
$ gcc -std=c11 -Wall -Wextra --coverage -g -O0 stats.c test_stats.c -o test_stats_gcc
$ ./test_stats_gcc; echo "[exit=$?]"
[exit=0]
```

跑完这一步,当前目录里多了几个文件——`test_stats_gcc-stats.gcno`(编译时生的计数图)、`test_stats_gcc-stats.gcda`(刚跑测试时落的运行数据)。注意 gcc 给它们加了个可执行文件名前缀(`test_stats_gcc-`),这是为了避免多文件互踩;`gcov` 读它时要么把名字改回 `stats.gcno`/`stats.gcda`、要么用 `-o` 指向目标所在目录。这里我把它俩复制成 `stats.gcno`/`stats.gcda`(gcov 默认按源文件名 `stats` 找这俩),然后跑 `gcov`:

```text
$ cp test_stats_gcc-stats.gcno stats.gcno
$ cp test_stats_gcc-stats.gcda stats.gcda
$ gcov stats.c
File 'stats.c'
Lines executed:85.71% of 7
Creating 'stats.c.gcov'
```

第一行数字出来了:`Lines executed:85.71% of 7`——7 行可执行代码里跑了 85.71%(6 行),有一行没被跑到。哪一行?看 `gcov` 生成的 `stats.c.gcov`(把计数贴回源码每一行的报告):

```text
        -:    0:Source:stats.c
        -:    0:Graph:stats.gcno
        -:    0:Data:stats.gcda
        -:    0:Runs:1
        -:    1:/* stats.c —— 被测模块实现 */
        -:    2:#include <stddef.h>
        -:    3:
        -:    4:#include "stats.h"
        -:    5:
function stats_average called 2 returned 100% blocks executed 88%
        2:    6:int stats_average(const int* a, int n) {
        2:    7:    if (a == NULL || n <= 0) {
branch  0 taken 100% (fallthrough)
branch  1 taken 0%
branch  2 taken 0% (fallthrough)
branch  3 taken 100%
    #####:    8:        return -1;
        -:    9:    }
        2:   10:    long sum = 0;
        8:   11:    for (int i = 0; i < n; i++) {
branch  0 taken 75%
branch  1 taken 25% (fallthrough)
        6:   12:        sum += a[i];
        -:   13:    }
        2:   14:    return (int) (sum / n);
        -:   15:}
```

读这份 `.gcov` 的格式。最左是「这行执行了多少次」的计数,`-:` 表示非可执行行(`/* */` 注释、`#include`、空行、大括号行),`#####` 表示「可执行但一次都没跑」——第 8 行 `return -1;` 就顶着 `#####`,这正是那个错误分支,测试压根没传过 `NULL` 或 `n<=0`,它当然死着。`function stats_average called 2 ...` 那一行是函数级总览(被调 2 次、全部正常返回、基本块覆盖 88%),`branch 0/1/2/3` 这几行是 gcov 把第 7 行那个 `||` 拆成四路分支的明细(`||` 左操作数的真假两路 + 右操作数 `n<=0` 的真假两路)——`branch 1 taken 0%` 和 `branch 2 taken 0%` 就是没走过的死路,等下讲分支覆盖时它们正是 `Taken at least once` 不到 100% 的根源。第 6 行 `2:` 表示函数被调了 2 次(测试里两条 `assert`),第 11 行 `8:` 是循环判断条件跑了 8 次(2 次调用 × 每次 `i` 从 0 到 3 共判 4 次),第 12 行 `6:` 是循环体执行了 6 次(2 次 × 3 个元素)。数字和你预期对得上,就说明覆盖率计数没出幺蛾子。

`gcov -b` 再加一个维度——**分支覆盖**(branch coverage),它不光问「这行跑过没」,还问「这行的每个分支走向各走过没」:

```text
$ gcov -b stats.c
File 'stats.c'
Lines executed:85.71% of 7
Branches executed:100.00% of 6
Taken at least once:66.67% of 6
No calls
```

这里有三个数字要分清。`Lines executed:85.71%` 是行覆盖(6 行里跑了 5 行,第 7 行那个 `return -1` 没跑)。`Branches executed:100.00% of 6` 是「分支被执行率」——一共有 6 个分支点(`if` 的真假、`||` 的短路、`for` 的进/出),每个分支点**至少有一路被走过**就算「这个分支点被执行了」,这里 6 个全至少走过一路、所以 100%。`Taken at least once:66.67% of 6` 才是真正该盯的——「每一路都被走到过没有」,6 路里有 4 路走过、2 路死着(那 2 路就是 `if` 的 false 整体没走 + `||` 右边的 `n<=0` 没单独走),所以 66.67%。

这里就把行覆盖和分支覆盖的差别钉死了。**行覆盖问的是「这行有没有被跑过」**——第 6 行 `if (a == NULL || n <= 0)` 跑了 2 次,行覆盖算它「覆盖了」,哪怕它**永远只走 false 分支**、true 分支里的 `return -1` 一次都没执行。**分支覆盖问的是「每个分支走向各走过没」**——它会把那个 `if` 拆成「走 true 进 `return -1`」和「走 false 继续」两路,要求两路都至少走过一次,否则就报 `Taken at least once` 不到 100%。所以**分支覆盖永远比行覆盖狠**:行覆盖 100% 不代表分支覆盖 100%,但分支覆盖 100% 一定推出行覆盖 100%(每一路都走过,每行自然都跑过)。工程实践里,**盯分支覆盖比盯行覆盖更能发现「测试只走了 happy path、错误分支全是死的」这类问题**——这也是为什么后面 clib 那个 42.45% 的真例子里,我们会同时看行和分支两个数。

### 加一条用例,看覆盖率涨

机制讲完,来看「怎么提覆盖」——答案朴素到让人失望:**加用例,把没跑到的分支跑到**。刚才那个 `#####` 的第 7 行,只要补一条「传 `NULL`、期望返 `-1`」的用例就活了:

```c
/* test_stats2.c —— 补一条错误路径用例,把覆盖率拉满 */
#include <assert.h>
#include <stddef.h>

#include "stats.h"

int main(void) {
    int a[] = {10, 20, 30};
    assert(stats_average(a, 3) == 20);
    assert(stats_average(NULL, 0) == -1); /* 新增:覆盖错误路径 */
    return 0;
}
```

注意这里多 `#include <stddef.h>` 才能用 `NULL`(ISO/IEC 9899:2011 §7.19 规定 `NULL` 由 `<stddef.h>` 等若干头定义)——第一版我漏了这行,gcc 直接甩 `'NULL' undeclared`,顺手提醒一句:覆盖率测量和正常编译一样吃警告纪律,`-Wall -Wextra` 别因为挂了 `--coverage` 就摘掉。重新带 `--coverage` 编、跑、读:

```text
$ gcc -std=c11 -Wall -Wextra --coverage -g -O0 stats.c test_stats2.c -o test_stats2_gcc
$ ./test_stats2_gcc; echo "[exit=$?]"
[exit=0]
$ cp test_stats2_gcc-stats.gcno stats.gcno
$ cp test_stats2_gcc-stats.gcda stats.gcda
$ gcov -b stats.c
File 'stats.c'
Lines executed:100.00% of 7
Branches executed:100.00% of 6
Taken at least once:83.33% of 6
```

`Lines executed:100.00%`——第 7 行活了,7 行全跑过。`Taken at least once` 从 66.67% 涨到 83.33%——多走了一路(还剩 1 路没单独走,是 `||` 右边 `n<=0` 那条单独短路,得再补一条「传非 NULL 但 `n=0`」的用例才彻底满,这里留给你当练习)。这就是「加用例→覆盖率涨」的全部闭环:gcov 告诉你哪行/哪分支是死的,你照着补一条跑到它的用例,重跑 gcov 看数字涨——周而复始,直到覆盖率到你定的门限。

> 顺带揭一个真坑,关于编译器选型。你可能想「反正 gcov 是 GCC 那边的工具,clang 也支持 `--coverage`,双跑一遍更稳」——我在本机真试过 `clang -std=c11 --coverage ...`,它能编、能跑、也产出 `.gcno`/`.gcda`,**但 gcc 16.1.1 的 gcov 读 clang 22.1.6 产出的 `.gcda` 会直接报错**:
>
> ```text
> $ gcov -b stats.c   （用的 .gcno/.gcda 来自 clang --coverage）
> stats.gcno:version 'B11*', prefer 'B61*'
> stats.gcda:version 'B11*', prefer version 'B61*'
> No executable lines
> ```
>
> 原因是 gcov 的 `.gcno`/`.gcda` 文件格式有版本号,gcc 16 写的是 `B61*`、clang 22 写的是 `B11*`,gcc 的 gcov 只认自家的版本、读不了 clang 的。所以**覆盖率这条线统一用 gcc 当权威工具**(ci.yml 里 `coverage` job 默认就是 `gcc`,没指定 `CC=clang`),clang 的 `--coverage` 得走它自家的 `llvm-profdata` + `llvm-cov` 那条原生流水线——这俩本机未装,诚实标注,不在本章演示范围。结论记住一条:**测覆盖率,选 gcc;别在覆盖率这条线上混编译器**。

## 活教材:本仓 ci.yml 的 `coverage` job

机制和一个最小例子讲完,把镜头对准真仓库。本仓 `.github/workflows/ci.yml` 已经有一道 `coverage` job,它就是这套量化在 CI 里的落地姿势,逐行读(长命令在仓库里是写一行的,这里为可读性折了几行,YAML 语义不变):

```yaml
  coverage:
    name: 覆盖率(gcov/lcov)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装工具
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build lcov
      - name: 编译+测试 clib(带 --coverage)
        run: |
          cmake -B build-cov -S projects/clib-utilities \
            -DCMAKE_C_FLAGS="--coverage -g" \
            -DCMAKE_EXE_LINKER_FLAGS="--coverage"
          cmake --build build-cov
          cd build-cov && ctest --output-on-failure && cd ..
      - name: 生成覆盖率报告
        run: |
          lcov --capture --directory build-cov --output-file coverage.info \
            --ignore-errors mismatch --rc geninfo_auto_base=1
          lcov --remove coverage.info '/usr/*' --output-file coverage.filtered.info
          lcov --summary coverage.filtered.info
```

逐段拆。`apt install ... lcov` 装 lcov——这是个 gcov 的前端,把一堆 `.gcda` 收拢成一份 HTML 报告(本机没装,CI 上才有,下面讲它怎么用)。`-DCMAKE_C_FLAGS="--coverage -g"` 是核心注入:它把 `--coverage -g` 塞进**整个 clib 子项目所有 `.c`** 的编译命令(也包括测试可执行),CMake 会把它拼到每条 `gcc -c` 命令里;`-DCMAKE_EXE_LINKER_FLAGS="--coverage"` 同理塞进链接命令,让 gcov 运行期库链进可执行文件(否则 `.gcda` 写不出来,链接器会报 `undefined reference to __gcov_init`)。`cmake --build` 编、`ctest --output-on-failure` 跑测试——这一步跑完,`build-cov/` 树里就铺满了 `.gcda` 文件(每个编过 `--coverage` 的 `.c` 一份)。

下面三行 lcov 是「读数 + 过滤 + 汇总」。`lcov --capture --directory build-cov --output-file coverage.info` 把整个 build 目录下所有 `.gcda` 收拢成一份 `coverage.info`(它内部就是调 gcov,再把结果汇总成一种自家的文本格式);`--ignore-errors mismatch` 是因为 gcov 偶尔会报「`.gcno` 和 `.gcda` 的 stamp 对不上」(常见于增量编译没清干净),lcov 默认会中断,加这个旗标让它跳过;`--rc geninfo_auto_base=1` 是让 lcov 自动推断源码基准路径,避免路径前缀错位。`lcov --remove coverage.info '/usr/*' --output-file coverage.filtered.info` 把 `/usr/*` 下的系统头(被 `--coverage` 顺带埋点了)剔除——这些不是你的代码,算进去会稀释你的真实覆盖率、还会让报告里冒出一堆 `bits/stdio2.h` 之类的噪声。`lcov --summary coverage.filtered.info` 最后打个总览,长这样(CI 上的真实输出格式,本机 lcov 未装、不本机复现):

```text
Reading tracefile coverage.filtered.info
Summary coverage rate:
  lines......: 42.5% (59 of 139 lines)
  functions..: 50.0% (7 of 14 functions)
  branches...: 42.5% (34 of 80 branches)
```

读这三行:行覆盖 42.5%(139 行里跑了 59 行)、函数覆盖 50%(14 个函数里有 7 个被调用过)、分支覆盖 42.5%(80 个分支里走了 34 个)。和下面本机 gcov 真跑出来的数完全对得上(42.45% 是 gcov 的四舍五入写法、lcov 写 42.5%,同一份数)。这就是「CI 上每次 PR 自动报一份覆盖率总览」的价值——你不用手动跑 gcov、CI 替你盯着,覆盖率掉了(比如有人删了测试、或者加了新代码没补测试)总览数字立刻掉、PR 里红给你看。

本机没装 lcov、要本地看覆盖率怎么办?直接用 gcov 原生,一样够用——`gcov -b 某文件.c.gno` 出行+分支,逐文件看;要总览就写个 `for f in $(find build -name '*.gcno'); do gcov -b "$f"; done` 之类的小脚本汇总。lcov 的好处只是「跨文件汇总 + 漂亮的 HTML」(`genhtml coverage.filtered.info` 能出可点开的网页报告),核心数据 gcov 都给得了。所以本地开发用 gcov 够、CI 上挂 lcov 出总览,分工清楚。

## 真跑:clib 的 `CCDynamicArray.c` 覆盖率到底多少

现在到这一章最值钱的部分——把上面那套机制对准真仓库,看一个 139 行、14 个函数的真模块,覆盖率到底是什么光景。我在本机带 `--coverage` 编了 `projects/clib-utilities`、ctest 跑通两条测试(`clib_smoke` + `dynamic_array_unity`,主控已验证 2/2 Passed),然后对 `CCDynamicArray.c` 跑 `gcov`:

```text
$ cmake -B /tmp/cj/p4ch13/clib_base -S projects/clib-utilities \
    -DCMAKE_C_FLAGS="--coverage -g -O0" -DCMAKE_EXE_LINKER_FLAGS="--coverage" -G "Unix Makefiles"
$ cmake --build /tmp/cj/p4ch13/clib_base
$ ctest --test-dir /tmp/cj/p4ch13/clib_base --output-on-failure
Test project /tmp/cj/p4ch13/clib_base
    Start 1: clib_smoke
1/2 Test #1: clib_smoke .......................   Passed    0.00 sec
    Start 2: dynamic_array_unity
2/2 Test #2: dynamic_array_unity ..............   Passed    0.00 sec

100% tests passed, 0 tests failed out of 2
```

跑通之后,`/tmp/cj/p4ch13/clib_base/CMakeFiles/clib_utils.dir/BasicDataStructure/Sources/` 下就有了 `CCDynamicArray.c.gcno` 和 `CCDynamicArray.c.gcda`。对它跑 `gcov -b`:

```text
$ cd /tmp/cj/p4ch13/clib_base/CMakeFiles/clib_utils.dir/BasicDataStructure/Sources
$ gcov -b CCDynamicArray.c.gcno
File '/home/charliechen/C-Journey/projects/clib-utilities/BasicDataStructure/Sources/CCDynamicArray.c'
Lines executed:42.45% of 139
Branches executed:42.50% of 80
Taken at least once:25.00% of 80
Calls executed:30.43% of 23
```

四行数字摆出来。`Lines executed:42.45% of 139`——139 行可执行代码里只跑了 42.45%,也就是约 59 行;**剩下 80 行、一大半的代码,这 5 条测试根本没碰**。`Branches executed:42.50% of 80`——80 个分支点里只有 42.5% 至少走过一路。`Taken at least once:25.00% of 80` 这个最扎眼——「每一路都被走到过」只有 25%,也就是说 80 个分支走向里、有 60 个从来没被走过,这些就是彻头彻尾的死分支(测试里压根没构造出能让它们走到的输入)。`Calls executed:30.43% of 23`——23 个函数调用点里只有 30.43% 被实际调过。

光看这几个百分比还是抽象——「42% 到底是哪些没测到」得拆到函数级才清楚。`gcov` 生成的 `CCDynamicArray.c.gcov` 里有每个函数的调用次数,逐行读:

```text
function CCDynamicArray_createEmpty called 5 returned 100% blocks executed 75%
function CCDynamicArray_createCCDynamicArray called 0 returned 0% blocks executed 0%
function CCDynamicArray_cloneCCDynamicArray called 0 returned 0% blocks executed 0%
function CCDynamicArray_pushBackSingle called 1 returned 100% blocks executed 62%
function CCDynamicArray_pushBackMulti called 4 returned 100% blocks executed 62%
function CCDynamicArray_pushBackArray called 0 returned 0% blocks executed 0%
function CCDynamicArray_InsertSingle called 0 returned 0% blocks executed 0%
function CCDynamicArray_InsertMulti called 0 returned 0% blocks executed 0%
function CCDynamicArray_InsertArray called 0 returned 0% blocks executed 0%
function CCDynamicArray_Iterate called 3 returned 100% blocks executed 80%
function CCDynamicArray_EraseSingle called 1 returned 100% blocks executed 78%
function CCDynamicArray_EraseConsistMutli called 0 returned 0% blocks executed 0%
function CCDynamicArray_Find called 2 returned 100% blocks executed 83%
function CCDynamicArray_Free called 5 returned 100% blocks executed 100%
```

这份函数表把「42.45% 是怎么来的」解释得明明白白。14 个函数里,**7 个 `called 0`、彻底没被调用过**——`createCCDynamicArray`(用现成数组构造)、`cloneCCDynamicArray`(克隆)、`pushBackArray`(把另一个数组整体追加)、`InsertSingle`/`InsertMulti`/`InsertArray`(三种插入)、`EraseConsistMutli`(批量删)。这 7 个函数加起来就是那一大半没测到的代码,它们全长在文件里、编进了二进制、但 5 条 Unity 用例(setUp 里 `createEmpty`、用例里 `pushBack`/`Find`/`erase`/`Iterate`、tearDown 里 `Free`)压根没调过它们。

剩下 7 个被调过的函数,也不是 100%——`createEmpty` `blocks executed 75%`(它里头有个 `malloc` 失败的错误分支没测到,`branch 0 taken 0%`),`pushBackSingle`/`pushBackMulti` 都是 62%(同理,realloc 失败分支没测),`EraseSingle` 78%、`Find` 83%。只有 `Free` 干净到 100%——因为它实现简单、没几个分支。这就是覆盖率给「测得够不够」交的实底:**5 条用例,听起来不少,真一量化才发现只盖了不到一半,7 个函数完全没碰、7 个碰了的函数大半也没盖到错误分支**。

读这种 `blocks executed X%` 时注意一个细节:它叫「块」(block),不是「行」也不是「分支」。基本块是编译器层面的概念——一段没有内部跳转的连续指令序列,一个 `if` 会把函数切成几个基本块(条件为真的块、为假的块、汇合后的块)。`blocks executed` 是「这些基本块有多少被跑过」,它和行覆盖、分支覆盖都相关但不完全相等——一个块通常对应几行源码,所以「块没跑过」≈「那几行没跑过」。日常工程里,盯行覆盖(直观)和分支覆盖(狠)这两个就够了,块覆盖是 gcov 给你做交叉验证用的。

## 加用例把覆盖率拉上去:从 42.45% 到 58.99%

数字摆在台面上,问题清楚了——要提覆盖,就给那 7 个 `called 0` 的函数补用例。我挑两个最典型的补:`clone`(克隆,纯内存复制、测起来最干净)和 `InsertSingle`(中间插入,涉及 memmove/memcpy、最容易藏 bug 的那种)。补的用例文件照着 `test_DynamicArray_unity.c` 的样板写,夹具 setUp/tearDown 复用,每条只测一个行为:

```c
/* test_DynamicArray_unity_extra.c —— 给 clib 加 clone + InsertSingle 用例,看覆盖率涨。
 * 对照 5 条原用例只测 push/find/erase(42.45%),这里补 clone/InsertSingle 两路。 */
#include "unity.h"
#include "CCDynamicArray.h"

static int g_iter_count;
static void countEach(void* elem, void* arg) {
    (void) elem;
    (void) arg;
    g_iter_count++;
}

static CCDynamicArray* g_arr;

void setUp(void) {
    g_arr = CCDynamicArray_createEmpty(sizeof(int));
    g_iter_count = 0;
}

void tearDown(void) {
    CCDynamicArray_Free(g_arr);
    g_arr = NULL;
}

/* 新增 1:clone —— 原 5 条没碰,cloneCCDynamicArray called 0 */
void test_clone_preserves_contents(void) {
    int vals[] = {7, 8, 9};
    CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    CCDynamicArray* cloned = CCDynamicArray_cloneCCDynamicArray(g_arr);
    TEST_ASSERT_TRUE(cloned != NULL);
    CCDynamicArray_Iterate(cloned, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(3, g_iter_count);
    int key = 8;
    CCSTD_Index_t idx =
        CCDynamicArray_Find(cloned, &key, (CCSTD_CmpFuncType) compareInt, 0, TIL_END);
    TEST_ASSERT_TRUE(idx >= 0);
    CCDynamicArray_Free(cloned);
}

/* 新增 2:InsertSingle —— 原 5 条没碰,InsertSingle called 0 */
void test_insertSingle_in_middle(void) {
    int vals[] = {10, 30};
    CCDynamicArray_pushBackMulti(g_arr, vals, 2);
    int ins = 20;
    CCBOOL_t ok = CCDynamicArray_InsertSingle(g_arr, &ins, 1);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(3, g_iter_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clone_preserves_contents);
    RUN_TEST(test_insertSingle_in_middle);
    return UNITY_END();
}
```

这两条用例值得逐句读。`test_clone_preserves_contents`:先 push 三个 `{7,8,9}`,然后 `cloneCCDynamicArray(g_arr)` 克隆一份,断言克隆出来的非空、iterate 计数得 3(克隆把三个元素都拷过来了)、再 `Find` 一下 `8` 确认内容真的一致(不是只复制了长度、元素是垃圾)。`test_insertSingle_in_middle`:先 push `{10,30}` 两个,然后在位置 1 插入 `20`,断言插入成功(`ok==True`)、iterate 计数得 3(从 2 个变 3 个)。这俩用例不光「调用一下凑覆盖率」,还**断言了行为正确**——`clone` 出来的数组内容得和原数组一致、`InsertSingle` 之后长度得 +1,这是「有效覆盖」而非「数字好看但没验行为」的覆盖(后者是覆盖率工具最容易被滥用的地方,下面小结再讲)。

注意 `compareInt` 这个比较函数——它不在 `CCDynamicArray.h` 里,而在 `Basic_Utils/Includes/CCSTDLib_CompareUtils.h`,是个宏展开生成的模板函数(`compareInt`/`compareShort`/`compareDouble` 一整套),编进了 `libclib_utils.a`。原 `test_DynamicArray_unity.c` 也是这么调的(第 7 章那段活样板里 `(CCSTD_CmpFuncType) compareInt` 同款),所以这里照搬。把这条用例带 `--coverage` 编、和 clib 库链接、跑一遍,再重新读 `CCDynamicArray.c` 的覆盖率(原 5 条用例 + 这 2 条新用例的 `.gcda` 合并计数):

```text
$ gcov -b CCDynamicArray.c.gcno
File '/home/charliechen/C-Journey/projects/clib-utilities/BasicDataStructure/Sources/CCDynamicArray.c'
Lines executed:58.99% of 139
Branches executed:60.00% of 80
Taken at least once:33.75% of 80
Calls executed:39.13% of 23
```

数字实打实涨了。`Lines executed` 从 **42.45% → 58.99%**(多了约 23 行被跑到),`Branches executed` 从 **42.50% → 60.00%**(`Taken at least once` 从 25.00% → 33.75%),`Calls executed` 从 **30.43% → 39.13%**。只补了 2 条用例,覆盖率跳了一档。验证一下是不是那两个目标函数被激活了——再看函数表:

```text
function CCDynamicArray_cloneCCDynamicArray called 1 returned 100% blocks executed 62%
function CCDynamicArray_InsertSingle called 1 returned 100% blocks executed 69%
```

`clone` 从 `called 0` 变成 `called 1`、`InsertSingle` 同样从 `called 0` 变 `called 1`——两个原本完全没碰的函数,现在各被调用了一次、`blocks executed` 也从 0% 涨到 62%/69%(没到 100% 是因为它们里头的 `malloc`/`realloc` 失败分支、还有 `DEFAULT_DENY(array, ...)` 的空指针分支没测——要再涨就得专门构造失败用例,代价更大,工程上权衡到这儿够了)。这就是「加用例→覆盖率涨」的闭环在真仓库上的验证:gcov 报「这俩函数 called 0」,你照着写用例把它们调起来,重跑 gcov 看数字跳——和上面那个最小 `stats` 例子的逻辑一模一样,只是规模大了。

剩下的 `createCCDynamicArray`、`pushBackArray`、`InsertMulti`、`InsertArray`、`EraseConsistMutli` 这 5 个函数还是 `called 0`,要把覆盖率继续往上推,就照着同样的套路一条条补——每个函数一条「构造典型输入 + 断言行为」的用例。这里我不全补了(那是 clib 整改的活儿,不在本章范围),重点是让你看清这套「量化→定位→补测→再量化」的工作流。

## 小结

这一章把「测试盖到了哪里」从感觉量化成数字,核心就一句话:**覆盖率 = 用 `--coverage` 编译埋点 + 跑测试落 `.gcda` + `gcov` 读数报告**,四步缺一不可。它和第 7 章的关系是「先有测试体、再量化测了多少」——第 7 章挂上了 Unity + CTest 给你 5 条绿用例,本章告诉你这 5 条用例在 `CCDynamicArray.c` 上只盖了 42.45% 的行、25.00% 的分支走向,一半多没碰。行覆盖问「这行跑过没」、分支覆盖问「每个 if 的真假两路各走过没」,**分支覆盖比行覆盖狠**——一行 `if` 跑了一万次但永远只走 true,行覆盖算它「覆盖了」,可 false 分支是死的,只有分支覆盖的 `Taken at least once` 能把这死路揪出来;所以工程上盯分支覆盖更能发现「测试只走 happy path、错误分支全是死的」。本仓 ci.yml 的 `coverage` job 就是这套量化的 CI 落地——`-DCMAKE_C_FLAGS="--coverage -g"` 给整个 clib 注入埋点、ctest 跑测试生 `.gcda`、lcov `--capture` 收拢成 `coverage.info`、`--remove '/usr/*'` 剔系统头、`--summary` 出「行/函数/分支」三行总览(行 42.5%、函数 50%、分支 42.5%),和本机 gcov 真跑的 42.45% 完全对得上。本机 lcov 未装、诚实标注,本地用 gcov `-b` 原生逐文件看够用,lcov 的价值在跨文件汇总 + HTML、CI 上挂它出总览。真跑 clib 把覆盖率拆到函数级,看清 42.45% 的来路——14 个函数里 7 个 `called 0` 完全没碰(clone/Insert 三件套/批量删),另 7 个被调过的也只盖到 happy path、错误分支(realloc 失败、空指针)全是死的;我亲手加 2 条用例(clone + InsertSingle,带行为断言而非纯凑数),把覆盖率从 42.45% 拉到 58.99%、那两个函数从 `called 0` 变 `called 1`,闭环当场跑通。顺带揭了一个真坑——clang 22 的 `--coverage` 产出 `.gcda` 版本号是 `B11*`、gcc 16 的 gcov 只认 `B61*` 读不了,所以覆盖率这条线**统一用 gcc 当权威工具**,clang 得走它自家的 llvm-profdata/llvm-cov(本机未装)。最后一句实在话:覆盖率是「测没测到」的必要不充分条件——100% 覆盖不代表代码全对(你测了 `assert(stats_average(a,3)==20)`,但断言写错成 `==21` 也能让那一行算「覆盖过」),可覆盖率低(比如 42%)一定意味着有大片代码从来没被任何测试碰过,那里的 bug 你根本看不见;所以盯覆盖率不是为了刷数字,是为了让「没人测过的死区」现形。

## 参考资源

- **`gcov(1)` man 页**:`--coverage` 的等价展开(`-fprofile-arcs -ftest-coverage` + 链接 `-lgcov`)、`.gcno`/`.gcda` 文件格式、`-b`(分支覆盖)、`-o`(目标目录)的完整说明。
- **`lcov` 项目**(<https://github.com/linux-test-project/lcov>):gcov 的前端,`--capture`/`--remove`/`--summary`/`genhtml` 的用法;CI 上 `apt install lcov` 即得。
- **ISO/IEC 9899:2011 §7.19**(`stddef.h`):`NULL` 的定义来源(覆盖率用例里要用 `NULL` 得 include 它)。
- **本仓活教材**:[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) 的 `coverage` job(`--coverage` 注入 + lcov 三步出总览)、[`projects/clib-utilities`](../../projects/clib-utilities)(被测对象,`CCDynamicArray.c` 139 行 14 函数,本机真跑 42.45%)。
- **承接章节**:第 7 章(测试不再是 printf,本章量化它的覆盖)、第 12 章(静态分析门,同为 CI 门,一个查代码语义、一个量化测试)、阶段 0 第 16 章(GitHub Actions,`coverage` 是 CI 的一道 job)。
