#pragma once
#include "CCSTDLib_Types.h"
#include "CCSTDLib_HandleExports.h"
#include "CCSTDLib_FetchError.h"
typedef void* CCDynamicPlugin;
typedef void* CCDynamicTarget;

typedef enum __CCDynamicLoaderError {
	DynamicLoader_NOERROR,
	DynamicLoader_NOPathFound,
	DynamicLoader_NoSymbolFound,
}CCDynamicLoaderError;


typedef struct __CCDynamicLoader
{
	CCDynamicPlugin			dllModule;
	CCSTDLib_FetchError*	error;
}CCDynamicLoader;

CCDynamicLoader*	CCDynamicLoader_LoadDynamicPlugin(const char* path);
CCBOOL_t			CCDynamicLoader_ReLoadDynamicPlugin(CCDynamicLoader* plugin, const char* path);
CCBOOL_t			CCDynamicLoader_checkIfValidLoader(CCDynamicLoader* loader);
CCBOOL_t			CCDynamicLoader_checkIfValidFunction(CCDynamicTarget* target);
CCDynamicTarget*	CCDynamicLoader_FetchFunctions(CCDynamicLoader* who, const char* symbol);
#define				CCDynamicLoader_FetchConcreteFunc(type, who, symbol) \
					(type) CCDynamicLoader_FetchFunctions(who, symbol)
CCBOOL_t			CCDynamicLoader_FreeDynamicPlugin(CCDynamicLoader* loader);
