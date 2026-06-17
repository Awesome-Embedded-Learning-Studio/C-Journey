#pragma once
#ifndef __CCSTDLIBS_TYPES__
#define __CCSTDLIBS_TYPES__
#include "CCSTDLibs_MyCompiles.h"

#ifdef CCSTD_LANGUAGE_C
typedef int				CCBOOL_t;			// CCBool_t
typedef unsigned int	CCSize_t;			// CCSize_t
typedef void*			CCAny_t;			// CCAny_t
typedef unsigned char	CCBytes_t;			// CCBytes_t	
#define NUL_PTR			NULL				// NUL_PTR
#define True			(1)
#define False			(0)

// Using In find
typedef enum _CCSTD_CmpRes {
	CCSTD_CmpRes_SAME		= 0,
	CCSTD_CmpRes_BIGGER		= 1,
	CCSTD_CmpRes_SMALLER	= -1
}CCSTD_CmpRes;

typedef CCSTD_CmpRes(*CCSTD_CmpFuncType)(void*, void*);
typedef long			CCSTD_Index_t;
#else	
CCSTD_NAMESPACE_BEGIN
#include <cstddef>
using	CCBOOL_t		= bool;				// CCBool_t
using	CCSize_t		= unsigned int;		// CCSize_t
using	CCAny_t			= void*;			// CCAny_t
using	CCBytes_t		= unsigned char;	// CCBytes_t
const	nullptr_t		NUL_PTR = nullptr;	// NUL_PTR
const	bool			True = true;
const	bool			False = false;
enum class CCSTD_CmpRes {
	CCSTD_CmpRes_SAME = 0,
	CCSTD_CmpRes_BIGGER = 1,
	CCSTD_CmpRes_SMALLER = -1
};

using CCSTD_CmpFuncType		= CCSTD_CmpRes(*)(void*, void*);
using CCSTD_Index_t			= long;
CCSTD_NAMESPACE_END
#endif

#endif


