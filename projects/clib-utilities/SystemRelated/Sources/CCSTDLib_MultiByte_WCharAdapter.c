#include <Windows.h>
#include "CCSTDLib_Utils.h"
#include "CCSTDLib_MultiByte_WCharAdapter.h"
CCWideChar* Ascii2Wide(const char* str)
{
#ifdef CCSTD_USE_WINDOWS
	unsigned long req_size = strlen(str);
	unsigned long bytesCnt = MultiByteToWideChar(CP_ACP, 0, str, req_size, NULL, 0);
	CCSTD_MALLOC_TYPES_RAW_ARRAY(res, CCWideChar, bytesCnt + 1);
	MultiByteToWideChar(CP_ACP, 0, str, bytesCnt, res, req_size, res, bytesCnt);
	res[bytesCnt] = '\0';
#else
	const char* res = NUL_PTR;
	COPY_TO_HEAP(res, str);
#endif
	return res;
}


const char* Wide2Ascii(CCWideChar* str)
{
#ifdef CCSTD_USE_WINDOWS
	unsigned long req_size = lstrlen(str);
	unsigned long bytesCnt = WideCharToMultiByte(CP_OEMCP, 0, str, req_size, NULL, 0, NULL, False);
	CCSTD_MALLOC_TYPES_RAW_ARRAY(res, char, bytesCnt + 1);
	WideCharToMultiByte(CP_UTF8, 0, str, req_size, res, bytesCnt, NULL, False);
	res[bytesCnt] = '\0';
	return res;
#else
	const char* res = NUL_PTR;
	COPY_TO_HEAP(res, str);
	return str;
#endif
}
