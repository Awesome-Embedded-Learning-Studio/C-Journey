#include "CCThread.h"
#include "CCMutex.h"
#include <stdio.h>

CCMutex	m;
static int g_val;
#define THREAD_CNT 10

CCThread_Tasks_Func_RetType task(CCThread_Tasks_Func_Param param) {
	CCMutex_lock(&m);
	g_val++;
	printf("g_val:> %d\n", g_val);
	CCMutex_unlock(&m);
	return 0;
}

typedef struct _demoAtomicInt {
	int		value;
	CCMutex* guard;
}DemoAtomicInt;

void initDemo(DemoAtomicInt* d) {
	d->guard = CCMutex_createCCMutex();
	d->value = 0;
}

void addDemo(DemoAtomicInt* d) {
	CCMutex_lock(d->guard);
	d->value++;
	printf("demoInt:> %d\n", d->value);
	CCMutex_unlock(d->guard);
}

void releaseDemo(DemoAtomicInt* d) {
	CCMutex_freeLock(d->guard);
}

int testGlobalMutex()
{
	CCMutex_createGlobalCCMutex(&m);
	CCThread* threads[THREAD_CNT];
	for (int i = 0; i < THREAD_CNT; i++)
	{
		threads[i] = CCThread_createImmThread(task, NUL_PTR);
	}

	for (int i = 0; i < THREAD_CNT; i++) {
		CCThread_EraseThread(threads[i]);
	}
	
	CCMutex_freeGlobalLock(&m);
	return 0;
}

DemoAtomicInt g_int;

CCThread_Tasks_Func_RetType addInt(CCThread_Tasks_Func_Param param) {
	addDemo(&g_int);
	return 0;
}

int testLocalMutex()
{
	initDemo(&g_int);
	CCThread* threads[THREAD_CNT];
	for (int i = 0; i < THREAD_CNT; i++)
		threads[i] = CCThread_createImmThread(addInt, NULL);
	for (int i = 0; i < THREAD_CNT; i++)
		CCThread_EraseThread(threads[i]);
	releaseDemo(&g_int);
	return 0;
}


int testMutex()
{
	return testGlobalMutex() || testLocalMutex();
}
