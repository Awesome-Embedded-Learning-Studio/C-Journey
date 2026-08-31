#include "jyp/jyp_vector.h"
#include "jyp/internal/allocation.h"
#include "jyp/jyp_reallocate_policy.h"
#include <stddef.h>
#include <string.h>

static size_t new_policy(size_t old_size) {
    return 2 * old_size;
}

static const JYPMemoryAllocatePolicy p = {
    .reallocate_size_policy = new_policy
};

// 地址三件套(base, index, elem_size)收进一个函数:从此只见 (vector, index)
static inline void* elem_index_addr(const JYPVector* v, size_t index) {
    return (char*)v->data_begin_ + (index * v->elem_size_);
}

JYPVector* jyp_create_vector(size_t elem_size, size_t initial_capacity)
{
    JYPVector* vec = ALLOCATE_ELEMS(JYPVector, 1);
    vec->data_begin_ = allocate_elements(elem_size, initial_capacity);
    vec->elem_size_ = elem_size;
    vec->capacity_ = initial_capacity;
    vec->current_count_ = 0;
    vec->policy = p;
    return vec;
}

void jyp_release_vector(JYPVector* vector)
{
    release(vector->data_begin_);
    release(vector);
}

void jyp_release_vector_with(JYPVector* vector, void (*dtor)(void* elem))
{
    if (dtor) {
        for (size_t i = 0; i < vector->current_count_; i++) {
            dtor(elem_index_addr(vector, i));
        }
    }
    jyp_release_vector(vector);
}

void jyp_insert_vector_at(JYPVector* vector, const void* data, size_t index)
{
    if (index > vector->current_count_) { // == count 合法(push_back 就靠它),> count 免谈
        return;
    }

    jyp_reserve_vector(vector, vector->current_count_ + 1);

    // [index, count) 右移一格到 [index+1, count+1) —— 源和目的地重叠,必须 memmove
    memmove(elem_index_addr(vector, index + 1),
            elem_index_addr(vector, index),
            (vector->current_count_ - index) * vector->elem_size_);

    memcpy(elem_index_addr(vector, index), data, vector->elem_size_); // 真正把元素放进去
    vector->current_count_++;
}

void jyp_remove_vector_at(JYPVector* vector, void* out, size_t index)
{
    if (index >= vector->current_count_) { // 顺手也救了空数组上的 pop_back(count-1 下溢)
        return;
    }
    if (out) { // 想留全尸就把 out 传进来
        memcpy(out, elem_index_addr(vector, index), vector->elem_size_);
    }

    // 和 insert 同一段 memmove,参数互换、往左挪
    memmove(elem_index_addr(vector, index),
            elem_index_addr(vector, index + 1),
            (vector->current_count_ - index - 1) * vector->elem_size_);
    vector->current_count_--;
}

void* jyp_get_vector_at(JYPVector* vector, size_t index)
{
    if (index >= vector->current_count_) {
        return NULL;
    }
    return elem_index_addr(vector, index);
}

// 扩容只保证"至少装得下 want 个",别的不管
void jyp_reserve_vector(JYPVector* vector, size_t want)
{
    if (want <= vector->capacity_) {
        return;
    }
    size_t new_cap = vector->policy.reallocate_size_policy(vector->capacity_);
    if (new_cap < want) {
        new_cap = want; // 容量 0 起步 / 策略太保守时的兜底
    }
    vector->data_begin_ = reallocate(vector->data_begin_, new_cap * vector->elem_size_); // reallocate 要的是字节!
    vector->capacity_ = new_cap;
}

void jyp_clear_vector(JYPVector* vector)
{
    vector->current_count_ = 0; // 容量留着,下一轮 push 不用重新长
}

void jyp_shrink_to_fit_vector(JYPVector* vector)
{
    if (vector->capacity_ == vector->current_count_) {
        return;
    }
    const size_t new_cap = vector->current_count_;
    if (new_cap == 0) { // 人都走光了,房子直接退了
        release(vector->data_begin_);
        vector->data_begin_ = NULL;
    } else {
        vector->data_begin_ = reallocate(vector->data_begin_, new_cap * vector->elem_size_);
    }
    vector->capacity_ = new_cap;
}

JYPVector* jyp_clone_vector(const JYPVector* vector)
{
    JYPVector* out = jyp_create_vector(vector->elem_size_, vector->current_count_);
    out->policy = vector->policy; // 策略一并继承
    out->current_count_ = vector->current_count_;
    if (out->current_count_ > 0) {
        memcpy(out->data_begin_, vector->data_begin_, out->current_count_ * vector->elem_size_);
    }
    return out;
}

size_t jyp_find_in_vector(const JYPVector* vector, const void* key,
                          int (*cmp)(const void* key, const void* elem))
{
    for (size_t i = 0; i < vector->current_count_; i++) {
        if (cmp(key, elem_index_addr(vector, i)) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

void jyp_set_resize_vector_policy(JYPVector* vector, const JYPMemoryAllocatePolicy* memory_allocations){
    vector->policy = *memory_allocations;
}
