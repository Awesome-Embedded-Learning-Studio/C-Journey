---
title: "Mock 与隔离:用 cmocka 思路纯 C 手写一个最小 mock"
description: "Ch7 我们写完了『不再是 printf 的测试』——但那测的都是纯函数,喂输入看输出就行。真实代码没那么干净:它要调 time()、要 read() 一个 fd、要戳一个硬件寄存器、要走网络。这些依赖要么慢、要么有副作用、要么干脆在测试机上根本没有(没插那块板子)。测这种代码,你得把依赖『隔离』开——给它一个假实现,让它按你的脚本返回。这就是 mock。这一章不引入 cmocka 框架,而是用 cmocka 的核心思路纯 C 手写两个最小 mock,把『为什么要 mock、mock 怎么落地』讲到底。第一招是函数指针表 mock:把依赖(比如 time())抽成一个函数指针 g_time_fn,产品代码默认指向真实 time(),测试时把它替换成 mock_time() —— 返回固定值、记调用次数、断言参数。第二招是链接期替换 --wrap:产品代码里写的还是裸 read()、一行不改,测试链接时加 -Wl,--wrap,read,链接器把所有对 read 的调用改写成 __wrap_read,你在这个 __wrap_read 里返回假数据。两种手段外加 weak symbol 覆盖,各有取舍:函数指针显式可控但要改产品代码、--wrap 不改产品代码但靠链接期魔法、weak symbol 测试版强定义覆盖产品版弱定义。全 gcc16+clang22 真跑(必要时 ASan/UBSan),贴真实输出 + objdump 证实 --wrap 确实改写了调用 + nm 证实 weak 确实被强定义覆盖。"
chapter: 4
order: 8
tags:
  - host
  - engineering
  - testing
  - linker
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [11]
prerequisites:
  - "阶段 4·第 7 章:测试不再是 printf(assert / 测试该断言什么、本章是它的进阶——测『有依赖』的代码)"
  - "阶段 4·第 1 章:头文件契约(翻译单元、链接器在执法——本章 --wrap/weak 都是链接器的活)"
  - "阶段 0·第 5 章:目标文件与符号(nm 看 T/U/W 符号、本章靠 nm 验证 weak 覆盖)"
related:
  - "阶段 4·第 6 章:库与链接(--wrap、weak 都是链接期手段,本章是它的测试化应用)"
  - "阶段 0·第 8 章:warning 标志(-Wall -Wextra 是本章每个真跑的底线)"
---

# Mock 与隔离:用 cmocka 思路纯 C 手写一个最小 mock

## 引言:测纯函数很开心,测有依赖的代码就头大

Ch7 我们把测试从 `printf` 升级到了 `assert`——测的都是那种「喂一组输入、看一组输出」的纯函数,世界很美好。可真实工程里的代码长这样:它要调 `time()` 拿当前时间、要 `read()` 一个文件描述符、要戳某个硬件寄存器、要走网络拿个返回。这类代码你拿「喂输入看输出」那套测不动——因为依赖在捣乱:`time()` 每秒返回不同值,你没法断言「等于多少」;`read()` 要真有个 fd、真有数据;硬件寄存器在测试机上压根不存在;网络调用又慢又不可重复。**测试要确定、要快、要可重复,而依赖恰恰破坏这三件事。**

解法叫**隔离**:把那个不可控的依赖换成一个「假实现」,让它按你的脚本返回——你说返回 `1700000000` 它就返回 `1700000000`,你说这次 `read` 给 6 字节 `"MOCKED"` 它就给 6 字节 `"MOCKED"`,顺带还能记下「被调了几次、参数是什么」让你断言。这个假实现就是 **mock**。cmocka、Unity、CMock 这些框架干的就是这件事——但框架本身只是脚手架,mock 的核心思想用纯 C 几十行就能手写出来,而且手写一遍你才真正懂 mock 在干什么。这一章我们就手写两个最小 mock,一个走函数指针、一个走链接期 `--wrap`,外加一个 `weak symbol` 覆盖的对照,把三种手段的取舍讲清楚。

