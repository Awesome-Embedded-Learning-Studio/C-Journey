---
title: "静态分析门:clang-tidy 与 cppcheck 进 CI"
description: "编译器警告(-Wall -Wextra)查语法和类型错、sanitizer 查运行时错,可总有一类问题它们俩都管不到——『代码能编过、但写得不对』的语义毛病:隐式 narrowing 截断、双下划线开头的保留标识符、缺括号让 else 配错 if。这一章讲 CI 的第三层防线——编译时静态分析。先用三个亲手复现的小程序证明 clang-tidy 抓到的 finding 里有的是 gcc/clang 的 -Wall -Wextra 一声不吭(双下划线标识符、缺括号)、有的是 -Wextra 不管只有 -Wconversion 才顺手抓到(narrowing),把『静态分析到底补在哪』说准。再把本仓这套活教材逐行读透:根目录 .clang-tidy 选了 bugprone/performance/readability 三族、关了教程会刷屏的 magic-numbers/cognitive-complexity,scripts/clang_tidy_check.py 为每个 CMake 子项目配 compile_commands.json 再跑 clang-tidy -p,.github/workflows/ci.yml 的 static-analysis 是第 5 道 CI 门——真在本仓跑这个脚本(examples 全过、退出 0)。cppcheck 本机未装,诚实标注,只讲怎么装怎么跑、不编造输出。最后把 projects/clib-utilities 现存的 4 个真 finding 当活样板——narrowing conversion in CCSTDLib_FetchError.c:44、reserved identifier __CCThread_TrampolineArg、int→ptr in CCThread.c:117、缺括号×2——逐条给修法,并说明它为啥暂未纳入硬门(legacy 整改中)。承接阶段0 Ch10 sanitizer 是运行时、本章是编译时静态。gcc 16.1.1 + clang 22.1.6 双真跑,贴真实 clang-tidy 输出 + ISO §7.1.3 保留标识符条款。"
chapter: 4
order: 12
tags:
  - host
  - engineering
  - toolchain
  - testing
  - open-source
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 8 章:警告旗标进阶(-Wall -Wextra 是 best-effort、有盲区,本章补它的编译时缺口)"
  - "第 10 章:ASan+UBSan 深入(运行时 sanitizer 是动态插桩,本章是编译时静态分析,两者互补)"
  - "阶段 0·第 17 章:clang-format(代码格式统一;本章是 clang-tidy 语义分析,纯增量、别混)"
  - "第 5 章:CMake 工程化(EXPORT_COMPILE_COMMANDS 产出 compile_commands.json 是 clang-tidy 的输入)"
related:
  - "阶段 0·第 16 章:GitHub Actions(static-analysis 是 CI 第 5 道门,本章逐行读 ci.yml)"
  - "第 1 章:头文件契约(reserved identifier 这条直接对应 ISO §7.1.3,头文件契约的延伸)"
---

# 静态分析门:clang-tidy 与 cppcheck 进 CI

## 引言:编译器警告和 sanitizer 之外,还有一片没人管的角落

到这里你已经攒下两道防线了。第一道是**编译器警告**——阶段 0 第 8 章我们真跑过 `-Wall -Wextra`,看它抓 `unused-parameter`、抓 `=` 写成 `==`、抓条件里的赋值;也亲眼见过它的盲区:`-Wuninitialized` 在条件分支里干脆一声不吭,所以那条铁律叫「警告是 best-effort、没 warning 不等于没 bug」。第二道是 **sanitizer**——本阶段第 10 章我们复现了本仓 CI 的 `sanitize` job,看 ASan 给 use-after-free 拽出三段栈、UBSan 把有符号溢出精确报到行列,代价是约 2 倍运行开销、而且**必须把程序跑起来才抓得到**。

可就有这么一类问题,这两道防线都够不着。看一眼这段代码:

```c
typedef struct __MyHiddenState { /* 双下划线开头 */
    int counter;
} __MyHiddenState;
int _Upper_started = 0; /* 下划线 + 大写 */
```

它编得过去,`-Wall -Wextra` 静默通过,跑起来也不崩、sanitizer 没话说——**但它违反了 ISO C**。C 标准把「双下划线开头」和「下划线 + 大写字母开头」的标识符**保留给实现**(编译器/标准库)专用,你占了这两个起名空间,就有可能撞上编译器将来的内部宏、或者标准库的内部符号,出问题时报错信息会匪夷所思(条款见 ISO/IEC 9899:2011 §7.1.3,后面贴原文)。编译器对此**完全不警告**——它默认你有意这么写,毕竟这是「保留」不是「禁止」;sanitizer 更管不着,这是编译时的事、运行时根本看不见。这类「能编过、跑也没事、但写得不对」的语义毛病,就是**静态分析**要补的缺口。

这一章我们把第三层防线立起来。先在 `/tmp/cj/p4ch12/` 写三个故意有 finding 的小程序,真跑 clang-tidy,看它抓什么、又怎么和编译器警告错位互补;再把本仓这套「活教材」逐行读透——根目录的 `.clang-tidy`(为啥选这三族、为啥关掉那几个)、`scripts/clang_tidy_check.py`(怎么给每个 CMake 子项目配 `compile_commands.json`、怎么跑)、`.github/workflows/ci.yml` 的 `static-analysis` job(全仓第 5 道 CI 门),并在本仓真跑这个脚本(退出 0、examples 全过);最后把 `projects/clib-utilities` 现存的 4 个真 finding 摆出来当「真实工程里静态分析一开就一堆 finding」的活样板,逐条给修法。

