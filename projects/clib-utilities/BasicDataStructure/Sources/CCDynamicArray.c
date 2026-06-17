#include "CCDynamicArray.h"
#include "CCSTDLib_Utils.h"
#include "CCRange.h"
#include "CCSTDLib_Index_Handle.h"
CCDynamicArray* CCDynamicArray_createEmpty(CCSize_t elemSize)
{
	CCSTD_MALLOC_ONE(new_one, CCDynamicArray);
	new_one->core_data = NUL_PTR;
	new_one->core_size = 0;
	new_one->elem_size = elemSize;
	return new_one;
}


CCDynamicArray* CCDynamicArray_createCCDynamicArray(void* arrayBegin, CCSize_t n, CCSize_t size)
{
	CCSTD_MALLOC_ONE(new_one, CCDynamicArray);
	CCSTD_MALLOC_BYTES(core_datas, n * size);

	memcpy(core_datas, arrayBegin, n * size);
	new_one->core_data = core_datas;
	new_one->core_size = n;
	new_one->elem_size = size;
	return new_one;
}

CCDynamicArray* CCDynamicArray_cloneCCDynamicArray(CCDynamicArray* array)
{
	DEFAULT_DENY(array, NULL);
	CCSTD_MALLOC_ONE(new_one, CCDynamicArray);
	CCSTD_MALLOC_BYTES(core_datas, array->core_size * array->elem_size);

	memcpy(core_datas, array->core_data, array->core_size * array->elem_size);
	new_one->core_data = core_datas;
	new_one->core_size = array->core_size;
	new_one->elem_size = array->elem_size;

	return new_one;
}

CCBOOL_t		CCDynamicArray_pushBackSingle(CCDynamicArray* array, void* sigElem)
{
	DEFAULT_DENY(array, False);
	DEFAULT_DENY(sigElem, False);

	CCAny_t check = realloc(array->core_data, (array->core_size + 1) *array->elem_size);
	DEFAULT_DENY(check, False);
	array->core_data = check;
	CCBytes_t* p = (CCBytes_t*)array->core_data + array->core_size * array->elem_size;
	memcpy(p, sigElem, array->elem_size);
	array->core_size += 1;
	return True;
}

CCBOOL_t		CCDynamicArray_pushBackMulti(CCDynamicArray* pushee, void* array, CCSize_t n)
{
	DEFAULT_DENY(array, False);
	DEFAULT_DENY(pushee, False);

	CCAny_t check = realloc(pushee->core_data, (pushee->core_size + n) * pushee->elem_size);
	DEFAULT_DENY(check, False);
	pushee->core_data = check;
	CCBytes_t* p = (CCBytes_t*)pushee->core_data + pushee->core_size * pushee->elem_size;
	memcpy(p, array, pushee->elem_size * n);
	pushee->core_size += n;
	return True;
}


CCBOOL_t		CCDynamicArray_pushBackArray(CCDynamicArray* pushee, CCDynamicArray* array)
{
	DEFAULT_DENY(array, False);
	DEFAULT_DENY(pushee, False);
	DENY_NO_SAME_TYPE_CONT(array->elem_size, pushee->elem_size, False);
	CCAny_t check = realloc(pushee->core_data, (pushee->core_size + array->core_size) * pushee->elem_size);
	DEFAULT_DENY(check, False);
	pushee->core_data = check;
	CCBytes_t* p = (CCBytes_t*)pushee->core_data + pushee->core_size * pushee->elem_size;
	memcpy(p, array->core_data, array->elem_size * array->core_size);
	pushee->core_size += array->core_size;
	return True;
}

CCBOOL_t		CCDynamicArray_InsertSingle(CCDynamicArray* array, void* sigElem, int place)
{
	DEFAULT_DENY(array, False);
	DEFAULT_DENY(sigElem, False);

	CCIntRange range = CCIntRange_makeIntRange(0, array->core_size);

	if (!CCIntRange_InRange(place, &range))
		return False;

	CCAny_t check = realloc(array->core_data, (array->core_size + 1) * array->elem_size);
	DEFAULT_DENY(check, False);
	array->core_data = check;
	CCBytes_t* movingSrc = (CCBytes_t*)array->core_data + (place - 1) * array->elem_size;
	CCBytes_t* movingDst = (CCBytes_t*)array->core_data + (place) * array->elem_size;
	memmove(movingDst, movingSrc, array->elem_size * (array->core_size - place + 1));
	memcpy(movingSrc, sigElem, array->elem_size);

	array->core_size++;
	return True;
}



