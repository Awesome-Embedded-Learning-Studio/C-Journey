#pragma once
#include "CCSTDLib_Types.h"
typedef struct __CCPlainObject
{
	void*		object;
	CCSize_t	size;
	CCBOOL_t	isInHeap;
}CCPlainObject;

// Use if sizeof() able
#define MAKE_STACK_PlainObject_Stack_PlainType(obj_name, datas) \
	CCPlainObject obj_name; \
	do{\
		obj_name->object	= &datas;\
		obj_name->size		= sizeof(datas);\
		obj_name->isInHeap	= False;\
	}while(0)

// Use if user_def_size
CCPlainObject*	createPlainObject(void* data, CCSize_t dataSize, CCBOOL_t req_copy);
CCPlainObject*	clonePlainObject(CCPlainObject* other);
CCBOOL_t		changePlainObject(CCPlainObject* plain, void* newData, CCSize_t dataSize, CCBOOL_t req_copy);;

CCBOOL_t		freePlainObject(CCPlainObject* p);


typedef struct __CCObject {
	CCPlainObject*	coreObject;
	const char*		typeString;
}CCObject;

CCObject* CCObject_createObject(	void* data, CCSize_t dataSize,
									CCBOOL_t req_copy, const char* typeString);

CCObject* CCObject_cloneObject(CCObject* other);

CCBOOL_t  CCObject_freeObject(CCObject* object);