先诚实标注本机工具链:**clang-tidy 22.1.6 已装**、**cmake 4.3.4 已装**、**gcc 16.1.1 / clang 22.1.1 双在线**;**cppcheck 未装**(本章讲它时只讲怎么装怎么跑、不编造输出)。

## 静态分析到底补在哪:三类 finding 的对照

把「静态分析补编译器的盲区」这句话讲准,最好的办法是把同一个 finding 在编译器和 clang-tidy 两边都跑一遍,看谁管谁不管。我准备了三个小程序,各自代表一种典型的错位关系。

### 第一类:narrowing conversion——`-Wextra` 不管,`-Wconversion` 和 clang-tidy 都管

把一个 `long` 塞进 `int`、可能截断丢数据,这是 narrowing conversion(窄化转换):

```c
/* narrow.c — 故意触发 bugprone-narrowing-conversions */
#include <stdio.h>

long compute_total(long big) {
    return big + 1000;
}

int main(void) {
    long big = 1L << 40;            /* 一个装不进 int 的大数 */
    int small = compute_total(big); /* long → int 隐式截断:narrowing */
    printf("small = %d\n", small);
    return 0;
}
```

先让编译器说话——`-Wall -Wextra` 这套标配,两个编译器**都一声不吭**:

```text
$ gcc -std=c11 -Wall -Wextra -c narrow.c -o narrow.o
$                                     ← 静默通过,没 warning
$ clang -std=c11 -Wall -Wextra -c narrow.c -o narrow.o
$                                     ← 同样静默
```

`-Wall -Wextra` 不管 narrowing,这一点阶段 0 第 8 章提过一句——它得靠更激进的 `-Wconversion` 才顺手抓到:

```text
$ gcc -std=c11 -Wall -Wextra -Wconversion -c narrow.c -o narrow.o
narrow.c: In function 'main':
narrow.c:10:18: warning: conversion from 'long int' to 'int' may change value [-Wconversion]
   10 |     int  small = compute_total(big);  /* long → int 隐式截断:narrowing */
      |                  ^~~~~~~~~~~~~
$ clang -std=c11 -Wall -Wextra -Wconversion -c narrow.c -o narrow.o
narrow.c:10:18: warning: implicit conversion loses integer precision: 'long' to 'int' [-Wshorten-64-to-32]
   10 |     int  small = compute_total(big);  /* long → int 隐式截断:narrowing */
      |          ~~~~~   ^~~~~~~~~~~~~~~~~~
1 warning generated.
```

所以 narrowing 这一类严格说是「`-Wextra` 默认不抓、`-Wconversion` 才顺手抓」——它落在编译器和静态分析的**交集**里。那为什么 clang-tidy 还要再抓一遍?因为 `-Wconversion` 在老代码上会刷一大片、很多项目不敢开,而 clang-tidy 的 `bugprone-narrowing-conversions` 可以单独配置、和别的 check 组合成一道**独立于编译器 flags 的门**——CI 不动 `CFLAGS`、只跑 clang-tidy,照样能把 narrowing 拦下来。这就是「静态分析作为独立防线」的价值之一。真跑 clang-tidy 给你看(`-p` 指向 CMake 产出的 `compile_commands.json`,下一节讲它怎么来的):

```text
$ clang-tidy -p /tmp/cj/p4ch12/build narrow.c
15 warnings generated.
/tmp/cj/p4ch12/narrow.c:10:18: warning: narrowing conversion from 'long' to signed type 'int' is implementation-defined [bugprone-narrowing-conversions]
   10 |     int  small = compute_total(big);  /* long → int 隐式截断:narrowing */
      |                  ^
Suppressed 14 warnings (14 in non-user code).
Use -header-filter=.* or leave it as default to display errors from all non-system headers. ...
```

注意 clang-tidy 报的措辞和编译器不一样——它说 `narrowing conversion from 'long' to signed type 'int' is implementation-defined`,这一句「**implementation-defined**」点出了本质:有符号整数窄化的结果落在 ISO/IEC 9899:2011 §6.3.1.3 那一档(「否则结果由实现定义」),不是 UB、但**换编译器就可能不一样**。底部那行 `Suppressed 14 warnings (14 in non-user code)` 是 clang-tidy 故意把 `<stdio.h>` 之类系统头里的 finding 压掉了——`.clang-tidy` 里的 `HeaderFilterRegex` 在干这个活,只报你自己代码的 finding、不刷系统头的噪声。

### 第二类:缺括号——编译器**完全不管**,clang-tidy 独占

narrowing 好歹还有 `-Wconversion` 这个编译器同伴。下面这类就完全是 clang-tidy 的独占领地了——「`if`/`else`/`for` 的单语句体该不该加花括号」:

```c
/* dangle_else.c — 故意触发 readability-braces-around-statements + dangling else */
#include <stdio.h>

int classify(int x, int flag) {
    if (flag)
        if (x > 0) /* 内层 if 没括号,else 配谁? */
            return 1;
        else /* 这个 else 实际配内层 if,但缩进在骗你 */
            return 2;
    return 0;
}

int main(void) {
    printf("%d\n", classify(5, 1));
    return 0;
}
```

