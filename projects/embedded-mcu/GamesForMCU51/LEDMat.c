#include "LEDMat.h"
#include "Utilitys.h"
/**
  * @brief  写列数据
  * @param  无
  * @retval 无
  */
void writeBytes(unsigned char Byte)
{
	unsigned char i;
	for(i=0;i<8;i++)
	{
		SER=Byte&(0x80>>i);
		SCK=1;
		SCK=0;
	}
	RCK=1;
	RCK=0;
}

/**
  * @brief  点阵屏初始化
  * @param  无
  * @retval 无
  */
void MatrixLED_Init()
{
	SCK=0;
	RCK=0;
}

/**
  * @brief  列显示
  * @param  无
  * @retval 无
  */
void MatrixLEDWriteColumnly(unsigned char Byte, unsigned char Column)
{
	writeBytes(Byte);
	P0 = ~(0x80>>Column);
}

void MatrixLEDWritePointOnly(unsigned char X, unsigned char Y)
{
	unsigned char Byte;
	Byte = 0;
	SET_BIT(Byte, Y);
	writeBytes(Byte);
	P0 = ~(0x80>> X);
}


