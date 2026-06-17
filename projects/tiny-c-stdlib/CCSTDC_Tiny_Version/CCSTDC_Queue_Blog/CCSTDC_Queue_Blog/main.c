#include"CCSTDC_Queue.h"
#include<time.h>

struct stu {

	char name[20];
	int age;
	int score;
};
void test()
{
	(int*)1;
	int a = 1;
	int* pa = &a;
	
	struct stu lihua;
	struct stu* plihua = &lihua;

	int b = plihua->age;
	b = lihua.age;
	(*plihua).age;

	void* pAny;
	pAny = (int*)1;
	pAny = (double*)2;

	
}



void intergerPrint(int* num) {
	printf("%d ", *num);
}

CCSTDC_Bool intBig(int* num1, int* num2) {
	return *num1 > *num2;
}

void test1()
{
	CCSTDC_Queue* queue = CCSTDC_Queue_initEmptyQueue(10);
	for (int i = 0; i < 20; i++) {
		CCSTDC_Queue_enQueue(queue, &i, sizeof(i), CCSTDC_Throw);
		CCSTDC_Queue_PrintQueue(queue, intergerPrint);
		printf("\n");
	}

	for (int i = 0; i < 10; i++) {
		CCSTDC_Queue_FlowForward(queue);
		CCSTDC_Queue_PrintQueue(queue, intergerPrint);
		printf("\n");
	}

	for (int i = 0; i < 10; i++) {
		CCSTDC_Queue_FlowBackward(queue);
		CCSTDC_Queue_PrintQueue(queue, intergerPrint);
		printf("\n");
	}


	void* curFront, * curEnd;
	curFront = CCSTDC_Queue_FetchFrontDataCore(queue);
	curEnd = CCSTDC_Queue_FetchRearDataCore(queue);
	printf("Cur front:> ");
	intergerPrint(curFront);
	printf("Cur End:> ");
	intergerPrint(curEnd);
	free(curFront);
	free(curEnd);

	for (int i = 0; i < 5; i++) {
		CCSTDC_Queue_deQueue(queue);
		CCSTDC_Queue_PrintQueue(queue, intergerPrint);
		printf("\n");
	}

	free(queue);
}

int tets1()
{
	srand((unsigned int)time(NULL));
	CCSTDC_PriorityQueue* queue = CCSTDC_PriorityQueue_initEmptyQueue(10, intBig);
	for (int i = 0; i < 20; i++) {
		int data = rand() % 20;
		CCSTDC_PriorityQueue_enQueue(queue, &data, sizeof(data), CCSTDC_Throw);
		CCSTDC_PriorityQueue_PrintQueue(queue, intergerPrint);
		printf("\n");
	}

	CCSTDC_PriorityQueue_DestroyAll(queue);
}

int add(int a1, int a2) {
	return a1 + a2;
}

int min1(int a1, int a2) {
	return a1 - a2;
}

int main()
{
	int(*p[2])(int, int) = { add, min1 };
	int a3 = p[1](1, 2);
	printf("%d", a3);
	
}

