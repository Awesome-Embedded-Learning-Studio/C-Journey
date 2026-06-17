#pragma once
#ifndef __CCMutex__H_
#define __CCMutex__H_
#include "CCSTDLibs_MyCompiles.h"
typedef enum __CCMutexError {
	CCMutex_NO_ERROR,
	CCMutex_NUL_Mutex,
	CCMutex_UNINIT_ERROR
}CCMutexError;

#ifdef CCSTD_USE_WINDOWS
// Windows Part
#include "CCSTDLib_Types.h"
struct	_RTL_CRITICAL_SECTION;
typedef struct	_RTL_CRITICAL_SECTION CCMutexCore;
typedef struct __CCMutex
{
	CCMutexCore*		core_lock;
	CCMutexError		e;
}CCMutex;

#else
struct CCMutex {
	void* empty;
};

#endif // Compiles according OS

CCBOOL_t		CCMutex_createGlobalCCMutex(CCMutex* locker);
CCBOOL_t		CCMutex_freeGlobalLock(CCMutex* locker);
CCMutex*		CCMutex_createCCMutex();
CCBOOL_t		CCMutex_lock(CCMutex* locker);
CCBOOL_t		CCMutex_unlock(CCMutex* locker);
CCBOOL_t		CCMutex_trylock(CCMutex* locker);
CCBOOL_t		CCMutex_freeLock(CCMutex* locker);
CCBOOL_t		CCMutex_mutexFine(CCMutex* locker);
CCMutexError	CCMutex_Error(CCMutex* locker);
#endif // __CCMutex__H_

