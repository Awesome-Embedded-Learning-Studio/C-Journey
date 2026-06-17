#include <stdio.h>
#include "CCDynamicLoader.h"
static const char* DynamicLoadingPath =
"D:\\My coding repositary\\finAPI\\UtilitiesLib\\UtilitiesLib\\test\\testSrc\\DynamicPlugin\\DLLDemo.dll";
static const char* DynamicLoadingInvalidPath = "C:/AnyThingInvalid";
static const char* loadingName = "add";
typedef int(*Func_Type)(int, int);


int testDynamicPlugin()
{
	// Try Invalid loading
	CCDynamicLoader* plugin = CCDynamicLoader_LoadDynamicPlugin(DynamicLoadingInvalidPath);
	if (!CCDynamicLoader_checkIfValidLoader(plugin))
	{
		fprintf(stderr, "\nAs is expected! the plugin isn't exsited with disp:> \n");
	}
	CCDynamicLoader_ReLoadDynamicPlugin(plugin, DynamicLoadingPath);
	if (!CCDynamicLoader_checkIfValidLoader(plugin))
	{
		fprintf(stderr, "the plugin isn't exsited with disp:> \n");
		CCDynamicLoader_FreeDynamicPlugin(plugin);
		return 1;
	}

	Func_Type func = CCDynamicLoader_FetchConcreteFunc(Func_Type, plugin, loadingName);
	if (!CCDynamicLoader_checkIfValidFunction(func))
	{
		fprintf(stderr, "Functions are un-find!\n");
		CCDynamicLoader_FreeDynamicPlugin(plugin);
		return 1;
	}
	printf("\nLoad Functions success!\n");
	int testNum = 1, testNum2 = 2;
	printf("\n%d + %d = %d\n", testNum, testNum2, func(testNum2, testNum));
	CCDynamicLoader_FreeDynamicPlugin(plugin);
	return 0;
}
