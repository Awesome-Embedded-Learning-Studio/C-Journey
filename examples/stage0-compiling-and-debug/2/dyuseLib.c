#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef int(*PFUNC)(int, int);
typedef void(*PPRINT)(const char*);

int main()
{
	void* handle;

	handle = dlopen("./libsrc.so", RTLD_LAZY);
	if(handle == NULL){
		fprintf(stderr, "Load failed:> %s\n", dlerror());
		return -1;
	}
	
	PFUNC myAdd = (PFUNC)dlsym(handle, "add");
	if(myAdd == NULL)
	{
		fprintf(stderr, "add Load failed:> %s\n", dlerror());
     		dlclose(handle);
		return -1;
	}
	
	PPRINT printInfo = (PPRINT)dlsym(handle, "printMsg");
	if(printInfo == NULL)
	{
		fprintf(stderr, "printMsg Load failed:> %s\n", dlerror());
     		dlclose(handle);
		return -1;
	}
	
	printInfo("Load symbols all success");
	printf("%d + %d = %d\n", 1, 2, myAdd(1, 2));
	dlclose(handle);
	return 0;

}
