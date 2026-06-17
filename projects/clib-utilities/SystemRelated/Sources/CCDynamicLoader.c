#include "CCDynamicLoader.h"
#include "CCSTDLib_Utils.h"
#include "CCSTDLib_Types.h"
#include "CCSTDLib_MultiByte_WCharAdapter.h"
CCBOOL_t CCDynamicLoader_checkIfValidLoader(CCDynamicLoader* loader)
{
	return loader && loader->dllModule != NUL_PTR 
		&& loader->error && loader->error->code == DynamicLoader_NOERROR;
}

#ifdef CCSTD_USE_WINDOWS
#include <Windows.h>
CCDynamicLoader* CCDynamicLoader_LoadDynamicPlugin(const char* path)
{
	CCWideChar* res = Ascii2Wide(path);
	CCSTD_MALLOC_ONE(loader, CCDynamicLoader);
	loader->error = initError();
	loader->dllModule = NULL;
	HMODULE	dlls = LoadLibraryEx(res, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!dlls) {
		freshError(loader->error);
		CCSTD_SAFE_FREE(res);
		return loader;
	}
	loader->dllModule = dlls;
	CCSTD_SAFE_FREE(res);
	return loader;
}

CCBOOL_t	CCDynamicLoader_ReLoadDynamicPlugin(CCDynamicLoader* plugin, const char* path)
{
	DEFAULT_DENY(plugin, NUL_PTR);
	CCWideChar* res = Ascii2Wide(path);
	if(plugin->dllModule)
		FreeLibrary(plugin->dllModule);
	HMODULE	dlls = LoadLibraryEx(res, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!dlls) {
		freshError(plugin->error);
		CCSTD_SAFE_FREE(res);
		return False;
	}
	plugin->dllModule = dlls;
	clearError(plugin->error);
	CCSTD_SAFE_FREE(res);
	return True;
}

CCBOOL_t			CCDynamicLoader_checkIfValidFunction(CCDynamicTarget* target)
{
	return target != NUL_PTR;
}

CCDynamicTarget* CCDynamicLoader_FetchFunctions(CCDynamicLoader* who, const char* symbol)
{
	DEFAULT_DENY(who, NULL);
	DEFAULT_DENY(who->dllModule, NULL);

	CCDynamicTarget* res = GetProcAddress(who->dllModule, symbol);
	if (!res) {
		freshError(who->error);
		return NUL_PTR;
	}

	return res;
}
CCBOOL_t		CCDynamicLoader_FreeDynamicPlugin(CCDynamicLoader* loader)
{
	DEFAULT_DENY(loader, NULL);
	FreeLibrary(loader->dllModule);
	freeError(loader->error);
	CCSTD_SAFE_FREE(loader);
	return True;
}


#else
CCDynamicLoader* CCDynamicLoader_LoadDynamicPlugin(const char* path);
CCBOOL_t			CCDynamicLoader_ReLoadDynamicPlugin(CCDynamicLoader* plugin, const char* path);
CCBOOL_t			CCDynamicLoader_checkIfValidLoader(CCDynamicLoader* loader);
CCBOOL_t			CCDynamicLoader_checkIfValidFunction(CCDynamicTarget* target);
CCDynamicTarget* CCDynamicLoader_FetchFunctions(CCDynamicLoader* who, const char* symbol);
CCBOOL_t			CCDynamicLoader_FreeDynamicPlugin(CCDynamicLoader* loader);

#endif
