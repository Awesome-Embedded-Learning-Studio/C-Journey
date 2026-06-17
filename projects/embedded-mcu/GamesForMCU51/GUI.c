#include "GUI.h"

void ShowCurSituation(struct Player* player, struct Obtacle* obtacle,unsigned int speed)
{
	unsigned int i;
	i = 0;
	while(i++ < speed)
	{
		ShowPlayerPos(player);
		Delay10mus(40);
		ShowObtacle(obtacle);
		Delay10mus(40);
	}
}

void ShowGameSpeed(unsigned int* pSpeed)
{
	ShowNum(MAX_DIGITS_VALUE_SHOW, *pSpeed);
}