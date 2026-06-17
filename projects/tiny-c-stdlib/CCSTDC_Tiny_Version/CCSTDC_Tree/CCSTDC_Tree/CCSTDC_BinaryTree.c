#include "CCSTDC_BinaryTree.h"


CCSTDC_BinarySearchTree* CCSTDC_BinarySearchTree_initEmptyBinarySearchTree(Comparator compFunc)
{
	if (compFunc == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	CCSTDC_BinarySearchTree* curSpace = (CCSTDC_BinarySearchTree*)malloc(sizeof(CCSTDC_BinarySearchTree));
	if (curSpace == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	curSpace->curCountElem = 0;
	curSpace->toRoot = NULL;
	curSpace->compFunc = compFunc;
}

/* WARNING : the functions are not suppose to change the eff! make a promise for the const*/
CCSTDC_Bool CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(
	CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode* curNode,
	CCSTDC_Order_Code code,
	FUNCPTR function,
	void* const communications
)
{
	if (insertTree == NULL || function == NULL) 
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (curNode == NULL)
	{
		return;
	}
	switch (code)
	{
	case CCSTDC_PreOrder:

		function(curNode->curElem, communications);
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree, curNode->leftChild, code, function, communications);
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree,curNode->rightChild,code, function,communications);
		break;
	case CCSTDC_InOrder:
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree, curNode->leftChild, code, function, communications);
		function(curNode->curElem, communications);
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree, curNode->rightChild, code, function, communications);
		break;
	case CCSTDC_PostOrder:
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree, curNode->leftChild, code, function, communications);
		CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(insertTree, curNode->rightChild, code, function, communications);
		function(curNode->curElem, communications);
		break;
	default:
		break;
	}
}

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_CopyBinarySearchSubTree(
	CCSTDC_BinarySearchTreeNode* startFromWhere
)
{
	if (startFromWhere == NULL)
	{	
		return NULL;
	}

	CCSTDC_BinarySearchTreeNode* left = CCSTDC_BinarySearchTree_CopyBinarySearchSubTree(startFromWhere->leftChild);
	CCSTDC_BinarySearchTreeNode* right = CCSTDC_BinarySearchTree_CopyBinarySearchSubTree(startFromWhere->rightChild);
	CCSTDC_BinarySearchTreeNode* newOne = CCSTDC_BinarySearchTree_SimplyCopyNode(startFromWhere);
	newOne->leftChild = left;
	newOne->rightChild = right;

	return newOne;
}

