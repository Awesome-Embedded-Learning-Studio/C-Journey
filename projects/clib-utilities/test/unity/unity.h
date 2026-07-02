/* 极简 Unity-兼容测试框架(ThrowTheSwitch/Unity 的核心子集)。
 * 真项目应 vendor 真正的 unity.c/unity.h/unity_internals.h(三文件、~2000 行);
 * 这里只留教学所需的核心:TEST_ASSERT_* 断言宏 + setUp/tearDown + RUN_TEST/UNITY_BEGIN/END。
 * 失败用 setjmp/longjmp 隔离——一条用例 FAIL 不影响下一条跑。*/
#ifndef MINI_UNITY_H
#define MINI_UNITY_H
#include <setjmp.h>
#include <stdio.h>

void setUp(void);    /* 每条用例前跑(由测试文件定义) */
void tearDown(void); /* 每条用例后跑 */

extern jmp_buf unity_jmp_buf;
extern int unity_fail_count;
extern int unity_pass_count;

void unity_run_test(void (*test)(void), const char* name);

#define UNITY_BEGIN() ((void)(unity_fail_count = 0), (void)(unity_pass_count = 0), 0)
#define UNITY_END() \
    (printf("\n%d Tests %d Failures\n", unity_pass_count + unity_fail_count, unity_fail_count), \
     (unity_fail_count > 0 ? 1 : 0))
#define RUN_TEST(test) unity_run_test(test, #test)

#define TEST_ASSERT_EQUAL_INT(exp, act) \
    do { if ((exp) != (act)) { \
        printf("  %s:%d FAIL: expected %d, got %d\n", __FILE__, __LINE__, (int)(exp), (int)(act)); \
        longjmp(unity_jmp_buf, 1); } } while (0)
#define TEST_ASSERT_TRUE(x) \
    do { if (!(x)) { \
        printf("  %s:%d FAIL: expected TRUE (%s)\n", __FILE__, __LINE__, #x); \
        longjmp(unity_jmp_buf, 1); } } while (0)
#define TEST_ASSERT_FALSE(x) \
    do { if ((x)) { \
        printf("  %s:%d FAIL: expected FALSE (%s)\n", __FILE__, __LINE__, #x); \
        longjmp(unity_jmp_buf, 1); } } while (0)
#endif
