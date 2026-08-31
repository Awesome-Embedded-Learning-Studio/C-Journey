// jyp_vector 的较真测试:example 是给人看的,这里的断言是给编译器看的
#include "jyp/jyp_vector.h"
#include "jyp/internal/allocation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int at(JYPVector* v, size_t i) { return *(int*)jyp_get_vector_at(v, i); }

// bsearch/qsort 同款比较器:防溢出的写法,(a>b)-(a<b),别用减法糊弄
static int int_cmp(const void* key, const void* elem) {
    const int k = *(const int*)key;
    const int e = *(const int*)elem;
    return (k > e) - (k < e);
}

// dtor 收到的是元素槽位的地址;元素本体是 char*,得 *(char**)elem 解出来再 free。
// (直接把 free 当 dtor 传 = 对着数组缓冲区中间的地址调 free,ASan 一炸一个准)
static void free_str(void* elem) { free(*(char**)elem); }

static void test_basic_push_insert_remove(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 2);

    int x = 10;
    int y = 20;
    int z = 30;
    jyp_push_back_vector(v, &x);
    jyp_push_back_vector(v, &y);
    jyp_push_back_vector(v, &z); // 第三次:count==capacity,必须扩容 2→4
    assert(v->capacity_ == 4);
    assert(jyp_vector_size(v) == 3);
    assert(at(v, 0) == 10 && at(v, 1) == 20 && at(v, 2) == 30);

    int n = 99;
    jyp_insert_vector_at(v, &n, 1); // 中间插入:重叠挪位的高危路径
    assert(jyp_vector_size(v) == 4);
    assert(at(v, 0) == 10 && at(v, 1) == 99 && at(v, 2) == 20 && at(v, 3) == 30);

    int out = 0;
    jyp_pop_back_vector(v, &out);
    assert(out == 30 && jyp_vector_size(v) == 3);

    jyp_remove_vector_at(v, &out, 0);
    assert(out == 10 && at(v, 0) == 99 && at(v, 1) == 20);

    jyp_insert_vector_at(v, &n, 99); // 越界插入:安静返回
    assert(jyp_vector_size(v) == 2);
    assert(jyp_get_vector_at(v, 2) == NULL); // 越界读取给 NULL

    assert(*(int*)jyp_front_vector(v) == 99);
    assert(*(int*)jyp_back_vector(v) == 20);

    jyp_push_front_vector(v, &n);
    assert(at(v, 0) == 99 && at(v, 1) == 99 && at(v, 2) == 20);
    jyp_pop_front_vector(v, &out);
    assert(out == 99 && at(v, 0) == 99 && at(v, 1) == 20);

    jyp_release_vector(v);
}

static void test_reserve_clear(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 2);

    jyp_reserve_vector(v, 10);
    assert(v->capacity_ == 10);
    for (int i = 0; i < 8; i++) {
        jyp_push_back_vector(v, &i);
    }
    assert(jyp_vector_size(v) == 8 && v->capacity_ == 10); // reserve 之后一口长不打嗝

    jyp_clear_vector(v);
    assert(jyp_vector_empty(v) && v->capacity_ == 10); // 人走了,房子还在
    int n = 7;
    jyp_push_back_vector(v, &n);
    assert(jyp_vector_size(v) == 1 && at(v, 0) == 7);

    jyp_release_vector(v);
}

static void test_shrink_to_fit(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 8);
    for (int i = 1; i <= 3; i++) {
        jyp_push_back_vector(v, &i);
    }
    jyp_shrink_to_fit_vector(v); // 8 的房子住 3 个人,退掉 5 间
    assert(v->capacity_ == 3 && jyp_vector_size(v) == 3);
    assert(at(v, 0) == 1 && at(v, 1) == 2 && at(v, 2) == 3); // 东西没丢

    int n = 4;
    jyp_push_back_vector(v, &n); // 退完再长:走默认策略 3→6
    assert(v->capacity_ == 6 && jyp_vector_size(v) == 4);
    jyp_release_vector(v);

    // 空数组 shrink:房子整个退了,之后照样能住人
    JYPVector* e = jyp_create_vector(sizeof(int), 4);
    jyp_shrink_to_fit_vector(e);
    assert(e->capacity_ == 0 && e->data_begin_ == NULL);
    jyp_push_back_vector(e, &n);
    assert(jyp_vector_size(e) == 1 && at(e, 0) == 4);
    jyp_release_vector(e);
}

static void test_clone(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 2);
    for (int i = 1; i <= 3; i++) {
        jyp_push_back_vector(v, &i);
    }

    JYPVector* c = jyp_clone_vector(v);
    assert(jyp_vector_size(c) == 3 && c->capacity_ == 3); // 克隆是紧凑的
    assert(at(c, 0) == 1 && at(c, 1) == 2 && at(c, 2) == 3);

    *(int*)jyp_get_vector_at(v, 0) = 777; // 改原件
    assert(at(c, 0) == 1);                // 克隆件岿然不动
    *(int*)jyp_get_vector_at(c, 2) = 888; // 改克隆件
    assert(at(v, 2) == 3);                // 原件岿然不动

    int n = 5;
    jyp_push_back_vector(v, &n);
    assert(jyp_vector_size(v) == 4 && jyp_vector_size(c) == 3); // 各过各的日子

    jyp_release_vector(v);
    jyp_release_vector(c);
}

