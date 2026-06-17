#ifndef __KEYEVENT_H_
#define __KEYEVENT_H_
#include <REGX52.H>
#include "key.h"
#include "player.h"
#include "Utilitys.h"

void KeyEventReactor(struct Player* player, PressedKey key);
MenuChoice KeyEvent_GameMenuSelect(PressedKey key);
// void KeyEvent_ModifyValue(unsigned int* value ,PressedKey key, unsigned int Max, unsigned int Min);
#endif

