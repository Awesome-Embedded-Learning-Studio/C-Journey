// 分配器地基:记账换装、指定分配器优先、账目对得上
#include "jyp/internal/allocation.h"
#include <assert.h>
#include <stdio.h>

// 记账本:每次 malloc/free/realloc 都记一笔
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

static void reset_book(void) {
    g_book.allocs = 0;
    g_book.frees = 0;
    g_book.reallocs = 0;
}

static void test_explicit_allocator_param(void) {
    reset_book();
    const JYPMemoryAllocator counting = {
        .allocator = counting_alloc,
        .deallocator = counting_free,
        .reallocator = counting_realloc,
    };

    void* p = allocate_raw(16, &counting); // 指定分配器优先于全局
    assert(p != NULL && g_book.allocs == 1);
    release_raw(p, &counting);
    assert(g_book.frees == 1);
}

static void test_global_allocator_switch(void) {
    static const JYPMemoryAllocator counting = {
        .allocator = counting_alloc,
        .deallocator = counting_free,
        .reallocator = counting_realloc,
    };

    SetNewGlobalAllocator(&counting); // 换装
    reset_book();

    void* p = allocate(16); // 不指定分配器,就该走全局这身新衣服
    assert(p != NULL && g_book.allocs == 1);
    void* q = reallocate(p, 32);
    assert(q != NULL && g_book.reallocs == 1);
    release(q);
    assert(g_book.frees == 1);

    SetNewGlobalAllocator(NULL); // 换回默认
    void* d = allocate(8);
    release(d);
    assert(g_book.allocs == 1 && g_book.frees == 1); // 默认分配器干活,账本冻结
}

int main(void) {
    test_explicit_allocator_param();
    test_global_allocator_switch();
    printf("test_jyp_allocation: 全部通过\n");
    return 0;
}
