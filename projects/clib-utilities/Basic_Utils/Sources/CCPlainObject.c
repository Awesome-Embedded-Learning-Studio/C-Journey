#include "CCPlainObject.h"
#include "CCSTDLib_Utils.h"
CCPlainObject* createPlainObject(void* data, CCSize_t dataSize, CCBOOL_t req_copy)
{
	CCSTD_MALLOC_ONE(new_one, CCPlainObject);
	if (req_copy)
	{
		CCSTD_MALLOC_BYTES(_data, dataSize);
		memcpy(_data, data, dataSize);
		new_one->object = _data;
	}
	else
		new_one->object = data;
	new_one->size = dataSize;
	new_one->isInHeap = req_copy;
	return new_one;
}

CCPlainObject* clonePlainObject(CCPlainObject* other)
{
	DEFAULT_DENY(other, NUL_PTR);
	CCSTD_MALLOC_ONE(new_one, CCPlainObject);
	CCSTD_MALLOC_BYTES(_data, other->size);
	memcpy(_data, other->object, other->size);
	new_one->size = other->size;
	new_one->object = _data;
	new_one->isInHeap = True;
	return new_one;
}

CCBOOL_t changePlainObject(CCPlainObject* obj, void* newData, CCSize_t dataSize, CCBOOL_t req_copy)
{
	DEFAULT_DENY(obj, False);
	if (req_copy) {
		if (obj->isInHeap)
			CCSTD_SAFE_FREE(obj->object);
		CCSTD_MALLOC_BYTES(new_obj, dataSize);
		memcpy(new_obj, newData, dataSize);
		obj->object = new_obj;
	}
	else
		obj->object = newData;
	obj->size = dataSize;
	obj->isInHeap = req_copy;
	return True;
}

CCBOOL_t freePlainObject(CCPlainObject* p)
{
	DEFAULT_DENY(p, False);
	if(p->isInHeap)
		CCSTD_SAFE_FREE(p->object);
	CCSTD_SAFE_FREE(p);
	return True;
}


CCObject* CCObject_createObject(void* data, CCSize_t dataSize,
	CCBOOL_t req_copy, const char* typeString)
{
	CCSTD_MALLOC_ONE(new_one, CCObject);
	new_one->coreObject = createPlainObject(data, dataSize, req_copy);
	if (!new_one->coreObject) {
		CCSTD_SAFE_FREE(new_one);
		return False;
	}

	int str_len = 1;
	COPY_TO_HEAP(res, typeString);
	new_one->typeString = res;
	return new_one;
}

CCObject* CCObject_cloneObject(CCObject* other)
{
	DEFAULT_DENY(other, NUL_PTR);
	CCSTD_MALLOC_ONE(new_one, CCObject);
	new_one->coreObject = clonePlainObject(other->coreObject);
	if (!new_one->coreObject)
	{
		CCSTD_SAFE_FREE(other);
		return False;
	}

	COPY_TO_HEAP(res, other->typeString);
	new_one->typeString = res;

	return new_one;
}

CCBOOL_t  CCObject_freeObject(CCObject* object)
{
	CCSTD_SAFE_FREE(object->coreObject);
	CCSTD_SAFE_FREE(object->typeString);
	CCSTD_SAFE_FREE(object);

	return True;
}
