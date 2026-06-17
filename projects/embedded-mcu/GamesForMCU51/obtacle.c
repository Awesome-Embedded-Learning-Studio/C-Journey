#include "obtacle.h"
#include "stdlib.h"

//void GeneralObtacleInit(struct Obtacle* obtacle)
//{
//	obtacle->column = LED_MAX_COL;
//	obtacle->obtacleByte = 0;
//}

void ShowObtacle(struct Obtacle* pObt)
{
	MatrixLEDWriteColumnly(pObt->obtacleByte,pObt->column);
}

void SetObtacle(struct Obtacle* pObt, unsigned char obt)
{
	pObt->obtacleByte = obt;
}	

void RunObtacle(struct Obtacle* pObt)
{
	unsigned char res;
	if(pObt -> column <= LED_MIN_COL) // Deal OverFlow
	{
		res = rand() % 0xFF;
		SetObtacle(pObt, res);
		pObt -> column = LED_MAX_COL;
	}
	
	(pObt -> column)--; // Left Move 
	
}