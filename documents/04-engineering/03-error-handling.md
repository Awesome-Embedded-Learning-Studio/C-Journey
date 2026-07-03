---
title: "错误处理三件套:返回码、errno 约定与 context 对象"
description: "C 没有 exception、没有 try/catch,函数出错了怎么告诉调用者?这一章用同一个 div 除法函数,真跑三种递进的错误报告风格对照:① 直接返回码(函数返 -1/NULL 表出错,调用者必须查——演示漏查返回码、拿个垃圾值继续跑的后果);② errno 约定(失败时设 errno、调用者 strerror/perror——呼应阶段0 那条「errno 只在出错有意义」,真跑一次成功调用后 errno 残留上一次失败的值);③ context 对象(把错误码+人话+出错位置打包进一个堆上分配的、线程安全的错误对象,API 返成功/失败、细节从 ctx 取——照着 projects/clib-utilities 的 FetchError 模型精简复刻一套)。三种风格不是互斥的,是工程里层层叠加的真实演化:返回码最朴素但最容易被忽略,errno 是标准库统一约定但单点全局、线程局部,context 对象把错误细节具象化、能跨调用留存、不抢返回值。全 gcc16+clang22 双跑,context 那段加 ASan/UBSan 验证堆管理干净。"
chapter: 4
order: 3
tags:
  - host
  - engineering
  - system-programming
difficulty: intermediate
reading_time_minutes: 16
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段 4·第 1 章:头文件契约(include guard、ODR、struct 在头里怎么放)"
  - "阶段 4·第 2 章:API 设计与不透明类型(opaque 指针当句柄,本章 context 就是同款思路)"
  - "阶段 0·第 10 章:Sanitizer 门禁(本章 context 段用 ASan/UBSan 兜底)"
related:
  - "阶段 0·第 8 章:警告旗标(-Werror 让「漏查返回码」这种隐患提前现形)"
  - "阶段 2:指针与内存(context 对象靠堆分配/句柄,前置基础)"
---

# 错误处理三件套:返回码、errno 约定与 context 对象

## 引言:C 没有 try/catch,出错怎么告诉调用者

写过 C++ 或 Python 的人,一出错就 `throw` / `raise`,外层一层 `catch` / `except` 兜住,错误能沿着调用栈自己往上爬。C **没有这套机制**——语言层面根本没有 exception,函数出错了,**它自己不主动说、调用者不主动查,这个错误就凭空消失了**,程序拿一个没意义的值继续往下跑,跑到某天某刻崩出一个诡异的段错误,你再回头查,根本不知道错最早是从哪个函数冒出来的。

这一章讲清 C 工程里「告诉调用者出错了」的三种递进做法,是后面整个工程化阶段绕不开的地基。我们会拿**同一个 `div` 除法函数**,用三种风格各写一遍,看它们各自怎么报告「除零」这个错、调用方又怎么把错接住。其一,**直接返回码**——函数返个负数或 `NULL` 当失败信号,这是 C 最原生的写法,朴素、零开销,但也最容易被调用方「顺手忽略」。其二,**errno 约定**——失败时往一个标准库统一管的变量里写错误码,调用方用 `strerror` / `perror` 把它翻成人话,这是 C 标准库自己用的那一套(阶段 0 我们见过,这里收尾)。其三,**context 对象**——把错误码、人话描述、甚至出错位置打包进一个堆上分配的对象里,API 只返成功/失败、所有细节从 context 取,这是 `projects/clib-utilities` 里 `FetchError` 那套活教材的思路。

三种风格不是「三选一」,而是工程里**层层叠加的真实演化**:返回码最朴素但最脆,errno 把错误码标准化了但单点全局(虽然是线程局部的),context 对象把错误细节真正具象化、能跨调用留存、不抢返回值位置。每一步演化都是为了补上一步的某个短板。我们一种一种来,每种都真跑。

## 风格①:直接返回码——最朴素,也最容易被忽略

