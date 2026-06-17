#ifndef __OBTACLE_H_
#define __OBTACLE_H_

#include <REGX52.H>
#include "LEDMat.h"
#include "Utilitys.h"
#define WAITRUN Delay1ms(SPEED)

struct Obtacle
{
	char obtacleByte;
	char column;
};


// void RunPerObtacle(unsigned char speed, Obtacle obtacle);
// #define DefSpeed_RunObtacle(obtacle) RunPerObtacle(SPEED, obtacle)

// void GeneralObtacleInit(struct Obtacle* obtacle);
void ShowObtacle(struct Obtacle* pObt);
void SetObtacle(struct Obtacle* pObt, unsigned char obt);
void RunObtacle(struct Obtacle* pObt);
#endif