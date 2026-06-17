#pragma once
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_Types.h"
typedef enum _CCThread_Error {
	CCThread_NO_ERROR,
	CCThread_CreateError,
	CCThread_MultiRun,
	CCThread_NULError,
	CCThread_ReqFurtherResume,
	CCThread_UNABLE_JOIN,
	CCThread_Unable_Close,
	CCThread_Unknown_Error
}CCThread_Error;

#ifdef CCSTD_USE_WINDOWS

typedef void*						CCThread_Core_Base;
typedef void*						CCThread_Tasks_Func_Param;
typedef unsigned long				CCThread_Tasks_Func_RetType;
typedef CCThread_Tasks_Func_RetType (*CCThread_Task_Func_Type)(CCThread_Tasks_Func_Param);
typedef unsigned long				CCThread_ID;


typedef struct __CCThread
{
	CCThread_Core_Base*			thread_core;
	CCThread_Task_Func_Type		pFunc;
	CCThread_ID					id;
	CCBOOL_t					isRun;
	CCThread_Error				error;
}CCThread;

CCThread*		CCThread_createThread(CCThread_Task_Func_Type pFunc, CCThread_Tasks_Func_Param params, CCBOOL_t req_imm_run);
#define			CCThread_createImmThread(pFunc, params) CCThread_createThread(pFunc, params, True)
#define			CCThread_createSuspendThread(pFunc, params) CCThread_createThread(pFunc, params, False)

CCBOOL_t		CCThread_runThread(CCThread* thread);
CCBOOL_t		CCThread_joinThread(CCThread* thread);
CCBOOL_t		CCThread_EraseThread(CCThread* thread);
CCBOOL_t		CCThread_isFine(CCThread* thread);
CCThread_Error	CCThread_getError(CCThread* thread);

#else
struct CCThread
{
	void* empty;
};

#endif // OS_Related

