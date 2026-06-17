#include "DigitalLED.h"

unsigned char Digits[10] = {
	0x3f,0x06,0x5b,0x4f,
	0x66,0x6d,0x7d,0x07,
	0x7f,0x6f}; // From 0 To 9

// Same as the Array Index Counter, count the first Position as the Index 0
#define GetLocation(location) (0x03 - location) 

void ShowDigit(Location location, DigitNum digits)
{
	unsigned char loc = GetLocation(location);
	P2_4 = GET_BIT(loc, 2);
	P2_3 = GET_BIT(loc, 1);
	P2_2 = GET_BIT(loc, 0);
	
	P0 = Digits[digits];
	
	Delay10mus(10);
	P0 = 0;
}

void ShowNum(Location maxPos, unsigned int num)
{
	unsigned char cnt;
	unsigned int copy;
	copy = num;
	cnt = 0;
	while(copy != 0)
	{
		ShowDigit(maxPos - cnt, copy%10);
		copy /= 10;
		cnt++;
	}
}

void ShowVoid(){
	P0 = 0;
}