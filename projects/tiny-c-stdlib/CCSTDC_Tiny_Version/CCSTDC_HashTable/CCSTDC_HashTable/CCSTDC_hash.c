#include"CCSTDC_hash.h"

CCSTDC_HashMap* CCSTDC_HashMap_EmptyInit(size_t wishedMaxSize, HashFunction hashfunction, Comparator compFunc)
{
	CCSTDC_HashMap* getMap = (CCSTDC_HashMap*)malloc(sizeof(CCSTDC_HashMap));
	if (getMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getMap->maxSize = wishedMaxSize;
	getMap->reflectFunction = hashfunction;
	getMap->compFunc = compFunc;
	getMap->DepracatedKeyArray = NULL;
	getMap->currentDeprecateKeyArraySize = 0;
	getMap->toMap = NULL;
	HashCellArray arrayForCell = (HashCellArray)malloc(sizeof(CCSTDC_HashCell*) * wishedMaxSize);
	if (arrayForCell == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	for (int i = 0; i < getMap->maxSize; i++)
	{
		arrayForCell[i] = NULL;
	}

	getMap->toMap = &arrayForCell[0];

	return getMap;
}

CCSTDC_HashCell* CCSTDC_HashMap_CopyCell(CCSTDC_HashCell* hashCell)
{
	if (hashCell == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_HashCell* cell = (CCSTDC_HashCell*)malloc(sizeof(CCSTDC_HashCell));
	if (cell == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	cell->appearTimes = hashCell->appearTimes;
	cell->key = hashCell->key;
	cell->next = NULL;
	cell->elemSize = hashCell->elemSize;
	void* space = malloc(cell->elemSize);
	memcpy(space, hashCell->elem, cell->elemSize);
	cell->elem = space;

	return cell;
}


CCSTDC_HashMap* CCSTDC_HashMap_CopyHash(CCSTDC_HashMap* AnotherHashMap)
{
	if(AnotherHashMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_HashMap* getMap = (CCSTDC_HashMap*)malloc(sizeof(CCSTDC_HashMap));
	if (getMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	getMap->compFunc = AnotherHashMap->compFunc;
	getMap->maxSize = AnotherHashMap->maxSize;
	getMap->reflectFunction = AnotherHashMap->reflectFunction;
	getMap->currentDeprecateKeyArraySize = AnotherHashMap->currentDeprecateKeyArraySize;
	HashCellArray newArray = (HashCellArray)malloc(sizeof(CCSTDC_HashCell*) * getMap->maxSize);
	if (newArray == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	for (int i = 0; i < getMap->maxSize; i++)
	{
		CCSTDC_HashCell* position = AnotherHashMap->toMap[i];
		CCSTDC_HashCell* copy = getMap->toMap[i];
		if (position == NULL)
		{
			continue;
		}
		else
		{
			while (position != NULL)
			{
				copy = CCSTDC_HashMap_CopyCell(position);
				position = position->next;
				copy = copy->next;
			}
		}
	}

	getMap->DepracatedKeyArray = (HashKey*)malloc(sizeof(HashKey) * AnotherHashMap->currentDeprecateKeyArraySize);
	if (getMap->DepracatedKeyArray == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	memcpy(
		getMap->DepracatedKeyArray,
		AnotherHashMap->DepracatedKeyArray,
		sizeof(HashKey) * AnotherHashMap->currentDeprecateKeyArraySize
	);

	return getMap;
}

CCSTDC_HashCell* CCSTDC_HashMap_WrapHashCell(CCSTDC_HashMap* toWhichMap, void* elem, size_t dataSize)
{
	if (toWhichMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	CCSTDC_HashCell* getCell = (CCSTDC_HashCell*)malloc(sizeof(CCSTDC_HashCell));
	if (getCell == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	void* forData = malloc(dataSize);
	if (forData == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	memcpy(forData, elem, dataSize);

	getCell->elem = forData;
	getCell->next = NULL;
	getCell->key = toWhichMap->reflectFunction(elem);
	getCell->appearTimes = 1;
	getCell->elemSize = dataSize;
	return getCell;
}

CCSTDC_HashCellArray CCSTDC_HashMap_WrapHashCellList(CCSTDC_HashMap* toWhichMap, void* elem, size_t dataSize, int howMany)
{
	if (toWhichMap == NULL || elem == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (howMany < 0)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	HashCellArray array = (HashCellArray)malloc(sizeof(CCSTDC_HashCell*) * howMany);
	for (int i = 0; i < howMany; i++)
	{
		array[i] = CCSTDC_HashMap_WrapHashCell(toWhichMap, (char*)elem + i * dataSize, dataSize);
	}

	return array;
}

CCSTDC_Bool	CCSTDC_HashMap_isValidKey(CCSTDC_HashMap* HashMap, HashKey key)
{
	if (HashMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (key > HashMap->maxSize)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	for (int i = 0; i < HashMap->currentDeprecateKeyArraySize; i++)
	{
		if (key == HashMap->DepracatedKeyArray[i]) {
			return CCSTDC_False;
		}
	}

	return CCSTDC_True;
}


CCSTDC_HashMap* CCSTDC_HashMap_PutIntoHash(CCSTDC_HashMap* HashMap, CCSTDC_HashCell* cell)
{
	if (HashMap == NULL || cell == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (!CCSTDC_HashMap_isValidKey(HashMap, cell->key)) {
		printf("Detact for the invalid insertion: key has already been depracated!");
		return HashMap;
	}

	
	if (HashMap->toMap[cell->key] == NULL)
	{
		HashMap->toMap[cell->key] = cell;
		return HashMap;
	}
	else if (HashMap->compFunc(HashMap->toMap[cell->key]->elem, cell->elem))
	{
		HashMap->toMap[cell->key]->appearTimes++;
		return HashMap;
	}
	CCSTDC_HashCell** position = &HashMap->toMap[cell->key];
	while ((*position) != NULL) {
		if (HashMap->compFunc((*position)->elem, cell->elem))
		{
			(*position)->appearTimes++;
			return HashMap;
		}
		position = &(*position)->next;
	}

	*position = cell;

	return HashMap;
}

void* CCSTDC_HashMap_FindTarget(CCSTDC_HashMap* HashMap, HashKey key, void* elem)
{
	if (elem == NULL || HashMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (key > HashMap->maxSize)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	CCSTDC_HashCell* position = HashMap->toMap[key];
	/* 鉴定为压根不存在 */
	if (position == NULL)
	{
		return NULL;
	}
	/* 鉴定为一值一键 */
	else if (position->next == NULL)
	{
		return position->elem;
	}
	else
	{
		while (position != NULL)
		{
			if (HashMap->compFunc(position->elem, elem))
			{
				return position->elem;
			}

			position = position->next;
		}

		return NULL;
	}
}

CCSTDC_Bool	CCSTDC_HashMap_PrintCell(CCSTDC_HashCell* cell, Printer cellPrinter)
{
	if (cell == NULL || cellPrinter == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	printf("AppearTimes:%d\n", cell->appearTimes);
	cellPrinter(cell->elem);
	putchar('\n');
}

CCSTDC_Bool CCSTDC_HashMap_PrintMap(CCSTDC_HashMap* map, Printer printElem)
{
	if (map == NULL || printElem == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	for (int i = 0; i < map->maxSize; i++)
	{
		CCSTDC_HashCell* position = map->toMap[i];
		if(position != NULL)
		{
			printf("KEY: %d\n", map->toMap[i]->key);
		}
		while (position != NULL)
		{
			CCSTDC_HashMap_PrintCell(position, printElem);
			position = position->next;
		}
	}
}

CCSTDC_Bool	CCSTDC_HashMap_DeleteCell(CCSTDC_HashMap* HashMap, HashKey key, void* elem)
{
	if (HashMap == NULL || elem == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (key > HashMap->maxSize)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	CCSTDC_HashCell* position = HashMap->toMap[key];
	if (position == NULL)
	{
		return -1;
	}
	else if (position->next == NULL)
	{
		free(position->elem);
		free(position);
		return 0;
	}
	else if(position->next != NULL)
	{
		while (position != NULL) {
			if (HashMap->compFunc(position->elem, elem))
			{
				free(position->elem);
				free(position);
				return 0;
			}
			position = position->next;
		}		
		return -1;
	}
}

CCSTDC_Bool	CCSTDC_HashMap_AdjustInvalidKeyGroup(CCSTDC_HashMap* HashMap, HashKey deprecatedkey)
{
	if (HashMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (deprecatedkey > HashMap->maxSize)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	HashMap->currentDeprecateKeyArraySize++;
	HashMap->DepracatedKeyArray = (HashKey*)realloc(HashMap->DepracatedKeyArray, HashMap->currentDeprecateKeyArraySize * sizeof(HashKey));
	if (HashMap->DepracatedKeyArray == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_FAILED_MALLOC);
	}

	HashMap->DepracatedKeyArray[HashMap->currentDeprecateKeyArraySize - 1] = deprecatedkey;
	HashMap->toMap[deprecatedkey] = NULL;
	return 0;
}


CCSTDC_Bool	CCSTDC_HashMap_RemoveKey(CCSTDC_HashMap* HashMap, HashKey key)
{
	if (HashMap == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (key > HashMap->maxSize)
	{
		CCSTDC_Error_Throw(CCSTDC_INVALID_INPUT);
	}

	if (!CCSTDC_HashMap_isValidKey(HashMap, key))
	{
		printf("It has already been depracated key!\n");
		return 1;
	}

	CCSTDC_HashCell* position = HashMap->toMap[key];
	if (position == NULL)
	{
		CCSTDC_HashMap_AdjustInvalidKeyGroup(HashMap, key);
	}

	else
	{
		while (position != NULL)
		{
			CCSTDC_HashCell* forDel = position;
			position = forDel->next;
			free(forDel->elem);
			free(forDel);
		}

		CCSTDC_HashMap_AdjustInvalidKeyGroup(HashMap, key);
	}

	return 0;
}

CCSTDC_Bool	CCSTDC_HashMap_EraseMap(CCSTDC_HashMap* map)
{
	if (map == NULL)
	{
		CCSTDC_Error_Throw(CCSTDC_NULL_INPUT);
	}

	if (map->toMap == NULL)
	{
		free(map);
		return;
	}


	for (int i = 0; i < map->maxSize; i++)
	{
		if (map->toMap[i] == NULL)
		{
			continue;
		}
		else
		{
			CCSTDC_HashCell* position = map->toMap[i];
			while (position != NULL)
			{
				CCSTDC_HashCell* forDel = position;
				position = forDel->next;
				free(forDel->elem);
				free(forDel);
			}
		}
	}

	free(map->DepracatedKeyArray);

	free(map);

	map = NULL;
}