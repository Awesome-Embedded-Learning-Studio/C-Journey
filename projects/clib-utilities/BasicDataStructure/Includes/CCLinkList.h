#pragma once
#ifndef __CCLinkList_H_
#define __CCLinkList_H_
#include "DataStructBasicHeader.h"

typedef struct __CCLinkList
{
	CCAny_t data;
	struct __CCSimpleForwardLinkList* next;
	struct __CCSimpleForwardLinkList* prev;
}CCLinkList;




#endif