static void test_find(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 2);
    int vals[] = {10, 99, 20, 99, 30};
    for (int i = 0; i < 5; i++) {
        jyp_push_back_vector(v, &vals[i]);
    }

    int key = 20;
    assert(jyp_find_in_vector(v, &key, int_cmp) == 2);

    key = 42; // 没有的东西
    assert(jyp_find_in_vector(v, &key, int_cmp) == SIZE_MAX);

    key = 99; // 有俩,认第一个
    assert(jyp_find_in_vector(v, &key, int_cmp) == 1);

    jyp_clear_vector(v);
    assert(jyp_find_in_vector(v, &key, int_cmp) == SIZE_MAX); // 空数组也认账

    jyp_release_vector(v);
}

static size_t plus_four_policy(size_t old_size) {
    return old_size + 4; // 步子小一点的增长策略
}

static void test_custom_policy(void) {
    JYPVector* v = jyp_create_vector(sizeof(int), 2);
    const JYPMemoryAllocatePolicy pol = {
        .reallocate_size_policy = plus_four_policy,
    };
    jyp_set_resize_vector_policy(v, &pol);

    for (int i = 1; i <= 7; i++) {
        jyp_push_back_vector(v, &i);
    }
    assert(v->capacity_ == 10); // 2 --(+4)--> 6,第 7 个又坐满 → 6 --(+4)--> 10
    jyp_push_back_vector(v, &(int){8});
    assert(v->capacity_ == 10); // 刚扩过,这一发免费
    assert(jyp_vector_size(v) == 8);

    jyp_release_vector(v);
}

static void test_default_resize_policy(void) {
    JYPMemoryAllocatePolicy blank;
    JYPMemoryAllocatePolicy* ret = jyp_memory_allocate_policy_make_default(&blank);
    assert(ret == &blank);                        // 就地填,不另开内存
    assert(blank.reallocate_size_policy(4) == 8); // 默认翻倍
    assert(blank.reallocate_size_policy(0) == 0); // 0 起步自己长不动(靠 reserve 的兜底)
}

// 记账本:vector 的每一笔分配都得走当前全局分配器
static struct {
    int allocs;
    int frees;
    int reallocs;
} g_book;

static void* counting_alloc(size_t size) {
    g_book.allocs++;
    return malloc(size);
}

static void counting_free(void* ptr) {
    g_book.frees++;
    free(ptr);
}

static void* counting_realloc(void* ptr, size_t new_size) {
    g_book.reallocs++;
    return realloc(ptr, new_size);
}

static void test_global_allocator_respected(void) {
    static const JYPMemoryAllocator counting = {
        .allocator = counting_alloc,
        .deallocator = counting_free,
        .reallocator = counting_realloc,
    };

    SetNewGlobalAllocator(&counting);
    g_book.allocs = 0;
    g_book.frees = 0;
    g_book.reallocs = 0;

    JYPVector* v = jyp_create_vector(sizeof(int), 2);
    assert(g_book.allocs == 2); // 一笔 struct,一笔数据缓冲

    for (int i = 1; i <= 3; i++) {
        jyp_push_back_vector(v, &i); // 第三次触发扩容,走记账的 realloc
    }
    assert(g_book.reallocs == 1);

    jyp_release_vector(v);
    assert(g_book.frees == 2); // 缓冲和 struct 都归还,一笔不多一笔不少

    SetNewGlobalAllocator(NULL); // 用完记得还,别祸害后面的测试
}

static void test_owned_strings(void) {
    JYPVector* sv = jyp_create_vector(sizeof(char*), 2);
    const char* raws[] = {"hello", "jyp", "vector"};
    for (int i = 0; i < 3; i++) {
        char* p = malloc(strlen(raws[i]) + 1);
        strcpy(p, raws[i]);
        jyp_push_back_vector(sv, (const void*)&p);
    }
    assert(strcmp(*(char**)jyp_get_vector_at(sv, 1), "jyp") == 0);

    // 存 owned 指针必须 release_with;漏了 dtor 的话 LSan 当场揭发
    JYPVector* sv2 = jyp_clone_vector(sv); // 浅拷贝:俩 vector 指同一批字符串
    jyp_release_vector_with(sv, NULL);     // 这份不办后事,只拆房子(字符串归 sv2 管)
    assert(strcmp(*(char**)jyp_get_vector_at(sv2, 2), "vector") == 0); // 字符串还活着
    jyp_release_vector_with(sv2, free_str); // 后事只能 sv2 这份办,free 一遍,正好
}

int main(void) {
    test_basic_push_insert_remove();
    test_reserve_clear();
    test_shrink_to_fit();
    test_clone();
    test_find();
    test_custom_policy();
    test_default_resize_policy();
    test_global_allocator_respected();
    test_owned_strings();
    printf("test_jyp_vector: 全部通过\n");
    return 0;
}