先说**真正要命的那个 bug**:这里有个 dangling else(悬空 else)。C 的语法规则是「`else` 配最近的那个能配的 `if`」(ISO/IEC 9899:2011 §6.5.4.1 的语法,else binds to nearest if),所以那个看似配外层 `if (flag)` 的 `else`,**实际配的是内层 `if (x > 0)`**——缩进在骗你。`flag=1`、`x=5` 时你以为走 `if (x > 0) return 1`,没错;可 `flag=1`、`x=-5` 时你以为「外层 if 命中、走内层、x>0 不成立、没 return、最后 return 0」,实际却是「`else` 配内层、`x=-5` 触发 `return 2`」。这种 bug 调起来能把人逼疯,因为**你读代码时眼睛跟着缩进走、编译器却跟着语法走**。

这件最要命的事,编译器**其实是会**抓的——`-Wdangling-else`(gcc 和 clang 都把它收在 `-Wall` 里)正好管:

```text
$ gcc -std=c11 -Wall -Wextra -c dangle_else.c -o dangle_else.o
dangle_else.c: In function 'classify':
dangle_else.c:5:8: warning: suggest explicit braces to avoid ambiguous 'else' [-Wdangling-else]
    5 |     if (flag)
      |        ^
```

但「单语句体要不要加花括号」这件**风格**上的事,编译器**完全不吭声**——`-Wall -Wextra -Wpedantic -Wconversion` 全开,它也不会说「你 `if (x > 0) return 1;` 该写成 `if (x > 0) { return 1; }`」。这件事只有 clang-tidy 的 `readability-braces-around-statements` 管。真跑:

```text
$ clang-tidy -p /tmp/cj/p4ch12/build dangle_else.c
18 warnings generated.
/tmp/cj/p4ch12/dangle_else.c:5:14: warning: statement should be inside braces [readability-braces-around-statements]
    5 |     if (flag)
      |              ^
      |               {
    6 |         if (x > 0)           /* 内层 if 没括号,else 配谁? */
    7 |             return 1;
    8 |         else                 /* 这个 else 实际配内层 if,但缩进在骗你 */
    9 |             return 2;
      |
/tmp/cj/p4ch12/dangle_else.c:6:19: warning: statement should be inside braces [readability-braces-around-statements]
    6 |         if (x > 0)           /* 内层 if 没括号,else 配谁? */
      |                   ^
      |                    {
    7 |             return 1;
    8 |         else                 /* 这个 else 实际配内层 if,但缩进在骗你 */
      |         }
/tmp/cj/p4ch12/dangle_else.c:8:13: warning: statement should be inside braces [readability-braces-around-statements]
    8 |         else                 /* 这个 else 实际配内层 if,但缩进在骗你 */
      |             ^
      |              {
    9 |             return 2;
      |
Suppressed 15 warnings (14 in non-user code, 1 with check filters).
```

clang-tidy 把三个该加括号的位置(`if (flag)`、内层 `if (x > 0)`、`else`)**逐个**点出来,而且还在建议里直接给你画出该插 `{` 和 `}` 的位置(输出里 caret 行的下一行就印着一个孤零零的 `{`,那就是它建议你补上的花括号)。这种「风格统一」的活,编译器管不着(它是风格、不是正确性),sanitizer 更管不着(运行时不体现)——只有静态分析这种「**纯模式匹配 + 数据流分析**」的工具能管。把它收进 CI 硬门,全工程的 `if`/`else`/`for`/`while` 单语句体就被强制戴上了花括号,dangling else 这类 bug 顺带也就没了藏身之处(因为一旦加了括号,else 配谁就由括号写死了、不再有歧义)。

### 第三类:reserved identifier——编译器**完全不管**,clang-tidy 独占,还违反 ISO

回到开头那段双下划线标识符:

```c
/* reserved.c — 故意触发 bugprone-reserved-identifier */
#include <stdio.h>

/* 双下划线开头:ISO C 保留给实现(编译器/库)的标识符 */
typedef struct __MyHiddenState {
    int counter;
} __MyHiddenState;

/* _大写开头:同样保留(文件作用域) */
int _Upper_started = 0;

int main(void) {
    __MyHiddenState s = {0};
    s.counter++;
    _Upper_started = 1;
    printf("%d %d\n", s.counter, _Upper_started);
    return 0;
}
```

这段代码编得过、`-Wall -Wextra -Wpedantic` 全开也一声不吭——编译器**完全不管**你占没占保留标识符。我专门验证过:

```text
$ gcc -std=c11 -Wall -Wextra -Wpedantic -c reserved.c -o reserved.o
$                                     ← 静默通过
$ clang -std=c11 -Wall -Wextra -Wpedantic -c reserved.c -o reserved.o
$                                     ← 同样静默
```

可它确实违反标准。ISO/IEC 9899:2011 §7.1.3「Reserved identifiers」第 1 段白纸黑字写着(原文转述):所有包含双下划线的、以下划线开头再加一个大写字母的、以及以下划线开头的(文件作用域)标识符,**总是保留给实现使用**——也就是说,这些名字你不能拿来给自己用,标准把它们留给了编译器和标准库。你占了,今天没事不代表明天没事:编译器升级、或者换一个用这些名字做内部宏的库,你的代码就会以最莫名其妙的方式展开错。clang-tidy 的 `bugprone-reserved-identifier` 就是专门管这条的,真跑:

