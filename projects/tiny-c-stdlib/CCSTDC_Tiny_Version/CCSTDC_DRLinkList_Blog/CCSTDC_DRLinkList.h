#pragma once
#include<stdlib.h>
#include<stdio.h>
#include<memory.h>

typedef enum {
    CCSTDC_NULL_INPUT = 1,
    CCSTDC_INVALID_INPUT = 2,
    CCSTDC_FAILED_MALLOC = 3,
    CCSTDC_INVALID_FREE = 4
}CCSTDC_ErrorCode;

#define CCSTDC_Error_Throw(ErrorCode) printf("Error Occured!");exit(ErrorCode)
#define CCSTDC_LOG(MSG) printf("%s",MSG)
#define CCSTDC_END(listName) listName->cur_size
#define CCSTDC_HEAD 0
typedef unsigned int CCSTDC_Bool;
typedef unsigned int CCSTDC_Order;
typedef void(*Printer)(void*);
typedef CCSTDC_Bool(*CCSTDC_Comparator)(void*, void*);
typedef enum {
    PositiveOrder,
    NegativeOrger
}CCSTDC_Visit_Order;

typedef enum {
    CCSTDC_Copy,
    CCSTDC_Move
}CCSTDC_Moving_Choice;
typedef unsigned int CCSTDC_Moving_Selection;

typedef struct _CCSTDC_DRLinkNode {
    void* coreData;
    size_t dataSize;
    struct _CCSTDC_DRLinkNode* next;
    struct _CCSTDC_DRLinkNode* prev;
}CCSTDC_DRLinkNode;

typedef struct _CCSTDC_DRLinkList {
    CCSTDC_DRLinkNode* Head;
    CCSTDC_DRLinkNode* Rear;
    size_t cur_size;
}CCSTDC_DRLinkList;

typedef CCSTDC_DRLinkList CCSTDC_GainGuidePosArray;

CCSTDC_Bool CCSTDC_DRLinkList_isFirst(CCSTDC_DRLinkList* list, CCSTDC_DRLinkNode* curNode);
CCSTDC_Bool CCSTDC_DRLinkList_isLast(CCSTDC_DRLinkList* list, CCSTDC_DRLinkNode* curNode);

CCSTDC_DRLinkList* CCSTDC_DRLinkList_InitEmpty();
CCSTDC_DRLinkNode* CCSTDC_DRLinkList_WrapNode(void* elem, size_t dataSize);
CCSTDC_Bool CCSTDC_DRLinkList_Insert(CCSTDC_DRLinkList* list, void* elem, size_t dataSize,int pos);
#define CCSTDC_DRLinkList_PushBack(list,elem,dataSize) CCSTDC_DRLinkList_Insert(list, elem, dataSize, CCSTDC_END(list))
#define CCSTDC_DRLinkList_PushFront(list,elem, dataSize) CCSTDC_DRLinkList_Insert(list, elem, dataSize, CCSTDC_HEAD)

CCSTDC_Bool CCSTDC_DRLinkList_PrintList(CCSTDC_DRLinkList* list, Printer printFunc, CCSTDC_Order order);
CCSTDC_Bool CCSTDC_DRLinkList_isEmpty(CCSTDC_DRLinkList* list);
CCSTDC_Bool CCSTDC_DRLinkList_Erase(CCSTDC_DRLinkList* list, int pos);
#define CCSTDC_DRLinkList_PopBack(list) CCSTDC_DRLinkList_Erase(list,CCSTDC_END(list) - 1);
#define CCSTDC_DRLinkList_PopFront(list) CCSTDC_DRLinkList_Erase(list,CCSTDC_HEAD);

CCSTDC_Bool CCSTDC_DRLinkList_DestoryAll(CCSTDC_DRLinkList* list);

CCSTDC_GainGuidePosArray* CCSTDC_DRLinkList_FindTarget(CCSTDC_DRLinkList* list, void* elem, CCSTDC_Comparator compFunc, CCSTDC_Order order);

CCSTDC_Bool CCSTDC_DRLinkList_SetClear(CCSTDC_DRLinkList* list);

void* CCSTDC_DRLinkList_GetElemByPos(CCSTDC_DRLinkList* list,int pos,CCSTDC_Moving_Selection usrChoice);
#define CCSTDC_DRLinkList_GetFrontElem(list,usrChoice) CCSTDC_DRLinkList_GetElemByPos(list, 0, usrChoice)
#define CCSTDC_DRLinkList_GetRearElem(list,usrChoice) CCSTDC_DRLinkList_GetElemByPos(list, CCSTDC_END(list) - 1, usrChoice)
