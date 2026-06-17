#include "CCSTDLIB_CString.h"
int main()
{
	CString* a = NULL; 
	InitCString(&a, "Hello, world");
	CString* rev_a = reverseCString(a, CCSTDLIB_Copy);
	printf("%s\n", rev_a->coreData);

	CString* b = NULL, *c = NULL;
	InitCString(&b, "11");
	InitCString(&c, "111");
	printf("%d\n", compareCString(c, b));

	CString* src = NULL, *sub = NULL;
	InitCString(&src, "HHHHelloHellofioshoifhasndkHellodaiou");
	InitCString(&sub, "Hello");
	printf("\n%d\n", findSubCString(src, sub, 0));
	printf("\n%d\n", findSubCString(src, sub, 4));
	//ChangeCString(&src, "What is the String?");
	//printf("%s\n", src->coreData);

	//RemoveTargetCString(&src, 'S');
	//printf("%s\n", src->coreData);

	const char** res = SplitCString(src, sub);

	while (*res != NULL)
	{
		printf("%s\n", *res);
		res++;
	}
}