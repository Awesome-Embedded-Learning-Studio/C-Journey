#include"CCSTDC_Vector.h"

CCSTDC_Vector* CCSTDC_Vector_EmptyInit(int dataSize, int wishing_capicity)
{
    if (dataSize < 0 || wishing_capicity < 0) {
        /*Error Throw or Call exit(Error_Code). in C++, call std::terminate or std::abort is both acceptable*/
        CCSTDC_Error_Throw(1);
    }

    if (dataSize * wishing_capicity < 0) {
        CCSTDC_Error_Throw(2);
    }

    CCSTDC_Vector* getter = (CCSTDC_Vector*)malloc(sizeof(CCSTDC_Vector));//get a space
    void* getCoreData = malloc(dataSize * wishing_capicity);
    if (getter == NULL)/* NULL == (void*)0 */
    {
        /* when malloc space, we shouldn't get an invaild space pointer:NULL for example! */
        CCSTDC_Error_Throw(3);
    }
    if (getCoreData == NULL) {

        CCSTDC_Error_Throw(3);

    }
    getter->coreData = getCoreData;
    getter->dataSize = dataSize;
    getter->curSize = 0;
    getter->capicity = wishing_capicity;

    /*初始化完成，返回！*/
    return getter;
}

CCSTDC_Bool* CCSTDC_Vector_Resize(CCSTDC_Vector* vector, int wishing_capicity)
{
    /*判断有效性*/
    if (vector == NULL) {
        CCSTDC_Error_Throw(4);
    }
    if (wishing_capicity < 0)
    {
        CCSTDC_Error_Throw(1);
    }
    if (wishing_capicity * vector->dataSize < 0)
    {
        CCSTDC_Error_Throw(2);
    }
    void* newData = realloc(vector->coreData, wishing_capicity * vector->dataSize);
    if (newData == NULL) {
        CCSTDC_Error_Throw(3);
    }

    vector->coreData = newData;
    vector->capicity = wishing_capicity;

    return 1;
}

CCSTDC_Bool CCSTDC_Vector_PushBack(CCSTDC_Vector* vector, void* data, int datasize)
{
    if (data == NULL && vector == NULL) {
        CCSTDC_Error_Throw(4);
    }
    if (datasize < 0 && datasize != vector->dataSize) {
        CCSTDC_Error_Throw(1);
    }

    /*检查是否已满*/
    if (vector->curSize == vector->capicity - 1) {
        CCSTDC_Vector_Resize(vector, 2 * vector->capicity);
    }

    char* pushPtr = (char*)vector->coreData + datasize * vector->curSize;
    memcpy(pushPtr, data, datasize);

    vector->curSize++;
    return;
}

CCSTDC_Bool CCSTDC_Vector_PrintList(CCSTDC_Vector* vector, Printer printFunc)
{
    if (vector == NULL || printFunc == NULL)
    {
        CCSTDC_Error_Throw(4);
    }

    for (int i = 0; i < vector->curSize; i++)
    {
        printFunc(
            (char*)vector->coreData + i*vector->dataSize
        );
    }

    return 1;
}

CCSTDC_Bool CCSTDC_Vector_DestroyWhole(CCSTDC_Vector* vector)
{
    if (vector == NULL) {
        CCSTDC_Error_Throw(4);
    }

    if(vector->coreData != NULL)
    {
        free(vector->coreData);
    }

    free(vector);

    return 1;
}

CCSTDC_Bool CCSTDC_Vector_Insert(CCSTDC_Vector* vector, int pos, void* elem)
{
    if (vector == NULL || elem == NULL)
    {
        CCSTDC_Error_Throw(4);
    }

    if (pos < 0)
    {
        CCSTDC_Error_Throw(1);
    }

    if (pos >= vector->curSize) {
        pos = vector->curSize - 1;  /*做处理*/
    }

    if (vector->curSize == vector->capicity - 1) {
        CCSTDC_Vector_Resize(vector, 1.5 * vector->capicity);
    }

    for (int i = vector->curSize; i > pos; i--) {
        memcpy(
            (char*)vector->coreData + i * vector->dataSize,
            (char*)vector->coreData + (i - 1) * vector->dataSize,
            vector->dataSize
        );/*Warning for a funking low effiency*/
    }

    memcpy((char*)vector->coreData + pos * vector->dataSize, elem, vector->dataSize);

    vector->curSize++;

    return 1;

}

