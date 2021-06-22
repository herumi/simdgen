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

#define SG_PARA_MAX_NUM 4
typedef struct {
	int argN;
	int tmpN;
	int tblN;
	int opt[SG_PARAM_MAX_NUM];
	uint32_t n;
} sgPara;

typedef struct sgCode sgCode;

/*
	create sgCode handler
*/
SG_DLL_API int sgCreate(sgCode **code);
/*
	destroy sgCode handler
*/
SG_DLL_API void sgDestroy(sgCode *code);
/*
	append op to code
	dst = op(op1, op2, op3)
	dst, op1, op2, op3 are index of register
	if op1, op2, op3 are -1 and/or para == NULL then these are not used.
*/
SG_DLL_API int sgAppend(sgCode *code, int op, int dst, int op1, int op2, int op3, const sgPara *para);
SG_DLL_API int sgGet(void *const *addr, uint32_t *size, sgCode *code);
/*
	finish code generator and get address and size of code
*/
SG_DLL_API int sgGet(void *const *addr, uint32_t *size, sgCode *code);

#ifdef __cplusplus
}
#endif

