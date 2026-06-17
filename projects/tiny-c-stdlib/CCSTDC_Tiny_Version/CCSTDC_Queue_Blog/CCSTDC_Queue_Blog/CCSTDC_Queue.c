#include"CCSTDC_Queue.h"

CCSTDC_Queue* CCSTDC_Queue_initEmptyQueue(size_t limitedSize)
{
	CCSTDC_Queue* getQueue = (CCSTDC_Queue*)malloc(sizeof(CCSTDC_Queue));
	if (getQueue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getQueue->limited_size = limitedSize;

	CCSTDC_DRLinkList* getList = CCSTDC_DRLinkList_InitEmpty();
	
	getQueue->coreData = getList;

	return getQueue;
}

void* CCSTDC_Queue_enQueue(CCSTDC_Queue* queue,void* data, size_t dataSize, CCSTDC_PopOutChoice usrChoice)
{
	if (data == NULL || queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	if(queue->coreData->cur_size < queue->limited_size)
	{
		CCSTDC_DRLinkList_PushBack(queue->coreData, data, dataSize);
		return NULL;
	}
	else {
		switch (usrChoice)
		{
		case CCSTDC_Fetch:
		{
			void* data1 = CCSTDC_DRLinkList_GetRearElem(queue->coreData, CCSTDC_Copy);

			CCSTDC_DRLinkList_PopBack(queue->coreData);

			CCSTDC_DRLinkList_PushFront(queue->coreData, data, dataSize);

			return data1;
		}
		case CCSTDC_Throw:
		{
			CCSTDC_DRLinkList_PopBack(queue->coreData);

			CCSTDC_DRLinkList_PushFront(queue->coreData, data, dataSize);

			return NULL;
		}
		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
			break;
		}
	}
}

CCSTDC_Bool CCSTDC_Queue_deQueue(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_DRLinkList_PopBack(queue->coreData);

	return CCSTDC_True;
}

CCSTDC_Bool CCSTDC_Queue_FlowForward(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	void* data = CCSTDC_DRLinkList_GetRearElem(queue->coreData, CCSTDC_Copy);

	CCSTDC_DRLinkList_PopBack(queue->coreData);

	CCSTDC_DRLinkList_PushFront(queue->coreData, data, queue->coreData->Head->dataSize);

	free(data);
}

CCSTDC_Bool CCSTDC_Queue_FlowBackward(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}
	void* data = CCSTDC_DRLinkList_GetFrontElem(queue->coreData, CCSTDC_Copy);

	CCSTDC_DRLinkList_PopFront(queue->coreData);

	CCSTDC_DRLinkList_PushBack(queue->coreData, data, queue->coreData->Head->dataSize);

	free(data);
}

void* CCSTDC_Queue_FetchFrontDataCore(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	void* data = CCSTDC_DRLinkList_GetFrontElem(queue->coreData, CCSTDC_Copy);

	return data;
}

void* CCSTDC_Queue_FetchRearDataCore(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	void* data = CCSTDC_DRLinkList_GetRearElem(queue->coreData, CCSTDC_Copy);

	return data;
}

CCSTDC_Bool CCSTDC_Queue_DestroyAll(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_DRLinkList_DestoryAll(queue->coreData);

	free(queue);

	return CCSTDC_True;
}

CCSTDC_Bool CCSTDC_Queue_SetEmpty(CCSTDC_Queue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_DRLinkList_SetClear(queue->coreData);

	return CCSTDC_True;
}

CCSTDC_PriorityQueue* CCSTDC_PriorityQueue_initEmptyQueue(size_t limitedSize, Comparator compFunc)
{
	if (compFunc == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_PriorityQueue* getQueue = (CCSTDC_PriorityQueue*)malloc(sizeof(CCSTDC_PriorityQueue));
	if (getQueue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getQueue->orgQueue = CCSTDC_Queue_initEmptyQueue(limitedSize);

	getQueue->compFunc = compFunc;

	return getQueue;
}

void* CCSTDC_PriorityQueue_enQueue(CCSTDC_PriorityQueue* queue, void* data, size_t dataSize, CCSTDC_PopOutChoice usrChoice)
{
	if (queue == NULL || data == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (queue->orgQueue->coreData->cur_size == 0) {
		CCSTDC_DRLinkList_PushBack(queue->orgQueue->coreData, data,dataSize);
		return NULL;
	}

	if (queue->orgQueue->coreData->cur_size >= queue->orgQueue->limited_size) {
		switch (usrChoice)
		{
		case CCSTDC_Fetch:
		{

			void* data1 = CCSTDC_DRLinkList_GetRearElem(queue->orgQueue->coreData, CCSTDC_Copy);

			CCSTDC_DRLinkList_PopBack(queue->orgQueue->coreData);

			CCSTDC_DRLinkNode* pCurNode = queue->orgQueue->coreData->Head;

			int curCount = 0;
			while (queue->compFunc(pCurNode->coreData, data) == 0) {
				pCurNode = pCurNode->next;
				curCount++;
				if (pCurNode == queue->orgQueue->coreData->Head) {
					break;
				}
			}

			CCSTDC_DRLinkList_Insert(queue->orgQueue->coreData, data, dataSize, curCount);

			return data1;
		}
		case CCSTDC_Throw:
		{
			CCSTDC_DRLinkList_PopBack(queue->orgQueue->coreData);
			CCSTDC_DRLinkNode* pCurNode = queue->orgQueue->coreData->Head;

			int curCount = 0;
			while (queue->compFunc(pCurNode->coreData, data) == 0) {
				pCurNode = pCurNode->next;
				curCount++;
				if (pCurNode == queue->orgQueue->coreData->Head) {
					break;
				}
			}

			CCSTDC_DRLinkList_Insert(queue->orgQueue->coreData, data, dataSize, curCount);

			return NULL;
		}
		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
			break;
		}
	}
	else {
		CCSTDC_DRLinkNode* pCurNode = queue->orgQueue->coreData->Head;

		int curCount = 0;
		while (queue->compFunc(pCurNode->coreData, data) == 0) {
			pCurNode = pCurNode->next;
			curCount++;
			if (pCurNode == queue->orgQueue->coreData->Head) {
				break;
			}
		}

		CCSTDC_DRLinkList_Insert(queue->orgQueue->coreData, data, dataSize, curCount);
	}
}

CCSTDC_Bool CCSTDC_PriorityQueue_DestroyAll(CCSTDC_PriorityQueue* queue)
{
	if (queue == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_Queue_DestroyAll(queue->orgQueue);

	free(queue);

	return CCSTDC_True;
}




