#include "CCDynamicArray.h"
#include <stdio.h>

void handles(void* elem, void* arg) {
	printf("%d ", *(int*)elem);
}

int testDynamicArray()
{
	CCDynamicArray* arr = CCDynamicArray_createEmpty(int_SIZE);
	int t = 1;
	int _arr[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	CCDynamicArray_pushBackSingle(arr, &t);
	CCDynamicArray_pushBackSingle(arr, &t);
	CCDynamicArray_pushBackSingle(arr, &t);
	CCDynamicArray_pushBackMulti(arr, _arr, 10);
	CCDynamicArray_EraseSingle(arr, 9);
	CCDynamicArray_EraseConsistMutli(arr, 1, 3);
	CCDynamicArray_pushBackArray(arr, arr);
	CCDynamicArray_InsertSingle(arr, &_arr[0], 3);
	CCDynamicArray_InsertSingle(arr, &_arr[1], 3);
	CCDynamicArray_InsertMulti(arr, _arr, 10, 3);
	CCDynamicArray_Iterate(arr, handles, NUL_PTR);

	int res = 3;
	CCSTD_Index_t resIndex = CCDynamicArray_Find(arr, &res, compareInt, 0, TIL_END);
	if (resIndex != -1) {
		printf("find the elem:> %d in the arr pos:> %d\n", res, resIndex);
	}
	else {
		printf("no find the elem:> %d\n", res);
	}

	resIndex = CCDynamicArray_Find(arr, &res, compareInt, resIndex + 1,  TIL_END);
	if (resIndex != -1) {
		printf("find the elem:> %d in the arr pos:> %d\n", res, resIndex);
	}
	else {
		printf("no find the elem:> %d\n", res);
	}

	CCDynamicArray_Free(arr);

	return 0;
}
