#include "CCSTDLIB_Object.h"

CCSTDLIB_BOOL CreateObject(Object** object, void** data, CCSTDLIB_Size size, CCSTDLIB_CString name)
{
	if (object == NULL) {
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	if ((*object) != NULL) {
		MSG("Object is already inited! The Original Data will be lost!");
	}

	CCSTDLIB_SafeMalloc(Object, *object);

	(*object)->coreData = *data;
	(*object)->szObj	= size;
	(*object)->objName	= name;
	return CCSTDLIB_TRUE;
}

CCSTDLIB_BOOL CopyObject(Object** dst, Object* src)
{
	if (dst == NULL) {
		THROW_CCSTDLIB_NULLPTR_EXCEPTION;
	}

	if ((*dst) != NULL) {
		MSG("Object is already inited! The Original Data will be lost!");
	}

	CCSTDLIB_SafeMalloc(Object, *dst);

	CCSTDLIB_SafeNMalloc(char, (*dst)->coreData, (*dst)->szObj);
		
	(*dst)->coreData = src->coreData;
	(*dst)->szObj = src->szObj;
	(*dst)->objName = src->objName;
	return CCSTDLIB_TRUE;
}

void* fetchRawDataFromObject(Object* object)
{
	return object->coreData;
}