```text
$ clang-tidy -p /tmp/cj/p4ch12/build reserved.c
17 warnings generated.
/tmp/cj/p4ch12/reserved.c:5:16: warning: declaration uses identifier '__MyHiddenState', which is a reserved identifier [bugprone-reserved-identifier]
    5 | typedef struct __MyHiddenState {
      |                ^~~~~~~~~~~~~~~
      |                MyHiddenState
/tmp/cj/p4ch12/reserved.c:7:3: warning: declaration uses identifier '__MyHiddenState', which is a reserved identifier [bugprone-reserved-identifier]
    7 | } __MyHiddenState;
      |   ^~~~~~~~~~~~~~~
      |   MyHiddenState
...
/tmp/cj/p4ch12/reserved.c:10:5: warning: declaration uses identifier '_Upper_started', which is a reserved identifier [bugprone-reserved-identifier]
   10 | int _Upper_started = 0;
      |     ^~~~~~~~~~~~~~
      |     Upper_started
Suppressed 14 warnings (14 in non-user code).
```

每处违章它都点了名,还在建议行(就是输出里紧跟在 caret 那一行下面的、印着替换名的 `MyHiddenState`/`Upper_started`)直接给出修法——把 `__MyHiddenState` 改名成 `MyHiddenState`、`_Upper_started` 改名成 `Upper_started`,去掉双下划线前缀和「下划线+大写」前缀,标识符就不再踩 ISO §7.1.3 的保留区了。修法就这么朴素:**别用双下划线、别用「下划线+大写」、文件作用域的标识符别以下划线开头**。这条对头文件尤其要紧——头文件里的 `__MYLIB_GUARD_H` 这种 guard 名也是违章的(阶段 4 第 1 章我们讲 include guard 时用的 `POINT_H` 才是合规的写法)。

### 三类摆一起:静态分析的定位

把三类 finding 在「编译器管不管 / clang-tidy 管不管」两个维度上摆开,静态分析的定位就清楚了。narrowing 是编译器(`-Wconversion`)和 clang-tidy 的交集——但 CI 不一定开 `-Wconversion`(它会刷屏),所以这条多半还是靠 clang-tidy 这道独立门抓。缺括号是编译器完全不管、clang-tidy 独占的风格问题。reserved-identifier 是违反 ISO §7.1.3、编译器完全不管、clang-tidy 独占的标准符合性问题。后两类尤其能说明「静态分析为什么不可或缺」——**它们是编译器和 sanitizer 都够不着的角落**,只有静态分析这种「在编译时、不运行程序、做模式匹配和数据流分析」的工具才抓得到。

到这里也能把本章和阶段 0 那两章的关系钉死了。**阶段 0 第 8 章**讲的是编译器警告——它做的是 best-effort 的静态分析,能力有限(条件分支里的未初始化它就漏),所以那章结尾说「真正的兜底是 sanitizer」;**本阶段第 10 章**讲的 sanitizer 是**运行时动态插桩**,得把程序跑起来才抓得到、有约 2 倍开销、不进发布构建;**本章**的 clang-tidy 是**编译时静态分析**,不运行程序、开销几乎为零(只在编译/CI 阶段跑一次)、可以也应当进 CI 硬门。三者错位互补:编译器警告抓得到的大部分,sanitizer 兜运行时的底,静态分析补编译器看不见的语义毛病。三者**不能互相替代**——这也是为什么本仓 CI 把它们拆成三道独立的 job。

## 本仓的活教材:从 `.clang-tidy` 到 CI 的 `static-analysis` 门

讲了原理,现在把镜头对准本仓自己——这一章最值钱的地方在于:**本仓库就是这套实践的活样本**,`.clang-tidy`、`scripts/clang_tidy_check.py`、`.github/workflows/ci.yml` 三件套齐全,而且**真能跑通**。我们一件件读。

### `.clang-tidy`:选三族、关噪声

根目录的 [`.clang-tidy`](../../.clang-tidy) 是整道门的配置中心,全文很短,值得逐行读:

```yaml
Checks: >
  -*,
  bugprone-*,
  performance-*,
  readability-*,
  -bugprone-easily-swappable-parameters,
  -readability-magic-numbers,
  -readability-identifier-length,
  -readability-function-cognitive-complexity,
  -readability-else-after-return,
  -readability-named-parameter,
  -readability-non-const-parameter,
  -readability-implicit-bool-conversion,
WarningsAsErrors: ''
HeaderFilterRegex: '^(?!.*(stdlib|bits/|sys/)).*$'
```

`Checks:` 这一长串是配置的核心,读法是「先全关(`-*`),再逐族打开(`bugprone-*`/`performance-*`/`readability-*`),再逐条关掉噪声大的」。clang-tidy 内置好几百个 check,一上来全开会刷屏刷到你怀疑人生;本仓的策略是只开**高价值、低误报**的三族——`bugprone-*` 抓上面那三类「容易写错」的代码(reserved-identifier、narrowing、分支错误)、`performance-*` 抓影响性能的写法(下面 clib 那段会看到一个 `performance-no-int-to-ptr`)、`readability-*` 抓可读性问题(缺括号、命名、魔法数字)。开完三族,再**反向关掉**六个在教程代码里会刷屏的:`bugprone-easily-swappable-parameters`(教程函数参数少、误报多)、`readability-magic-numbers`(教程里全是字面量、关掉才不会每行都报)、`readability-identifier-length`(短名 `i`/`j`/`n` 在循环里天经地义)、`readability-function-cognitive-complexity`(教程示例本就简单、但有些示范函数故意写得绕)、`readability-else-after-return`(早返回是合理的写法、不该强制去掉 else)、`readability-named-parameter`/`readability-non-const-parameter`/`readability-implicit-bool-conversion` 这几条也都是教程代码里合法的模式、不该被门挡住。这套「开三族 + 反向关六条」是踩过「全开刷屏」「只开 bugprone 漏 readability」之后攒出来的下限,你可以直接抄进自己的项目。

