#pragma once
#ifndef __CCSTDLIB_ERR_
#define __CCSTDLIB_ERR_

#include "CCSTDLibs_MyCompiles.h"

#ifdef CCSTD_LANGUAGE_C
enum CCSTD_ALARM_LEVEL {
	RETURN = 0,
	EXIT = 1
};

enum CCSTD_ERROR_CODE {
	MALLOC_FAILED	= 1,
	INVALID_POINTER	= 2
};

#else
enum class CCSTD_ALARM_LEVEL {
	RETURN	= 0,
	EXIT	= 1
};

enum class CCSTD_ERROR_CODE {
	MALLOC_FAILED = 1,
	INVALID_POINTER = 2
};
#endif

#endif