最原生的写法:函数返一个整数,**成功返 0、失败返非 0**(或者反过来,看团队约定;POSIX 那一脉大量函数是「成功返 0、失败返 -1」),出错的具体原因得靠返回值以外的渠道传达。要是函数本来的职责就是返一个「值」(比如算个商、查个指针),那通常的套路是:**真正要返的值走「出参指针」、状态走返回值**。看个 `div`:

```c
/* return_code.c */
#include <stdio.h>

/* 风格①:函数返 -1 表出错,调用者必须查返回码 */
static int safe_div(int a, int b, int* out) {
    if (b == 0) {
        return -1; /* 出错:返负数,约定的失败信号 */
    }
    *out = a / b;
    return 0; /* 成功:返 0 */
}

int main(void) {
    int result = 0;
    int rc = safe_div(10, 0, &result); /* 除零 */
    if (rc != 0) {
        printf("出错了:rc=%d,result 没被赋值(还是 %d)\n", rc, result);
        return 1;
    }
    /* 只有 rc==0 才走到这里 */
    printf("10/2 = %d\n", result);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra return_code.c -o return_code && ./return_code
出错了:rc=-1,result 没被赋值(还是 0)
```

`safe_div` 把真正要返的商写进 `*out`、把状态(成功 0 / 失败 -1)当返回值,这是「值走出参、状态走返回值」的标准套路。调用方拿到 `rc` 第一件事就是查它,`rc != 0` 就知道失败了、`result` 里压根没被写进有效值,直接 `return 1` 退出。注意我给 `result` 初始化成 0——这个初值在失败时**会被原样保留**,因为出错分支里函数根本没碰 `*out`;调用方若不查 `rc` 就用 `result`,用的就是这个初值。

这套写法零开销、不依赖任何全局状态、线程天然安全,唯一的毛病是——**调用方不查,就白搭**。C 编译器不会强制你查返回值,你随手一行 `safe_div(10, 0, &result);`,返回码当场被丢进虚空,函数老老实实返了 -1、可调用方压根没看。把上面那段改成「假装没看见返回码」:

```c
/* ignore_return.c */
#include <stdio.h>

/* 同一个 safe_div:返 -1 表出错。但这次调用者假装没看见返回码 */
static int safe_div(int a, int b, int* out) {
    if (b == 0) {
        return -1; /* 出错:函数根本没碰 *out */
    }
    *out = a / b;
    return 0;
}

int main(void) {
    int result = 999;                /* 故意给个初值,模拟「没查返回码就拿来用」 */
    (void) safe_div(10, 0, &result); /* 返回码被 (void) 丢掉了 */
    /* result 没被赋值,还是 999——调用方拿了个完全没意义的值 */
    printf("我假设成功,直接用 result = %d\n", result);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra ignore_return.c -o ignore_return && ./ignore_return
我假设成功,直接用 result = 999
```

`safe_div` 明明失败了(除零、返了 -1),可主程序压根没看返回码、直接拿 `result` 当成功结果用——打印出来的是那个**毫无意义的初值 999**。这就是返回码模型最大的脆点:**它靠调用方的自觉来生效**。要是 `result` 后面被喂给一个数组下标、一个指针偏移,程序就在你完全意想不到的地方炸了,查日志根本看不出来源头是 `safe_div` 失败。

这里有个真实可用的兜底:把 `-Wall -Wextra` 升到 `-Werror`(阶段 0 第 8 章讲过),或者更狠地开 `-Wunused-result`(配合给函数加 `__attribute__((warn_unused_result))`),让编译器在调用方丢弃返回值时**当场报错**。可这些是「编译期能帮你的事」,运行期你忘了查、编译器也没招——返回码模型永远依赖调用方自觉。所以演化出了第二种:让错误信息走一条调用方**没法忽略**的渠道。

## 风格②:errno 约定——标准库统一管的错误码

