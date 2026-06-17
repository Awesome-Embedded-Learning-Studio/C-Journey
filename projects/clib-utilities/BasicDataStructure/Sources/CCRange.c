#include "CCRange.h"
#include "CCSTDLib_Utils.h"
CCBOOL_t CCIntRange_checkRangeIfisBasicValid(CCIntRange* range)
{
	return range->begin <= range->end;
}

CCBOOL_t CCIntRange_checkRangeIfisUnsignedValid(CCIntRange* range)
{
	return	CCIntRange_checkRangeIfisBasicValid(range) &&
		range->begin >= 0 &&
		range->end >= 0;
}

CCIntRange	CCIntRange_makeIntRange(CCIntRange_t beg, CCIntRange_t end)
{
	CCIntRange r;
	r.begin = beg;
	r.end = end;
	return r;
}

CCIntRange* CCIntRange_makeHeapIntRange(CCIntRange_t beg, CCIntRange_t end)
{
	CCSTD_MALLOC_ONE(range, CCIntRange);
	range->begin = beg;
	range->end = end;
	return range;
}

CCSize_t	CCIntRange_GetSize(CCIntRange* range) {
	if (CCIntRange_checkRangeIfisBasicValid(range))
		return range->end - range->begin + 1;
	else
		return -1;
}

CCBOOL_t	CCIntRange_InRange(CCIntRange_t index, CCIntRange* range)
{
	if (!CCIntRange_checkRangeIfisBasicValid(range))
		return False;
	return index >= range->begin && index <= range->end;
}


