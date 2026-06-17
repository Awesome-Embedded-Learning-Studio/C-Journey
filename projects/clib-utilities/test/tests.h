#pragma once

#ifndef __CCSTD_TEST_DATA_STRUCT_
#define __CCSTD_TEST_DATA_STRUCT_
#include "CCSTDLibs_MyCompiles.h"

#if ACCEPT_VLD_INCLUDE
#include "vld.h"
#endif

// Tests DataStructures
int testDynamicArray();

// Systems
int testMutex();
int testDynamicPlugin();
#endif
