#include <REGX52.H>
#include "Delay.h"
/*
	Þô¼ü

*/
unsigned char Key(void)
{
	unsigned char KeyNumber = 0;
	
	if(P3_1 == 0)
	{
		Delay20ms();
		while(P3_1 == 0);
		Delay20ms();
		KeyNumber = 1;
	}
	if(P3_0 == 0)
	{
		Delay20ms();
		while(P3_0 == 0);
		Delay20ms();
		KeyNumber = 2;
	}
	if(P3_2 == 0)
	{
		Delay20ms();
		while(P3_2 == 0);
		Delay20ms();
		KeyNumber = 3;
	}
	if(P3_3 == 0)
	{
		Delay20ms();
		while(P3_3 == 0);
		Delay20ms();
		KeyNumber = 4;
	}
	
	
	
	return KeyNumber;
}