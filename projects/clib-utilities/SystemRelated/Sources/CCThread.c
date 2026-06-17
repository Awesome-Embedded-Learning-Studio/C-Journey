#include <Windows.h>
#include "CCThread.h"
#include "CCSTDLib_Utils.h"
#ifdef CCSTD_USE_WINDOWS

CCThread* CCThread_createThread(CCThread_Task_Func_Type pFunc, CCThread_Tasks_Func_Param params, CCBOOL_t req_imm_run)
{
	CCSTD_MALLOC_ONE(new_thread, CCThread);
	new_thread->thread_core = CreateThread(
		NULL, 0,
		pFunc, params,
		(req_imm_run ? 0 : CREATE_SUSPENDED),
		&new_thread->id				// Init the Threads id
	);
	if (!new_thread->thread_core) {
		new_thread->error = CCThread_CreateError;
		return new_thread;
	}

	new_thread->pFunc = pFunc;
	new_thread->error = CCThread_NO_ERROR;
	new_thread->isRun = req_imm_run;

	return new_thread;
}

CCBOOL_t CCThread_runThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	if (thread->isRun)
	{
		thread->error = CCThread_MultiRun;
		return False;
	}

	int res = ResumeThread(thread->thread_core);
	switch (res) {
	case 0: // Call bad
		thread->error = CCThread_MultiRun;
		return False;
	case 1:
		thread->isRun = True;
		return True;
	default:
		thread->error = CCThread_ReqFurtherResume;
		return False;
	}
}

CCBOOL_t CCThread_joinThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	DWORD res = WaitForSingleObject(thread->thread_core, INFINITE);
	switch (res)
	{
	case WAIT_FAILED:
		thread->error = CCThread_UNABLE_JOIN;
		return False;
	case WAIT_OBJECT_0:
		thread->isRun = False;
		return True;
	default:
		thread->error = CCThread_Unknown_Error;
		return False;
	}
}

CCBOOL_t CCThread_EraseThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	if (thread->isRun) {
		CCBOOL_t res = CCThread_joinThread(thread);
		if (!res) {
			return False;
		}
	}
	if (!CloseHandle(thread->thread_core)) {
		thread->error = CCThread_Unable_Close;
		return False;
	}
	CCSTD_SAFE_FREE(thread);
	return True;
}

CCBOOL_t		CCThread_isFine(CCThread* thread)
{
	return !thread && thread->error == CCThread_NO_ERROR;
}
CCThread_Error CCThread_getError(CCThread* thread)
{
	if (!thread)
		return CCThread_NULError;
	return thread->error;
}

#else

#endif
