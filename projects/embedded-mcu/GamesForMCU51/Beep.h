#ifndef __BEEP_H_
#define __BEEP_H_
#include <REGX52.H>
#include "delay.h"

sbit Beep = P2^5;

void BeepAny(unsigned int frequency);
void BeepFin();
#define BeepMedium 			BeepAny(30)
#define BeepHigh			BeepAny(10)
#define BeepLow				BeepAny(100)
#define BeepWTF				BeepAny(3)
#endif