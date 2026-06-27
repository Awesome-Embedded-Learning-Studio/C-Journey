#include "CCMutex.h"
#include "CCSTDLib_Utils.h"
#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>
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
#include <pthread.h>

CCBOOL_t CCMutex_createGlobalCCMutex(CCMutex* locker)
{
	CCSTD_MALLOC_ONE(new_core_lock, pthread_mutex_t);
	if (pthread_mutex_init(new_core_lock, NULL) != 0) {
		CCSTD_SAFE_FREE(new_core_lock);
		locker->e = CCMutex_UNINIT_ERROR;
		return False;
	}
	locker->core_lock = new_core_lock;
	locker->e = CCMutex_NO_ERROR;
	return True;
}

CCBOOL_t CCMutex_freeGlobalLock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	DEFAULT_DENY(locker->core_lock, False);
	pthread_mutex_destroy((pthread_mutex_t*)locker->core_lock);
	CCSTD_SAFE_FREE(locker->core_lock);
	return True;
}

CCMutex* CCMutex_createCCMutex()
{
	CCSTD_MALLOC_ONE(new_mutex, CCMutex);
	CCSTD_MALLOC_ONE(new_core_lock, pthread_mutex_t);
	if (pthread_mutex_init(new_core_lock, NULL) != 0) {
		CCSTD_SAFE_FREE(new_core_lock);
		new_mutex->core_lock = NUL_PTR;
		new_mutex->e = CCMutex_UNINIT_ERROR;
		return new_mutex;
	}
	new_mutex->core_lock = new_core_lock;
	new_mutex->e = CCMutex_NO_ERROR;
	return new_mutex;
}

CCBOOL_t	CCMutex_lock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	DEFAULT_DENY(locker->core_lock, False);
	if (pthread_mutex_lock((pthread_mutex_t*)locker->core_lock) != 0) {
		locker->e = CCMutex_UNINIT_ERROR;
		return False;
	}
	return True;
}

CCBOOL_t	CCMutex_unlock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	DEFAULT_DENY(locker->core_lock, False);
	if (pthread_mutex_unlock((pthread_mutex_t*)locker->core_lock) != 0) {
		locker->e = CCMutex_UNINIT_ERROR;
		return False;
	}
	return True;
}

CCBOOL_t	CCMutex_trylock(CCMutex* locker)
{
	if (!locker || !locker->core_lock) {
		if (locker)
			locker->e = CCMutex_UNINIT_ERROR;
		return False;
	}
	if (pthread_mutex_trylock((pthread_mutex_t*)locker->core_lock) == 0)
		return True;
	locker->e = CCMutex_NO_ERROR;	/* 竞争失败不算内部错误,语义同 Windows */
	return False;
}
CCBOOL_t	CCMutex_freeLock(CCMutex* locker)
{
	DEFAULT_DENY(locker, False);
	if (locker->core_lock) {
		pthread_mutex_destroy((pthread_mutex_t*)locker->core_lock);
		CCSTD_SAFE_FREE(locker->core_lock);
	}
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

#endif // Compiles according OS