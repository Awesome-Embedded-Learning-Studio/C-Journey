#pragma once
#include"CCSTDC_Utils.h"
#include<stdlib.h>
#include<stdio.h>
#include<memory.h>
typedef struct _CCSTDC_Stack {
	void* coreData;
	size_t curSize;
	size_t capicity;
	size_t dataSize;
}CCSTDC_Stack;

CCSTDC_Stack* CCSTDC_Stack_initEmpty(size_t dataSize, int num);

CCSTDC_Bool CCSTDC_Stack_PushBack(CCSTDC_Stack* stack, void* data, size_t dataSize);

void* CCSTDC_Stack_PopBack(CCSTDC_Stack* stack);

CCSTDC_Bool CCSTDC_Stack_isEmpty(CCSTDC_Stack* stack);

CCSTDC_Bool CCSTDC_Stack_isFull(CCSTDC_Stack* stack);

void* CCSTDC_Stack_PopBackAllInTurn(CCSTDC_Stack* stack);

void* CCSTDC_Stack_GetFront(CCSTDC_Stack* stack);

void* CCSTDC_Stack_GetRear(CCSTDC_Stack* stack);

void CCSTDC_Stack_SimplePop(CCSTDC_Stack* stack);

void CCSTDC_Stack_MakeClear(CCSTDC_Stack* stack);

void CCSTDC_Stack_DestroyStack(CCSTDC_Stack* stack);