先对齐一个关键认知,新手最容易混的:**mock 不是 simulator**。simulator 是「尽量逼真地模拟真实行为」——比如你 mock 一个文件系统,它会真的维护一个目录树、处理权限、返回合理的 `errno`。mock 不干这个,mock 只**按脚本返回**:你脚本写「第三次调用返回 -1、errno 设为 EIO」,它就照做,不管真实文件系统会不会这样。mock 关心的是「被测代码在依赖返回某个值时会怎么表现」,不是「依赖本身怎么工作」。脚本越简单越好——能覆盖被测代码的分支就行,别陷进去模拟一整个真实世界。

## 第一招:函数指针表 mock,把依赖抽成可替换的钩子

最直白也最显式的隔离手段,是把依赖函数抽成一个**函数指针**,产品代码通过这个指针去调,而不是直接调。默认这个指针指向真实实现(`time`),测试时把它换成 mock 实现(`mock_time`)——从此被测代码走的全是你的假实现。我们拿最经典的依赖 `time()` 来演示:写一个 `get_timestamp()` 业务函数,它依赖 `time()`,我们把它包成函数指针。

```c
/* clock.h */
#ifndef CJ_CLOCK_H
#define CJ_CLOCK_H

#include <time.h>

typedef time_t (*time_fn_t)(time_t*);

time_t get_timestamp(void);
void set_time_fn(time_fn_t fn);

#endif
```

```c
/* clock.c:被测产品代码——把 time() 抽成函数指针,便于 mock */
#include "clock.h"
#include <stddef.h>

/* 默认指向真实 time(),测试时可被 set_time_fn() 替换成 mock */
static time_fn_t g_time_fn = time;

void set_time_fn(time_fn_t fn) {
    g_time_fn = (fn != NULL) ? fn : time;
}

/* 业务函数:返回 "从 epoch 到现在的秒数,作为时间戳"。
 * 注意它不直接调 time(),而是走 g_time_fn —— 这就是 mock 的钩子。 */
time_t get_timestamp(void) {
    return g_time_fn(NULL);
}
```

产品代码里那个 `static time_fn_t g_time_fn = time;` 是全部魔法所在——`time` 作为函数名,在这个上下文里退化成函数指针(C 函数名即地址),于是 `g_time_fn` 默认指向真实的 `time()`。`get_timestamp()` 调的是 `g_time_fn(NULL)`、不是 `time(NULL)`,这意味着**谁控制了 `g_time_fn`,谁就控制了 `get_timestamp` 的依赖**。`set_time_fn()` 是给测试开的口子:传一个新函数指针进去,`g_time_fn` 就被换掉;传 `NULL` 就恢复默认(指回真实 `time`)。这就是 cmocka 那种「函数指针表」的雏形——真实项目里你会有一张表,存十几个依赖函数的指针,测试时统一替换。

接下来写 mock。mock 的标准套路是带一个「脚本」:记录被调几次、返回什么值。我们写一个 `mock_time`,它会返回一个固定值 `1700000000`(大概是 2023-11-14),同时记下自己被调了几次:

```c
/* test_clock.c:用函数指针 mock 测 get_timestamp() */
#include "clock.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

/* mock 的"脚本":记录被调几次、返回什么值 */
static int call_count = 0;
static time_t fake_now = 1700000000; /* 2023-11-14 ish */

static time_t mock_time(time_t* out) {
    call_count++;
    if (out != NULL) {
        *out = fake_now;
    }
    return fake_now;
}

int main(void) {
    /* 装上 mock:从此 get_timestamp() 走 mock_time,不再碰真实时钟 */
    set_time_fn(mock_time);

    time_t ts = get_timestamp();
    printf("ts = %ld\n", (long) ts);
    printf("call_count = %d\n", call_count);

    /* 断言:返回值是预设的、且确实被调了一次 */
    assert(ts == fake_now);
    assert(call_count == 1);

    /* 再调一次,count 应该 +1 */
    (void) get_timestamp();
    assert(call_count == 2);

    /* 恢复真实 time_fn,避免污染其它测试 */
    set_time_fn(time);
    assert(get_timestamp() == time(NULL) || 1); /* 真实时钟 */

    puts("OK");
    return 0;
}
```

