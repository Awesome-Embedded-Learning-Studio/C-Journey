#include "CCBasicString.h"

#ifdef CCSTD_LANGUAGE_C
#include "CCRange.h"
#include "CCSTDLib_Utils.h"
#include <string.h>

CCSize_t _private_CCBasicString_CalcRawEnd(const char* string) {
	return strlen(string);
}


CCBasicString* 
CCBasicString_createCCBasicString_fromRaw(const char* string, int begin, int end)
{
	// HANDLE_SPECIAL
	if (end == UPDATE_TIL_END)
		end = _private_CCBasicString_CalcRawEnd(string);

	CCIntRange r = CCIntRange_makeIntRange(begin, end);

	if (!CCIntRange_checkRangeIfisUnsignedValid(&r))
		return NUL_PTR;

	int malloc_size = CCIntRange_GetSize(&r) + 1;
	CCSTD_MALLOC_BYTES(raw_ptr, malloc_size);
	
	memcpy(raw_ptr, string + begin, malloc_size - 1);
	((CCBytes_t*)raw_ptr)[malloc_size - 1] = '\0';
	CCSTD_MALLOC_ONE(new_one, CCBasicString);
	new_one->_heap_string = raw_ptr;
	new_one->string_length = CCIntRange_GetSize(&r);
	return new_one;
}

CCBasicString* CCBasicString_createCCBasicString_nChars(char ch, CCSize_t len)
{
	if (!IS_INVALID_SIZE(len))
		return NUL_PTR;

	CCSTD_MALLOC_BYTES(raw_ptr, len + 1);
	for (int i = 0; i < len; i++)
		((CCBytes_t*)raw_ptr)[i] = ch;
	((CCBytes_t*)raw_ptr)[len] = '\0';
	CCSTD_MALLOC_ONE(new_one, CCBasicString);
	new_one->_heap_string = raw_ptr;
	new_one->string_length = len;
	return new_one;
}

const char* CCBasicString_RawView(CCBasicString* strings) {
	DEFAULT_DENY(strings, NULL);
	return strings->_heap_string;
}

CCBasicString* CCBasicString_CloneOne(CCBasicString* appendee, int begin, int end)
{
	DEFAULT_DENY(appendee, NULL);
	return CCBasicString_createCCBasicString_fromRaw(
		CCBasicString_RawView(appendee),
		begin,
		end
	);
}

CCBOOL_t	CCBasicString_Free(CCBasicString* strings)
{
	SERIOUS_QUIT(strings, INVALID_POINTER);
	CCSTD_SAFE_FREE(strings->_heap_string);
	CCSTD_SAFE_FREE(strings);
	return True;
}

#endif