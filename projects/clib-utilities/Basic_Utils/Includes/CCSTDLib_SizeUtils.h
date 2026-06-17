#pragma once
#ifndef __CCSTDLIB_SIZE_UTILS_H_
#define __CCSTDLIB_SIZE_UTILS_H_

// DENY_INVALID_SIZE
#define IS_INVALID_SIZE(size)	( (size >= 0) ) 

// Sizes
#define TYPE_SIZE(type) \
	type##_SIZE = sizeof(type) \

#define WITH_U_TYPE_SIZE(type) \
	type##_SIZE = sizeof(type), \
	U##type##_SIZE = sizeof(unsigned type)

enum DEF_TYPE_SIZE {
	WITH_U_TYPE_SIZE(int),
	WITH_U_TYPE_SIZE(short),
	WITH_U_TYPE_SIZE(char),
	TYPE_SIZE(float),
	TYPE_SIZE(double)
};

#endif
