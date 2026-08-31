#pragma once
#include <stddef.h>

typedef size_t (*NewSize)(size_t old_size);
typedef struct {
    NewSize reallocate_size_policy;
} JYPMemoryAllocatePolicy;

JYPMemoryAllocatePolicy* jyp_memory_allocate_policy_make_default(JYPMemoryAllocatePolicy* blank);
