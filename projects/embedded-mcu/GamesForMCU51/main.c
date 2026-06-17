#include <REGX52.H>
#include "Utilitys.h"
#include "obtacle.h"
#include "player.h"
#include "GUI.h"
#include "stdlib.h"
#include "beep.h"
#include "defaultAct.h"
#include "KeyEvent.h"
#include "DigitalLED.h"
#include "UART.h"
#include "MatKey.h"
struct Obtacle obtacle1;
struct Obtacle obtacle2;
static struct Player player;
static DeadTime cacheOfDeath;
void UART_GetInstruction() interrupt 4
{
	if(RI == 1)
	{
		RI = 0;
		UART_OperatePlayer(&player, SBUF);
	}
}

void main()
{
	// For Begin

	Score getscore;
	DeadTime deadTime;
	unsigned char begin_ob;
	unsigned char enable_deadTime;
	unsigned int speed;
	unsigned char difficulty;
	unsigned char isPulse;
	PressedKey pressedKey;
	MenuChoice choice;
	pressedKey = KeyUnPressed;
	choice = Unknown;
	enable_deadTime = END_GAME_MAX_ALLOWED_DEAD_TIME;
	difficulty = DEF_DIF;
	begin_ob = rand()%0xFF;
	speed = SPEED;
	DEBUG_Init;
	MatrixLED_Init();
	InitUART();
	PLAYER_INIT;
	OBTACLE_POS_INIT;
	SetObtacle(&obtacle1, begin_ob);
	
	// Begin Select
	while(choice == Unknown)
	{
		pressedKey = KeyScanner(KeyCheckedOn, KeySingleDetected);
		choice = KeyEvent_GameMenuSelect(pressedKey);
		switch(choice)
		{	
			case Continue:
				goto CONTINUE_GAME;
			case BackToMainMenu:
CLEARVOID:
				ShowVoid();
				choice = Unknown;
				break;
			case MODIFY_SPEED:
				choice = Unknown;
				if(SelectSpeed(&speed, &pressedKey) == QUIT)
					goto CLEARVOID;  
				break;
			case MODIFY_DIFFICULTY:
				choice = Unknown;
				if(SelectDifficulty(&difficulty, &pressedKey) == QUIT)
					goto CLEARVOID;
				enable_deadTime = difficulty;
				break;
			default:
				break;
		}
	}
	
	while(1)
	{
CONTINUE_GAME:
		pressedKey = KeyScanner(KeyCheckedOn, KeySingleDetected);
		KeyEventReactor(&player, pressedKey);
		ShowCurSituation(&player, &obtacle1, speed);
		RunObtacle(&obtacle1);
		AfterEffect(&player, &obtacle1, &getscore, &deadTime);
		DEBUG(player.posX);
		isPulse = CheckPulse();
		if(isPulse)
		{
			BeepWTF;
			isPulse = 0;
			Delay1ms(100);
			while(!isPulse)
			{
				isPulse = CheckPulse();
			}
		}
		if(deadTime >= enable_deadTime)
		{
			srand(0);
			cacheOfDeath += deadTime;
			choice = Unknown;
			BeepFin();  
			while(choice == Unknown)
			{
				ShowNum(MAX_DIGITS_SCORE_SHOW, getscore);
				ShowNum(MAX_DIGITS_DEAD_SHOW , cacheOfDeath);
				pressedKey = KeyScanner(KeyCheckedOn, KeySingleDetected);
				choice = KeyEvent_GameMenuSelect(pressedKey);
				switch(choice)
				{
					case Continue:
						deadTime = 0;
						choice = Unknown;
						goto CONTINUE_GAME;
					case BackToMainMenu:
CLEARVOID2:
						ShowVoid();
						choice = Unknown;
						break;
					case MODIFY_SPEED:
						choice = Unknown;
						if(SelectSpeed(&speed, &pressedKey) == QUIT)
							goto CLEARVOID2;  
						break;
					case MODIFY_DIFFICULTY:
						choice = Unknown;
						if(SelectDifficulty(&difficulty, &pressedKey) == QUIT)
							goto CLEARVOID2;
						enable_deadTime = difficulty;
						break;
					default:
						break;
				}
			}
		}
	}
}