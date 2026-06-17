#include <REGX52.H>
#include "MatrixLED.h"
#include "Delay.h"
#iniclude "Key.h"

#define SLOWSPEED 10
#define MEDIUMSPEED 5
#define FASTSPEED 3
#define FUCKING_FASTSPEED 2
#define SLOW 1
#define MEDIUM 2
#define FAST 3
#define FUCKING_FAST 4
#define DEFCONF MEDIUM
#define DEFCONFSPEED MEDIUMSPEED

unsigned char Ani[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0x08,0x08,0x08,0xFF,0x00,0x0E,0x15,
0x15,0x15,0x08,0x00,0xFE,0x01,0x02,0x00,
0xFE,0x01,0x02,0x00,0x0E,0x11,0x11,0x11,
0x0E,0x00,0x00,0xFD,0x00,0x00,0x00,0x00
};

static unsigned char speed = DEFCONFSPEED;

unsigned char fetchSpeed()
{
	unsigned char keyNum;
	keyNum = Key();
	switch(keyNum)
	{
		case SLOW:
			return SLOWSPEED;
		case MEDIUM:
			return MEDIUMSPEED;
		case FAST:
			return FASTSPEED;
		case FUCKING_FAST:
			return FUCKING_FASTSPEED;
		default:
			return speed;
	}
}

void main()
{
	
	unsigned char i = 0;
	unsigned char offset = 3;
	unsigned char count = 0;
	MatrixLED_Init();
	while(1)
	{
		speed = fetchSpeed();
		for(i = 0;i < 8; i++)
		{
			MatrixLED_ShowColomn(i, Ani[i + offset]);
		}
		if(count++ > speed){
			count = 0;
			offset++;
			offset %= 32;
		}
	}
}