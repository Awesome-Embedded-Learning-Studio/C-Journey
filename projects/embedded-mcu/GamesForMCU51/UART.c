#include "UART.h"

void InitUART()
{
#if Enable_UCPT
	
	PCON &= 0x7F;		//波特率不倍速
	SCON = 0x50;		//8位数据,可变波特率
	TMOD &= 0x0F;		//设置定时器模式
	TMOD |= 0x20;		//设置定时器模式
	TL1 = 0xFA;			//设置定时初始值
	TH1 = 0xFA;			//设置定时重载值
	ET1 = 0;			//禁止定时器中断
	TR1 = 1;			//定时器1开始计时
	//开启中断
	EA=1;	//总中断控制
	ES=1;	//串口中断
#endif
}

void UART_OperatePlayer(struct Player* player, Instruction Buf)
{
#if Enable_UCPT
	switch(Buf)
	{
		case MOVE_UP:
			moveUp(player);break;
		case MOVE_DOWN:
			moveDown(player);break;
		case MOVE_LEFT:
			moveLeft(player);break;
		case MOVE_RIGHT:
			moveRight(player);break;
		default:break;
	}
#endif
}

