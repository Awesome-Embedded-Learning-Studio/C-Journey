#pragma once
#ifndef __CCSTDLIB_BASIC_UTILS_H_
#define __CCSTDLIB_BASIC_UTILS_H_
// 空指针守卫宏。
// 旧实现用 if(ERR_LEVEL == EXIT) 在运行期比较 enum(RETURN/EXIT),迫使编译器
// 连 exit(_RET) 这条死分支也做类型检查 —— 当 _RET 为 NUL_PTR/NULL(void*)时,
// 在 GCC 14+ 会触发 -Wint-conversion 硬错误。这里拆成两个独立宏,语义直观、
// 无死分支,且公共 API 不变:
//   DEFAULT_DENY —— 为空则提前 return RET(RET 须匹配函数返回类型:返回指针
//                   用 NUL_PTR/NULL,返回标量用 False / -1 等);
//   SERIOUS_QUIT —— 为空则 exit(RET),RET 须为 int 退出码(如 MALLOC_FAILED)。
#define DEFAULT_DENY(pointers, RET)				\
	do { if(!(pointers)) return (RET); } while(0)

#define SERIOUS_QUIT(pointers, RET)				\
	do { if(!(pointers)) exit(RET); } while(0)

#define TIL_END		(-1)

// Raw String Utils
#define COPY_TO_HEAP(ptr, strings) \
	int __len_of_str = 1;\
	const char* pStr = strings;\
	for(;__len_of_str++, *pStr != '\0';pStr++);\
	CCSTD_MALLOC_BYTES(ptr, __len_of_str); \
	memcpy(ptr, strings, __len_of_str)


#endif
