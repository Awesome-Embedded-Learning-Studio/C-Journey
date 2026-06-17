#include <REGX52.H>
sbit Key1 = P3^1;
sbit Key2 = P3^0;
sbit Key3 = P3^2;
sbit Key4 = P3^3;
// ...
// For More Check the Circuit

// Enums of the KeyPress
#define KeyCheckedOn 	1
#define KeyCheckedOff 	0
typedef enum{
	Key1Pressed,
	Key2Pressed,
	Key3Pressed,
	// Add More Keys Here
	KeyPressCheckerClosed,
	KeyUnChecked
} PressedKey;


typedef enum{
	KeySingleDetected,
	KeyConstantlyDetected
} KeyDetectedMode;

/************************************************************************************
 * Function: 		KeyScanner; 
 * Descriptions: 	Scanner with Mode certifications
 * Parameters:		char shallDetect
 * 					KeyDetectedMode detectMode
 *************************************************************************************/
PressedKey KeyScanner(char shallDetect, KeyDetectedMode detectMode);





