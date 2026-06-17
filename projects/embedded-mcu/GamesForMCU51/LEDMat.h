#ifndef __LEDMAT_H_
#define __LEDMAT_H_
#include <REGX52.H>
#include "delay.h"
sbit RCK=P3^5;		//RCLK
sbit SCK=P3^6;		//SRCLK
sbit SER=P3^4;		//SER
void MatrixLED_Init();
void MatrixLEDWriteColumnly(unsigned char Byte, unsigned char Colomn);
void MatrixLEDWritePointOnly(unsigned char X, unsigned char Y);
#endif