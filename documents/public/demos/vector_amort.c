/* vector_amort.c —— 翻倍还是加一?十万次 push,realloc 各调几次
 * 《动态数组(二)》的计数分配器实验,压成一个自包含文件,在线也能跑。
 * 想玩:把 plus_one 改成加四、翻三倍,或者把起始容量 1 改成 2,再点"运行"。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* —— 极简分配器层:默认 malloc 家族,可整体换装(对应 common 层) —— */
typedef void* (*AllocFn)(size_t);
typedef void  (*FreeFn)(void*);
typedef void* (*ReallocFn)(void*, size_t);

typedef struct {
    FreeFn    release;
    AllocFn   allocate;
    ReallocFn reallocate;
} Allocator;

static long g_reallocs; /* 计数器:换装分配器的全部意义就在这 */

static void* counting_alloc(size_t n)             { return malloc(n); }
static void  counting_free(void* p)               { free(p); }
static void* counting_realloc(void* p, size_t n)  { g_reallocs++; return realloc(p, n); }

static const Allocator counting = {
    counting_free,
    counting_alloc,
    counting_realloc,
};

/* —— 极简 vector:只留 push 需要的内核(对应 pack1_vector) —— */
typedef size_t (*PolicyFn)(size_t old_cap);

typedef struct {
    void*             data;
    size_t            elem_size;
    size_t            count;
    size_t            cap;
    PolicyFn          policy;
    const Allocator*  alloc_;
} Vector;

static void vec_reserve(Vector* v, size_t want) {
    if (want <= v->cap) {
        return;
    }
    size_t new_cap = v->policy(v->cap);
    if (new_cap < want) {
        new_cap = want; /* 容量 0 起步 / 策略太保守时的兜底 */
    }
    v->data = v->alloc_->reallocate(v->data, new_cap * v->elem_size); /* 收的是字节! */
    v->cap = new_cap;
}

static void vec_push(Vector* v, const void* elem) {
    vec_reserve(v, v->count + 1);
    memcpy((char*)v->data + v->count * v->elem_size, elem, v->elem_size);
    v->count++;
}

static Vector vec_create(size_t elem_size, size_t initial_cap, PolicyFn policy) {
    Vector v = {
        counting.allocate(elem_size * initial_cap),
        elem_size,
        0,
        initial_cap,
        policy,
        &counting,
    };
    return v;
}

static void vec_release(Vector* v) {
    v->alloc_->release(v->data);
}

/* —— 两种增长策略 —— */
static size_t doubling(size_t old_cap) { return 2 * old_cap; }
static size_t plus_one(size_t old_cap) { return old_cap + 1; }

static void run_round(const char* label, const char* verb, PolicyFn policy) {
    g_reallocs = 0;
    Vector v = vec_create(sizeof(int), 1, policy);
    for (int i = 0; i < 100000; i++) {
        vec_push(&v, &i);
    }
    printf("%s:100000 次 push,realloc %s %ld 次(容量 %zu)\n",
           label, verb, g_reallocs, v.cap);
    vec_release(&v);
}

int main(void) {
    run_round("翻倍策略", "只调了", doubling);
    run_round("+1 策略", "调了", plus_one);
    return 0;
}