C 标准库自己用的就是这套。`errno` 是 `<errno.h>` 声明的一个**可修改的左值**(C11 §7.5¶2),它本身不是个普通 `int` 变量——在 glibc 上,`errno` 实际是个宏,展开成 `(*__errno_location())`,调用一个函数拿到指向**当前线程私有那份** `int` 的指针再解引用。这套设计是为了让 `errno` **线程局部**:每个线程查到的都是自己的 errno,不会被别的线程覆盖。我们可以拿预处理看看这个展开:

```text
$ echo '#include <errno.h>
int main(void){ return errno; }' | gcc -std=c11 -E -x c - | grep -A3 "int main"
int main(void){ return 
# 2 "<stdin>" 3
                      (*__errno_location ())
# 2 "<stdin>"
```

`errno` 在 glibc 上就是 `(*__errno_location())`,函数 `__errno_location` 返回的是**指向当前线程私有 errno 的指针**。这一点 C11 标准留了余地(§7.5¶2 说「errno 可以有线程存储期」,是可选的),但 POSIX 强制要求它线程局部——所以在 Linux/macOS 这种 POSIX 系统上,errno 线程安全是有保证的。

约定是这样的:**库函数失败时,把一个约定的错误码写进 errno;调用方在「确认失败」之后,用 `strerror(errno)` 把码翻成人话、或者直接 `perror("前缀")` 一行打印「前缀: 人话」**。看个 errno 版的 `div`:

```c
/* errno_demo.c */
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* 风格②:出错时设 errno,函数本身仍返一个值(这里返商、失败返 0)。
 * 关键:只在失败分支里动 errno,成功分支不碰——errno 只在出错时有意义。 */
static int div_errno(int a, int b) {
    if (b == 0) {
        errno = EINVAL; /* 设约定的错误码 */
        return 0;       /* 失败的返回值,调用者得先看 errno */
    }
    return a / b;
}

int main(void) {
    errno = 0;        /* 调用前清零,这一步很关键 */
    div_errno(10, 0); /* 返回值这里用不上,细节全看 errno */
    if (errno != 0) {
        /* perror 直接打印 "用户传的前缀: strerror(errno)" */
        perror("div_errno 失败");
        printf("errno=%d, 说明=%s\n", errno, strerror(errno));
        return 1;
    }
    printf("10/2 = %d\n", div_errno(10, 2));
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra errno_demo.c -o errno_demo && ./errno_demo
div_errno 失败: Invalid argument
errno=22, 说明=Invalid argument
```

`div_errno` 出错时把 `errno` 设成 `EINVAL`(标准库里「非法参数」那个码,值是 22),调用方先用 `perror` 打一行、再用 `strerror(errno)` 把码翻成人话——两条都告诉你「Invalid argument」。这套写法的好处是错误码是**标准库统一管**的:`EINVAL`、`ERANGE`、`ENOMEM` 这些码在 `<errno.h>` 里都有名有姓、跨平台一致,调用方拿到码就知道是哪一类错。

可 errno 模型有个**铁律级别的坑,坑过无数人**:errno **只在「出错之后」才有意义,成功调用之后它的值是「未指定」的**(C11 §7.5¶3 明说了:库函数的成功调用**不要求**把 errno 清零,它可能保留上一次失败留下的值)。也就是说——**你不能拿 errno 来判断「成功有没有发生」,只能拿它问「刚才那次失败为什么」**。判断成功失败,只能靠函数返回值;errno 只在你已经知道失败之后,才去查「为什么」。真跑给你看:

```c
/* errno_misuse.c */
/* 演示「errno 只在出错时才有意义」这条铁律:
 * 成功调用之后,errno 的值是「未指定」的——可能是 0、可能是上一次失败留下的。
 * 拿 errno 来判断「成功有没有发生」是错的,只能拿它问「刚才那次失败为什么」。 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int may_fail(int x) {
    if (x < 0) {
        errno = ERANGE;
        return -1;
    }
    return x * 2; /* 成功分支:不动 errno */
}

int main(void) {
    /* 先制造一次失败,errno 被设成 ERANGE */
    errno = 0;
    may_fail(-1);
    printf("失败后 errno=%d (%s)\n", errno, strerror(errno));

    /* 然后一次成功调用——errno 没被清,「残留」着上次的 ERANGE */
    int r = may_fail(5);
    printf("成功调用返回 %d,此时 errno=%d (残留!别拿它判断成功)\n", r, errno);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra errno_misuse.c -o errno_misuse && ./errno_misuse
失败后 errno=34 (Numerical result out of range)
成功调用返回 10,此时 errno=34 (残留!别拿它判断成功)
```

