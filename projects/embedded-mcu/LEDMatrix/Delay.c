void Delay500ms()		//@11.0592MHz
{
	unsigned char i, j, k;
	i = 4;
	j = 129;
	k = 119;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay20ms()		//@11.0592MHz
{
	unsigned char i, j;
	i = 36;
	j = 217;
	do
	{
		while (--j);
	} while (--i);
}

void Delay200ms()		//@11.0592MHz
{
	unsigned char i, j, k;
	i = 2;
	j = 103;
	k = 147;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay330ms()		//@11.0592MHz
{
	unsigned char i, j, k;

	i = 3;
	j = 80;
	k = 169;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay1ms()		//@11.0592MHz
{
	unsigned char i, j;
	i = 2;
	j = 199;
	do
	{
		while (--j);
	} while (--i);
}

void Delay(unsigned char milisecond)
{
	unsigned  char i;
	for(i = 0; i < milisecond; i++)
		Delay1ms();
}
