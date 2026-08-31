#pragma once
#include <stddef.h>
#include <stdlib.h>

typedef void* (*JYPMemoryAllocateFn)(const size_t size);
typedef void (*JYPMemoryDeAllocateFn)(void* buffer);
typedef void* (*JYPMemoryReAllocateFn)(void* buffer, const size_t new_size);

typedef struct {
    JYPMemoryDeAllocateFn deallocator;
    JYPMemoryAllocateFn allocator;
    JYPMemoryReAllocateFn reallocator;
} JYPMemoryAllocator;

/**
 * @brief Set the New Global Allocator, Which should be static for allocate lives long enough!
 *
 * @param allocator nullptr for back to default allocator, if gcc, thats glibc :)
 */
void SetNewGlobalAllocator(const JYPMemoryAllocator* allocator);

void* allocate_raw(size_t size, const JYPMemoryAllocator* allocator);
void release_raw(void* buffer, const JYPMemoryAllocator* allocator);
void* reallocate_raw(void* buffer, size_t new_size, const JYPMemoryAllocator* allocator);

// If default, then use internal c memory managements
static inline void* allocate(const size_t size) {
    return allocate_raw(size, NULL);
}

static inline void release(void* buffer) {
    return release_raw(buffer, NULL);
}

static inline void* reallocate(void* buffer, const size_t new_size) {
    return reallocate_raw(buffer, new_size, NULL);
}

static inline void* allocate_elements(const size_t elem_size, const size_t elem_cnt) {
    return allocate(elem_size * elem_cnt);
}

#define ALLOCATE_ELEMS(ElementType, ElementCnt) allocate_elements(sizeof(ElementType), ElementCnt)
