#pragma once
/**
	@file
	@brief C interface of simggen
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/

#include <stdint.h> // for uint32_t

#if defined(_MSC_VER)
	#ifdef SG_DONT_EXPORT
		#define SG_DLL_API
	#else
		#ifdef SG_DLL_EXPORT
			#define SG_DLL_API __declspec(dllexport)
		#else
			#define SG_DLL_API __declspec(dllimport)
		#endif
	#endif
	#ifndef SG_NO_AUTOLINK
		#pragma comment(lib, "simdgen.lib")
	#endif
#else
	#define SG_DLL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SG_NOERR 0

#define SG_OP_COPY 0
#define SG_OP_ADD 1
#define SG_OP_SUB 2
#define SG_OP_MUL 3
#define SG_OP_DIV 4
#define SG_OP_EXP 5
#define SG_OP_LOG 6

typedef struct {
	int unrollN;
} SgPara;

typedef struct SgCode SgCode;

#define SG_FUNC_SRC_MAX_NUM 5
typedef struct {
	void *dst;
	const void *src[SG_FUNC_SRC_MAX_NUM];
	uint32_t srcN;
} FuncArg;

typedef void (*sgFuncType)(const FuncArg *arg, uint32_t n);

/*
	create SgCode handler
*/
SG_DLL_API int SgCreate(SgCode **code);
/*
	destroy SgCode handler
*/
SG_DLL_API void SgDestroy(SgCode *code);
/*
	parse src
*/
SG_DLL_API void SgParse(SgCode *code, FuncArg *arg, const char *src, const SgPara *para);
/*
	finish code generator and get address and size of code
*/
SG_DLL_API int SgGet(void *const *addr, uint32_t *size, SgCode *code);

#ifdef __cplusplus
}
#endif

