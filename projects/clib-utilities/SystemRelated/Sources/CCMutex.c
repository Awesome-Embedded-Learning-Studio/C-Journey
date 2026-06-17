#include "CCMutex.h"
#include "CCSTDLib_Utils.h"
#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>

CCBOOL_t CCMutex_createGlobalCCMutex(CCMutex* locker)
{
	CCSTD_MALLOC_ONE(new_core_lock, CRITICAL_SECTION);
	InitializeCriticalSection(new_core_lock);
	locker->core_lock = new_core_lock;
	locker->e = CCMutex_NO_ERROR;
	return True;
}

CCBOOL_t CCMutex_freeGlobalLock(CCMutex* locker)
{
	CCSTD_SAFE_FREE(locker->core_lock);
	return True;
}

CCMutex* CCMutex_createCCMutex()
{
	CCSTD_MALLOC_ONE(new_mutex, CCMutex);
	CCSTD_MALLOC_ONE(new_core_lock, CRITICAL_SECTION);
	InitializeCriticalSection(new_core_lock);
	new_mutex->core_lock = new_core_lock;
	new_mutex->e = CCMutex_NO_ERROR;
	return new_mutex;
}

CCBOOL_t	CCMutex_lock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	DEFAULT_DENY(locker->core_lock, False);
	EnterCriticalSection(locker->core_lock);
	return True;
}

CCBOOL_t	CCMutex_unlock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	DEFAULT_DENY(locker->core_lock, False);
	LeaveCriticalSection(locker->core_lock);
	return True;
}

CCBOOL_t	CCMutex_trylock(CCMutex* locker)
{
	if (!locker->core_lock) {
		locker->e = CCMutex_UNINIT_ERROR;
		return False;
	}

	return TryEnterCriticalSection(locker->core_lock);
}
CCBOOL_t	CCMutex_freeLock(CCMutex* locker)
{
	CCSTD_SAFE_FREE(locker->core_lock);
	CCSTD_SAFE_FREE(locker);
	return True;
}

CCBOOL_t	CCMutex_mutexFine(CCMutex* locker)
{
	return locker && locker->e == CCMutex_NO_ERROR;
}
CCMutexError	CCMutex_Error(CCMutex* locker)
{
	if (!locker)
		return CCMutex_NUL_Mutex;
	return locker->e;
}
#else

#endif // Compiles according OS