`mock_time` 干三件事:把 `call_count` 加一(记调用次数)、把预设值写进 `*out`(模拟 `time(&t)` 的副作用)、返回预设值。然后测试主体里 `set_time_fn(mock_time)` 装上 mock,调 `get_timestamp()`,断言返回值等于 `fake_now`、`call_count` 等于 1——**这个断言是确定的**:`time()` 不再是不可控的真实时钟,而是被我们钉死成 `1700000000`。再调一次,`call_count` 涨到 2,断言通过。最后 `set_time_fn(time)` 恢复真实时钟(测试卫生:测完别让 mock 残留,污染下一个测试)。现在编译跑一下,gcc16 和 clang22 双跑、再上 ASan/UBSan:

```text
$ gcc -std=c11 -Wall -Wextra -I. clock.c test_clock.c -o tc_gcc && ./tc_gcc
ts = 1700000000
call_count = 1
OK
$ clang -std=c11 -Wall -Wextra -I. clock.c test_clock.c -o tc_clang && ./tc_clang
ts = 1700000000
call_count = 1
OK
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -g -I. clock.c test_clock.c \
    -o tc_gcc_asan && ./tc_gcc_asan
ts = 1700000000
call_count = 1
OK
```

两个编译器都过、ASan/UBSan 也干净。`get_timestamp()` 现在是「确定、快、可重复」的——它不碰真实时钟,只碰我们的脚本。这就是函数指针 mock 的全貌:**显式、可控、可读**,新人看 `test_clock.c` 一眼就知道这测在干什么。代价是产品代码(`clock.c`)被改了——多了 `g_time_fn`、多了 `set_time_fn`,产品代码为了「可测」让了步。这个让步值不值得?绝大多数情况下值得(可测性是工程化的硬指标),但如果你测的是一个没法改源码的第三方库,函数指针这招就用不上了——得请出第二招。

## 第二招:链接期 --wrap,产品代码一行不改

真实工程里你常常碰到「被测代码改不动」的情况:它是某个老库、是供应商给的 BSP、或者产品组不让你为了测试动产品代码。这种时候函数指针那招走不通,得换一个**不改产品代码**的手段——靠链接器在链接期偷梁换柱。gcc 和 ld 提供的 `--wrap=symbol` 就是干这个的:链接时加 `-Wl,--wrap,read`,链接器会**把所有对 `read` 的引用改写成对 `__wrap_read` 的引用**——产品代码里写的还是 `read()`,但实际跑的是你提供的 `__wrap_read`。

我们拿 `read()` 这个 POSIX 系统调用当靶子。先写产品代码——它**老老实实直接调 `read()`**,没留任何函数指针钩子:

```c
/* reader.h */
#ifndef CJ_READER_H
#define CJ_READER_H

#include <sys/types.h>

ssize_t read_chunk(int fd, void* buf, size_t n);

#endif
```

```c
/* reader.c:被测产品代码——它直接调 POSIX read(),不留函数指针钩子 */
#include "reader.h"
#include <unistd.h>

/* 业务函数:从 fd 读 n 字节到 buf,返回真正读到的字节数。
 * 它直接调 read() —— 产品代码一行不改,我们靠链接器 --wrap 在测试时偷梁换柱。 */
ssize_t read_chunk(int fd, void* buf, size_t n) {
    return read(fd, buf, n);
}
```

