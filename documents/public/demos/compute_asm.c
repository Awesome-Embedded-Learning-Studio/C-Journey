#include <stdio.h>

__attribute__((noinline)) int compute(int x) {
    int a = x * 3;
    int b = a + 1;
    return b;
}

int main(void) {
    printf("compute(7) = %d\n", compute(7));
    return 0;
}
