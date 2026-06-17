#pragma once

#ifndef __CCSTD_CMP_UTILS_H_
#define __CCSTD_CMP_UTILS_H_

#define CCSTD_CMP_TYPE_DECLEAR(typeName, type) \
	CCSTD_CmpRes compare##typeName(const type* _elem1, const type* _elem2)

#define CCSTD_CMP_WITH_U_TYPE_DECLEAR(typeName, type) \
	CCSTD_CmpRes compare##typeName(const type* _elem1, const type* _elem2);\
	CCSTD_CmpRes compareUnsigned##typeName(const unsigned type* _elem1, const unsigned type* _elem2)

CCSTD_CMP_WITH_U_TYPE_DECLEAR(Int, int);
CCSTD_CMP_WITH_U_TYPE_DECLEAR(Short, short);
CCSTD_CMP_WITH_U_TYPE_DECLEAR(Char, char);
CCSTD_CMP_TYPE_DECLEAR(Float, float);
CCSTD_CMP_TYPE_DECLEAR(Double, double);

#endif
