#include "jyp/internal/allocation.h"
#include <stddef.h>
#include <stdlib.h>

static void* default_c_allocate(size_t size) {
    return malloc(size);
}

static void default_c_deallocate(void* ptr) {
    free(ptr);
}

static void* default_c_realloc(void* ptr, const size_t new_size) {
    return realloc(ptr, new_size);
}

static const JYPMemoryAllocator default_allocator = {
    .allocator = default_c_allocate,
    .deallocator = default_c_deallocate,
    .reallocator = default_c_realloc
};

// 当前全局分配器:没换过就一直是 malloc 家族
static const JYPMemoryAllocator* current_allocator = &default_allocator;

void SetNewGlobalAllocator(const JYPMemoryAllocator* allocator) {
    current_allocator = allocator ? allocator : &default_allocator;
}

void* allocate_raw(const size_t size, const JYPMemoryAllocator* allocator)
{
    const JYPMemoryAllocator* use_alloc = allocator ? allocator : current_allocator;
    return use_alloc->allocator(size);
}

void release_raw(void* buffer, const JYPMemoryAllocator* allocator)
{
    const JYPMemoryAllocator* use_free = allocator ? allocator : current_allocator;
    use_free->deallocator(buffer);
}

void* reallocate_raw(void* buffer, const size_t new_size, const JYPMemoryAllocator* allocator)
{
    const JYPMemoryAllocator* use_realloc = allocator ? allocator : current_allocator;
    return use_realloc->reallocator(buffer, new_size); // ← 原来这里忘了 return,返回的全是垃圾
}