第一次 `may_fail(-1)` 失败,errno 被设成 `ERANGE`(34)。第二次 `may_fail(5)` 明明成功了、返回了 10,可**errno 还是 34**——因为成功分支根本没动它,它「残留」着上次的值。要是你写 `if (errno == 0)` 来判断第二次调用有没有成功,你就会得出「第二次也失败了」这个荒谬的结论。所以正确姿势是:**先看返回值判断成功失败,失败了再去查 errno 问为什么**;errno 永远不能单独用来判断成功。这也是为什么我上面 `errno_demo` 里调用前先 `errno = 0;` 清零——清零之后,如果调用后 errno 仍然非 0,那才说明这次调用出了错(配合返回值一起看,双保险)。

至于 errno 是不是真的线程安全,我们打个真实验证一下——两个线程,一个把 errno 设成 34、睡两秒,另一个睡一秒后查自己的 errno,看会不会被第一个线程污染:

```c
/* errno_threads.c */
/* 验证 errno 是线程局部的:glibc 上 errno 展开成 (*__errno_location()),
 * 每个线程拿到自己那份。线程 A 设 errno、sleep;线程 B 不受影响。 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static void* worker_a(void* arg) {
    (void) arg;
    errno = ERANGE; /* A 线程把「自己那份」errno 设成 34 */
    sleep(2);       /* 故意睡,等 B 线程查它的 errno */
    printf("线程 A:errno=%d (自己设的)\n", errno);
    return NULL;
}

static void* worker_b(void* arg) {
    (void) arg;
    sleep(1);
    printf("线程 B:errno=%d (A 设了它的,A 动不到我的 errno)\n", errno);
    return NULL;
}

int main(void) {
    pthread_t ta, tb;
    pthread_create(&ta, NULL, worker_a, NULL);
    pthread_create(&tb, NULL, worker_b, NULL);
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra errno_threads.c -o errno_threads -pthread && ./errno_threads
线程 B:errno=0 (A 设了它的,A 动不到我的 errno)
线程 A:errno=34 (自己设的)
```

线程 A 把自己那份 errno 设成 34、睡两秒;线程 B 在中途查自己的 errno,是 0——A 设的 34 完全没污染到 B。这就是 `(*__errno_location())` 那个设计在干活:每个线程查 errno 都拿到自己私有的那一份,跨线程不串。所以 errno 模型的线程安全是有保证的,但它有个**结构性的短板**:errno 是个**全局单点**(虽然是线程局部的全局单点),同一时刻**只能记一个错误**——一个函数里调了三个子调用、第二个失败第三个也失败,errno 只会留最后一个的码,前面的全被覆盖;而且它「抢」不了返回值的位置,错误码还得靠返回值这条独立渠道传达。这些短板在「一个调用想带回**丰富错误细节**(码 + 人话 + 出错位置)、还想让细节**跨调用留存**」的场景下就显得不够用——于是演化出了第三种。

## 风格③:context 对象——把错误细节打包进一个堆上的小对象

`projects/clib-utilities` 里有个 `FetchError` 模型,正是这套思路的活教材。我把它最核心的字段拎出来看看(`SystemRelated/Includes/CCSTDLib_FetchError.h`,这里为排版工整去掉了源文件的列对齐空格):

