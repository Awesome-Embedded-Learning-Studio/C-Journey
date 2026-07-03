#include "unity.h"

jmp_buf unity_jmp_buf;
int unity_fail_count = 0;
int unity_pass_count = 0;

void unity_run_test(void (*test)(void), const char* name) {
    printf("  %s ... ", name);
    fflush(stdout);
    if (setjmp(unity_jmp_buf) == 0) {
        setUp();
        test();
        tearDown();
        printf("PASS\n");
        unity_pass_count++;
    } else {
        /* FAIL 已在断言宏里 print 了原因,这里只标记+收尾 */
        tearDown();
        unity_fail_count++;
    }
}
