#pragma once
#include "CCSTDLibs_MyCompiles.h"
#ifdef CCSTD_LANGUAGE_C
#define CCSTDLIB_API	declspec(dllexport)
#else
#define CCSTDLIB_API	extern "C" declspec(dllexport)
#endif
