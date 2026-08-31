// 演示:jyp vector 怎么用 —— 这是给人看的,较真的断言都在 test/ 里
#include "jyp/jyp_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_ints(const char* label, JYPVector* v) {
    printf("%s[%zu] =", label, jyp_vector_size(v));
    for (size_t i = 0; i < jyp_vector_size(v); i++) {
        printf(" %d", *(int*)jyp_get_vector_at(v, i));
    }
    printf("    (容量 %zu)\n", v->capacity_);
}

static char* dup_str(const char* s) {
    char* p = malloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

// release_with 的 dtor:元素是 char*,先解一层地址拿到指针本体
static void free_str(void* elem) { free(*(char**)elem); }

// bsearch/qsort 同款比较器
static int int_cmp(const void* key, const void* elem) {
    const int k = *(const int*)key;
    const int e = *(const int*)elem;
    return (k > e) - (k < e);
}

int main(void) {
    printf("== 基本款:push / insert / pop ==\n");
    JYPVector* v = jyp_create_vector(sizeof(int), 2);
    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        jyp_push_back_vector(v, &vals[i]); // 第三次坐不下了,自动扩容 2→4
    }
    print_ints("push x3      ", v);

    int n = 99;
    jyp_insert_vector_at(v, &n, 1); // 插到下标 1,后面整体右移
    print_ints("insert 99@1  ", v);

    int out = 0;
    jyp_pop_back_vector(v, &out);
    printf("pop_back 拿到 %d\n", out);
    print_ints("pop_back     ", v);

    jyp_remove_vector_at(v, &out, 0);
    printf("remove@0 拿到 %d\n", out);
    print_ints("remove@0     ", v);

    printf("\n== 手动挡:reserve / shrink_to_fit ==\n");
    jyp_reserve_vector(v, 10);
    print_ints("reserve(10)  ", v);
    jyp_shrink_to_fit_vector(v);
    print_ints("shrink_to_fit", v);

    printf("\n== clone:各自过日子 ==\n");
    JYPVector* c = jyp_clone_vector(v);
    *(int*)jyp_get_vector_at(v, 0) = 777; // 改原件
    print_ints("原件改[0]=777", v);
    print_ints("克隆件       ", c); // 克隆件岿然不动
    jyp_release_vector(c);

    printf("\n== find:bsearch 同款比较器 ==\n");
    int key = 20;
    size_t idx = jyp_find_in_vector(v, &key, int_cmp);
    printf("找 20 → 下标 %zu\n", idx);
    key = 42;
    idx = jyp_find_in_vector(v, &key, int_cmp);
    printf("找 42 → %s(没有就是 SIZE_MAX)\n", idx == SIZE_MAX ? "SIZE_MAX" : "?");
    jyp_release_vector(v);

    printf("\n== 存 owned 字符串:release_with 办后事 ==\n");
    JYPVector* sv = jyp_create_vector(sizeof(char*), 2);
    const char* raws[] = {"hello", "jyp", "vector"};
    for (int i = 0; i < 3; i++) {
        char* p = dup_str(raws[i]);
        jyp_push_back_vector(sv, (const void*)&p);
    }
    for (size_t i = 0; i < jyp_vector_size(sv); i++) {
        printf("sv[%zu] = \"%s\"\n", i, *(char**)jyp_get_vector_at(sv, i));
    }
    jyp_release_vector_with(sv, free_str); // 先逐个 free 字符串,再拆房子

    printf("\nexample: 演示完毕\n");
    return 0;
}
