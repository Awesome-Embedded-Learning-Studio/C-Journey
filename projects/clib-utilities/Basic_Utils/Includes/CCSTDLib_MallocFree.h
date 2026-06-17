#pragma once
#ifndef __CCSTD_MALLOC_FREE_H_
#define __CCSTD_MALLOC_FREE_H_


#define CCSTD_MALLOC_ONE(decleared_ptr_name, type)		\
type* decleared_ptr_name = NUL_PTR;						\
do{														\
	type* __CHECKING__ = (type*)malloc(sizeof(type));	\
	SERIOUS_QUIT(__CHECKING__, MALLOC_FAILED);			\
	decleared_ptr_name = __CHECKING__;					\
	memset(decleared_ptr_name, 0, sizeof(type));		\
}while(0)


#define CCSTD_MALLOC_BYTES(decleared_ptr_name, bytes_req) \
CCAny_t decleared_ptr_name;								\
do{														\
	CCBytes_t* __CHECKING__ = (CCBytes_t*)malloc(sizeof(CCBytes_t) * (bytes_req));\
	SERIOUS_QUIT(__CHECKING__, MALLOC_FAILED);			\
	decleared_ptr_name = (CCAny_t)__CHECKING__;			\
	memset(decleared_ptr_name, 0, sizeof(bytes_req));		\
}while(0)


#define CCSTD_MALLOC_TYPES_RAW_ARRAY(decleared_ptr_name, type, N) \
type* decleared_ptr_name = NUL_PTR;							\
do{															\
	type* __CHECKING__ = (type*)malloc(sizeof(type) * (N));	\
	SERIOUS_QUIT(__CHECKING__, MALLOC_FAILED);				\
	decleared_ptr_name = __CHECKING__;						\
	memset(decleared_ptr_name, 0, sizeof(type) * (N));			\
}while(0)

#define CCSTD_SAFE_FREE(pointers) \
do{ \
	free((pointers));\
	(pointers) = NULL;\
}while(0)

#endif
