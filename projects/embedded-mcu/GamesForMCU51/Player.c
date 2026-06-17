#include "player.h"
#include "Utilitys.h"
void ShowPlayerPos(struct Player* player)
{
	MatrixLEDWritePointOnly(player->posX, player->posY);
}

void moveUp(struct Player* player)
{
	player->posY++;
	if(player->posY >= LED_MAX_COL)
		player->posY = LED_MAX_COL - 1;
	BeepMedium;
}
void moveDown(struct Player* player)
{
	player->posY--;
	if(player->posY < LED_MIN_COL)
		player->posY = LED_MIN_COL;
	BeepMedium;
}

void moveLeft(struct Player* player)
{
	player->posX--;
	if(player->posX < MIN_PLAYER_X)
		player->posX = MIN_PLAYER_X;
	BeepMedium;
}

void moveRight(struct Player* player)
{
	player->posX++;
	if(player->posX >= MAX_PLAYER_X)
		player->posX = MAX_PLAYER_X;
	BeepMedium;
}


