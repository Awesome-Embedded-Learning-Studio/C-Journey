#include"CCSTDC_DRLinkList.h"
#define SLASH printf("\n")
void intergerPrint(int* num) {
	printf("%d ", *num);
}

CCSTDC_Bool intergerComp(int* a1, int* a2) {
	return *a1 == *a2;
}
int main()
{
	CCSTDC_DRLinkList* getList = CCSTDC_DRLinkList_InitEmpty();
	int t = 10;
	for (int i = 0; i < 10; i++) {
		CCSTDC_DRLinkList_Insert(getList, &t, sizeof(int), 0);
	}
	CCSTDC_DRLinkList_PrintList(getList, intergerPrint, PositiveOrder);

	CCSTDC_DRLinkList_PopFront(getList);
	SLASH;
	CCSTDC_DRLinkList_PrintList(getList, intergerPrint, PositiveOrder);
	int k = 2;

	CCSTDC_DRLinkList* pos = CCSTDC_DRLinkList_FindTarget(getList, &t, intergerComp, PositiveOrder);
	SLASH;
	CCSTDC_DRLinkList_PrintList(pos, intergerPrint, PositiveOrder);
	SLASH;
	int* geter = CCSTDC_DRLinkList_GetElemByPos(pos, -19, CCSTDC_Copy);
	intergerPrint(geter);

	CCSTDC_DRLinkList_DestoryAll(getList);
	CCSTDC_DRLinkList_DestoryAll(pos);
}