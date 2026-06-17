#ifndef __DIGITALLED_H_
#define __DIGITALLED_H_
#include <REGX52.H>
#include "Utilitys.h"
#include "delay.h"
typedef unsigned char Location;
typedef unsigned char DigitNum;
	
void ShowDigit(Location location, DigitNum digits);
void ShowNum(Location maxPos, unsigned int num);
void ShowVoid();
#endif