`WarningsAsErrors: ''` 这一行让 clang-tidy **只报 warning、不升级成 error**——退出码该是几还是几。你可能会问:那它怎么当 CI 硬门?答案在脚本里——`clang_tidy_check.py` 不靠 clang-tidy 自己的退出码,而是**解析它的 stdout,只要有 `warning:` 就算失败**(下面会看到)。这样做的好处是「`WarningsAsErrors` 留空、由脚本统一裁决」,比 clang-tidy 自己的 `-Werror` 更可控。

`HeaderFilterRegex: '^(?!.*(stdlib|bits/|sys/)).*$'` 这一行管「要不要报系统头里的 finding」。它是一个「负向先行断言」正则——匹配「不含 `stdlib`/`bits/`/`sys/` 的路径」,意思是「**只报非系统头的 finding**」。这样 `<stdio.h>`(展开成 `bits/stdio2.h` 之类)、`<sys/types.h>` 这类系统头里的 finding 被压掉了(前面那行 `Suppressed 14 warnings (14 in non-user code)` 就是它干的),你只看到自己代码的 finding,不被系统的噪声淹没。这一行尤其要紧——不开它,clang-tidy 会把 gcc/clang 自己系统头里的「违章」全报给你,那都是你管不了的代码。

### `scripts/clang_tidy_check.py`:给每个子项目配 compile_commands

光有 `.clang-tidy` 还不够,clang-tidy 跑起来有个硬要求——它得知道你每个 `.c` 是**用什么 flags 编的**(include 目录在哪、`-std` 是几、定义了哪些宏),否则它没法做语义分析、只能干模式匹配。这套「每个源文件的编译命令」就叫 **compile_commands.json**,CMake 用 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 就能产出(第 5 章讲 CMake 时细讲)。`scripts/clang_tidy_check.py` 干的就是「为每个 CMake 子项目配出 compile_commands、再对每个 `.c` 跑 clang-tidy、有 warning 就失败」。把它精简后的核心逻辑读一遍:

```python
PROJECTS = sorted((REPO / "examples").glob("*/CMakeLists.txt"))
# ...
for cl in PROJECTS:
    src = cl.parent
    bdir = BUILD_ROOT / src.relative_to(REPO)
    subprocess.run(
        ["cmake", "-S", str(src), "-B", str(bdir),
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], ...)
    cfiles = [p for p in src.rglob("*.c")
              if "/test/" not in str(p) and "/build/" not in str(p)]
    for f in sorted(cfiles):
        r = subprocess.run(
            ["clang-tidy", "-p", str(bdir), str(f)], ...)
        warns = [ln for ln in r.stdout.splitlines() if "warning:" in ln]
        if warns:
            print(f"[FAIL] {f.relative_to(REPO)}"); fail = True
        else:
            print(f"[OK]   {f.relative_to(REPO)}")
```

读法是这样的。第一步,`glob("*/CMakeLists.txt")` 找出 `examples/` 下所有自带 CMakeLists 的**一级子目录**——注意是一级(`*/`),所以 `examples/stage5-tcp-socket/SC1/` 这种在**二级**目录的 CMake 子项目**不在覆盖范围**;脚本注释里也诚实说了 clib-utilities 是 legacy、暂未纳入。第二步,对每个子项目,`cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` configure 出一个 `compile_commands.json`(放在 `/tmp/cj-clang-tidy/...` 下,不污染源码树)。第三步,`rglob("*.c")` 收集这个子项目下所有 `.c` 源,但排除掉 `test/`、`build/`、`_build/` 下的(测试代码和构建产物不查)。第四步,对每个 `.c` 跑 `clang-tidy -p <build_dir> <file>`——`-p` 指向 `compile_commands.json` 所在的目录,clang-tidy 会从中查出这个 `.c` 的编译命令、据此做语义分析。最后,**只要 stdout 里有 `warning:`、这个文件就判 `[FAIL]`、整个脚本退出码非 0**——这就是它当 CI 硬门的机制。

我在本仓真跑了一遍这个脚本:

```text
$ python3 scripts/clang_tidy_check.py
[OK]   examples/stage4-cmake-lib/src/mathlib.c

✅ clang-tidy 全部通过。
$ echo $?
0
```

退出码 0,`examples/stage4-cmake-lib/src/mathlib.c` 干干净净过门——这是当前 CI 上 static-analysis 这一格的真实表现。注意它只列了一个文件:正如脚本注释说的,目前 `glob("*/CMakeLists.txt")` 只匹配到 `examples/stage4-cmake-lib`(一级子目录),`stage5-tcp-socket/SC1-4` 在二级目录、还没被这道门覆盖。这和第 10 章讲 sanitizer 时提到的「覆盖盲区」是同一类问题——CI 的覆盖范围取决于脚本怎么 glob,不是「仓库里所有 `.c`」。

