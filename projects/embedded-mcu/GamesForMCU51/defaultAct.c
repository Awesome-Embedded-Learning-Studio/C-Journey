#include "defaultAct.h" 
unsigned int SPEEDTAB[MAX_SELECTABLE] = {FOR_UCPT, SLOW, MEDIUM, FAST, Hyper, WTF};

void PassProcess(struct Player* player, Score* score)
{
	BeepHigh;
	(*score) += player->posX;
}

void NotPassProcess(DeadTime* deadTime)
{
	BeepLow;
	(*deadTime)++;
}

void AfterEffect(struct Player* player, struct Obtacle* obtacle,Score* score, DeadTime* deadTime)
{
	if(player->posX != obtacle->column)
	{
		return;
	}
	if(JUDGE_IS_DEAD(player->posY, obtacle->obtacleByte))
	{
		NotPassProcess(deadTime);
	}
	else
	{
		PassProcess(player, score);
	}
}

unsigned char SelectSpeed(unsigned int* speed, PressedKey* pPressedkey)
{
	char curChoice;
	PressedKey pressedKey ;
	pressedKey = *pPressedkey;
	while(pressedKey != Key1Pressed)
	{
		ShowNum(3, *speed);
		pressedKey = KeyScanner(KeyCheckedOn, KeySingleDetected);
		switch(pressedKey)
		{
			case Key3Pressed:
				while(IsKeyPressed(Key3));
				curChoice++; curChoice%= MAX_SELECTABLE;BeepHigh;break;
			case Key4Pressed:
				while(IsKeyPressed(Key4));
				if(--curChoice < 0){curChoice = 0;}BeepHigh;break;
			case Key2Pressed:
				while(IsKeyPressed(Key2));
				return QUIT;
			default:
				break;
		}
		*speed = SPEEDTAB[curChoice];
	}
	*pPressedkey = KeyUnPressed;
	return MODIFY;
}

unsigned char SelectDifficulty(unsigned char* difficulty, PressedKey* pPressedkey)
{
	PressedKey pressedKey ;
	pressedKey = *pPressedkey;
	while(pressedKey != Key1Pressed)
	{
		ShowNum(3, *difficulty);
		pressedKey = KeyScanner(KeyCheckedOn, KeySingleDetected);
		switch(pressedKey)
		{
			case Key3Pressed:
				while(IsKeyPressed(Key3));
				(*difficulty) %= DIF_EASY;(*difficulty)++;BeepHigh;break;
			case Key4Pressed:
				while(IsKeyPressed(Key4));
				if(--(*difficulty) <= 0){*difficulty = 1;}
				BeepHigh;break;
			case Key2Pressed:
				while(IsKeyPressed(Key2));
				return QUIT;
			default:
				break;
		}
		
	}
	*pPressedkey = KeyUnPressed;
	return MODIFY;
}
