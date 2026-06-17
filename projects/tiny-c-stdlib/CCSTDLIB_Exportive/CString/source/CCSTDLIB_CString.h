#define _CRT_SECURE_NO_WARNINGS 1

#pragma once // For All compliers that can recognize the commend "once"

#ifndef _CCSTDLIB_CString_H_
#define _CCSTDLIB_CString_H_
#include "CCSTDLIB_Utilitys.h"
#include <string.h>
typedef struct  
{
	char* coreData;
	CCSTDLIB_Size strSize;
	CCSTDLIB_Size strCapicity;
}CCSTDLIB_CString;

#ifndef CString
typedef CCSTDLIB_CString CString;
#endif


// init a empty CString
CCSTDLIB_EXPORT CCSTDLIB_BOOL		InitEmptyCString(CCSTDLIB_CString** pStr);

// init initialized CString
CCSTDLIB_EXPORT CCSTDLIB_BOOL		InitCStringLimited(CCSTDLIB_CString** pStr, const char* string, const CCSTDLIB_Size nch);
#define InitCString(PP_CCSTDLIB_CString, CoreString) InitCStringLimited(PP_CCSTDLIB_CString, CoreString, (const CCSTDLIB_Size)strlen(CoreString))

// 
// Copy CString
CCSTDLIB_EXPORT CCSTDLIB_BOOL		CopyCString(CCSTDLIB_CString** dst, CCSTDLIB_CString* src);

// Get RawCoreString back 
CCSTDLIB_EXPORT const char*			getCStringData(CCSTDLIB_CString* src);

// Concat the CString
CCSTDLIB_EXPORT CCSTDLIB_CString*	catCString(CCSTDLIB_CString* appendee, CCSTDLIB_CString* appender);

// Insert the CString to the target legal Index
CCSTDLIB_EXPORT CCSTDLIB_CString*	insertCString(CCSTDLIB_CString* appendee, CCSTDLIB_CString* appender, CCSTDLIB_Index index);

// Reverse the CString 
CCSTDLIB_EXPORT CCSTDLIB_CString*	reverseCString(CCSTDLIB_CString* src, enum CopyMovePolicy policy);

// CompareString
CCSTDLIB_EXPORT CCSTDLIB_Compare	compareCString(CCSTDLIB_CString* str1, CCSTDLIB_CString* str2);

// findString
CCSTDLIB_EXPORT CCSTDLIB_Index		findSubCString(const CCSTDLIB_CString* source, CCSTDLIB_CString* sub, CCSTDLIB_Index from_index);

// get the char with giving index
CCSTDLIB_EXPORT CCSTDLIB_BOOL		getCharCString(const CCSTDLIB_CString* source, const CCSTDLIB_Index index);

// make modify to the target string
CCSTDLIB_EXPORT CCSTDLIB_BOOL		changeCStringLimited(CCSTDLIB_CString** source, const char* newSource, const CCSTDLIB_Size nch);
#define ChangeCString(ppSRC, orgCStr) changeCStringLimited(ppSRC, orgCStr, strlen(orgCStr));

// Erase String
CCSTDLIB_EXPORT CCSTDLIB_BOOL		EraseCString(CCSTDLIB_CString* source);

// Remove targets
CCSTDLIB_EXPORT CCSTDLIB_BOOL		RemoveTargetCString(CCSTDLIB_CString** source, const char ch);

// getSubString
CCSTDLIB_EXPORT const char*			GetSubString(const CCSTDLIB_CString* source, CCSTDLIB_Index beg, CCSTDLIB_Index end);

// split C String
CCSTDLIB_EXPORT const char**		SplitCString(const CCSTDLIB_CString* source, CCSTDLIB_CString* spliter);
#endif