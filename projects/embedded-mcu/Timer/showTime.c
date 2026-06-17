#include "lcd1602.h"

void showTime(unsigned char second, unsigned char minute, unsigned char hours)
{
	LCD_ShowNum(2,1,hours,2);
	LCD_ShowNum(2,4,minute,2);
	LCD_ShowNum(2,7,second,2);
}