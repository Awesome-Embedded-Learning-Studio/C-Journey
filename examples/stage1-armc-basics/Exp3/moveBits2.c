#include <stdio.h>
#define CLEAR_BIT(x, bit) (x = ((x) & (~(1 << (bit))) ) )

int main()
{
	int a = 0x11111111;
	printf("%d\n",a);
	for(int i = 0; i < 80; i++)
	{
		CLEAR_BIT(a, i);
		printf("%d\n",a);
	}
}
