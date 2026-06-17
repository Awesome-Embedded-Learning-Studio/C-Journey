#include "Beep.h"

void BeepAny(unsigned int frequency)
{
	unsigned int i;
	i = 100;
	while(i-- > 0)
	{
		Beep = !Beep;
		Delay10mus(frequency);
	}
	
	Beep = 0;
}

void BeepFin()
{
	unsigned char i;
	for(i = 10; i < 20; i++)
	{
		BeepAny(i);
	}
}