### `ci.yml` 的 `static-analysis`:全仓第 5 道 CI 门

把镜头拉到 CI 全景,本仓 `.github/workflows/ci.yml` 现在一共五道 job,`static-analysis` 是最后一道:

```yaml
  static-analysis:
    name: 静态分析(clang-tidy,examples)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装 clang-tidy + cmake
        run: sudo apt-get update && sudo apt-get install -y clang-tidy cmake ninja-build
      - name: 跑 clang-tidy(examples 硬门,阶段4·Ch12 引入)
        run: python3 scripts/clang_tidy_check.py
```

读法很简单——`apt` 装 `clang-tidy`(注意 Ubuntu LTS 上的 clang-tidy 版本未必有本机的 22.1.6 那么新,check 集会随版本变,这是「本地过、CI 红」的一个潜在来源),然后直接调上一节那个脚本。job 名字里的「**examples 硬门**」「**阶段4·Ch12 引入**」两条注释是刻意的——它告诉所有读 CI 的人:这道门是这一章立的、范围暂时只管 `examples/`、clib-utilities 还在外面(下面那段讲为啥)。

五道门摆一起看,你就明白这套防线是怎么错位互补的。`build-examples`(gcc+clang 矩阵)担保「能编过」;`sanitize`(ASan+UBSan)担保「运行起来没有内存错和 UB」;`docs`(frontmatter+markdownlint)担保「文档格式合法」;`format-check` 担保「代码风格统一」;`static-analysis`(clang-tidy)担保「没有语义级的常见错」。前三道在阶段 0 第 16 章会逐行拆,`format-check` 在阶段 0 第 17 章讲 clang-format 时讲过,**本章立的 `static-analysis` 是补在它们之外的语义层**——格式对(`format-check` 过)、能编过(`build-examples` 过)、跑起来没崩(`sanitize` 过),代码仍然可能有 reserved identifier、narrowing、缺括号这些「语义毛病」,这道门就是治这个的。

## cppcheck:另一套静态分析器(本机未装,诚实标注)

讲完了 clang-tidy,还有一件常和它并列提的工具得说——**cppcheck**。它和 clang-tidy 是同类的(都是编译时静态分析器、都不运行程序),但出身和侧重不同:clang-tidy 是 LLVM 项目的产品、和 clang 共享前端、check 集庞大且偏「代码风格 + 常见错」;cppcheck 是个独立的开源项目、专门设计来**抓 C/C++ 的真 bug**——空指针解引用、内存泄漏、未初始化变量、缓冲区越界、double-free 这类,它的卖点就是「**少误报**」,默认只报它有把握的。两者**互补**:clang-tidy 在风格和标准符合性(reserved-identifier、缺括号)上更强,cppcheck 在内存/指针类的真 bug 上更细。

本机我**没装 cppcheck**——这里诚实标注,不编造它的输出。装它很简单:

```text
$ sudo apt-get install cppcheck      # Debian/Ubuntu
$ cppcheck --version
Cppcheck 2.x                          # 装上之后的版本(本机未实测,以你装的为准)
```

跑起来典型用法是这样——`--enable=all` 打开所有类别的检查(默认只开 error 级,`all` 会加上 warning/style/performance/portability),`--suppress=missingIncludeSystem` 压掉「找不到系统头」的噪声(它不解析系统头、会产生大量无关警告),`-I` 指 include 目录、和你编译时一致:

```text
$ cppcheck --enable=all --suppress=missingIncludeSystem \
           -I include -I src path/to/your.c
```

cppcheck 也有自己的 `compile_commands.json` 支持(`-p` 或直接把 `compile_commands.json` 所在目录传给它),这样它就不需要你手动传 `-I`,比手敲准。它一样可以在 CI 里当门——`--error-exitcode=1` 让它发现 error 级问题时退出码非 0。这一章我**不贴 cppcheck 的真跑输出**(本机没装、不编造),你装上之后可以拿前面的 `narrow.c`/`reserved.c` 自己试——cppcheck 对 reserved-identifier 不管(那是 ISO 条款、不是 bug),对 narrowing 的报告也不如 clang-tidy 准,它的强项在内存/指针那一块,得拿真有越界/泄漏的代码才看得出价值(那种代码第 10 章 sanitizer 那篇写过一堆)。本仓的 CI 目前**没有 cppcheck 这道门**——clib-utilities 整改完、把 clang-tidy 这道门稳住之后,下一步可以考虑加 cppcheck 做内存类的补充检查,这是未来的工作。

## clib-utilities 的活样板:静态分析一开就有的 4 个真 finding

最后一段是这一章最「真实工程」的部分——**静态分析在一坨 legacy 代码上一开,几乎不可能干净**,本仓自己的 `projects/clib-utilities` 就是活样板。我把 `scripts/clang_tidy_check.py` 临时指向 clib-utilities、配上它的 `compile_commands.json`,真跑了一遍 clang-tidy,捞出 4 个真 finding。把它们逐条摆出来、给修法,顺带说清楚为啥 clib 暂时还在硬门外面。

真跑的方式是把 clib 的 CMakeLists 配出 `compile_commands.json`(放 `/tmp/cj/p4ch12/clib_build/`),然后对那两个有 finding 的源分别跑 clang-tidy:

