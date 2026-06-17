#include "KeyEvent.h"
#include "beep.h"
void KeyEventReactor(struct Player* player, PressedKey key)
{
	switch(key)
	{
		case Key1Pressed:
			moveUp(player);
			break;
		case Key2Pressed:
			moveDown(player);
			break;
		case Key3Pressed:
			moveLeft(player);
			break;
		case Key4Pressed:
			moveRight(player);
			break;
		default:
			break;
	}
	
	
}

MenuChoice KeyEvent_GameMenuSelect(PressedKey key)
{
	switch(key)
	{
		case Key1Pressed:
			return Continue; 
		case Key2Pressed:
			return BackToMainMenu;
		case Key3Pressed:
			return MODIFY_SPEED;
		case Key4Pressed:
			return MODIFY_DIFFICULTY;
		default:
			return Unknown;
	}
}
