#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include"CCSTDC_Utils.h"
#define CASE_BIIGER(EXP) if(EXP) return CCSTDC_Bigger;
#define CASE_SMALLER(EXP) if(EXP) return CCSTDC_Smaller;
#define CASE_EQUAL(EXP) if(EXP) return CCSTDC_Equal;
#define PRINT_TAB(spaceGapNum) for(int i = 0; i < spaceGapNum; i++) printf("\t");
#define PRINT_SLASH printf("\n")

typedef void(*FUNCPTR)(void* dataSelf,  void* const outer);
typedef int(*Comparator)(void*, void*);
typedef void(*Printer)(void*);

typedef enum {
	CCSTDC_Bigger = 1,
	CCSTDC_Equal = 0,
	CCSTDC_Smaller = -1
};

typedef enum {
	CCSTDC_PreOrder,
	CCSTDC_InOrder,
	CCSTDC_PostOrder
};

typedef unsigned int CCSTDC_Order_Code;
/*Main Structure Define here*/
typedef struct _binTree {
	void* curElem;
	size_t dataSize;
	size_t Appeartimes;
	struct _binTree* leftChild;
	struct _binTree* rightChild;
}CCSTDC_BinarySearchTreeNode;
/* Create a Main Body DataStructure */
typedef struct _binTreeSelf {
	unsigned int curCountElem;
	CCSTDC_BinarySearchTreeNode* toRoot;
	Comparator compFunc;
}CCSTDC_BinarySearchTree;

CCSTDC_BinarySearchTree* CCSTDC_BinarySearchTree_initEmptyBinarySearchTree(
	Comparator compFunc
);

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_SimplyCopyNode(
	CCSTDC_BinarySearchTreeNode* copied
);

CCSTDC_BinarySearchTree* CCSTDC_BinarySearchTree_CopyBinarySearchTree(
	CCSTDC_BinarySearchTree* aTree
);

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_CopyBinarySearchSubTree(
	CCSTDC_BinarySearchTreeNode* startFromWhere
);

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_wrapDataToNode(void* elem, size_t size);
#define CCSTDC_BinarySearchTree_FastwrapDataToNode(elem) CCSTDC_BinarySearchTree_wrapDataToNode(elem,sizeof(elem))

CCSTDC_BinarySearchTreeNode** CCSTDC_BinarySearchTree_wrapDataListToListNode(
	void* array,
	size_t dataSize,
	int arraySize
);

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_Insert(
	CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode* node,
	CCSTDC_BinarySearchTreeNode* curNode
);
#define CCSTDC_BinarySearchTree_DirectPush(insertTree,data) \
	CCSTDC_BinarySearchTree_Insert(insertTree, CCSTDC_BinarySearchTree_FastwrapDataToNode(data), insertTree->toRoot)

CCSTDC_Bool CCSTDC_BinarySearchTree_InsertArray(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode** nodeArray, CCSTDC_BinarySearchTreeNode* curNode, int arraySize);
#define  CCSTDC_BinarySearchTree_DirectPushArray(insertTree,array,arraySize) \
	CCSTDC_BinarySearchTree_InsertArray( \
		insertTree, \
		CCSTDC_BinarySearchTree_wrapDataListToListNode(array,sizeof(array[0]),arraySize), \
		insertTree->toRoot,arraySize)

CCSTDC_Bool _CCSTDC_BinarySearchTree_PrintDataInList(
	CCSTDC_BinarySearchTreeNode* curNode ,CCSTDC_Order_Code code,Printer print
);
#define CCSTDC_BinarySearchTree_PrintDataInList(insertTree,code,print) _CCSTDC_BinarySearchTree_PrintDataInList(insertTree->toRoot,code,print)

CCSTDC_Bool CCSTDC_BinarySearchTree_TravelAndImpactTheSubTree(
	CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode* curNode,
	CCSTDC_Order_Code code,
	FUNCPTR function,
	void* const  communications
);

CCSTDC_Bool CCSTDC_BinarySearchTree_DrawBinTree(CCSTDC_BinarySearchTreeNode* curNode, Printer usrPrint, unsigned int spaceGapNum);

CCSTDC_Bool CCSTDC_BinarySearchTree_EraseSubTree(CCSTDC_BinarySearchTreeNode* curNode);

CCSTDC_Bool CCSTDC_BinarySearchTree_EraseWholeTree(CCSTDC_BinarySearchTree* aTree);

CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_findTargetNode(CCSTDC_BinarySearchTree* aTree, CCSTDC_BinarySearchTreeNode* curNode, void* elem);

int CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(CCSTDC_BinarySearchTreeNode* curNode);

#define CCSTDC_BinarySearchTree_getCurrentTreeHeight(aValidTree) CCSTDC_BinarySearchTree_getCurrentSubTreeHeight(aValidTree->toRoot)

CCSTDC_Bool CCSTDC_BinarySearchTree_ALV_InsertArray(CCSTDC_BinarySearchTree* insertTree,
	CCSTDC_BinarySearchTreeNode** nodeArray, CCSTDC_BinarySearchTreeNode* curNode, int arraySize);

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_SingleRotateWithLeft(
	CCSTDC_BinarySearchTreeNode* node
);

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_SingleRotateWithRight(
	CCSTDC_BinarySearchTreeNode* node
);

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_DoubleRotateWithLeft(
	CCSTDC_BinarySearchTreeNode* node
);

static CCSTDC_BinarySearchTreeNode* CCSTDC_BinarySearchTree_ALV_DoubleRotateWithRight(
	CCSTDC_BinarySearchTreeNode* node
);