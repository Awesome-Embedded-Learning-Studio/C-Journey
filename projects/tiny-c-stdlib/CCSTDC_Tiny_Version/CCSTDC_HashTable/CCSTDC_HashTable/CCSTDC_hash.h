#pragma once
#include "CCSTDC_Utils.h"
#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
/* Á´Ê½´æ´¢ */
typedef struct hashCell {
	unsigned int		key;
	void*				elem;
	struct hashCell*	next;
	size_t				appearTimes;
	size_t				elemSize;
}CCSTDC_HashCell;

typedef CCSTDC_HashCell**	HashCellArray, **CCSTDC_HashCellArray;
typedef unsigned int		HashKey;
typedef HashKey(*HashFunction)(void*);
typedef CCSTDC_Bool(*Comparator)(void*, void*);
typedef void(*Printer)(void*);
typedef struct hashMap {
	size_t				maxSize;
	HashCellArray		toMap;
	HashFunction		reflectFunction;
	Comparator			compFunc;
	HashKey*			DepracatedKeyArray;
	size_t				currentDeprecateKeyArraySize;
}CCSTDC_HashMap;

CCSTDC_HashMap*			CCSTDC_HashMap_EmptyInit(size_t wishedMaxSize,HashFunction hashfunction, Comparator	compFunc);
CCSTDC_HashMap*			CCSTDC_HashMap_CopyHash(CCSTDC_HashMap* AnotherHashMap);
CCSTDC_HashCell*		CCSTDC_HashMap_CopyCell(CCSTDC_HashCell* hashCell);
CCSTDC_HashCell*		CCSTDC_HashMap_WrapHashCell(CCSTDC_HashMap* toWhichMap,void* elem, size_t dataSize);
CCSTDC_HashCellArray	CCSTDC_HashMap_WrapHashCellList(CCSTDC_HashMap* toWhichMap, void* elem, size_t dataSize,int howMany);
CCSTDC_HashMap*			CCSTDC_HashMap_PutIntoHash(CCSTDC_HashMap* HashMap, CCSTDC_HashCell* cell);
#define CCSTDC_HashMap_PutCellListIntoHash(hashmap,cellArray,cellArraySize) \
	for(int i = 0; i < cellArraySize; i++){CCSTDC_HashMap_PutIntoHash(hashmap,cellArray[i]);}
void*					CCSTDC_HashMap_FindTarget(CCSTDC_HashMap* HashMap, HashKey key,void* elem);
CCSTDC_Bool				CCSTDC_HashMap_DeleteCell(CCSTDC_HashMap* HashMap, HashKey key,void* elem);
CCSTDC_Bool				CCSTDC_HashMap_RemoveKey(CCSTDC_HashMap* HashMap, HashKey key);
CCSTDC_Bool				CCSTDC_HashMap_EraseMap(CCSTDC_HashMap* map);
CCSTDC_Bool				CCSTDC_HashMap_PrintMap(CCSTDC_HashMap* map, Printer print);
CCSTDC_Bool				CCSTDC_HashMap_PrintCell(CCSTDC_HashCell* map, Printer cellPrinter);
CCSTDC_Bool				CCSTDC_HashMap_isValidKey(CCSTDC_HashMap* HashMap, HashKey key);
CCSTDC_Bool				CCSTDC_HashMap_AdjustInvalidKeyGroup(CCSTDC_HashMap* HashMap, HashKey key);