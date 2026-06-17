#pragma once
#include "CCSTDLib_Types.h"
struct __CCPlainObject;

typedef struct __Pair
{
	struct __CCPlainObject* first;
	struct __CCPlainObject* second;
}CCPair;

CCPair*		CCPair_createPair(	void* first, void* second, 
								CCSize_t first_size, CCSize_t second_size);
CCPair*		CCPair_clonePair(const CCPair* p);
void*		CCPair_first(const CCPair* p);
void*		CCPair_second(const CCPair* p);

CCBOOL_t	CCPair_setFirst(CCPair* p, void* elem, CCSize_t first_size);
CCBOOL_t	CCPair_setSecond(CCPair* p, void* elem, CCSize_t second_size);





