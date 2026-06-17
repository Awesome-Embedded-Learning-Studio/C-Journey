#pragma once
typedef long	CCSTDLib_GeneralErrorCode;
typedef struct __CCSTDLib_FetchError
{
	CCSTDLib_GeneralErrorCode	code;
	const char*					discrip;
}CCSTDLib_FetchError;

CCSTDLib_FetchError*	initError();
void					freshError(CCSTDLib_FetchError* errorBuf);
void					clearError(CCSTDLib_FetchError* errorBuf);
void					setError(CCSTDLib_FetchError* error, 
								CCSTDLib_GeneralErrorCode code, const char* disp);
void					freeError(CCSTDLib_FetchError* errorBuf);
const char*				getError(CCSTDLib_FetchError* error);

