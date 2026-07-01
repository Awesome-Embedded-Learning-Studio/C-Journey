#include <limits.h>
#include <stdio.h>

int main(void) {
    int si = INT_MAX;
    printf("INT_MAX + 1 = %d\n", si + 1);  /* UB:有符号溢出 */
    return 0;
}