```c
typedef long CCSTDLib_GeneralErrorCode;
typedef struct __CCSTDLib_FetchError {
    CCSTDLib_GeneralErrorCode code;
    const char* discrip;
} CCSTDLib_FetchError;

CCSTDLib_FetchError* initError();
void freshError(CCSTDLib_FetchError* errorBuf);
void clearError(CCSTDLib_FetchError* errorBuf);
void setError(CCSTDLib_FetchError* error, CCSTDLib_GeneralErrorCode code, const char* disp);
void freeError(CCSTDLib_FetchError* errorBuf);
const char* getError(CCSTDLib_FetchError* error);
```

这套设计的关键:**错误本身被具象化成一个堆上分配的小对象**(里面有错误码 `code` 和人话描述 `discrip`),调用方拿到一个指向它的指针,出错时往里写码和描述、查错时从里读细节、用完自己 `freeError` 掉。错误码这边 `Basic_Utils/Includes/CCSTDLib_Err.h` 还专门集中了一份 `enum CCSTD_ERROR_CODE { MALLOC_FAILED = 1, INVALID_POINTER = 2 }`——把全库的错误码集中到一个枚举里统一管,调用方拿到码就能查表,这是工程化错误码的常见做法。

这套模型相对 errno 的进步在于三点。其一,错误细节**真正具象化**了:不再只是一个冷冰冰的整数码,而是「码 + 人话描述」甚至「码 + 人话 + 出错位置」的组合体,描述可以走 `printf` 风格的格式化字符串、把出错的参数值一起带进去(比如「除零:a=10, b=0」),调用方一眼看懂。其二,错误对象是**调用方持有的、可跨调用留存**的:errno 是「最近一次失败」的全局单点,下一个调用一来就被覆盖;context 对象是调用方自己 `malloc` 出来的、放在自己栈/堆里的,不查就不会被覆盖,可以一直留到调用方方便处理的时候再读。其三,**返回值位置被彻底解放**:API 只返一个极简的成功/失败(0 / 非 0),所有「为什么失败」的细节都进 context——返回值不再需要在「真正要返的值」和「状态码」之间争位置。

照着这套思路,我写一个精简复刻版的 context 错误对象,再让 `div` 用它来报错。注意我故意多加了一个 `where` 字段(用 `__func__` 记出错位置),这是 FetchError 没有但实战里特别值钱的一笔——日志里直接告诉你「错是从哪个函数冒出来的」,省得你满调用栈打断点:

```c
/* context_demo.c */
/* 风格③:context(上下文)错误对象。
 * 把「错误码 + 人话描述 + 发生位置」打包进一个堆上分配的对象,
 * 调用者用完自己 free——线程安全(每个线程一份自己的 ctx,不碰全局 errno)。
 * 这是 clib FetchError 那套模型的精简复刻:API 返成功/失败,细节从 ctx 取。 */
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long error_code_t;

typedef struct {
    error_code_t code; /* 错误码(0 表示无错) */
    char* desc;        /* 堆上的人话描述,可跨调用存活 */
    const char* where; /* 出错位置(__FILE__/__func__/行,调试用) */
} error_ctx;

/* 分配一个全新的、干净的错误上下文 */
error_ctx* error_ctx_new(void) {
    error_ctx* e = malloc(sizeof(error_ctx));
    if (!e) {
        return NULL;
    }
    e->code = 0;
    e->desc = NULL;
    e->where = NULL;
    return e;
}

/* 设置错误:把描述拷到堆上,记下位置 */
void error_ctx_set(error_ctx* e, error_code_t code, const char* where, const char* fmt, ...) {
    if (!e) {
        return;
    }
    free(e->desc);
    e->code = code;
    e->where = where;

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) {
        e->desc = NULL;
        va_end(ap2);
        return;
    }
    e->desc = malloc((size_t) need + 1);
    if (e->desc) {
        vsnprintf(e->desc, (size_t) need + 1, fmt, ap2);
    }
    va_end(ap2);
}

/* 清空(重置成「无错」),供复用 */
void error_ctx_clear(error_ctx* e) {
    if (!e) {
        return;
    }
    free(e->desc);
    e->code = 0;
    e->desc = NULL;
    e->where = NULL;
}

/* 释放整个对象 */
void error_ctx_free(error_ctx* e) {
    if (!e) {
        return;
    }
    free(e->desc);
    free(e);
}

/* 业务函数:除法,出错时往 ctx 里写细节,自己只返成功/失败。
 * 返回值极简(0/非 0),所有「为什么失败」都进 ctx——这就是 context 模型的核心。 */
static int ctx_div(int a, int b, int* out, error_ctx* err) {
    if (b == 0) {
        error_ctx_set(err, 1, __func__, "除零:a=%d, b=%d", a, b);
        return -1;
    }
    if (a == INT_MIN && b == -1) {
        error_ctx_set(err, 2, __func__, "有符号溢出:a=%d, b=%d", a, b);
        return -1;
    }
    *out = a / b;
    return 0;
}

int main(void) {
    error_ctx* err = error_ctx_new(); /* 调用者持有自己的 ctx */

    int result = 0;
    int rc = ctx_div(10, 0, &result, err);
    if (rc != 0) {
        /* 细节全在 ctx 里:码、人话、位置 */
        printf("[失败] code=%ld, 位置=%s, 说明=%s\n", err->code, err->where, err->desc);
    }

    /* 同一个 ctx 复用:清掉上次的错误,再调一次 */
    error_ctx_clear(err);
    rc = ctx_div(20, 4, &result, err);
    if (rc != 0) {
        printf("[失败] %s\n", err->desc);
    } else {
        printf("[成功] 20/4 = %d, ctx 里 code=%ld(说明无错)\n", result, err->code);
    }

    error_ctx_free(err); /* 用完自己 free,不然内存泄漏 */
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -Wextra context_demo.c -o context_demo && ./context_demo
[失败] code=1, 位置=ctx_div, 说明=除零:a=10, b=0
[成功] 20/4 = 5, ctx 里 code=0(说明无错)
```

