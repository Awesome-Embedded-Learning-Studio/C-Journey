#pragma once
#include <stddef.h>
#include <stdint.h> // SIZE_MAX:find 找不到时的哨兵
#include "jyp_reallocate_policy.h"

/**
 * @brief JYP 动态数组
 *
 */
typedef struct {
    void* data_begin_;     // 我们的起点是？
    size_t elem_size_;     // 你这元素多大啊？
    size_t current_count_; // 现在我们有多少个？
    size_t capacity_;      // 哦，容量多大呢？

    JYPMemoryAllocatePolicy policy; // resize策略
} JYPVector;

/**
 * @brief 创建一个vector!
 *
 * @param elem_size
 * @param initial_capacity
 * @return JYPVector*
 */
JYPVector* jyp_create_vector(size_t elem_size, size_t initial_capacity);

/**
 * @brief 用完了把他释放掉
 *
 * @param vector
 */
void jyp_release_vector(JYPVector* vector);

/**
 * @brief 带元素遗体的释放:先对每个元素调一次 dtor,再释放数组本体。
 *        elem 是元素的地址(元素本身是 char* 时,指针要 *(char**)elem 拿)。
 *        存 owned 指针必须用这个,直接 release 就是泄漏制造机
 */
void jyp_release_vector_with(JYPVector* vector, void (*dtor)(void* elem));

/**
 * @brief 把 data 指着的元素拷一份插到 index(index == count 就是 push_back)
 *        index 越界(> count)就什么都不做,绝不乱拷
 */
void jyp_insert_vector_at(JYPVector* vector, const void* data, size_t index);

/**
 * @brief 删掉 index 上的元素;想留个全尸就把 out 传进来,不要就传 NULL
 */
void jyp_remove_vector_at(JYPVector* vector, void* out, size_t index);

/**
 * @brief 拿到 index 上元素的指针,直接读、直接改都行;越界(>= count)给 NULL
 */
void* jyp_get_vector_at(JYPVector* vector, size_t index);

/**
 * @brief 预留容量:之后 push 到 want 个以内不会再触发扩容
 */
void jyp_reserve_vector(JYPVector* vector, size_t want);

/**
 * @brief 清空(count 归零),容量留着,下一轮 push 不用重新长
 */
void jyp_clear_vector(JYPVector* vector);

/**
 * @brief 把容量收缩到刚好 count 个,多出来的房子退给分配器
 */
void jyp_shrink_to_fit_vector(JYPVector* vector);

/**
 * @brief 克隆:数组本体新开一份,元素按字节复制。
 *        注意元素内部若是指针,指到的东西不复制(存 owned 指针时两份会打架)
 */
JYPVector* jyp_clone_vector(const JYPVector* vector);

/**
 * @brief 从头线性找第一个让 cmp(key, elem) == 0 的元素,给下标;没有给 SIZE_MAX。
 *        cmp 是 bsearch/qsort 同款签名,你现成的比较器直接拿来用
 */
size_t jyp_find_in_vector(const JYPVector* vector, const void* key,
                          int (*cmp)(const void* key, const void* elem));

void jyp_set_resize_vector_policy(JYPVector* vector, const JYPMemoryAllocatePolicy* memory_allocations);

// ---- 下面是图省事的宏,直接摸字段;哪天结构体转 opaque,它们是第一批下岗的 ----

#define jyp_vector_size(vector) ((vector)->current_count_)
#define jyp_vector_empty(vector) ((vector)->current_count_ == 0)

// 空数组上 front/back 给 NULL(back 的 count-1 下溢会被 get 的越界守卫接住)
#define jyp_front_vector(vector) jyp_get_vector_at(vector, 0)
#define jyp_back_vector(vector) jyp_get_vector_at(vector, (vector)->current_count_ - 1)

#define jyp_push_back_vector(vector, data) \
    jyp_insert_vector_at(vector, data, (vector)->current_count_)

#define jyp_pop_back_vector(vector, out) \
    jyp_remove_vector_at(vector, out, (vector)->current_count_ - 1)

// O(n):头部的插/删要挪整段元素,图方便可以,热路径别用
#define jyp_push_front_vector(vector, data) jyp_insert_vector_at(vector, data, 0)
#define jyp_pop_front_vector(vector, out) jyp_remove_vector_at(vector, out, 0)
