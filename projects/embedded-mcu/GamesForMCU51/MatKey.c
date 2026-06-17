#include "MatKey.h"
#include "Utilitys.h"
#include "DigitalLED.h"

static unsigned char curChoice;
unsigned char CheckPulse()
{
	unsigned int temp;
	P1 = 0x0f;
	temp = P1;						//若P3 = 1111 1110，就是扫描第一排
	temp = temp & 0x0f;	
	
	//检查是否有按键按下去了
	if(temp != 0x0f){
		// BeepHigh;//若第一个按键按下去，则P3 = 11101110 此时temp = 1110000 != 0xf0;
		
		temp = P1;					//延时后重新赋值
		temp = temp & 0x0f;
		if(temp == 0x0e)
		{
			return 1;
		}
		return 0;
	}
	return 0;
}

unsigned char isExitMatKeyMenu()
{
	unsigned int temp;
	P1 = 0x0f;
	temp = P1;						//若P3 = 1111 1110，就是扫描第一排
	temp = temp & 0xe0;	
	
	//检查是否有按键按下去了
	if(temp != 0xe0){
		// BeepHigh;//若第一个按键按下去，则P3 = 11101110 此时temp = 1110000 != 0xf0;
		
		temp = P1;					//延时后重新赋值
		temp = temp & 0x0f;
		if(temp == 0x0e)
		{
			return 1;
		}
		return 0;
	}
	return 0;
}

void Pulse()
{
	unsigned char state, stopState;
	state = 0;
	stopState = 1;
	while(stopState)
	{
		state = CheckPulse();
		if(state == 1)
		{
			break;
		}
	}
}