第一次 `ctx_div(10, 0, ...)`:失败,返回 -1,但**真正值钱的细节全进了 context**——错误码 1、出错位置 `ctx_div`(靠 `__func__` 自动记的)、人话描述「除零:a=10, b=0」(把出错的入参都带上了)。这一条日志看下去,根本不用打断点,你就知道是哪个函数、什么参数触发的错。第二次 `ctx_div(20, 4, ...)`:成功,返回 0,context 被清空后 `code=0` 表示无错。

这段代码有几个**值得特别点出来的工程细节**,因为它们全是 context 模型相对 errno 的「代价」所在。第一,`error_ctx_set` 里那段 `vsnprintf(NULL, 0, fmt, ap)` 先探一遍要多少字节、再 `malloc` 出正好大小的缓冲、第二次 `vsnprintf` 真往里写——这是 C 里做「格式化字符串塞堆」的标准两步法(直接 `vsprintf` 不知道要多大缓冲,容易溢出),`va_copy` 那一下是因为 `vsnprintf` 用过一次 `va_list` 之后状态未指定、得先复制一份留着第二次用。第二,描述字符串是**拷到堆上的**(`malloc` 出来的 `desc`),不是直接存调用方传进来的指针——因为调用方传的常常是个栈上的临时缓冲或字面量,函数返回后就失效了,context 要跨调用留存,就必须把描述拷成自己拥有的堆副本,这正是 clib `setError` 里 `COPY_TO_HEAP` 那一手的用意。第三,**用完必须 `error_ctx_free`**,不然就是内存泄漏——errno 模型没这毛病,因为 errno 是全局的、不用你分配;context 把所有权交给调用方,自由换来责任。

