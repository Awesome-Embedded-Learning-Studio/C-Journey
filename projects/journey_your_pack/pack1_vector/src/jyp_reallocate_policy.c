#include "jyp/jyp_reallocate_policy.h"
#include <stddef.h>

static size_t def_new_size_policy(size_t old)
{
    return 2 * old; // 默认，我们走两倍扩容
}

JYPMemoryAllocatePolicy* jyp_memory_allocate_policy_make_default(JYPMemoryAllocatePolicy* blank)
{
    blank->reallocate_size_policy = def_new_size_policy;
    return blank;
}