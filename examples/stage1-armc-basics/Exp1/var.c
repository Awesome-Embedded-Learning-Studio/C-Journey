#include <stdio.h>

/* 演示变量作用域与 static 存储期:
 *   - 内层 i 遮蔽外层 i;
 *   - static 让 k 的值存活到程序结束,但 k 的"作用域"仍只限于内层块。
 *     离开块后再用名字 k 访问它就是编译错误——这正是本例要展示的点。
 */
int main()
{
    int i = 1;
    {
        int i = 2;
        static int k = 4;
        printf("i = %d\n", i);
        printf("k = %d\n", k);
    }
    printf("i = %d\n", i);
    /* 此处已无法访问 k:作用域已结束(尽管它的值仍存活) */
    return 0;
}
