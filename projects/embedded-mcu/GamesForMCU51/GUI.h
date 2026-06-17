#ifndef __GUI_H_
#define __GUI_H_
#include "obtacle.h"
#include "player.h"
#include "DigitalLED.h"
void ShowCurSituation(struct Player* player, struct Obtacle* obtacle1, unsigned int speed);
void ShowGameSpeed(unsigned int* pSpeed);
#endif