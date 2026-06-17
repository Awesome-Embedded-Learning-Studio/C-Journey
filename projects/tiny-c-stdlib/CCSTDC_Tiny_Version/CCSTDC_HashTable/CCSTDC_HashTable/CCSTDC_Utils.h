#pragma once

typedef enum {
	CCSTDC_True = 1,
	CCSTDC_False = 2
}CCSTDC_BOOL;

typedef unsigned int CCSTDC_Bool;

typedef enum {
	CCSTDC_Move = 1,
	CCSTDC_Copy = 2
}CCSTDC_Moving_Choice;

typedef unsigned int CCSTDC_Moving_Selection;

typedef enum {
	CCSTDC_NULL_INPUT = 1,
	CCSTDC_INVALID_INPUT = 2,
	CCSTDC_FAILED_MALLOC = 3,
	CCSTDC_INVALID_FREE = 4
}CCSTDC_ErrorCode;

#define CCSTDC_Error_Throw(ErrorCode) printf("Error Occured!");exit(ErrorCode)
#define CCSTDC_LOG(MSG) printf("%s",MSG)

#define SLASH printf("\n")