```text
$ clang-tidy -p /tmp/cj/p4ch12/clib_build \
    projects/clib-utilities/SystemRelated/Sources/CCSTDLib_FetchError.c
.../CCSTDLib_FetchError.c:19:16: warning: statement should be inside braces [readability-braces-around-statements]
.../CCSTDLib_FetchError.c:44:29: warning: narrowing conversion from 'CCSTDLib_GeneralErrorCode' (aka 'long') to signed type 'int' is implementation-defined [bugprone-narrowing-conversions]

$ clang-tidy -p /tmp/cj/p4ch12/clib_build \
    projects/clib-utilities/SystemRelated/Sources/CCThread.c
.../CCThread.c:106:16: warning: declaration uses identifier '__CCThread_TrampolineArg', which is a reserved identifier [bugprone-reserved-identifier]
.../CCThread.c:117:9: warning: integer to pointer cast pessimizes optimization opportunities [performance-no-int-to-ptr]
.../CCThread.c:189:12: warning: statement should be inside braces [readability-braces-around-statements]
.../CCThread.c:203:14: warning: statement should be inside braces [readability-braces-around-statements]
```

去重归类,**正好是 4 类** finding,恰好把这一章讲的三类全用上了——外加一个 `performance-no-int-to-ptr` 是新面孔。我们一条一条看怎么修。

### Finding 1:`CCSTDLib_FetchError.c:44` 的 narrowing

那条 `const char* msg = strerror(code);` 之后,`errorBuf->code = code;` 把一个 `CCSTDLib_GeneralErrorCode`(在 POSIX 分支里 `typedef` 成 `long`,因为 `errno` 是 `int`、`strerror` 吃 `int`,但这个类型自己声明成了 `long`)赋给了一个 `int` 字段——`long → int` 是 implementation-defined 的窄化(ISO §6.3.1.3)。这和前面 `narrow.c` 是同一类问题。修法是**显式 cast 表明意图**,告诉读者「我知道这里会截断、我认了」:

```c
errorBuf->code = (int) code; /* 显式窄化,意图明确 */
```

更彻底的修法是把 `errorBuf->code` 的字段类型也改成 `CCSTDLib_GeneralErrorCode`、和存的值类型对齐,但从 ABI 兼容的角度看显式 cast 更轻量。这是个典型的 legacy 妥协——类型当初没设计对、现在改字段类型影响面大,先用 cast 兜住、留下 TODO。

### Finding 2:`CCThread.c:106` 的 reserved identifier

那个 trampoline 用的内部 `struct` 起名叫 `__CCThread_TrampolineArg`——双下划线开头,正撞 ISO §7.1.3 的保留区(和前面 `reserved.c` 的 `__MyHiddenState` 一模一样)。修法就是**去前缀重命名**:

```c
typedef struct CCThread_TrampolineArg { /* 去掉双下划线 */
    CCThread_Task_Func_type pFunc;
    CCThread_Tasks_Func_Param params;
} CCThread_TrampolineArg;
```

这是个文件内的 `static` 局部类型,改名不影响外部 ABI,改动面只在 `CCThread.c` 这一个文件里(几处引用一起改),是最干净的修法。

### Finding 3:`CCThread.c:117` 的 int→ptr cast

这行最有意思,代码长这样:

```c
CCSTD_SAFE_FREE(arg);
return (void*) (unsigned long) rc;
```

`rc` 是线程函数的返回值(`CCThread_Tasks_Func_RetType`,typedef 成 `unsigned long`),线程函数要返回 `void*`,于是这里 `unsigned long → void*` 做了一次整数到指针的强转。clang-tidy 的 `performance-no-int-to-ptr` 报它「**pessimizes optimization opportunities**」——意思是从整数转指针这种操作,会让编译器放弃一些指针别名分析方面的优化(它没法再假设这个指针真的指向一个对象)。这条的根因其实在更上游——这个库把线程函数返回值设计成 `unsigned long`、再强转回 `void*`,是个历史包袱。**最干净的修法是改类型设计**,让线程函数直接返回 `void*`、不再走整数中转;但那影响整个线程模块的对外签名、改动面大。**临时的兜底修法**是显式走 `intptr_t`/`uintptr_t`(POSIX/stdint 提供、保证能装下指针的整数类型),至少让转换有标准依据:

```c
#include <stdint.h>
return (void*) (intptr_t) rc; /* 用 intptr_t,标准保证可逆 */
```

不过这条 `performance-no-int-to-ptr` 即便这么改还是会报——因为本质问题(整数转指针)没消除,只是让代码更规范一点。彻底消掉它得改类型设计,这是 legacy 整改时才动得起的工程。

### Finding 4:`CCThread.c:189/203` 和 `CCSTDLib_FetchError.c:19` 的缺括号

剩下三条都是 `readability-braces-around-statements`——单语句体缺花括号,和前面 `dangle_else.c` 是同一类。比如 `CCSTDLib_FetchError.c:19` 那条 `if (!errorBuf) return;`:

```c
void freshError(CCSTDLib_FetchError* errorBuf) {
    if (!errorBuf) /* 旧:单语句无括号 */
        return;
    ...
}
```

修法就是给它戴上花括号:

```c
void freshError(CCSTDLib_FetchError* errorBuf) {
    if (!errorBuf) { /* 新:加括号 */
        return;
    }
    ...
}
```

