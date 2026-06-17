#include <stdio.h>

int main()
{
	int i = 1;
    {
        int i = 2;
        static int k =4;
        printf("i = %d\n", i);
        printf("k = %d\n", k);
    }
    printf("i = %d\n", i);
    printf("k = %d\n", k);    
}
