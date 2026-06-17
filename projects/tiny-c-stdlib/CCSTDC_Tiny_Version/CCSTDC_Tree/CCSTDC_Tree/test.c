#include"CCSTDC_BinaryTree.h"
#include<stdlib.h>
#include<time.h>
void PrintInt(int* a) {
	printf("%d ", *a);
}

int intComp(int* e1, int* e2) {
	CASE_BIIGER(*e1 > *e2);
	CASE_SMALLER(*e1 < *e2);
	CASE_EQUAL(*e1 == *e2);
}

int* getRandArray(int size) {
	srand((unsigned int)time(NULL));
	if (size < 0) {
		return NULL;
	}

	int* Array = (int*)malloc(sizeof(int) * size);
	if (Array == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	for (int i = 0; i < size; i++) {
		Array[i] = rand() % 100;
	}

	return Array;
}


int main()
{
	unsigned int size = 10UL;
	CCSTDC_BinarySearchTree* getTree = CCSTDC_BinarySearchTree_initEmptyBinarySearchTree(intComp);
	int* arrayWhat = getRandArray(size);
	
	CCSTDC_BinarySearchTreeNode** getArray = CCSTDC_BinarySearchTree_wrapDataListToListNode(arrayWhat, sizeof(int), size);
	
	// CCSTDC_BinarySearchTree_ALV_InsertArray(getTree, getArray, getTree->toRoot, size);

	CCSTDC_BinarySearchTree_InsertArray(getTree, getArray, getTree->toRoot, size);

	CCSTDC_BinarySearchTree_DirectPushArray(getTree, arrayWhat, 10);

	CCSTDC_BinarySearchTree_PrintDataInList(getTree, CCSTDC_PreOrder, PrintInt);
	PRINT_SLASH;

	CCSTDC_BinarySearchTree_DirectPush(getTree, &size);

	CCSTDC_BinarySearchTree_DrawBinTree(getTree->toRoot, PrintInt,0);

	// free(arrayWhat);

	CCSTDC_BinarySearchTreeNode* findNode = CCSTDC_BinarySearchTree_findTargetNode(getTree, getTree->toRoot, &size);

	if (findNode != NULL)
	{
		printf("Find The Target Node!\n");
		(*(int*)findNode->curElem)++;
		printf("The Node height is: %d\n", CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(findNode));
	}



	printf("Current tree Height: %d\n", CCSTDC_BinarySearchTree_getCurrentTreeHeight(getTree));
	

	CCSTDC_BinarySearchTree_DrawBinTree(getTree->toRoot, PrintInt, 0);
	CCSTDC_BinarySearchTree* copyTree = CCSTDC_BinarySearchTree_CopyBinarySearchTree(getTree);

	CCSTDC_BinarySearchTree_PrintDataInList(copyTree, CCSTDC_PreOrder, PrintInt);
	PRINT_SLASH;
	CCSTDC_BinarySearchTree_DrawBinTree(copyTree->toRoot, PrintInt, 0);
	CCSTDC_BinarySearchTree_EraseWholeTree(getTree);
	

}