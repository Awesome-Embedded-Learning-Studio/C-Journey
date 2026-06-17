#include <REGX52.H>
#include "Delay.h"

sbit RCK = P3^5;
sbit SCK = P3^6;
sbit SER = P3^4;

void _74HC595_WriteByte(unsigned char Byte)
{
	unsigned char i;
	for(i = 0; i < 8; i++)
	{
		SER = Byte&(0x80 >> i);
		SCK = 1;
		SCK = 0;
	}
	RCK = 1;
	RCK = 0;
}

void MatrixLED_Init()
{
	SCK = 0;
	RCK = 0;
}

void MatrixLED_ShowColomn(unsigned char column, Data)
{
	_74HC595_WriteByte(Data);
	P0 = ~(0x80 >> column);
	Delay(1);
	P0 = 0xFF;
}