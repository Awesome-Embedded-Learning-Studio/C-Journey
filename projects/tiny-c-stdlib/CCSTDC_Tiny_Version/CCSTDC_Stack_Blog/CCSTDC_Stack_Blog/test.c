#include"CCSTDC_Stack.h"

void intergerPrint(int* num) {
	printf("%d ", *num);
}

int main()
{
	CCSTDC_Stack* stack = CCSTDC_Stack_initEmpty(sizeof(int), 10);
	int arr[10] = { 1,2,3,4,5,6,7,8,9,10 };
	for (int i = 0; i < 10; i++) {
		CCSTDC_Stack_PushBack(stack, &arr[i], sizeof(int));
	}

	int* getSpace = CCSTDC_Stack_PopBackAllInTurn(stack);

	for (int i = 0; i < 10; i++) {
		intergerPrint(&getSpace[i]);
	}
	printf("\n");
	SLASH;
	if (CCSTDC_Stack_isEmpty(stack)) {
		CCSTDC_LOG("Stack is currently NULL");
	}

	CCSTDC_Stack_DestroyStack(stack);
	free(getSpace);

}