#include"CCSTDC_hash.h"
#define MYHASHKEYMAXSIZE 10
#include<time.h>
#include<stdlib.h>
#define SIZE 100

HashKey myHash(int* elem) {
	return *elem % MYHASHKEYMAXSIZE;
}

CCSTDC_Bool intComp(int* e1, int* e2)
{
	return *e1 == *e2;
}

void printInt(int* elem) {
	printf("%d ", *elem);
}

int* getRAnd(int size) {
	srand((unsigned int)time(NULL));
	int* array = (int*)malloc(size * sizeof(int));
	for (int i = 0; i < size ; i++)
	{
		array[i] = rand() % 100;
		printInt(&(array[i]));
	}
	return array;
}

int main()
{
	CCSTDC_HashMap* map = CCSTDC_HashMap_EmptyInit(MYHASHKEYMAXSIZE, myHash, intComp);

	int a = 5;
	int a2 = 15;
	CCSTDC_HashCell* cell = CCSTDC_HashMap_WrapHashCell(map, &a, sizeof(a));
	CCSTDC_HashCell* cell2 = CCSTDC_HashMap_WrapHashCell(map, &a2, sizeof(a2));
	int* randArr = getRAnd(SIZE);
	CCSTDC_HashCellArray arrayCell = CCSTDC_HashMap_WrapHashCellList(map, randArr, sizeof(int), SIZE);
	CCSTDC_HashMap_PutCellListIntoHash(map, arrayCell, SIZE);
	putchar('\n');
	CCSTDC_HashMap_PutIntoHash(map, cell);
	CCSTDC_HashMap_PutIntoHash(map, cell2);
	CCSTDC_HashMap_PrintMap(map, printInt);
	CCSTDC_HashMap_RemoveKey(map, 5);
	CCSTDC_HashMap_RemoveKey(map, 7);
	CCSTDC_HashMap_PrintMap(map, printInt);
	CCSTDC_HashMap_EraseMap(map);

}