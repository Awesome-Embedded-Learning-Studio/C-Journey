#include "CCSTDLib_FetchError.h"
#include "CCSTDLibs_MyCompiles.h"
#include "CCSTDLib_MultiByte_WCharAdapter.h"
#include "CCSTDLib_Utils.h"
#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>
#endif

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
	//��־λ���������˵��lpSource������dwFlags�ĵ�λָ����δ������й����������������Ҳ���������ȵĸ�ʽ�������,��ѡ������
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,//����dwFlags��־������
		code,//�������Ϣ�ı�ʶ������dwFlags��־ΪFORMAT_MESSAGE_FROM_STRINGʱ�ᱻ���ԡ�
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),//�������Ϣ�����Ա�ʶ����
		(LPTSTR)&lpMsgBuf,//���մ�����Ϣ�����Ļ�����ָ�롣
		0,//���FORMAT_MESSAGE_ALLOCATE_BUFFER��־û�б�ָ���������������ָ��Ϊ����������Ĵ�С�����ָ��ֵΪ0���������ָ��Ϊ������������������С����
		NULL//�����ʽ����Ϣ�еĲ���ֵ��һ������
	);
	errorBuf->code		= code;
	errorBuf->discrip	= Wide2Ascii(lpMsgBuf);
	LocalFree(lpMsgBuf);
#else
	/* POSIX:用 errno + strerror 复刻 Windows 端“抓错误码并描述入堆”的语义。 */
#include <errno.h>
#include <string.h>
	CCSTDLib_GeneralErrorCode code = errno;
	CCSTD_SAFE_FREE(errorBuf->discrip);
	const char* msg = strerror(code);
	COPY_TO_HEAP(heapMsg, msg);
	errorBuf->code		= code;
	errorBuf->discrip	= heapMsg;
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
