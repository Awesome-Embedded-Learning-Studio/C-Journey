#pragma once
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_Types.h"
#include "CCSTDLib_Utils.h"

typedef void(*HandleEachElem)(void* elemSelf, void* args);
#define DENY_NO_SAME_TYPE_CONT(cont1, cont2, RET) \
	if(cont1 != cont2) return (RET);

