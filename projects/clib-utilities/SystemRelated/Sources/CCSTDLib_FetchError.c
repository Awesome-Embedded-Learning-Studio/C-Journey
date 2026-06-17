#include <Windows.h>
#include "CCSTDLib_FetchError.h"
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_MultiByte_WCharAdapter.h"
#include "CCSTDLib_Utils.h"

CCSTDLib_FetchError*	initError()
{
	CCSTD_MALLOC_ONE(res, CCSTDLib_FetchError);
	res->discrip = NULL;
	clearError(res);
	return res;
}

void	freshError(CCSTDLib_FetchError* errorBuf)
{
	if (!errorBuf)
		return;
#ifdef CCSTD_USE_WINDOWS
	CCSTDLib_GeneralErrorCode code = GetLastError();
	CCSTD_SAFE_FREE(errorBuf->discrip);
	LPSTR lpMsgBuf = NULL;
	//标志位，决定如何说明lpSource参数，dwFlags的低位指定如何处理换行功能在输出缓冲区，也决定最大宽度的格式化输出行,可选参数。
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,//根据dwFlags标志而定。
		code,//请求的消息的标识符。当dwFlags标志为FORMAT_MESSAGE_FROM_STRING时会被忽略。
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),//请求的消息的语言标识符。
		(LPTSTR)&lpMsgBuf,//接收错误信息描述的缓冲区指针。
		0,//如果FORMAT_MESSAGE_ALLOCATE_BUFFER标志没有被指定，这个参数必须指定为输出缓冲区的大小，如果指定值为0，这个参数指定为分配给输出缓冲区的最小数。
		NULL//保存格式化信息中的插入值的一个数组
	);
	errorBuf->code		= code;
	errorBuf->discrip	= Wide2Ascii(lpMsgBuf);
	LocalFree(lpMsgBuf);
#else


#endif
}

void					clearError(CCSTDLib_FetchError* errorBuf)
{
	errorBuf->code = 0;
	CCSTD_SAFE_FREE(errorBuf->discrip);
	errorBuf->discrip = NULL;
}
void					setError(CCSTDLib_FetchError* error,
	CCSTDLib_GeneralErrorCode code, const char* disp)
{
	CCSTD_SAFE_FREE(error->discrip);
	error->code = code;
	CCSTD_SAFE_FREE(error->discrip);
	COPY_TO_HEAP(res, disp);
	error->discrip = res;
}

const char* getError(CCSTDLib_FetchError* error) {
	DEFAULT_DENY(error, NUL_PTR);
	return error->discrip;
}

void freeError(CCSTDLib_FetchError* errorBuf)
{
	CCSTD_SAFE_FREE(errorBuf->discrip);
	CCSTD_SAFE_FREE(errorBuf);
}
