#include "CCSTDLib_Index_Handle.h"

CCSTD_Index_t	handleEnd(CCSTD_Index_t* size, CCSTD_Index_t afterTreat)
{
	if (size == TIL_END)
		return afterTreat;
	return size;
}
