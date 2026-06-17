#include <stdio.h>
#define SET_BIT(x, bit) (x = ((x) | (1 << bit) ))


int main()
{
	// a is set to be 00000001
	int a = 1;
	for(int i = 0; i < 200; i++)
	{
		SET_BIT(a, i);
		printf("%d\n",a);
	}
	return 0;
}
