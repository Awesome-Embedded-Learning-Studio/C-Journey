#include"CCSTDLIB_CString.h"

CCSTDLIB_BOOL InitEmptyCString(CCSTDLIB_CString** pStr)
{
	if ((*pStr) != NULL)
	{
		MSG("Warning for the not empty String, this will possibly cause Leaky Memory");
	}

	CCSTDLIB_SafeMalloc(CCSTDLIB_CString, *pStr);

	(*pStr)->coreData = NULL;
	(*pStr)->strCapicity = 0;
	(*pStr)->strSize = 0;

	return CCSTDLIB_TRUE;
}

CCSTDLIB_BOOL InitCStringLimited(CCSTDLIB_CString** pStr, const char* string, const CCSTDLIB_Size nch)
{
	if ((*pStr) != NULL)
	{
		MSG("Warning for the not empty String, this will possibly cause Leaky Memory");
	}

	if (nch > strlen(string))
	{
#ifdef CCSTDLIB_CString_Enable_OverFlowy_Init
		MSG("Detected for the OverFlowed Init, Process Autoset the whole String");
		nch = strlen(string);
#else
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;
#endif
	}

	CCSTDLIB_SafeMalloc(CCSTDLIB_CString, *pStr);

	CCSTDLIB_SafeNMalloc(char, (*pStr)->coreData, nch + 1); // nch + 1 for Null Terminate Char

	memcpy((*pStr)->coreData, string, nch + 1);

	(*pStr)->strCapicity = nch + 1;

	(*pStr)->strSize = nch;

	return CCSTDLIB_TRUE;
}

CCSTDLIB_BOOL CopyCString(CCSTDLIB_CString** dst, CCSTDLIB_CString* src)
{
	if ((*dst) != NULL)
	{
		MSG("Warning for the not empty String, this will possibly cause Leaky Memory");
	}

	CCSTDLIB_SafeMalloc(CCSTDLIB_CString, *dst);

	CCSTDLIB_SafeNMalloc(char, (*dst)->coreData, src->strCapicity);

	memcpy((*dst)->coreData, src->coreData, SZCHAR * src->strSize);

	(*dst)->strCapicity = src->strCapicity;

	(*dst)->strSize = src->strSize;

	return CCSTDLIB_TRUE;
}

const char* getCStringData(CCSTDLIB_CString* src)
{
	char* data = NULL;
	CCSTDLIB_SafeNMalloc(char, data, src->strSize);

	return (const char*)memcpy(data, src->coreData, SZCHAR * src->strSize);
}

