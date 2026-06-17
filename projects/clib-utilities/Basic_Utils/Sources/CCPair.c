#include "CCPair.h"
#include "CCSTDLib_Utils.h"
#include "CCPlainObject.h"

CCPair* CCPair_createPair(void* first, void* second,
	CCSize_t first_size, CCSize_t second_size)
{
	CCSTD_MALLOC_ONE(new_one, CCPair);
	new_one->first = createPlainObject(first, first_size, True);
	new_one->second = createPlainObject(second, second_size, True);
	return new_one;
}
CCPair* CCPair_clonePair(const CCPair* p)
{
	CCSTD_MALLOC_ONE(new_one, CCPair);
	new_one->first = clonePlainObject(p->first);
	new_one->second = clonePlainObject(p->second);
	return new_one;
}

void* CCPair_first(const CCPair* p) {
	DEFAULT_DENY(p, NUL_PTR);
	return p->first->object;
}
void* CCPair_second(const CCPair* p) {
	DEFAULT_DENY(p, NUL_PTR);
	return p->second->object;
}

CCBOOL_t CCPair_setFirst(CCPair* p, void* elem, CCSize_t first_size) 
{
	DEFAULT_DENY(p, False);
	return changePlainObject(p->first, elem, first_size, True);
}
CCBOOL_t CCPair_setSecond(CCPair* p, void* elem, CCSize_t second_size)
{
	DEFAULT_DENY(p, False);
	return changePlainObject(p->second, elem, second_size, True);
}