CCSTDC_BinarySearchTree* CCSTDC_BinarySearchTree_CopyBinarySearchTree(CCSTDC_BinarySearchTree* aTree)
{
	if (aTree == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_BinarySearchTree* curSpace = (CCSTDC_BinarySearchTree*)malloc(sizeof(CCSTDC_BinarySearchTree));
	if (curSpace == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	curSpace->curCountElem = aTree->curCountElem;
	curSpace->toRoot = CCSTDC_BinarySearchTree_CopyBinarySearchSubTree(aTree->toRoot);
	curSpace->compFunc = aTree->compFunc;

	return curSpace;
}

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_SimplyCopyNode(
	CCSTDC_BinarySearchTreeNode* copied
)
{
	if (copied == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	CCSTDC_BinarySearchTreeNode* node = (CCSTDC_BinarySearchTreeNode*)malloc(sizeof(CCSTDC_BinarySearchTreeNode));
	if (node == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	void* data = malloc(copied->dataSize);
	if (data == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	memcpy(data, copied->curElem, copied->dataSize);

	node->curElem = data;
	node->dataSize = copied->dataSize;
	node->leftChild = NULL;
	node->rightChild = NULL;
	node->Appeartimes = 0;
	return node;

}


CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_wrapDataToNode(void* elem, size_t size)
{
	if (elem == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	
	CCSTDC_BinarySearchTreeNode* node = (CCSTDC_BinarySearchTreeNode*)malloc(sizeof(CCSTDC_BinarySearchTreeNode));
	if (node == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	void* data = malloc(size);
	if (data == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	memcpy(data, elem, size);

	node->curElem = data;
	node->dataSize = size;
	node->leftChild = NULL;
	node->rightChild = NULL;
	node->Appeartimes = 1;
	return node;
}

CCSTDC_BinarySearchTreeNode** CCSTDC_BinarySearchTree_wrapDataListToListNode(
	void* array,
	size_t dataSize,
	int arraySize
)
{
	if (array == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (arraySize < 0) {
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	CCSTDC_BinarySearchTreeNode** arrayNode = 
		(CCSTDC_BinarySearchTreeNode**)malloc(sizeof(CCSTDC_BinarySearchTreeNode*) * arraySize);

	if (arrayNode == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	for (int i = 0; i < arraySize; i++) {
		CCSTDC_BinarySearchTreeNode* node = (CCSTDC_BinarySearchTreeNode*)malloc(sizeof(CCSTDC_BinarySearchTreeNode));
		if (node == NULL) {
			CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
		}

		void* data = malloc(dataSize);
		if (data == NULL)
		{
			CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
		}

		memcpy(data, (char*)array + i * dataSize, dataSize);

		node->curElem = data;
		node->dataSize = dataSize;
		node->leftChild = NULL;
		node->rightChild = NULL;
		node->Appeartimes = 1;

		arrayNode[i] = node;
	}

	return arrayNode;
}

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_Insert(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode* node, CCSTDC_BinarySearchTreeNode* curNode)
{
	if (insertTree == NULL || node == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	if (curNode == NULL)
	{
		curNode = node;
		insertTree->curCountElem++;
	}
	else
	{
		if (insertTree->compFunc(node->curElem, curNode->curElem) == CCSTDC_Bigger)
		{
			if(curNode->rightChild != NULL)
			{
				CCSTDC_BinarySearchTree_Insert(insertTree, node, curNode->rightChild);
			}
			else
			{
				curNode->rightChild = node;
				insertTree->curCountElem++;
			}
			
		}
		else if (insertTree->compFunc(node->curElem, curNode->curElem) == CCSTDC_Smaller)
		{
			if(curNode->leftChild != NULL)
			{
				CCSTDC_BinarySearchTree_Insert(insertTree, node, curNode->leftChild);
			}
			else
			{
				curNode->leftChild = node;
				insertTree->curCountElem++;
			}
			
		}
		else {
			curNode -> Appeartimes++;		
		}
	}

	return curNode;
}

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_InsertAsAVLTree(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode* node, CCSTDC_BinarySearchTreeNode* curNode)
{
	if (insertTree == NULL || node == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (curNode == NULL)
	{
		curNode = node;
		insertTree->curCountElem++;
	}
	else if (insertTree->compFunc(node->curElem, curNode->curElem) == CCSTDC_Bigger) {
		curNode->rightChild = CCSTDC_BinarySearchTree_InsertAsAVLTree(insertTree, node, curNode->rightChild);
		if (CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->rightChild) -
			CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->leftChild) == 2) {
			if (insertTree->compFunc(node->curElem, curNode->rightChild->curElem) == CCSTDC_Bigger)
			{
				curNode = CCSTDC_BinarySearchTree_ALV_SingleRotateWithRight(curNode);
			}
			else
			{
				curNode = CCSTDC_BinarySearchTree_ALV_DoubleRotateWithRight(curNode);
			}
		}
	}

	else if (insertTree->compFunc(node->curElem, curNode->curElem) == CCSTDC_Smaller)
	{
		curNode->leftChild = CCSTDC_BinarySearchTree_InsertAsAVLTree(insertTree, node, curNode->leftChild);
		if (CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->leftChild) -
			CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->rightChild) == 2)
		{
			if (insertTree->compFunc(node->curElem, curNode->leftChild->curElem) == CCSTDC_Smaller)
			{
				curNode = CCSTDC_BinarySearchTree_ALV_SingleRotateWithLeft(curNode);
			}
			else
			{
				curNode = CCSTDC_BinarySearchTree_ALV_DoubleRotateWithLeft(curNode);
			}
		}
	}

	return curNode;
}

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_SingleRotateWithLeft(
	CCSTDC_BinarySearchTreeNode* node
)
{
	CCSTDC_BinarySearchTreeNode* tmp = node->leftChild;
	node->leftChild = tmp->rightChild;
	tmp->rightChild = node;

	return tmp;
}

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_SingleRotateWithRight(
	CCSTDC_BinarySearchTreeNode* node
)
{
	CCSTDC_BinarySearchTreeNode* tmp = node->rightChild;
	node->rightChild = tmp->leftChild;
	tmp->leftChild = node;

	return tmp;
}

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_DoubleRotateWithLeft(
	CCSTDC_BinarySearchTreeNode* node
)
{
	node->leftChild = CCSTDC_BinarySearchTree_ALV_SingleRotateWithRight(node->leftChild);

	return CCSTDC_BinarySearchTree_ALV_SingleRotateWithLeft(node);
}

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_DoubleRotateWithRight(
	CCSTDC_BinarySearchTreeNode* node
)
{
	node->rightChild = CCSTDC_BinarySearchTree_ALV_SingleRotateWithLeft(node->rightChild);

	return CCSTDC_BinarySearchTree_ALV_SingleRotateWithRight(node);
}

CCSTDC_Bool CCSTDC_BinarySearchTree_ALV_InsertArray(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode** nodeArray, CCSTDC_BinarySearchTreeNode* curNode, int arraySize)
{
	int isEmptyFlag = 0;
	if (insertTree == NULL || nodeArray == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (arraySize < 0)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	CCSTDC_BinarySearchTreeNode* pHead = curNode;

	for (int i = 0; i < arraySize; i++)
	{
		pHead = CCSTDC_BinarySearchTree_InsertAsAVLTree(insertTree, nodeArray[i], pHead);
	}

	insertTree->toRoot = pHead;
}

CCSTDC_Bool CCSTDC_BinarySearchTree_InsertArray(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode** nodeArray, CCSTDC_BinarySearchTreeNode* curNode,int arraySize)
{
	if (insertTree == NULL || nodeArray == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (arraySize < 0)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	CCSTDC_BinarySearchTreeNode* pHead = curNode;

	for (int i = 0; i < arraySize; i++)
	{
		pHead = CCSTDC_BinarySearchTree_Insert(insertTree, nodeArray[i], pHead);
	}

	insertTree->toRoot = pHead;
}



CCSTDC_Bool _CCSTDC_BinarySearchTree_PrintDataInList(CCSTDC_BinarySearchTreeNode* curNode, CCSTDC_Order_Code order,Printer print)
{
	if (print == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);	
	}

	if (curNode == NULL)
	{
		return;
	}
	switch (order)
	{
	case CCSTDC_PreOrder:
	{
		print(curNode->curElem);
		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->leftChild, order, print);
		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->rightChild, order, print);
	}break;
	case CCSTDC_InOrder:
	{

		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->leftChild, order, print);
		print(curNode->curElem);
		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->rightChild, order, print);
	}break;
	case CCSTDC_PostOrder:
	{
		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->leftChild, order, print);
		_CCSTDC_BinarySearchTree_PrintDataInList(curNode->rightChild, order, print);
		print(curNode->curElem);
	}break;
	default:
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);

		break;
	}
}

CCSTDC_Bool CCSTDC_BinarySearchTree_EraseSubTree(CCSTDC_BinarySearchTreeNode* curNode)
{
	if (curNode == NULL)
	{
		return;
	}

	CCSTDC_BinarySearchTree_EraseSubTree(curNode->leftChild);
	free(curNode->curElem);
	CCSTDC_BinarySearchTree_EraseSubTree(curNode->rightChild);
	free(curNode);
}

CCSTDC_Bool CCSTDC_BinarySearchTree_DrawBinTree(CCSTDC_BinarySearchTreeNode* curNode,Printer usrPrint,unsigned int spaceGapNum)
{
	if (usrPrint == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (curNode->leftChild != NULL) {
		CCSTDC_BinarySearchTree_DrawBinTree(curNode->leftChild, usrPrint,spaceGapNum + 1);
	}
	PRINT_TAB(spaceGapNum);
	usrPrint(curNode->curElem);
	PRINT_SLASH;
	if (curNode->rightChild != NULL) {
		CCSTDC_BinarySearchTree_DrawBinTree(curNode->rightChild, usrPrint, spaceGapNum + 1);
	}

}

CCSTDC_Bool CCSTDC_BinarySearchTree_EraseWholeTree(CCSTDC_BinarySearchTree* aTree)
{
	if (aTree == NULL) {
		return;
	}

	if (aTree->curCountElem == 0) {
		return;
	}

	CCSTDC_BinarySearchTree_EraseSubTree(aTree->toRoot);
	free(aTree);
}

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_findTargetNode(CCSTDC_BinarySearchTree* aTree,CCSTDC_BinarySearchTreeNode* curNode,void* elem)
{
	if (aTree == NULL || elem == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (curNode == NULL) {
		return NULL;
	}

	if (aTree->compFunc(elem, curNode->curElem) == CCSTDC_Bigger) {
		CCSTDC_BinarySearchTree_findTargetNode(aTree, curNode->rightChild, elem);
	}

	else if (aTree->compFunc(elem, curNode->curElem) == CCSTDC_Smaller) {
		CCSTDC_BinarySearchTree_findTargetNode(aTree, curNode->leftChild, elem);
	}

	else if (aTree->compFunc(elem, curNode->curElem) == CCSTDC_Equal)
	{
		return curNode;
	}
}

int CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(CCSTDC_BinarySearchTreeNode* curNode)
{

	if (curNode == NULL)
	{
		return 0;
	}
	
	return CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->leftChild)
			> CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->rightChild) ?
		CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->leftChild) + 1 : CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(curNode->rightChild) + 1;
}

