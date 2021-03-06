#pragma once
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4305)
#endif

namespace sg {

struct ExpTbl {
	float log2_e;
#ifdef SG_X64
	static const int N = 5;
	float coef[N];
	float log2;
#else
	uint32_t not_mask17;
	float one;
	float coeff1;
	float coeff2;
#endif
	static const int tmpRegN = 2;
	static const int tmpMaskN = 0;
	ExpTbl()
		: log2_e(1.0f / std::log(2.0f))
#ifdef SG_X64
		, log2(std::log(2.0f))
#else
		, not_mask17(~((1u << 17) - 1))
		, one(1.0f)
		, coeff1(0.6931473921)
		, coeff2(0.2413862043)
#endif
	{
#ifdef SG_X64
		const uint32_t tbl[N] = {
			0x3f800000,
			0x3effff12,
			0x3e2aaa56,
			0x3d2b89cc,
			0x3c091331,
		};
		for (int i = 0; i < N; i++) {
			coef[i] = u2f(tbl[i]);
		}
#endif
	}
};

struct LogTbl {
	static const int N = 9;
	uint32_t i127shl23;
	uint32_t x7fffff;
#ifdef SG_X64
	uint32_t x7fffffff;
	float one;
	float f1div8;
#endif
	float log2;
	float f2div3;
	float log1p5;
	float coef[N];
#ifdef SG_X64
	static const int tmpRegN = 3;
#else
	static const int tmpRegN = 4;
#endif
	static const int tmpMaskN = 1;
	LogTbl()
		: i127shl23(127 << 23)
		, x7fffff(0x7fffff)
#ifdef SG_X64
		, x7fffffff(0x7fffffff)
		, one(1.0)
		, f1div8(1.0f / 8)
#endif
		, log2(std::log(2.0f))
		, f2div3(2.0f / 3)
		, log1p5(std::log(1.5f))
	{
		const float tbl[N] = {
			 1.0, // must be 1
			-0.49999985195974875681242,
			 0.33333220526061677705782,
			-0.25004206220486390058000,
			 0.20010985747510067100077,
			-0.16481566812093889672203,
			 0.13988269735629330763020,
			-0.15049504706005165294002,
			 0.14095711402233803479921,
		};
		for (int i = 0; i < N; i++) {
			coef[i] = tbl[i];
		}
	}
};

extern const ExpTbl g_expTbl;
extern const LogTbl g_logTbl;
} // sg

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