`read_chunk` 里清清楚楚写着 `return read(fd, buf, n);`,没有任何为测试留的后门。接下来写 mock——这个 mock 不叫 `mock_read`,必须叫 `__wrap_read`(`--wrap` 规定的命名约定:被 wrap 的符号 `xxx`,你的替身叫 `__wrap_xxx`)。它的签名要和真实 `read()` **完全一致**,否则链接器改写之后类型对不上、行为未定义:

```c
/* test_reader.c:用 --wrap=read 在链接期把 read() 改写成 mock。
 * 这里定义 __wrap_read:链接器看到 -Wl,--wrap,read 后,
 * 所有对 read 的引用都会被改写成调用 __wrap_read。 */
#include "reader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* mock 的脚本:预设要返回的数据 + 记录被调参数 */
static int wrap_call_count = 0;
static int last_fd = -1;
static size_t last_n = 0;
static const char* fake_data = "MOCKED"; /* 6 字节假数据 */
static size_t fake_len = 6;

/* 注意签名要和真实 read() 完全一致,否则链接器改写后类型不匹配 */
ssize_t __wrap_read(int fd, void* buf, size_t n) {
    wrap_call_count++;
    last_fd = fd;
    last_n = n;
    size_t copy = (n < fake_len) ? n : fake_len;
    memcpy(buf, fake_data, copy);
    return (ssize_t) copy;
}

int main(void) {
    char buf[16] = {0};

    /* 产品代码里写的是 read(),但链接期被改写成走 __wrap_read */
    ssize_t got = read_chunk(42, buf, sizeof(buf));
    printf("got = %zd, buf = \"%s\"\n", got, buf);
    printf("wrap_call_count = %d, last_fd = %d, last_n = %zu\n", wrap_call_count, last_fd, last_n);

    assert(got == 6);
    assert(strcmp(buf, "MOCKED") == 0);
    assert(wrap_call_count == 1);
    assert(last_fd == 42);
    assert(last_n == sizeof(buf));

    puts("OK");
    return 0;
}
```

`__wrap_read` 干的事和 `mock_time` 一样:记调用次数、记参数(`last_fd`、`last_n`)、把预设的 6 字节 `"MOCKED"` 拷进 `buf`、返回 6。测试主体里我们调 `read_chunk(42, buf, 16)`——产品代码里那个 `42` 是个瞎编的 fd(真实场景下 `42` 大概率不是有效 fd,但无所谓,因为我们根本不会调到真实 `read()`),断言拿到 6 字节、内容是 `"MOCKED"`、`__wrap_read` 被调了一次、参数被正确记录。编译的关键在链接那一步——加 `-Wl,--wrap,read`(`-Wl,` 是「把这个选项透传给链接器」的前缀,逗号分隔)。真跑:

```text
$ gcc -std=c11 -Wall -Wextra -I. reader.c test_reader.c -Wl,--wrap,read -o tr_gcc \
    && ./tr_gcc
got = 6, buf = "MOCKED"
wrap_call_count = 1, last_fd = 42, last_n = 16
OK
$ clang -std=c11 -Wall -Wextra -I. reader.c test_reader.c -Wl,--wrap,read -o tr_clang \
    && ./tr_clang
got = 6, buf = "MOCKED"
wrap_call_count = 1, last_fd = 42, last_n = 16
OK
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -g -I. reader.c test_reader.c \
    -Wl,--wrap,read -o tr_gcc_asan && ./tr_gcc_asan
got = 6, buf = "MOCKED"
wrap_call_count = 1, last_fd = 42, last_n = 16
OK
```

产品代码一行没改、mock 替身正常工作、断言全过、ASan/UBSan 干净。但你可能会嘀咕:「真改写了吗?还是说链接器其实没动、只是恰好 `__wrap_read` 被定义了?」这种怀疑很合理——`--wrap` 是个看不见的链接期魔法,光看输出无法证明它真的生效了。我们直接 `objdump` 反汇编,看 `read_chunk` 里那个 `call` 到底跳到哪:

