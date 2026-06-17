#include"CCSTDC_Vector.h"
void integerPrint(int* num) {
	printf("%d ", *num);
}

void integerFind(int* num1, int* num2) {
	return *num1 == *num2;
}
void CCSTDC_Test1() {
	CCSTDC_Vector* getVector = CCSTDC_Vector_EmptyInit(sizeof(int), 10);
	for (int i = 0; i < 200; i++) {
		CCSTDC_Vector_PushBack(getVector, &i, sizeof(int));
	}

	CCSTDC_Vector_PrintList(getVector, integerPrint);

	CCSTDC_Vector_DestroyWhole(getVector);
}

void CCSTDC_Test2() {
	CCSTDC_Vector* insert_test = CCSTDC_Vector_EmptyInit(sizeof(int), 10);
	for (int i = 0; i < 10; i++) {
		CCSTDC_Vector_PushBack(insert_test, &i, sizeof(int));
	}

	int insertion = 120;

	CCSTDC_Vector_PrintList(insert_test, integerPrint);
	printf("\n");
	CCSTDC_Vector_Insert(insert_test, 30, &insertion);
	CCSTDC_Vector_PrintList(insert_test, integerPrint);
	CCSTDC_Vector* check_test = CCSTDC_Vector_EmptyInit(sizeof(int), 10);
	int check = 2;
	for (int i = 0; i < 20; i++) {
		CCSTDC_Vector_PushBack(check_test, &check, sizeof(int));
	}


	CCSTDC_Vector_Insert(check_test, 10, &check);
	CCSTDC_Vector_Insert(check_test, 6, &check);

	//int neverFind = 1000;
	CCSTDC_GainGuide_PosArray* getRes = CCSTDC_Vector_FindTarget(check_test, &check, integerFind);
	CCSTDC_Vector_PrintList(getRes, integerPrint);
	free(getRes);

	int* get = CCSTDC_Vector_GetPyPos(check_test, 10, CCSTDC_Copy);
	printf("Got the 11th elem:> %d", *get);
	free(get);

	CCSTDC_Vector_DestroyWhole(insert_test);
	CCSTDC_Vector_DestroyWhole(check_test);
}


void CCSTDC_Test3()
{
	CCSTDC_Vector* check_test = CCSTDC_Vector_EmptyInit(sizeof(int), 10);
	for (int i = 0; i < 20; i++) {
		CCSTDC_Vector_PushBack(check_test, &i, sizeof(int));
	}

	CCSTDC_Vector_PrintList(check_test, integerPrint);
	printf("\n");
	CCSTDC_Vector_EraseByPos(check_test, 11);
	CCSTDC_Vector_PrintList(check_test, integerPrint);

	CCSTDC_Vector_DestroyWhole(check_test);
}
int main()
{
	CCSTDC_Vector* check_test = CCSTDC_Vector_EmptyInit(sizeof(int), 10);
	int del = 10;
	for (int i = 0; i < 10; i++) {
		CCSTDC_Vector_PushBack(check_test, &i, sizeof(int));
		CCSTDC_Vector_Insert(check_test, i*2, &del);
	}
	CCSTDC_Vector_PrintList(check_test, integerPrint);
	printf("\n");
	CCSTDC_Vector_EraseByGivenData(check_test, &del, integerFind);
	CCSTDC_Vector_PrintList(check_test, integerPrint);
}