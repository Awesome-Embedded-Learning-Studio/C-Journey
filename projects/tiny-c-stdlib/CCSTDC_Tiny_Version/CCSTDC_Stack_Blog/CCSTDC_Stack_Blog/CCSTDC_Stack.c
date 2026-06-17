#include "CCSTDC_Stack.h"

CCSTDC_Stack* CCSTDC_Stack_initEmpty(size_t dataSize, int num)
{
	CCSTDC_Stack* getStack = (CCSTDC_Stack*)malloc(sizeof(CCSTDC_Stack));
	if (getStack == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	void* getCoreData = malloc(dataSize * num);
	if (getCoreData == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getStack->coreData = getCoreData;
	getStack->capicity = num;
	getStack->dataSize = dataSize;
	getStack->curSize = 0;

	return getStack;
}

CCSTDC_Bool CCSTDC_Stack_PushBack(CCSTDC_Stack* stack, void* data, size_t dataSize)
{
	if (stack == NULL || data == NULL ) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (dataSize != stack->dataSize) {
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	if (stack->curSize == stack->capicity){
		CCSTDC_LOG("Stack is full, reject to push in");

		return CCSTDC_False;
	}

	memcpy(
		(char*)stack->coreData + stack->dataSize * stack->curSize, 
		data, 
		dataSize
	);

	stack->curSize++;
	return CCSTDC_True;
}

void* CCSTDC_Stack_PopBack(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is empty, reject to pop out!");

		return NULL;
	}
	stack->curSize--;
	void* dataPiece = malloc(stack->dataSize);
	if (dataPiece == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}
	return dataPiece;
}

CCSTDC_Bool CCSTDC_Stack_isEmpty(CCSTDC_Stack* stack)
{
	if (stack == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	return stack->curSize == 0;
}

CCSTDC_Bool CCSTDC_Stack_isFull(CCSTDC_Stack* stack)
{
	if (stack == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	return stack->curSize == stack->capicity;
	
}

void* CCSTDC_Stack_PopBackAllInTurn(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is empty, reject to pop out!");

		return NULL;
	}

	void* getCoreData = malloc(stack->dataSize * stack->curSize);
	if (getCoreData == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	for (int i = stack->curSize - 1, j=0; i >= 0; i--,j++) {
		memcpy(
			(char*)getCoreData + j * stack->dataSize,
			(char*)stack->coreData + stack->dataSize * i,
			stack->dataSize
		);
	}
	stack->curSize = 0;
	return getCoreData;
}

void* CCSTDC_Stack_GetFront(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is null,can not pop out");
		return NULL;
	}
	void* getCoreData = malloc(stack->dataSize);
	if (getCoreData == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}
	memcpy(
		getCoreData,
		(char*)stack->coreData + stack->dataSize * (stack->curSize-1),
		stack->dataSize
	);
	return getCoreData;
}

void* CCSTDC_Stack_GetRear(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is null,can not pop out");
		return NULL;
	}
	void* getCoreData = malloc(stack->dataSize);
	if (getCoreData == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}
	memcpy(
		getCoreData,
		stack->coreData,
		stack->dataSize
	);
	return getCoreData;
}

void CCSTDC_Stack_SimplePop(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is null,can not pop out");
		return;
	}
	stack->curSize--;
}

void CCSTDC_Stack_MakeClear(CCSTDC_Stack* stack)
{
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is null,can not pop out");
		return;
	}
	stack->capicity = 0;
}

void CCSTDC_Stack_DestroyStack(CCSTDC_Stack* stack)
{
	if (stack == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	if (stack->coreData != NULL) {
		free(stack->coreData);
	}

	free(stack);
}
