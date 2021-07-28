#pragma once

namespace sg {

struct ExpTbl {
	static const int N = 5;
	float log2;
	float log2_e;
	float coef[N];
	static const int tmpRegN = 2;
	static const int tmpMaskN = 0;
	ExpTbl()
		: log2(std::log(2.0f))
		, log2_e(1.0f / log2)
	{
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
	}
} g_expTbl;

struct LogTbl {
	static const int N = 9;
	uint32_t i127shl23;
	uint32_t x7fffff;
	uint32_t x7fffffff;
	float one;
	float f1div8;
	float log2;
	float f2div3;
	float log1p5;
	float coef[N];
	static const int tmpRegN = 3;
	static const int tmpMaskN = 1;
	LogTbl()
		: i127shl23(127 << 23)
		, x7fffff(0x7fffff)
		, x7fffffff(0x7fffffff)
		, one(1.0)
		, f1div8(1.0f / 8)
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
} g_logTbl;

} // sg