`CCThread.c` 的 189 行和 203 行同理。这种修改纯粹机械、没有副作用,是 clib 整改里最该先动的一批——改动小、收益直接(风格统一 + 消除 dangling else 隐患)。

### 为啥 clib 还在硬门外

看完这 4 个 finding 你就明白了——`scripts/clang_tidy_check.py` 故意把 clib 排在 `examples/` 之外、只 `glob("*/CMakeLists.txt")` 一级子目录,是有原因的。源码里那条注释说得很直白:「**clib-utilities 是 legacy、有 4 个已知 finding 待整改,修齐后再纳入**」。如果现在就把 clib 也塞进硬门,CI 立刻红、所有 PR 都过不去——而整改这 4 个 finding 牵涉到「类型设计」(`int → CCSTDLib_GeneralErrorCode` 那条)、「跨文件重命名」(`__CCThread_TrampolineArg`)、「对外签名」(`unsigned long → void*`),不是改一两行就能收的事。所以本仓的策略是**分阶段**:`static-analysis` 这道门先只担保 `examples/`(干净的新代码),clib-utilities 标记为「legacy 整改中」、修齐后再纳入。这是真实工程里上静态分析的常态——**先在新代码上立门、老代码留出口子、按优先级慢慢还债**,而不是一开就全红、逼所有人停手。

## 小结

一句话收口:静态分析是编译器警告和 sanitizer 之外的第三层防线——它**在编译时、不运行程序、做模式匹配和数据流分析**,专治那些「能编过、跑也没事、但写得不对」的语义毛病。这一章我们用三个小程序把它和编译器的错位关系说准了:narrowing(`long → int`)是 `-Wextra` 不管、`-Wconversion` 和 clang-tidy 都管的交集,但因为 CI 不一定开 `-Wconversion`,这道门多半还是靠 clang-tidy 抓;缺括号(`readability-braces-around-statements`)是编译器**完全不管**、clang-tidy 独占的风格问题,顺带还能堵住 dangling else 的歧义;reserved identifier(双下划线/下划线大写,违反 ISO §7.1.3)是编译器**完全不管**、clang-tidy 独占的标准符合性问题——后两类最能说明「静态分析为什么不可或缺」。本仓的活教材三件套齐全:`.clang-tidy` 开 `bugprone/performance/readability` 三族、反向关掉教程会刷屏的六条,`HeaderFilterRegex` 压掉系统头噪声;`scripts/clang_tidy_check.py` 为每个 CMake 子项目配 `compile_commands.json`(`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`)、再对每个 `.c` 跑 `clang-tidy -p`、stdout 有 `warning:` 即失败,我**真在本仓跑过、examples 全过、退出 0**;`.github/workflows/ci.yml` 的 `static-analysis` 是全仓第 5 道 CI 门(前四道是 build-examples/sanitize/docs/format-check)。cppcheck 是另一套静态分析器、强项在内存/指针类真 bug、和 clang-tidy 互补,本机未装、不编输出,讲清 `apt install cppcheck` + `--enable=all` 的用法待你自验。最后 `projects/clib-utilities` 是「真实工程里静态分析一开就一堆 finding」的活样板——4 个真 finding(`CCSTDLib_FetchError.c:44` narrowing、`CCThread.c:106` reserved identifier、`CCThread.c:117` int→ptr、缺括号 ×3)逐条给过修法(显式 cast、去 `__` 重命名、走 `intptr_t`、加花括号),并说清它为啥暂在硬门外(legacy 整改牵涉类型设计/跨文件改名/对外签名,分阶段纳入)。这套防线和第 10 章 sanitizer 一动一静、一运行时一编译时,合起来才把「代码正确」这件事兜得完整。

带着这套理解,下一章我们换条线,看怎么用 Valgrind 做 sanitizer 之外的内存排查——它的脾气、开销、适用场景都和 sanitizer 不一样,是兜底的另一条路。

## 参考资源

- **本仓活教材**:[`.clang-tidy`](../../.clang-tidy)(Checks 配置)、[`scripts/clang_tidy_check.py`](../../scripts/clang_tidy_check.py)(compile_commands + 跑 clang-tidy 的脚本)、[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) 的 `static-analysis` job(全仓第 5 道 CI 门)。
- **ISO/IEC 9899:2011**:§7.1.3「Reserved identifiers」(双下划线、下划线+大写、文件作用域下划线开头的标识符保留给实现,`bugprone-reserved-identifier` 的依据);§6.3.1.3(有符号整数窄化是 implementation-defined,`bugprone-narrowing-conversions` 的依据);§6.5.4.1 语法(else 配最近的 if,dangling else 的根源)。
- **clang-tidy 官方文档**:完整 check 清单(`clang.llvm.org/extra/clang-tidy/checks/`)、`-p`/`compile_commands.json` 的用法、`.clang-tidy` 配置文件格式。
- **cppcheck**:`cppcheck.sourceforge.io`、`man cppcheck`(`--enable=all`、`--suppress=`、`--error-exitcode=`)。
- **承接章节**:阶段 0 第 8 章(警告旗标,best-effort 静态分析的源头)、第 10 章(sanitizer,运行时动态)、第 17 章(clang-format,格式而非语义——和本章 clang-tidy 别混);本阶段第 5 章(CMake 的 `CMAKE_EXPORT_COMPILE_COMMANDS`)、第 1 章(头文件契约,reserved identifier 的延伸)。
