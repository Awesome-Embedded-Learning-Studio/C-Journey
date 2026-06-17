#include "KeyScanner.h"
static PressedKey pressedKey = KeyUnChecked;
/************************************************************************************
 * Micros Of the Function Related
 *************************************************************************************/
#define IsKeyPressed(WhatKey) 								WhatKey == 0
#define DetailedKeyEvent(WhatKey, Event) 					if(IsKeyPressed(WhatKey)) Event
#define ReturnPressedKeyModule(WhatKey,ReturnKeyDescriptor) if(IsKeyPressed(WhatKey)) return ReturnKeyDescriptor
#define ReturnNoKeyPressed 									return KeyUnChecked
#define ReturnLastKey										return pressedKey

/************************************************************************************
 * Function: 		KeyScanner; 
 * Descriptions: 	Scanner with Mode certifications
 * Parameters:		char shallDetect
 * 					KeyDetectedMode detectMode
 *************************************************************************************/
PressedKey KeyScanner(char shallDetect, KeyDetectedMode detectMode)
{
	if(shallDetect) // Allowed monitoring KeyPressEvent
	{

		ReturnPressedKeyModule(Key1, Key1Pressed);
		ReturnPressedKeyModule(Key2, Key2Pressed);
		ReturnPressedKeyModule(Key3, Key3Pressed);
		// Add More Module here if there are more Keys
		if(detectMode == KeyConstantlyDetected)
		{	
			ReturnLastKey;
		}
		else
		{
			ReturnNoKeyPressed;
		}
	}
	else 
	{
		return KeyPressCheckerClosed;
	}		
 }