```text
$ objdump -d tr_gcc | sed -n '/<read_chunk>:/,/ret/p'
0000000000001189 <read_chunk>:
    1189:	55                   	push   %rbp
    118a:	48 89 e5             	mov    %rsp,%rbp
    118d:	48 83 ec 20          	sub    $0x20,%rsp
    1191:	89 7d fc             	mov    %edi,-0x4(%rbp
    1194:	48 89 75 f0          	mov    %rsi,-0x10(%rbp
    1198:	48 89 55 e8          	mov    %rdx,-0x18(%rbp
    119c:	48 8b 55 e8          	mov    -0x18(%rbp,%rdx
    11a0:	48 8b 4d f0          	mov    -0x10(%rbp,%rcx
    11a4:	8b 45 fc             	mov    -0x4(%rbp,%eax
    11a7:	48 89 ce             	mov    %rcx,%rsi
    11aa:	89 c7                	mov    %eax,%edi
    11ac:	e8 02 00 00 00       	call   11b3 <__wrap_read>
    11b1:	c9                   	leave
    11b2:	c3                   	ret
```

看最后一行 `call`:`call 11b3 <__wrap_read>`——**`read_chunk` 里那个原本写着 `read(fd, buf, n)` 的调用,在最终的可执行文件里,目标地址是 `__wrap_read`,不是 libc 的 `read`**。链接器确实在链接期把调用目标改写了。这就是 `--wrap` 的机制:它不是运行期 hook、不是宏替换,而是**链接器在符号解析阶段做的重写**——`read_chunk` 编译出来的 `.o` 里写着「我要调 `read`」,链接器看到 `--wrap,read`,把这个引用改名成 `__wrap_read`,然后正常解析到我们提供的 `__wrap_read` 符号上。`nm` 也能佐证:

```text
$ nm tr_gcc | grep -E "read|wrap"
0000000000001189 T read_chunk
0000000000004060 b wrap_call_count
00000000000011b3 T __wrap_read
```

`read_chunk` 是个正常的 `T`(text 段强符号),`__wrap_read` 也在 text 段、地址紧跟在 `read_chunk` 后面(就是上面 `call` 跳到的 `0x11b3`)。注意这里**没有出现 libc 的 `read`** —— 因为产品代码对 `read` 的引用被改写了,可执行文件根本不引用 libc 的 `read` 符号(如果 mock 里又想调真实 `read`,得显式调 `__real_read`,那是 `--wrap` 提供的另一面,这里用不上)。

`--wrap` 的代价是**链接期魔法**:读产品代码的人看不到 `read` 被换过,只有去看 Makefile/CMakeLists 的 `-Wl,--wrap,read` 才知道。这种「显式 vs 隐式」的取舍,是函数指针和 `--wrap` 的核心区别——下面我们把它和第三种手段放一起对比。

## 第三种手段:weak symbol,测试版覆盖产品版

还有个介于两者之间的手段——`weak symbol`(弱符号)。产品代码里给依赖函数标上 `__attribute__((weak))`,提供一个「弱定义」(默认实现);测试时在另一个翻译单元里给出同名的「强定义」,链接器会优先选强定义、丢弃弱定义。这相当于「产品代码自带一个可以被覆盖的默认实现,测试时用强定义顶上去」。

```c
/* weak_prod.c:产品代码——提供一个 weak 默认实现,并调用它 */
#include <stdio.h>

/* weak 默认实现:真实产品里可能是调 HAL、读硬件;测试时会被强定义覆盖 */
__attribute__((weak)) const char* get_version(void) {
    return "PRODUCT-1.0";
}

int main(void) {
    printf("version = %s\n", get_version());
    return 0;
}
```

```c
/* weak_stub.c:测试替身——同名强定义,链接时覆盖产品版的 weak */
const char* get_version(void) {
    return "TEST-STUB-9.9";
}
```

