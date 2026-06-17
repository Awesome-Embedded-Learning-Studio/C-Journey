#ifndef __UTILITY_H_
#define __UTILITY_H_
#include "lcd1602.h"
// Program Mode
#define DebugMode 		0
#define Enable_UCPT		1

// Abstract Type Configure
typedef unsigned char 	DeadTime;
typedef unsigned int  	Score;
typedef void(*ShowNumberFuncPtr)(unsigned int*);
typedef enum{
	Unknown				= 0,
	Continue			= 1,
	MODIFY_SPEED		= 2,
	MODIFY_DIFFICULTY	= 3,
	BackToMainMenu		= 4
}MenuChoice;

#define	MODIFY	1
#define	QUIT 	0

// Bytes Operation
#define SET_BIT(Bytee, Where) ( Bytee = (Bytee) | ( 1 << Where) )
#define CLEAR_BIT(Bytee, Where) ( Bytee = (Bytee) & ~( 1 << Where) )
#define	GET_BIT(Bytee, Where)	( (Bytee & (1 << Where)) >> Where )

// SPEED CONFIGURE
#define MAX_SELECTABLE 6
#define FOR_UCPT	150
#define SLOW 		90
#define MEDIUM 		70
#define FAST	 	30
#define Hyper		20
#define WTF			10

#define SPEED		MEDIUM

// difficulty config
typedef enum{
	DIF_EASY 	= 3,
	DIF_MEDIUM 	= 2,
	DIF_HARD	= 1
}Difficulty;

#define DEF_DIF	DIF_MEDIUM

// MAX ALLOWED SHOW OBJs
#define MAX_ALLOWED_OBJS 2
// LED Matrix Config
#define LED_MAX_COL	8
#define LED_MIN_COL	0

// MAX_ALLOWED_APPEARED_Obtacles
#define MAX_ALLOWED_OBTACLES 3

// Player Pos
#define DEF_X 2
#define DEF_Y 4
#define MIN_PLAYER_X 	1
#define MAX_PLAYER_X 	5
#define PLAYER_INIT 	player.posX = DEF_X;player.posY = DEF_Y

// Obtacle Pos
#define OBTACLE_POS_INIT obtacle1.column = LED_MAX_COL
// Score Show Config
#define MAX_DIGITS_DEAD_SHOW 	3
#define MAX_DIGITS_SCORE_SHOW 	7

// Show Config Value
#define MAX_DIGITS_VALUE_SHOW	7

#define JUDGE_IS_DEAD(posPlayer, posObtacle) ((1 << posPlayer) & (posObtacle)) 
#define END_GAME_MAX_ALLOWED_DEAD_TIME 1

// Debug Special
#if DebugMode
#define DEBUG(X) 	LCD_ShowNum(0,0,X, 1)
#define DEBUG_Init	LCD_Init()
#else
#define DEBUG(X) 	;
#define DEBUG_Init 	;
#endif

#endif