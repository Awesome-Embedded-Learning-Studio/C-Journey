#pragma once
#include "DataStructBasicHeader.h"
#ifndef _CCDYNAMIC_ARR_H_
#define _CCDYNAMIC_ARR_H_

#ifdef CCSTD_LANGUAGE_C
typedef struct __CCDynamicArray
{
	CCAny_t		core_data;
	CCSize_t	core_size;
	CCSize_t	elem_size;
}CCDynamicArray;


CCDynamicArray* CCDynamicArray_createEmpty(CCSize_t elemSize);
CCDynamicArray* CCDynamicArray_createCCDynamicArray(void* arrayBegin, CCSize_t n, CCSize_t size);
CCDynamicArray* CCDynamicArray_cloneCCDynamicArray(CCDynamicArray* array);

CCBOOL_t		CCDynamicArray_pushBackSingle(CCDynamicArray* array, void* sigElem);
CCBOOL_t		CCDynamicArray_pushBackMulti(CCDynamicArray* pushee, void* array, CCSize_t n);
CCBOOL_t		CCDynamicArray_pushBackArray(CCDynamicArray* pushee, CCDynamicArray* array);

CCBOOL_t		CCDynamicArray_InsertSingle(CCDynamicArray* array, void* sigElem, int place);
CCBOOL_t		CCDynamicArray_InsertMulti(CCDynamicArray* pushee, void* array, CCSize_t n, int where);
CCBOOL_t		CCDynamicArray_InsertArray(CCDynamicArray* pushee, CCDynamicArray* array, int where);

CCBOOL_t		CCDynamicArray_Iterate(CCDynamicArray* arr, HandleEachElem func, void* args);

CCBOOL_t		CCDynamicArray_EraseSingle(CCDynamicArray* arr, int place);
CCBOOL_t		CCDynamicArray_EraseConsistMutli(CCDynamicArray* arr, int beg, int end);

CCSTD_Index_t	CCDynamicArray_Find(CCDynamicArray* arr, void* elem, CCSTD_CmpFuncType func, CCSTD_Index_t from, CCSTD_Index_t end);

CCBOOL_t		CCDynamicArray_Free(CCDynamicArray* arr);





#endif	


#endif

