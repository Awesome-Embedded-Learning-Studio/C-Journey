#include "CCSTDLib_Utils.h"

#define REGISTER_CMP_TYPE_IMPL(typeName, type) \
CCSTD_CmpRes compare##typeName(const type* _elem1, const type* _elem2){	\
	if(*_elem1 == *_elem2)									\
		return CCSTD_CmpRes_SAME;							\
	else if(*_elem1 > *_elem2)								\
		return CCSTD_CmpRes_BIGGER;							\
	return CCSTD_CmpRes_SMALLER;							\
}

#define REGISTER_CMP_WITH_U_TYPE_IMPL(typeName, type) \
CCSTD_CmpRes compare##typeName(const type* _elem1, const type* _elem2){	\
	if(*_elem1 == *_elem2)									\
		return CCSTD_CmpRes_SAME;							\
	else if(*_elem1 > *_elem2)								\
		return CCSTD_CmpRes_BIGGER;							\
	return CCSTD_CmpRes_SMALLER;							\
}\
\
CCSTD_CmpRes compareUnsigned##typeName(const unsigned type* _elem1, const unsigned type* _elem2){	\
	if(*_elem1 == *_elem2)									\
		return CCSTD_CmpRes_SAME;							\
	else if(*_elem1 > *_elem2)								\
		return CCSTD_CmpRes_BIGGER;							\
	return CCSTD_CmpRes_SMALLER;							\
}


REGISTER_CMP_WITH_U_TYPE_IMPL(Int, int)
REGISTER_CMP_WITH_U_TYPE_IMPL(Char, char)
REGISTER_CMP_WITH_U_TYPE_IMPL(Short, short)
REGISTER_CMP_TYPE_IMPL(Double, double)
REGISTER_CMP_TYPE_IMPL(Float, float)




