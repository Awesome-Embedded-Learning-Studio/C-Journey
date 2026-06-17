#ifndef _CCSTDLIB_OBJECT_H_
#define _CCSTDLIB_OBJECT_H_

#include "CCSTDLIB_Utilitys.h"
#include "CCSTDLIB_CString.h"
// ObjectType
typedef struct {
	void*				coreData;
	CCSTDLIB_Size		szObj;
	CCSTDLIB_CString	objName;
}Object;

// Create Object
CCSTDLIB_EXPORT CCSTDLIB_BOOL CreateObject(Object** object, void** data, CCSTDLIB_Size size, CCSTDLIB_CString name);

// Copy Object
CCSTDLIB_EXPORT CCSTDLIB_BOOL CopyObject(Object** dst, Object* src);

// getRawObject
CCSTDLIB_EXPORT void* fetchRawDataFromObject(Object* object);


// Delete

#endif // !_CCSTDLIB_OBJECT_H_

