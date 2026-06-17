#ifndef __UART_H_
#define __UART_H_
#include <REGX52.H>
#include "Utilitys.h"
#include "player.h"
typedef unsigned char Instruction;
static Instruction playerInstruction;

typedef enum{
	None		= 0,
	MOVE_UP		= (int)'w',
	MOVE_DOWN	= (int)'s',
	MOVE_LEFT	= (int)'a',
	MOVE_RIGHT	= (int)'d'
}InstructionEnum;


void InitUART(); // UART_Init
void UART_OperatePlayer(struct Player* player, Instruction Buf);

#endif