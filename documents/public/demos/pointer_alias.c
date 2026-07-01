#include <stdio.h>

int main(void) {
    int n = 42;
    int* p = &n;     /* p 指向 n */

    printf("改之前: n = %d, *p = %d\n", n, *p);
    *p = 100;        /* 通过指针改 n */
    printf("改之后: n = %d, *p = %d\n", n, *p);
    return 0;
}
