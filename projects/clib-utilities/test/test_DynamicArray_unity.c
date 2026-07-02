/* test_DynamicArray_unity.c —— 把原来的 ad-hoc printf 测试迁成 Unity 断言用例。
 * 对比 test/testDynamicArray.c(老版:一锅操作+printf、靠人眼判断),
 * 这里每条用例只测一个行为、用 TEST_ASSERT_* 自动判 pass/fail。*/
#include "unity.h"
#include "CCDynamicArray.h"

static int g_iter_count;
static void countEach(void* elem, void* arg) {
    (void) elem;
    (void) arg;
    g_iter_count++;
}

static CCDynamicArray* g_arr; /* 共享夹具:setUp 建、tearDown 拆 */

void setUp(void) {
    g_arr = CCDynamicArray_createEmpty(sizeof(int));
    g_iter_count = 0;
}

void tearDown(void) {
    CCDynamicArray_Free(g_arr);
    g_arr = NULL;
}

void test_pushSingle_then_iterate_counts_one(void) {
    int v = 42;
    CCBOOL_t ok = CCDynamicArray_pushBackSingle(g_arr, &v);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(1, g_iter_count);
}

void test_pushMulti_then_iterate_counts_three(void) {
    int vals[] = {10, 20, 30};
    CCBOOL_t ok = CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(3, g_iter_count);
}

void test_find_present_returns_nonneg_index(void) {
    int vals[] = {10, 20, 30};
    CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    int key = 20;
    CCSTD_Index_t idx = CCDynamicArray_Find(g_arr, &key, (CCSTD_CmpFuncType) compareInt, 0, TIL_END);
    TEST_ASSERT_TRUE(idx >= 0);
}

void test_find_absent_returns_notfound(void) {
    int vals[] = {10, 20, 30};
    CCDynamicArray_pushBackMulti(g_arr, vals, 3);
    int key = 999;
    CCSTD_Index_t idx = CCDynamicArray_Find(g_arr, &key, (CCSTD_CmpFuncType) compareInt, 0, TIL_END);
    TEST_ASSERT_TRUE(idx < 0);
}

void test_eraseSingle_shrinks_by_one(void) {
    int vals[] = {1, 2, 3, 4, 5};
    CCDynamicArray_pushBackMulti(g_arr, vals, 5);
    CCBOOL_t ok = CCDynamicArray_EraseSingle(g_arr, 2);
    TEST_ASSERT_TRUE(ok);
    CCDynamicArray_Iterate(g_arr, countEach, NUL_PTR);
    TEST_ASSERT_EQUAL_INT(4, g_iter_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pushSingle_then_iterate_counts_one);
    RUN_TEST(test_pushMulti_then_iterate_counts_three);
    RUN_TEST(test_find_present_returns_nonneg_index);
    RUN_TEST(test_find_absent_returns_notfound);
    RUN_TEST(test_eraseSingle_shrinks_by_one);
    return UNITY_END();
}
