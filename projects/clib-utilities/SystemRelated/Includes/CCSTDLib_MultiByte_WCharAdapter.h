#pragma once
#include "CCSTDLibs_MyCompiles.h"

#ifdef CCSTD_USE_WINDOWS
#include <vcruntime.h>
typedef wchar_t		CCWideChar;
#else
typedef char		CCWideChar;
#endif

// These Arrays are malloc in heap, remember to Free them when unused or untrackable!
CCWideChar* Ascii2Wide(const char* str);
const char* Wide2Ascii(CCWideChar* str);


