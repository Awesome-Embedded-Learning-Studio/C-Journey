#pragma once
#ifndef __CCSTDLIBS_UTILS__
#define __CCSTDLIBS_UTILS__
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_Err.h"
#include "CCSTDLib_Types.h"
// Mallocs
#ifdef CCSTD_LANGUAGE_C
#include <stdlib.h>
#include <memory.h>

#include "CCSTDLib_BasicUtils.h"
#include "CCSTDLib_MallocFree.h"
#include "CCSTDLib_SizeUtils.h"
#include "CCSTDLib_CompareUtils.h"

#else
#include <new>
#endif	// LanguaggeSelections

#endif