CCSTDC_GainGuide_PosArray* CCSTDC_Vector_FindTarget(CCSTDC_Vector* vec, void* Elem, CCSTDC_Comparator compareFunc) 
{
    if (vec == NULL || Elem == NULL || compareFunc == NULL)
    {
        CCSTDC_Error_Throw(4);
    }

    CCSTDC_GainGuide_PosArray* getArr = CCSTDC_Vector_EmptyInit(sizeof(int), 10);

    for (int i = 0; i < vec->curSize; i++) {
        if (compareFunc(
            (char*)vec->coreData + i * vec->dataSize,
            Elem
        ))
        {
            CCSTDC_Vector_PushBack(getArr, &i, sizeof(int));
        }
    }

    if (getArr->curSize == 0) {
        free(getArr);
        return NULL;
    }

    return getArr;
}

void* CCSTDC_Vector_GetPyPos(CCSTDC_Vector* vec, int pos, CCSTDC_Moving_Selection usrChoice)
{
    if (vec == NULL)
    {
        CCSTDC_Error_Throw(4);
    }
    if (pos < 0 || pos >= vec->curSize)
    {
        CCSTDC_Error_Throw(1);
    }
    switch (usrChoice)
    {
    case CCSTDC_Move:
    {
        return (char*)vec->coreData + pos * vec->dataSize;
    }
    case CCSTDC_Copy:
    {
        void* data = malloc(vec->dataSize);
        if (data == NULL) {
            CCSTDC_Error_Throw(3);
        }

        memcpy(data, (char*)vec->coreData + pos * vec->dataSize, vec->dataSize);

        return data;
    }
    default:
        printf("Invaild Selection Code");
        CCSTDC_Error_Throw(1);
        break;
    }

}

CCSTDC_Bool CCSTDC_Vector_EraseByPos(CCSTDC_Vector* vec, int place)
{
    if (vec == NULL)
    {
        CCSTDC_Error_Throw(4);
    }
    if (place < 0 || place >= vec->curSize)
    {
        CCSTDC_Error_Throw(1);
    }

    for (int i = place; i < vec->curSize - 1; i++)
    {
        memcpy(
            (char*)vec->coreData + i * vec->dataSize,
            (char*)vec->coreData + (i + 1) * vec->dataSize,
            vec->dataSize
        );

    }

    vec->curSize--;

    if (vec->curSize >= 10 && vec->curSize < (int)(vec->capicity / 2)) {
        CCSTDC_Vector_Resize(vec, (int)(vec->capicity / 2));
    }
    return 1;
}

CCSTDC_Bool CCSTDC_Vector_EraseByGivenData(CCSTDC_Vector* vec, void* targetElem, CCSTDC_Comparator compareFunc)
{
    if (vec == NULL || targetElem == NULL || compareFunc == NULL)
    {
        CCSTDC_Error_Throw(4);
    }

    for (int i = 0; i < vec->curSize; i++)
    {
        if (
            compareFunc(
                (char*)vec->coreData + i * vec->dataSize,
                targetElem
            ))
        {
            memcpy(
                (char*)vec->coreData + i * vec->dataSize,
                (char*)vec->coreData + (i + 1) * vec->dataSize,
                vec->dataSize * (vec->curSize - i)
                );

            vec->curSize--;
        }
    }

    if (vec->curSize >= 10 && vec->curSize < (int)(vec->capicity / 2)) {
        CCSTDC_Vector_Resize(vec, (int)(vec->capicity / 2));
    }

    return 1;
}