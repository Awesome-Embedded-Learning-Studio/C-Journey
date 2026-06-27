#pragma once
#ifndef __CCMutex__H_
#define __CCMutex__H_
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_Types.h"   /* CCBOOL_t */
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
/* Linux/POSIX 实现:core_lock 用 void* 持有 pthread_mutex_t*,避免在头文件
   里直接拉入 <pthread.h>(保持头文件轻量,与 Windows 分支同样不暴露细节)。 */
typedef struct __CCMutex
{
	void*			core_lock;	/* pthread_mutex_t* */
	CCMutexError	e;
}CCMutex;

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

