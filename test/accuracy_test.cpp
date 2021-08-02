#include <simdgen/simdgen.h>
#include <cybozu/test.hpp>
#include <cmath>
#include <float.h>
#include <vector>

typedef std::vector<float> floatVec;
const float MAX_E = 1e-6;

float diff(float x, float y)
{
	float d = std::fabs(x - y);
	return std::fabs(x) < 1e-10 ? d : d / x;
}

void checkRange(float (*f)(float), SgFuncFloat1 *g, float begin, float end, float step)
{
	floatVec dst, src;
	float maxe = 0;
	float maxx = 0;
	double ave = 0;
	int aveN = 0;
	size_t n = size_t((end - begin) / step);
	src.resize(n);
	dst.resize(n);
	for (size_t i = 0; i < n; i++) {
		src[i] = begin;
		begin += step;
	}
	g(&dst[0], &src[0], n);

	for (size_t i = 0; i < n; i++) {
		float x = src[i];
		float y0 = f(x);
		float y1 = dst[i];
		float e;
		e = diff(y0, y1);
		if (!(e <= MAX_E)) {
			printf("err x=%e y0=%e y1=%e e=%e\n", x, y0, y1, e);
		}
		if (e > maxe) {
			maxe = e;
			maxx = x;
		}
		ave += e;
		aveN++;
	}
	ave /= aveN;
	printf("range [%.2e, %.2e] step=%.2e\n", begin, end, step);
	printf("maxe=%e (x=%e)\n", maxe, maxx);
	printf("ave=%e\n", ave);
	CYBOZU_TEST_ASSERT(ave <= MAX_E);
	CYBOZU_TEST_ASSERT(maxe <= MAX_E);
}

template<size_t N>
void checkTable(float (*f)(float), SgFuncFloat1 *g, const float (&tbl)[N])
{
	float dst[N];
	g(dst, tbl, N);
	for (size_t i = 0; i < N; i++) {
		float x = tbl[i];
		float y0 = f(x);
		float y1 = dst[i];
		float e;
		e = diff(y0, y1);
		CYBOZU_TEST_ASSERT(e <= MAX_E);
		if (!(e < MAX_E)) {
			printf("err x=%e y0=%e y1=%e e=%e\n", x, y0, y1, e);
		}
	}
}

CYBOZU_TEST_AUTO(exp)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1 *addr = SgGetFuncFloat1(sg, "x", "exp(x)");
	const float limitTbl[] = {
		-FLT_MAX, -1000, -100, -80, -5.3, -1, -FLT_MIN, 0, FLT_MIN, 0.5, 1,  5.3, 80 //, 100, 1000, FLT_MAX
	};
	checkTable(expf, addr, limitTbl);
	checkRange(expf, addr, -3, -2, 1e-4);
	checkRange(expf, addr, 0, 1, 1e-4);
	checkRange(expf, addr, 10, 11, 1e-4);
	SgDestroy(sg);
}

CYBOZU_TEST_AUTO(log)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1 *addr = SgGetFuncFloat1(sg, "x", "log(x)");
	const float limitTbl[] = {
		FLT_MIN, 0.5, 1,  5.3, 80, 100, 1000, FLT_MAX
	};
	checkTable(logf, addr, limitTbl);
	checkRange(logf, addr, FLT_MIN, 1, 1e-4);
	checkRange(logf, addr, 1 - 1e-5, 1 + 1e-5, 1e-7);
	checkRange(logf, addr, 10, 11, 1e-4);
	checkRange(logf, addr, 1000, 1000 + 1, 1e-4);
	SgDestroy(sg);
}

