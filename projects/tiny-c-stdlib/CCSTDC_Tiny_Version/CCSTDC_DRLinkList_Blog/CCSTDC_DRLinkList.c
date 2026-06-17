#include"CCSTDC_DRLinkList.h"

CCSTDC_DRLinkList* CCSTDC_DRLinkList_InitEmpty()
{
	CCSTDC_DRLinkList* getNew = (CCSTDC_DRLinkList*)malloc(sizeof(CCSTDC_DRLinkList));

	if (getNew == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getNew->cur_size = 0;
	getNew->Head = NULL;
	getNew->Rear = NULL;
	return getNew;
}

CCSTDC_DRLinkNode* CCSTDC_DRLinkList_WrapNode(void* elem, size_t dataSize)
{
	if (elem == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_DRLinkNode* getNode = (CCSTDC_DRLinkNode*)malloc(sizeof(CCSTDC_DRLinkNode));
	if (getNode == NULL) {
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	void* data = malloc(dataSize);
	if (data == NULL) {
		free(getNode);
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	memcpy(data, elem, dataSize);

	getNode->coreData = data;
	getNode->dataSize = dataSize;
	getNode->next = NULL;
	getNode->prev = NULL;

	return getNode;
}

CCSTDC_Bool CCSTDC_DRLinkList_isFirst(CCSTDC_DRLinkList* list, CCSTDC_DRLinkNode* curNode)
{
	if (list == NULL || curNode == NULL) {
		CCSTDC_Error_Throw(1);
	}
	return list->Head == curNode;
}

CCSTDC_Bool CCSTDC_DRLinkList_isLast(CCSTDC_DRLinkList* list, CCSTDC_DRLinkNode* curNode)
{
	if (list == NULL || curNode == NULL) {
		CCSTDC_Error_Throw(1);
	}
	return list->Rear == curNode;
}


CCSTDC_Bool CCSTDC_DRLinkList_ResetLast(CCSTDC_DRLinkList* list, CCSTDC_DRLinkNode* curNode)
{
	CCSTDC_DRLinkNode* pCur = list->Head;
	while (pCur->next != list->Head) {
		pCur = pCur->next;
	}

}

CCSTDC_Bool CCSTDC_DRLinkList_Insert(CCSTDC_DRLinkList* list, void* elem, size_t datasize,int pos)
{
	if (list == NULL || elem == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (pos < 0) {
		if (pos <= -(int)list->cur_size) {
			pos = CCSTDC_HEAD;	
		}
		else {
			pos = pos + list->cur_size;
		}
	}

	if (pos >= list->cur_size) {
		pos = CCSTDC_END(list);
	}

	CCSTDC_DRLinkNode* node = CCSTDC_DRLinkList_WrapNode(elem,datasize);

	if(list->cur_size == 0)
	{
		list->Head = node;
		list->Rear = node;
		node->next = node;
		node->prev = node;
		list->cur_size++;
		return 1;
	}

	if (pos == 0)
	{		
		node->next = list->Head;
		node->prev = list->Rear;

		list->Head->prev = node;
		list->Rear->next = node;

		list->Head = node;
		list->cur_size++;

		return 1;
		
	}
	else if (pos == CCSTDC_END(list)) {
		node->next = list->Head;
		node->prev = list->Rear;
		list->Head->prev = node;
		list->Rear->next = node;
		list->Rear = node;
		list->cur_size++;
		return 1;
	}
	else 
	{
		CCSTDC_DRLinkNode* makeInsertion = list->Head;
		for (int i = 0; i < pos - 1; i++) {
			makeInsertion = makeInsertion->next;
		}
		node->next = makeInsertion->next;
		makeInsertion->next->prev = node;
		makeInsertion->next = node;
		node->prev = makeInsertion;
		
		list->cur_size++;
		return 1;
	}
}

CCSTDC_Bool CCSTDC_DRLinkList_PrintList(CCSTDC_DRLinkList* list, Printer printFunc, CCSTDC_Order order)
{
	if (list == NULL || printFunc == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (list->Head == NULL) {
		CCSTDC_LOG("LinkList NULL, there is no neccessity of printing somthing...");
		return 1;
	}

	switch (order)
	{
	case PositiveOrder:
	{
		CCSTDC_DRLinkNode* pCur = list->Head;
		
		while (pCur->next != list->Head) {

			printFunc(pCur->coreData);

			pCur = pCur->next;

			
		}
		printFunc(pCur->coreData);
		break;
	}
	case NegativeOrger:
	{
		CCSTDC_DRLinkNode* pCur = list->Rear;
		while (!CCSTDC_DRLinkList_isLast(list, pCur->prev)) { /*分离查找和比较，防止复杂度上升到O(N^2)*/
			
			printFunc(pCur->coreData);

			pCur = pCur->prev;
		}
		printFunc(pCur->coreData);
		break;
	}
	default:CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
		break;
	}

	return 1;
}

CCSTDC_Bool CCSTDC_DRLinkList_DestoryAll(CCSTDC_DRLinkList* list)
{
	if (list == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (list->cur_size == 1) {
		free(list->Head->coreData);
		free(list->Head);
		free(list);
		return 1;
	}

	CCSTDC_DRLinkNode* pCur = list->Head->next;
	for(int i = 0; i < list->cur_size - 1;i++){

		free(pCur->prev->coreData);
		free(pCur->prev);
		pCur = pCur->next;
	}
	free(list);

	return 1;
}

CCSTDC_Bool CCSTDC_DRLinkList_isEmpty(CCSTDC_DRLinkList* list){
	return list == NULL || list->Head == NULL;
}

CCSTDC_Bool CCSTDC_DRLinkList_Erase(CCSTDC_DRLinkList* list, int pos)
{
	if (list == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (pos < 0) {
		if (pos <= -(int)list->cur_size) {
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
		}
		else {
			pos = pos + list->cur_size;
		}
	}

	if (pos >= list->cur_size) {
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	if (pos == 0) {
		list->Head = list->Head->next;
		free(list->Head->prev->coreData);
		free(list->Head->prev);
		list->Rear->next = list->Head;
		list->Head->prev = list->Rear;
		list->cur_size--;
		return 1;
	}
	else if (pos == CCSTDC_END(list)) {
		list->Rear = list->Rear->prev;
		free(list->Rear->next->coreData);
		free(list->Rear->next);
		list->Rear->next = list->Head;
		list->Head->prev = list->Rear;
		list->cur_size--;
		return 1;
	}
	else
	{
		CCSTDC_DRLinkNode* pCur = list->Head;
		for (int i = 0; i < pos; i++) {
			pCur = pCur->next;
		}

		pCur->prev->next = pCur->next;
		pCur->next->prev = pCur->prev;
		free(pCur->coreData);
		free(pCur);
		list->cur_size--;
		return 1;
	}
}

CCSTDC_GainGuidePosArray* CCSTDC_DRLinkList_FindTarget(CCSTDC_DRLinkList* list, void* elem, CCSTDC_Comparator compFunc, CCSTDC_Order order)
{
	if (list == NULL || elem == NULL || compFunc == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (list->Head == NULL) {
		CCSTDC_LOG("No need to find sth... the list is still NULL");
		return 1;
	}
	
	switch (order) 
	{
		case PositiveOrder:
		{
			int counter = 0;
			CCSTDC_DRLinkNode* pCur = list->Head;
			CCSTDC_DRLinkList* getPos = CCSTDC_DRLinkList_InitEmpty();
			while (pCur->next != list->Head)
			{
				if (compFunc(pCur->coreData, elem)) {
					CCSTDC_DRLinkList_PushBack(getPos, &counter, sizeof(int));
				}
				counter++;
				pCur = pCur->next;
			}
			if (compFunc(pCur->coreData, elem)) {
				CCSTDC_DRLinkList_PushBack(getPos, &counter, sizeof(int));
			}
			return getPos;
		}break;

		case NegativeOrger:
		{
			int counter = CCSTDC_END(list) - 1;
			CCSTDC_DRLinkNode* pCur = list->Rear;
			CCSTDC_DRLinkList* getPos = CCSTDC_DRLinkList_InitEmpty();
			while (pCur->prev != list->Rear)
			{
				if (compFunc(pCur->coreData, elem)) {
					CCSTDC_DRLinkList_PushBack(getPos, &counter, sizeof(int));
				}
				counter--;
				pCur = pCur->prev;
			}
			if (compFunc(pCur->coreData, elem)) {
				CCSTDC_DRLinkList_PushBack(getPos, &counter, sizeof(int));
			}
			return getPos;
		}break;

		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
			break;
		
	}
		
	
}

CCSTDC_Bool CCSTDC_DRLinkList_SetClear(CCSTDC_DRLinkList* list)
{
	if (list == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (list->cur_size == 1) {
		free(list->Head->coreData);
		free(list->Head);
		return 1;
	}

	CCSTDC_DRLinkNode* pCur = list->Head->next;
	for (int i = 0; i < list->cur_size - 1; i++) {

		free(pCur->prev->coreData);
		free(pCur->prev);
		pCur = pCur->next;
	}
	

	return 1;
}

void* CCSTDC_DRLinkList_GetElemByPos(CCSTDC_DRLinkList* list, int pos,CCSTDC_Moving_Selection usrChoice)
{
	if (list == NULL) {
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (pos < 0) {
		if (pos <= -(int)list->cur_size) {
			int temp = -pos;
			temp %= list->cur_size;
			pos = list->cur_size - temp;
		}
		else {
			pos = pos + list->cur_size;
		}
	}

	if (pos >= list->cur_size) {
		pos %= list->cur_size;
	}


	if (pos == 0) {
		CCSTDC_DRLinkNode* pCur = list->Head;
		switch (usrChoice)
		{
		case CCSTDC_Move:
			return pCur->coreData;
		case CCSTDC_Copy:
		{
			void* dataPiece = malloc(pCur->dataSize);
			if (dataPiece == NULL) {
				CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
			}
			memcpy(dataPiece, pCur->coreData, pCur->dataSize);
			return dataPiece;
		}
		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
		}

	}

	if (pos == list->cur_size - 1) {
		CCSTDC_DRLinkNode* pCur = list->Rear;
		switch (usrChoice)
		{
		case CCSTDC_Move:
			return pCur->coreData;
		case CCSTDC_Copy:
		{
			void* dataPiece = malloc(pCur->dataSize);
			if (dataPiece == NULL) {
				CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
			}
			memcpy(dataPiece, pCur->coreData, pCur->dataSize);
			return dataPiece;
		}
		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
		}
	}


	CCSTDC_DRLinkNode* pCur = list->Head;
	for (int i = 0; i < pos; i++) {
		pCur = pCur->next;
	}

	switch (usrChoice) 
	{
		case CCSTDC_Move:
			return pCur->coreData;
		case CCSTDC_Copy:
		{
			void* dataPiece = malloc(pCur->dataSize);
			if (dataPiece == NULL) {
				CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
			}
			memcpy(dataPiece, pCur->coreData, pCur->dataSize);
			return dataPiece;
		}
		default:
			CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}
}