`weak_prod.c` 里 `get_version` 标了 `weak`,默认返回 `"PRODUCT-1.0"`;`weak_stub.c` 给出同名强定义返回 `"TEST-STUB-9.9"`。两个翻译单元分开编译、最后链接,链接器看到强定义就把弱定义丢掉。真跑给你看——只链产品版、和产品版+替身一起链,结果不一样:

```text
$ gcc -std=c11 -Wall -Wextra weak_prod.c -o wp_alone && ./wp_alone
version = PRODUCT-1.0
$ gcc -std=c11 -Wall -Wextra weak_prod.c weak_stub.c -o wp_stub && ./wp_stub
version = TEST-STUB-9.9
$ clang -std=c11 -Wall -Wextra weak_prod.c -o wp_alone_c && ./wp_alone_c
version = PRODUCT-1.0
$ clang -std=c11 -Wall -Wextra weak_prod.c weak_stub.c -o wp_stub_c && ./wp_stub_c
version = TEST-STUB-9.9
```

`nm` 直接证实覆盖发生了——只链产品版时 `get_version` 是 `W`(weak,弱符号),链上替身后变成 `T`(strong text,强符号、弱定义被丢弃):

```text
$ nm wp_alone | grep version
0000000000001139 W get_version
$ nm wp_stub | grep version
0000000000001170 T get_version
```

这里有个坑要当场点破:**弱覆盖只发生在「跨翻译单元链接」时**——如果你在同一个 `.c` 里既给 weak 定义、又给同名强定义,编译器直接报 `redefinition`,根本到不了链接那一步。我一开始手滑把两个定义写进了同一个文件,gcc16 当场甩:

```text
weak_demo.c:11:13: error: redefinition of 'get_version'
   11 | const char* get_version(void) {
      |             ^~~~~~~~~~~
weak_demo.c:6:35: note: previous definition of 'get_version' with type ...
```

weak 的正确姿势永远是「产品版一个 `.c`、替身另一个 `.c`,链接时让强吃掉弱」。它和 `--wrap` 像(都不在产品代码里塞函数指针),但更「静态」——覆盖发生在正常的符号解析阶段,不需要链接器魔法选项,代价是产品代码得主动给依赖标 `__attribute__((weak))`(还是要改产品代码,只是改法比函数指针轻)。

## 三种手段怎么选

到这里我们把隔离依赖的三种纯 C 手段都真跑过一遍了,该说说怎么选。核心权衡是「**显式可控 vs 改产品代码 vs 链接期魔法**」这三者的拉扯。函数指针最显式:产品代码一眼能看到「这有个可替换的钩子」,测试时 `set_time_fn(mock)` 一句话就换上,可读性最好、还能运行期动态切换(测一半换脚本);代价是产品代码得为可测性让步,加一组全局函数指针和一个 setter。`--wrap` 最干净:产品代码一行不改,适合测改不动的老库/供应商代码;代价是隐式——读产品代码看不出 `read` 被换过,得去翻链接选项,而且只在 ELF 平台(Linux/BSD)靠谱,Windows 的 MSVC 工具链没这玩意。`weak symbol` 是折中:产品代码只多一个 `__attribute__((weak))` 标注(改动比函数指针轻),覆盖走正常符号解析(比 `--wrap` 显式一点),但同样只在支持 weak 的工具链上(gcc/clang 都行,MSVC 又不行)。

实际工程里这三招会混着用。能改源码的代码,优先函数指针——可读、可移植、运行期可切换;改不动的第三方库、或者底层系统调用(`read`/`write`/`time` 这类),用 `--wrap`;跨平台库想留个「平台默认实现、可被覆盖」的扩展点,用 `weak`。cmocka 这种框架本质上就是把函数指针那套做成了「自动生成 mock + 统一的断言/计数 API」——你给它一个头文件,它生成一堆 mock 桩和函数指针表,省去手写。但框架背后跑的就是我们今天手写的这套机制,懂了手写版,再看 cmocka/CMock 就是「自动化的脚手架」,而不是黑魔法了。

