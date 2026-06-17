// prework of my LIBS


#ifndef _CCSTDLIB_UTILITYS_H_
#define _CCSTDLIB_UTILITYS_H_
// preExtented 

#ifdef __cplusplus
#define CCSTDLIB_EXPORT extern "C" __declspec( dllexport )
#else
#define CCSTDLIB_EXPORT __declspec( dllexport )
#endif

/*-------------------------------------------------------------
* accquired basic headers
*-------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

/*-------------------------------------------------------------
* firstly, define micros and my type
*-------------------------------------------------------------*/
// boolean
#define CCSTDLIB_TRUE	1
#define CCSTDLIB_FALSE	0
typedef short CCSTDLIB_BOOL;

// size
typedef unsigned int CCSTDLIB_Size;

// index
typedef int CCSTDLIB_Index; // do not use unsigned int, for the exceptional case

// define my Result of Compare
typedef enum {
	CCSTDLIB_Smaller	= -1,
	CCSTDLIB_Equal		= 0,
	CCSTDLIB_Bigger	= 1
}CCSTDLIB_Compare;

// fastsize symbolized
enum FastSybolizedSize {
	SZCHAR = sizeof(char),
	SZINT = sizeof(int),
	SZLONG = sizeof(long),
	SZLONGLONG = sizeof(long long),
	SZFLOAT = sizeof(float),
	SZDOUBLE = sizeof(double),
	SZLONGDOUBLE = sizeof(long double),
	SZSHORT = sizeof(short),
	SZWCHAR_T = sizeof(wchar_t),
	SZCCSTDBOOL = sizeof(CCSTDLIB_BOOL)
};

enum CopyMovePolicy {
	CCSTDLIB_Copy,
	CCSTDLIB_Move
};

// Messages

// Mat
// Opencv
// Eilgn

#define MessageStringType const char*
#define MSG(MSG_STR) printf("%s\n", MSG_STR)


/*-------------------------------------------------------------
* define my Exception 
*-------------------------------------------------------------*/
#define CCSTDLIB_DEFAULT_EXCEPTION_PROCESS(CODE) exit(CODE)
#define CCSTDLIB_FILEELVEL_INFO do{	printf("In Line: %d\n", __LINE__);\
									printf("In Function: %s\n",__FUNCTION__);\
									printf("In File: %s\n",__FILE__);}while(0)
#define CCSTDLIB_Throw(MSG_STR, CODE) do{MSG(MSG_STR);CCSTDLIB_FILEELVEL_INFO;CCSTDLIB_DEFAULT_EXCEPTION_PROCESS(CODE);}while(0)
#define CCSTDLIB_MAXALLOWED_LOGLENGTH 1024
#define CCSTDLIB_ErrorLog_FILE_LEVEL_GetRawStr(LOG_MSG) sprintf(LOG_MSG, " Catch Error: %s, In Line: %d \n In Function: %s\n In File:%s\n",__LINE__, __FUNCTION__, __FILE__);

enum CCSTDLIB_ErrorCode
{
	CCSTDLIB_NULLPTR_CODE,
	CCSTDLIB_OVERFLOW_CODE,
	CCSTDLIB_UNDERFLOW_CODE,
	CCSTDLIB_CANNOT_MALLOC_CODE,
	CCSTDLIB_UNFIT_PARAM_CODE,
	CCSTDLIB_ILLEGAL_FREE_CODE
};

#define CCSTDLIB_NULLPTR_STR					"Never Pass a NullPtr, especially for the case that Dereferring the passing ptr"
#define CCSTDLIB_OVERFLOW_STR					"No, DisLike A OverFlow Value"
#define CCSTDLIB_UNDERFLOW_STR					"No, DisLike A UnderFlow Value"
#define CCSTDLIB_UNFIT_PARAM_STR				"So, what about re-considered a value that is VALID"
#define CCSTDLIB_CANNOT_MALLOC_STR				"Can not malloc space! perhaps for the limited Memory" 
#define CCSTDLIB_ILLEGAL_FREE_STR				"I can not free it twice!"


#define THROW_CCSTDLIB_NULLPTR_EXCEPTION		CCSTDLIB_Throw(CCSTDLIB_NULLPTR_STR, CCSTDLIB_NULLPTR_CODE)
#define THROW_CCSTDLIB_OVERFLOW_EXCEPTION		CCSTDLIB_Throw(CCSTDLIB_OVERFLOW_STR, CCSTDLIB_OVERFLOW_CODE)
#define THROW_CCSTDLIB_UNDERFLOW_EXCEPTION		CCSTDLIB_Throw(CCSTDLIB_UNDERFLOW_STR, CCSTDLIB_UNDERFLOW_CODE)
#define THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION	CCSTDLIB_Throw(CCSTDLIB_UNFIT_PARAM_STR, CCSTDLIB_UNFIT_PARAM_CODE)
#define THROW_CCSTDLIB_CANNOT_MALLOC_EXCEPTION	CCSTDLIB_Throw(CCSTDLIB_CANNOT_MALLOC_STR, CCSTDLIB_CANNOT_MALLOC_CODE)
#define THROW_CCSTDLIB_ILLEGAL_FREE_EXCEPTION	CCSTDLIB_Throw(CCSTDLIB_ILLEGAL_FREE_STR, CCSTDLIB_ILLEGAL_FREE_CODE)


// fast Memory Operation
#ifdef CCSTDLIB_Zero
#undef CCSTDLIB_Zero
#endif

#define CCSTDLIB_Zero 0
#define CCSTDLIB_ZeroMemory(ptrName, size) memset(ptrName, 0, size)

#define CCSTDLIB_Raw_Malloc(Type) (Type*)malloc(sizeof(Type))
#define CCSTDLIB_Raw_NMalloc(Type, N) (Type*)malloc(sizeof(Type) * (N))
#define CCSTDLIB_Raw_REALLOC(Type, ptrName ,newSize) do{ \
										Type* __CCSTDLIB_SPECIAL_PTR__ = (Type*)realloc(ptrName, newSize * sizeof(Type));\
										if(__CCSTDLIB_SPECIAL_PTR__ == NULL) \
											THROW_CCSTDLIB_CANNOT_MALLOC_EXCEPTION;\
										ptrName = __CCSTDLIB_SPECIAL_PTR__;}while (0)
#define CCSTDLIB_SafeMalloc(Type, ptrName) do{ \
								Type* __CCSTDLIB_SPECIAL_PTR__ = CCSTDLIB_Raw_Malloc(Type); \
								if( __CCSTDLIB_SPECIAL_PTR__ == NULL)\
									THROW_CCSTDLIB_CANNOT_MALLOC_EXCEPTION;\
								ptrName = __CCSTDLIB_SPECIAL_PTR__;\
								CCSTDLIB_ZeroMemory(ptrName, sizeof(Type));\
								}while(0)

#define CCSTDLIB_SafeNMalloc(Type, ptrName, N) do{ \
								Type* __CCSTDLIB_SPECIAL_PTR__ = CCSTDLIB_Raw_NMalloc(Type, N); \
								if( __CCSTDLIB_SPECIAL_PTR__ == NULL)\
									THROW_CCSTDLIB_CANNOT_MALLOC_EXCEPTION;\
								ptrName = __CCSTDLIB_SPECIAL_PTR__;\
								CCSTDLIB_ZeroMemory(ptrName, sizeof(Type) * N);\
								}while(0)


#define CCSTDLIB_SafeFree(ptrName) do{	if(ptrName == NULL)\
											THROW_CCSTDLIB_ILLEGAL_FREE_EXCEPTION;\
										free(ptrName);ptrName = NULL;}while(0)







#endif