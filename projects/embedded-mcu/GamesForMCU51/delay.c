void Delay1ms(unsigned int times)		//@11.0592MHz
{
	int i;
	for(i = 0; i < times; i++)
	{
		unsigned char i, j;

		i = 15;
		j = 90;
		do
		{
			while (--j);
		} while (--i);
	}
}

void Delay10mus(unsigned int times)		//@11.0592MHz
{
	int i;
	for(i = 0; i < times; i++)
	{		
		unsigned char i;

		i = 25;
		while (--i);
	}

}
