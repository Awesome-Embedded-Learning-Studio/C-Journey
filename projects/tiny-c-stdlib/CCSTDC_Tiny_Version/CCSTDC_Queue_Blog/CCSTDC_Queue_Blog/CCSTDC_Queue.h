#include"CCSTDC_Utils.h"
#include"CCSTDC_DRLinkList.h"

#define GETQUEUECORE(queue) queue->coreData
typedef unsigned int CCSTDC_PopOutChoice;
typedef enum {
	CCSTDC_Throw,
	CCSTDC_Fetch
}CCSTDC_PopOut_Selections;
typedef struct _CCSTDC_Recycle_Queue {
	CCSTDC_DRLinkList* coreData;
	size_t limited_size;
}CCSTDC_Queue;


CCSTDC_Queue* CCSTDC_Queue_initEmptyQueue(size_t limitedSize);
void* CCSTDC_Queue_enQueue(CCSTDC_Queue* queue, void* data, size_t dataSize, CCSTDC_PopOutChoice usrChoice);
CCSTDC_Bool CCSTDC_Queue_deQueue(CCSTDC_Queue* queue);
#define CCSTDC_Queue_PrintQueue(queue,printFunc) CCSTDC_DRLinkList_PrintList(GETQUEUECORE(queue),printFunc,PositiveOrder);
CCSTDC_Bool CCSTDC_Queue_FlowForward(CCSTDC_Queue* queue);
CCSTDC_Bool CCSTDC_Queue_FlowBackward(CCSTDC_Queue* queue);
void* CCSTDC_Queue_FetchFrontDataCore(CCSTDC_Queue* queue);
void* CCSTDC_Queue_FetchRearDataCore(CCSTDC_Queue* queue);
CCSTDC_Bool CCSTDC_Queue_DestroyAll(CCSTDC_Queue* queue);
CCSTDC_Bool CCSTDC_Queue_SetEmpty(CCSTDC_Queue* queue);

typedef CCSTDC_Bool(*Comparator)(void*, void*);
typedef struct _CCSTDC_PriorityQueue {
	CCSTDC_Queue* orgQueue;
	CCSTDC_Comparator compFunc;
}CCSTDC_PriorityQueue;

CCSTDC_PriorityQueue* CCSTDC_PriorityQueue_initEmptyQueue(size_t limitedSize, Comparator compFunc);
void* CCSTDC_PriorityQueue_enQueue(CCSTDC_PriorityQueue* queue, void* data, size_t dataSize, CCSTDC_PopOutChoice usrChoice);
#define CCSTDC_PriorityQueue_deQueue(queue) CCSTDC_Queue_deQueue(queue->orgQueue)
#define CCSTDC_PriorityQueue_PrintQueue(queue,printFunc) CCSTDC_DRLinkList_PrintList(GETQUEUECORE(queue->orgQueue),printFunc,PositiveOrder)
#define CCSTDC_PriotityQueue_FetchFrontDataCore(queue) CCSTDC_Queue_FetchFrontDataCore(queue->orgQueue)
#define CCSTDC_PriorityQueue_FetchRearDataCore(queue) CCSTDC_Queue_FetchRearDataCore(queue->orgQueue)
CCSTDC_Bool CCSTDC_PriorityQueue_DestroyAll(CCSTDC_PriorityQueue* queue);
#define CCSTDC_PriorityQueue_SetEmpty(queue) CCSTDC_Queue_SetEmpty(queue->orgQueue);