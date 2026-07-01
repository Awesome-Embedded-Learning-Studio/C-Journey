#include <stdio.h>

int main(void) {
    int n = 10;
    int* p = &n;   /* p 指向 n */
    *p = 42;       /* 通过指针改 n */
    printf("n = %d\n", n);
    return 0;
}
