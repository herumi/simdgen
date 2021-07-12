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
//		#pragma comment(lib, "simdgen.lib")
	#endif
#else
	#define SG_DLL_API
#endif

#if !(defined(SG_AVX) || defined(XG_SVE))
	#if defined(_WIN64) || defined(__x86_64__)
		#define SG_X64
	#else
		#define SG_AARCH64
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SgCode SgCode;
/*
	create SgCode handler
*/
SG_DLL_API int SgCreate(SgCode **code);
/*
	destroy SgCode handler
*/
SG_DLL_API void SgDestroy(SgCode *code);

#ifdef __cplusplus
}
#endif

