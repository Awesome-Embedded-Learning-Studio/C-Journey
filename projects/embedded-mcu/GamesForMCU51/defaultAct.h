#ifndef __DEFAULTACT_H_
#define __DEFAULTACT_H_
#include "Beep.h"
#include "Utilitys.h"
#include "obtacle.h"
#include "player.h"
#include "key.h"
#include "DigitalLED.h"
void PassProcess(struct Player* player, Score* score);
void NotPassProcess(DeadTime* deadTime);
void AfterEffect(	struct Player* player, \
					struct Obtacle* obtacle,\
					Score* score, \
					DeadTime* deadTime);
					
unsigned char SelectSpeed(unsigned int* speed, PressedKey* pPressedkey);
unsigned char SelectDifficulty(unsigned char* difficulty, PressedKey* pPressedkey);	
#endif