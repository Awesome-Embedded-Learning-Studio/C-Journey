#pragma once
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_Types.h"
#ifndef _INT_RANGE_H_
#define _INT_RANGE_H_
typedef int CCIntRange_t;
typedef struct _intRange
{
	CCIntRange_t begin;
	CCIntRange_t end;
}CCIntRange;

CCIntRange	CCIntRange_makeIntRange(CCIntRange_t beg, CCIntRange_t end);
CCBOOL_t	CCIntRange_checkRangeIfisBasicValid(CCIntRange* range);
CCBOOL_t	CCIntRange_checkRangeIfisUnsignedValid(CCIntRange* range);
CCIntRange* CCIntRange_makeHeapIntRange(CCIntRange_t beg, CCIntRange_t end);
CCSize_t	CCIntRange_GetSize(CCIntRange* range);
CCBOOL_t	CCIntRange_InRange(CCIntRange_t index, CCIntRange* range);


#endif

