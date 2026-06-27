#include "CCSTDLib_Utils.h"
#include "CCSTDLib_MultiByte_WCharAdapter.h"
#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>
#endif
CCWideChar* Ascii2Wide(const char* str)
{
#ifdef CCSTD_USE_WINDOWS
	unsigned long req_size = strlen(str);
	unsigned long bytesCnt = MultiByteToWideChar(CP_ACP, 0, str, req_size, NULL, 0);
	CCSTD_MALLOC_TYPES_RAW_ARRAY(res, CCWideChar, bytesCnt + 1);
	MultiByteToWideChar(CP_ACP, 0, str, bytesCnt, res, req_size, res, bytesCnt);
	res[bytesCnt] = '\0';
	return res;
#else
	/* POSIX:窄字符即原生字符集,直接复制一份到堆上返回。 */
	COPY_TO_HEAP(res, str);
	return (CCWideChar*)res;
#endif
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
	/* POSIX:与 Ascii2Wide 对称,直接复制一份到堆上返回。 */
	COPY_TO_HEAP(res, str);
	return res;
#endif
}