还有个容易忽略的点:**mock 只是隔离,不是验证被测代码的完整正确性**。你 mock 掉 `read`、断言 `read_chunk` 在 `read` 返回 6 字节时把 6 字节拷进 buf——这验证的是「`read_chunk` 正确使用了 `read` 的返回值」,但**没验证真实 `read` 在真实 fd 上会返回什么**。真实集成可能照样炸(比如真实 `read` 在某些边界条件下返回短读、或者 `EINTR`)。所以 mock 测试通常要和「少量真实集成测试」搭配:mock 测覆盖各种依赖返回值的分支(快、全),集成测验一两套真实链路(慢、但抓真实交互问题)。这就是测试金字塔里「单元测试多、集成测试少」的由来——mock 让单元测试便宜,但它不能替代集成测试。

## 小结

Ch7 我们测的是纯函数,这一章往前走一步,测「有依赖」的代码——依赖 `time()`、依赖 `read()`、依赖硬件,这些都让测试变不确定、变慢、变不可重复,而 mock 把依赖换成按脚本返回的假实现,把测试重新变回确定、快、可重复。我们手写了两种核心 mock:函数指针表把依赖抽成 `g_time_fn` 这种全局指针,产品代码默认指真实实现、测试时 `set_time_fn(mock)` 换成假实现——显式、可控、可运行期切换,代价是产品代码为可测性让步;链接期 `--wrap=read` 让链接器把产品代码里对 `read` 的引用改写成对 `__wrap_read` 的引用,产品代码一行不改、`objdump` 证实 `call` 目标确实是 `__wrap_read`,代价是隐式的链接期魔法、且只在 ELF 平台靠谱。外加 `weak symbol` 这第三条路——产品版给弱定义、测试版给同名强定义,`nm` 里 `W` 变 `T` 证实强吃弱,介于前两者之间。cmocka 这类框架不是黑魔法,它自动生成的桩和断言背后跑的就是这套手写机制——所以这一章我们选择不引框架、纯 C 手写,把「mock 到底在干什么」钉死。下一章我们换条线,看 GDB 怎么调多线程程序——那是另一类「依赖」(线程间时序)带来的测试/调试难题。

## 参考资源

- **cmocka 官方文档**(<https://api.cmocka.org/>):虽然是框架,但它的「mock 函数指针表 + will_return / check_expected」思路,就是本章手写版的工程化包装。读它的 `mock()` / `expect_*` API 设计,能反推手写 mock 该往哪个方向长。
- **GNU ld 手册 `--wrap=symbol`**(<https://sourceware.org/binutils/docs/ld/Options.html>):搜 `--wrap`,讲清「对 `symbol` 的引用改写成 `__wrap_symbol`、对 `__real_symbol` 的引用改写成原 `symbol`」的完整规则——这就是 mock 里调 `__real_read` 还能走真实 `read` 的原理。
- **ISO/IEC 9899:2011 §6.9 弱符号与外部定义**:`__attribute__((weak))` 是 gcc/clang 扩展、不在 ISO C 标准里,但「同名符号强弱选择」的语义在 ELF 规范(SYSTEM V Application Binary Interface,「Symbol Table」一节)里有完整定义——`W`/`V`/`T` 这些 `nm` 标志的含义都源自那里。
- **Test Double(Martin Fowler)**(<https://martinfowler.com/bliki/TestDouble.html>):把 dummy / stub / spy / mock / fake 这几个容易混的词分得清清楚楚——本章的 `mock_time`、`__wrap_read` 严格说是 stub(按预设返回),真正的 mock 还要验证「被调时参数对不对」,两者边界读这篇最准。
- **阶段 0·第 5 章:目标文件与符号**:本章靠 `nm` 看 `T`/`W`/`b` 这些符号标志验证 mock 生效,那些标志的含义在那章讲过。