CCSTDLIB_CString* catCString(CCSTDLIB_CString* appendee, CCSTDLIB_CString* appender)
{
	if (!appendee || !appender) {
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	CCSTDLIB_Raw_REALLOC(char, appendee->coreData, appendee->strSize + appender->strSize - 1);

	memcpy(appendee->coreData + appendee->strSize - 1, appender->coreData, appender->strSize);

	appendee->strCapicity += appender->strCapicity - 1;
	appendee->strSize += appender->strSize - 1;

	return appendee;
}

CCSTDLIB_EXPORT CCSTDLIB_CString* insertCString(
	CCSTDLIB_CString*	appendee, 
	CCSTDLIB_CString*	appender, 
	CCSTDLIB_Index		index)
{
	if (!appendee || !appender) {
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	if ((unsigned int)index > appendee->strSize)
	{
#ifdef CCSTDLIB_CString_Enable_OverFlowy_Init
		MSG("Detected for the OverFlowed Init, Process Autoset the whole String");
		index = strlen(string);
#else
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;
#endif
	}

	CCSTDLIB_Raw_REALLOC(char, appendee->coreData, appendee->strSize + appender->strSize - 1);
	
	memmove(appendee->coreData +index + appender->strSize - 1, appendee->coreData + index, appendee->strSize - index);

	memmove(appendee->coreData + index, appender->coreData, appender->strSize - 1);

	appendee->strCapicity += appender->strCapicity - 1;
	appendee->strSize += appender->strSize - 1;

	return appendee;
}

CCSTDLIB_CString* reverseCString(CCSTDLIB_CString* src, enum CopyMovePolicy policy)
{
	switch (policy)
	{
	case CCSTDLIB_Copy:
	{
		char* tmp = NULL;
		CCSTDLIB_CString* res = NULL;
		CCSTDLIB_SafeNMalloc(char, tmp, src->strCapicity);
		strcpy(tmp ,src->coreData);
		
		for (unsigned int i = 0; i < (src->strSize - 1) / 2; i++) {
			char tmpchar = tmp[i];
			tmp[i] = tmp[src->strSize - i - 1];
			tmp[src->strSize - i - 1] = tmpchar;
		}

		CCSTDLIB_SafeMalloc(CCSTDLIB_CString, res);

		InitCString(&res, tmp);

		CCSTDLIB_SafeFree(tmp);

		return res;
	}
		break;
	case CCSTDLIB_Move:

		for (unsigned int i = 0; i < (src->strSize - 1) / 2; i++) {
			char tmpchar = src->coreData[i];
			src->coreData[i] = src->coreData[src->strSize - i - 2];
			src->coreData[src->strSize - i - 2] = tmpchar;
		}

		return src;
		break;
	default:
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;
		break;
	}

}

CCSTDLIB_Compare compareCString(CCSTDLIB_CString* str1, CCSTDLIB_CString* str2)
{
	if (str1 == NULL || str2 == NULL)
	{
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	const char* p1 = str1->coreData;
	const char* p2 = str2->coreData;
	while (
		*p1 != 0 && *p2 != 0 &&
		*p1++ == *p2++
		);

	return (CCSTDLIB_Compare)((*p1 - *p2 > 0) - (*p2 - *p1 > 0));
}

// for next vals , do not use it directly in the source

// we make initNextArr as we use KMP to the subs itself
void initNextArr(CCSTDLIB_CString* subs, int** nextArr)
{
	int prefixPos = -1;
	unsigned int suffixPos = 0;

	if (*nextArr != NULL)
	{
		MSG("Warning for the overlapped the next arr");
		nextArr = NULL;
	}

	CCSTDLIB_SafeNMalloc(int, *nextArr, subs->strSize);
	(*nextArr)[0] = -1;

	while (suffixPos < subs->strSize)
	{
		if (prefixPos == -1 || subs->coreData[suffixPos] == subs->coreData[prefixPos])
		{
			prefixPos++;
			suffixPos++;
			if (subs->coreData[suffixPos] != subs->coreData[prefixPos])
			{
				(*nextArr)[suffixPos] = prefixPos;
			}
		}
		else
		{
			prefixPos = (*nextArr)[prefixPos];
		}
	}
}

// Always return first place
CCSTDLIB_Index findSubCString(const CCSTDLIB_CString* source, CCSTDLIB_CString* sub, CCSTDLIB_Index from_index)
{
	if (source == NULL || sub == NULL)
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;

	if (from_index < 0 )
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;

	if ((unsigned int)from_index >= source->strSize)
	{
		return -1;
	}

	// create nextArr
	int* next = NULL;
	initNextArr(sub, &next);

	int subIndex = 0;
	int srcIndex = from_index;

	while (subIndex < (int)sub->strSize && srcIndex < (int)source->strSize)
	{
		if (subIndex == -1 || source->coreData[srcIndex] == sub->coreData[subIndex])
		{
			// dis-comment this if debuging
			// printf("%d:\t %c <-> %c\n",srcIndex, source->coreData[srcIndex], sub->coreData[subIndex]);
			subIndex++;
			srcIndex++;
		}
		else
		{
			subIndex = next[subIndex];
		}

	}

	if (subIndex == sub->strSize)
	{
		return srcIndex - subIndex;
	}
	else
	{
		return -1;
	}
}


CCSTDLIB_BOOL getCharCString(const CCSTDLIB_CString* source, const CCSTDLIB_Index index)
{
	if ((unsigned int)index >= source->strSize || index < 0)
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;

	return source->coreData[index];
}

CCSTDLIB_BOOL EraseCString(CCSTDLIB_CString* source)
{
	if (source == NULL)
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;

	CCSTDLIB_SafeFree(source->coreData);

	CCSTDLIB_SafeFree(source);

	return CCSTDLIB_TRUE;
}

// From Beg
CCSTDLIB_BOOL changeCStringLimited(CCSTDLIB_CString** source, const char* newSource, const CCSTDLIB_Size nch)
{
	if (newSource == NULL || *source == NULL)
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;

	if(nch > strlen(newSource) || nch < 0)
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;

	if (strlen(newSource) > (*source)->strSize)
	{
		EraseCString(*source);
		InitCString(source, newSource);
	}
	else
	{
		memcpy((*source)->coreData, newSource, nch); 
	}
	return CCSTDLIB_TRUE;
}

CCSTDLIB_BOOL RemoveTargetCString(CCSTDLIB_CString** source, const char ch)
{
	if (*source == NULL || (*source)->coreData == NULL)
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;

	if (ch == '\0') // hey, do not remove \0!
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;

	char* newCoreStr = NULL;

	CCSTDLIB_SafeNMalloc(char, newCoreStr, (*source)->strSize);

	for (unsigned int i = 0, j = 0; i < (*source)->strSize; i++)
	{
		if ((*source)->coreData[i] == ch)
			continue;

		newCoreStr[j++] = (*source)->coreData[i];
	}

	EraseCString(*source);

	InitCString(source, newCoreStr);

	return CCSTDLIB_TRUE;
}

const char* GetSubString(const CCSTDLIB_CString* source, CCSTDLIB_Index beg, CCSTDLIB_Index end)
{
	if (source == NULL)
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	
	if (beg >= end || beg < 0 || end < 0 || (unsigned int)beg > source->strSize || (unsigned int)end > source->strSize)
		THROW_CCSTDLIB_UNFIT_PARAM_EXCEPTION;

	char* res = NULL;
	CCSTDLIB_SafeNMalloc(char, res, end - beg + 2);
	for (int i = beg, j = 0; i <= end; i++)
	{
		res[j++] = source->coreData[i];
		if (i == end)
		{
			res[j] = '\0';
		}
	}

	return res;
}


const char** SplitCString(const CCSTDLIB_CString* source, CCSTDLIB_CString* spliter)
{

	if (source == NULL || source->coreData == NULL || spliter == NULL)
	{
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	if (source->coreData == "" || spliter->coreData == "")
	{
		return NULL;
	}
	CCSTDLIB_Index* pIndexArr = NULL;
	const char** pStr = NULL;
	int curNum = 1;
	unsigned int curCheckIndex = 0;
	// find All Begin
	while (curCheckIndex <= source->strSize)
	{	
		curCheckIndex = findSubCString(source, spliter, curCheckIndex);
		if (curCheckIndex == -1)
		{
			break;
		}
		CCSTDLIB_Raw_REALLOC(CCSTDLIB_Index, pIndexArr, curNum);
		pIndexArr[curNum - 1] = curCheckIndex;
		curNum++;
		curCheckIndex++;
	}
	CCSTDLIB_Raw_REALLOC(CCSTDLIB_Index, pIndexArr, curNum);
	// get All target Length
	pIndexArr[curNum - 1] = source->strSize;
	CCSTDLIB_SafeNMalloc(const char*, pStr, curNum);
	for (int i = 0; i < curNum - 1; i++)
	{
		printf("%d %d\n", pIndexArr[i], pIndexArr[i + 1]);
		pStr[i] = GetSubString(source, pIndexArr[i], pIndexArr[i + 1] - 1);
	}

	pStr[curNum - 1] = NULL;

	CCSTDLIB_SafeFree(pIndexArr);
	return pStr;
}