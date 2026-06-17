#pragma once
#include "DataStructBasicHeader.h"
#ifndef __CCBASIC_STRING_H_
#define __CCBASIC_STRING_H_

#ifdef	CCSTD_LANGUAGE_C

#define UPDATE_FROM_BEGIN		(0)
#define UPDATE_TIL_END			(-1)

typedef struct _basic_string
{
	const char* _heap_string;
	CCSize_t	string_length;
}CCBasicString;

CCBasicString* CCBasicString_createCCBasicString_fromRaw(const char* string, int begin, int end);
CCBasicString* CCBasicString_createCCBasicString_nChars(char ch, CCSize_t len);
CCBasicString* CCBasicString_CloneOne(CCBasicString* appendee, int begin, int end);


typedef enum _concatOptions {
	RET_APPENDEE,
	RET_NEW
}ConcatOptions;

CCBasicString* CCBasicString_concatBasicString_withRaw(CCBasicString* appendee,
										const char* raw, ConcatOptions option);
CCBasicString* CCBasicString_concatBasicString_withOther(CCBasicString* appendee,
											CCBasicString* appender, ConcatOptions option);

const char* CCBasicString_clone_from_CCBasicString(CCBasicString* strings);
const char* CCBasicString_RawView(CCBasicString* strings);

CCBOOL_t	CCBasicString_Free(CCBasicString* strings);

#endif // C Times
#endif