CCBOOL_t		CCDynamicArray_InsertMulti(CCDynamicArray* pushee, void* array, CCSize_t n, int where)
{
	DEFAULT_DENY(pushee, False);
	DEFAULT_DENY(array, False);

	CCIntRange range = CCIntRange_makeIntRange(0, pushee->core_size);

	if (!CCIntRange_InRange(where, &range))
		return False;

	CCBytes_t* check = realloc(pushee->core_data, (pushee->core_size + n) * pushee->elem_size);
	DEFAULT_DENY(check, False);
	pushee->core_data = check;

	CCBytes_t* movingSrc = (CCBytes_t*)pushee->core_data + (where - 1)*pushee->elem_size;
	CCBytes_t* movingDst = (CCBytes_t*)pushee->core_data + (where + n - 1) * pushee->elem_size;
	CCBytes_t* copyFrom = array;
	memmove(movingDst, movingSrc, pushee->elem_size * (pushee->core_size - where + 1));
	if (copyFrom == pushee->core_data)
		copyFrom += n * pushee->elem_size;

	memcpy(movingSrc, copyFrom, n * pushee->elem_size);

	pushee->core_size += n;

	return True;
}

CCBOOL_t CCDynamicArray_InsertArray(CCDynamicArray* pushee, CCDynamicArray* array, int where)
{
	DENY_NO_SAME_TYPE_CONT(array->elem_size, pushee->elem_size, False);
	return CCDynamicArray_InsertMulti(pushee, array->core_data, array->core_size, where);
}


CCBOOL_t		CCDynamicArray_Iterate(CCDynamicArray* arr, HandleEachElem func, void* args)
{
	DEFAULT_DENY(arr, False);
	DEFAULT_DENY(arr->core_data, False);
	CCBytes_t* end = (CCBytes_t*)arr->core_data + arr->core_size * arr->elem_size;
	for (CCBytes_t* oper = arr->core_data;
		oper != end;
		oper += arr->elem_size) {
		func(oper, args);
	}
	return True;
}

CCBOOL_t		CCDynamicArray_EraseSingle(CCDynamicArray* arr, int place)
{
	DEFAULT_DENY(arr, False);
	CCIntRange range = CCIntRange_makeIntRange(0, arr->core_size - 1);

	if (!CCIntRange_InRange(place, &range))
		return False;

	CCBytes_t* movingSrc = (CCBytes_t*)arr->core_data + (place + 1)*arr->elem_size;
	CCBytes_t* movingDst = (CCBytes_t*)arr->core_data + (place) * arr->elem_size;
	memmove(movingDst, movingSrc, arr->elem_size * (arr->core_size - place));
	arr->core_size -= 1;
	return True;
}

CCBOOL_t		CCDynamicArray_EraseConsistMutli(CCDynamicArray* arr, int beg, int end)
{
	DEFAULT_DENY(arr, False);
	CCIntRange range = CCIntRange_makeIntRange(0, arr->core_size - 1);
	if (!CCIntRange_InRange(beg, &range) || !CCIntRange_InRange(end, &range))
		return False;

	CCIntRange rangeCheckValid = CCIntRange_makeIntRange(beg, end);
	if (!CCIntRange_checkRangeIfisUnsignedValid(&rangeCheckValid))
		return False;

	CCBytes_t* movingSrc = (CCBytes_t*)arr->core_data + (end + 1)*arr->elem_size;
	CCBytes_t* movingDst = (CCBytes_t*)arr->core_data + beg * arr->elem_size;
	memmove(movingDst, movingSrc, arr->elem_size * (arr->core_size - end));
	arr->core_size -= CCIntRange_GetSize(&rangeCheckValid);
	return True;
}

CCSTD_Index_t	CCDynamicArray_Find(CCDynamicArray* arr, void* elem, 
	CCSTD_CmpFuncType func, CCSTD_Index_t from, CCSTD_Index_t end)
{
	DEFAULT_DENY(arr, -1);
	end = handleEnd(end, arr->core_size - 1);

	CCIntRange r = CCIntRange_makeIntRange(from, end);
	if (!CCIntRange_checkRangeIfisUnsignedValid(&r))
		return -1;
	if (end >= arr->core_size)
		return -1;

	for (CCSTD_Index_t t = from; t < end; t++)
	{
		if (func
			(
				elem,
				(CCAny_t*)((CCBytes_t*)arr->core_data + t * arr->elem_size)
			) == CCSTD_CmpRes_SAME)
		{
			return t;
		}
	}
	return -1;
}

CCBOOL_t		CCDynamicArray_Free(CCDynamicArray* arr)
{
	CCSTD_SAFE_FREE(arr->core_data);
	CCSTD_SAFE_FREE(arr);
	return True;
}