这套堆上分配 + 字符串拷贝的代码,正是该上 sanitizer 验证一遍的——`error_ctx` 反复 `malloc`/`free`,稍微写歪就是一个泄漏或 use-after-free。我们用 ASan + UBSan 跑一遍:

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined context_demo.c -o ctx_asan && ./ctx_asan
[失败] code=1, 位置=ctx_div, 说明=除零:a=10, b=0
[成功] 20/4 = 5, ctx 里 code=0(说明无错)
```

ASan/UBSan 一声不吭,正常退出——说明这段反复 `malloc`/`free` 的堆管理是干净的,没有泄漏、没有越界、没有 UB。clang 那边也一样:

```text
$ clang -std=c11 -Wall -Wextra -fsanitize=address,undefined context_demo.c -o ctx_asan_c && ./ctx_asan_c
[失败] code=1, 位置=ctx_div, 说明=除零:a=10, b=0
[成功] 20/4 = 5, ctx 里 code=0(说明无错)
```

到这里三种风格都真跑过了。最后说一句怎么选。返回码最朴素、零开销、零依赖,适合「失败原因单一、调用方就那几处」的小函数,但靠调用方自觉,大规模代码里迟早有人漏查。errno 适合「你的函数就是标准库那一脉、错误码能映射到 `EINVAL`/`ERANGE` 这种约定码」的场景,调用方用 `strerror` 一翻就懂,但永远记住 errno 只在出错后查、不能用来判断成功。context 对象适合「错误细节丰富(要带码 + 人话 + 位置)、要跨调用留存、是库的对外 API」的场景,代价是堆分配和所有权管理。真实工程里这三种常常**并存**:底层小函数用返回码、贴近标准库的用 errno、对外公开 API 用 context——没有谁替代谁,各管各的层。

## 小结

C 没有 exception,「告诉调用者出错了」这件事,语言不管,得我们自己用约定来做。最朴素的返回码模型让函数返 -1/NULL 表失败,零开销、零依赖、线程天然安全,但编译器不强制调用方查返回值,漏查就是拿个垃圾值继续跑——这是它最大的脆点。errno 约定把错误码标准化了(`EINVAL`/`ERANGE` 那一套),调用方用 `strerror`/`perror` 翻成人话,而且 glibc 上 errno 展开成 `(*__errno_location())`、线程局部有保证;但它有个铁律坑——errno 只在「确认出错之后」才有意义,成功调用之后它的值是「未指定」的、可能残留上一次失败的码,所以永远不能拿 errno 判断成功失败,只能拿它问「刚才那次为什么失败」。context 对象(像 clib 的 `FetchError`)把错误码 + 人话描述 + 出错位置打包进一个堆上分配的对象,API 只返成功/失败、细节全进 ctx,错误细节具象化、可跨调用留存、不抢返回值位置——代价是要管堆分配和所有权,用完得 `free`,所以这种代码该上 ASan/UBSan 验证堆管理干净。三种风格不是三选一,工程里常常并存:底层小函数返回码、贴近标准库的 errno、对外公开 API 用 context。带着这套地基,下一章我们把错误码、错误处理跟更完整的「模块边界设计」接起来,看怎么把错误约定写进头文件契约、让消费者一读 `.h` 就知道怎么接错。

## 参考资源

- **ISO/IEC 9899:2011** §7.5(`errno` 的定义:可修改左值、可有线程存储期、成功调用不保证清零——errno「只在出错有意义」的标准依据)、§7.21.6.1(`vsnprintf` 探长度写堆的两步法)。
- **`man errno` / `man strerror` / `man perror`**:POSIX 对 errno 线程局部的强制要求、错误码常量清单(`E*`)、`strerror`/`perror` 的用法。
- **`man pthread_create`**:线程局部 errno 的多线程验证(本章 `errno_threads.c` 的依据)。
- **Effective C**(Robert C. Seacord):第 4 章「statically typed, automatically checked」,讲返回值检查与错误处理工程实践。
- **Expert C Programming**(Peter van der Linden):第 7 章,讲 errno 的历史包袱与「库函数不保证成功后清 errno」这一坑。
- **本仓库** `projects/clib-utilities/SystemRelated/{Includes/CCSTDLib_FetchError.h,Sources/CCSTDLib_FetchError.c}`(`initError`/`setError`/`getError`/`freeError` 的 context 错误模型=本章正面活教材)、`Basic_Utils/Includes/CCSTDLib_Err.h`(集中错误码 enum 的工程化做法)。
