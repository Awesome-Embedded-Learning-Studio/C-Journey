#include <stddef.h>
#include <stdio.h>

/* 试试改字段顺序!把 char c 挪到 int 后面,或加几个字段,看 sizeof 怎么变 */

/* A:字段顺序乱排 —— char / int / char */
typedef struct {
    char c;
    int i;
    char d;
} A;

/* B:字段重排 —— int 放前,两个 char 挤后面 */
typedef struct {
    int i;
    char c;
    char d;
} B;

int main(void) {
    printf("字段和都是 6 字节,但 sizeof 不同:\n");
    printf("  sizeof(A) = %zu  (c 偏移 %zu, i 偏移 %zu, d 偏移 %zu)\n", sizeof(A), offsetof(A, c),
           offsetof(A, i), offsetof(A, d));
    printf("  sizeof(B) = %zu  (i 偏移 %zu, c 偏移 %zu, d 偏移 %zu)\n", sizeof(B), offsetof(B, i),
           offsetof(B, c), offsetof(B, d));
    printf("\nA 字段之间塞了 padding,B 只在尾部塞 —— 这就是「字段顺序影响内存占用」。\n");
    printf("试着把 char c 删掉、或加一个 double,看 sizeof 怎么跳。\n");
    return 0;
}
