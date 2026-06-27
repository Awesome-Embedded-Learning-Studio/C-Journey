#include "CCThread.h"
#include "CCSTDLib_Utils.h"
#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>

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
#include <pthread.h>
#include <stdlib.h>

/* pthread 线程函数要求返回 void*;而本库对外的任务签名是
   unsigned long (*)(void*)。用一个 trampoline 把后者包成前者,
   使 createThread/runThread/joinThread 的跨平台语义与 Windows 分支一致:
     req_imm_run == True  -> 立即启动线程;
     req_imm_run == False -> 暂不启动,runThread 时再起。
   POSIX 没有原生 CREATE_SUSPENDED,故挂起语义靠“暂不 pthread_create”实现。 */
typedef struct __CCThread_TrampolineArg
{
	CCThread_Task_Func_Type	pFunc;
	CCThread_Tasks_Func_Param	params;
}CCThread_TrampolineArg;

static void* CCThread_trampoline(void* raw)
{
	CCThread_TrampolineArg* arg = (CCThread_TrampolineArg*)raw;
	CCThread_Tasks_Func_RetType rc = arg->pFunc(arg->params);
	CCSTD_SAFE_FREE(arg);
	return (void*)(unsigned long)rc;
}

CCThread* CCThread_createThread(CCThread_Task_Func_Type pFunc, CCThread_Tasks_Func_Param params, CCBOOL_t req_imm_run)
{
	CCSTD_MALLOC_ONE(new_thread, CCThread);
	new_thread->thread_core	= NUL_PTR;
	new_thread->pFunc		= pFunc;
	new_thread->id			= 0;
	new_thread->isRun		= False;
	new_thread->error		= CCThread_NO_ERROR;

	if (req_imm_run)
	{
		CCSTD_MALLOC_ONE(tramp, CCThread_TrampolineArg);
		tramp->pFunc	= pFunc;
		tramp->params	= params;

		CCSTD_MALLOC_ONE(tid, pthread_t);
		if (pthread_create(tid, NULL, CCThread_trampoline, tramp) != 0) {
			CCSTD_SAFE_FREE(tramp);
			CCSTD_SAFE_FREE(tid);
			new_thread->error = CCThread_CreateError;
			return new_thread;
		}
		new_thread->thread_core	= tid;
		new_thread->isRun		= True;
	}
	return new_thread;
}

CCBOOL_t CCThread_runThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	if (thread->isRun) {
		thread->error = CCThread_MultiRun;
		return False;
	}

	CCSTD_MALLOC_ONE(tramp, CCThread_TrampolineArg);
	tramp->pFunc	= thread->pFunc;
	tramp->params	= NUL_PTR;

	CCSTD_MALLOC_ONE(tid, pthread_t);
	if (pthread_create(tid, NULL, CCThread_trampoline, tramp) != 0) {
		CCSTD_SAFE_FREE(tramp);
		CCSTD_SAFE_FREE(tid);
		thread->error = CCThread_CreateError;
		return False;
	}
	thread->thread_core	= tid;
	thread->isRun		= True;
	return True;
}

CCBOOL_t CCThread_joinThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	DEFAULT_DENY(thread->thread_core, False);
	if (pthread_join(*(pthread_t*)thread->thread_core, NULL) != 0) {
		thread->error = CCThread_UNABLE_JOIN;
		return False;
	}
	thread->isRun = False;
	return True;
}

CCBOOL_t CCThread_EraseThread(CCThread* thread)
{
	DEFAULT_DENY(thread, False);
	if (thread->isRun) {
		CCBOOL_t res = CCThread_joinThread(thread);
		if (!res)
			return False;
	}
	CCSTD_SAFE_FREE(thread->thread_core);
	CCSTD_SAFE_FREE(thread);
	return True;
}

CCBOOL_t		CCThread_isFine(CCThread* thread)
{
	return thread && thread->error == CCThread_NO_ERROR;
}
CCThread_Error CCThread_getError(CCThread* thread)
{
	if (!thread)
		return CCThread_NULError;
	return thread->error;
}

#endif
