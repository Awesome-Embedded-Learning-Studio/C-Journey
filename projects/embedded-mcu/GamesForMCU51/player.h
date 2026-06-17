#ifndef __PLAYER_H_
#define __PLAYER_H_
#include "LEDMat.h"
#include "Beep.h"
struct Player{
	char posY;
	char posX;
};



void ShowPlayerPos(struct Player* player);
void moveUp(struct Player* pPlayer);
void moveDown(struct Player* pPlayer);
void moveLeft(struct Player* player);
void moveRight(struct Player* player);
#endif