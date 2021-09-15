#include <simdgen/simdgen.h>
#include <cybozu/test.hpp>
#include <cmath>
#include <float.h>
#include <vector>

typedef std::vector<float> floatVec;
float MAX_E = 1e-6;

void loop(float (*f)(float), float *dst, const float *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		dst[i] = f(src[i]);
	}
}

void bench(const char *msg, float (*f)(float), SgFuncFloat1 g)
{
	printf("%s\n", msg);
	clock_t begin, end;
	const size_t N = 4000;
	const size_t C = 10000;
	static float x[N], y1[N], y2[N];
	for (size_t i = 0; i < N; i++) {
		x[i] = 5 + sin(i / 3.141592) * 2;
	}
	begin = clock();
	for (size_t i = 0; i < C; i++) {
		loop(f, y1, x, N);
	}
	end = clock();
	printf("C  %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
	begin = clock();
	for (size_t i = 0; i < C; i++) {
		g(y2, x, N);
	}
	end = clock();
	printf("sg %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
}

float diff(float x, float y)
{
	float d = std::fabs(x - y);
	return std::fabs(x) < 1e-10 ? d : d / x;
}

void checkRange(float (*f)(float), SgFuncFloat1 g, float begin, float end, float step)
{
	floatVec dst, src;
	float maxe = 0;
	float maxx = 0;
	double ave = 0;
	int aveN = 0;
	size_t n = size_t((end - begin) / step);
	src.resize(n);
	dst.resize(n);
	{
		float x = begin;
		for (size_t i = 0; i < n; i++) {
			src[i] = x;
			x += step;
		}
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
	printf("range [%f, %f] step=%f\n", begin, end, step);
	printf("maxe=%f (x=%f)\n", maxe, maxx);
	printf("ave=%f\n", ave);
	CYBOZU_TEST_ASSERT(ave <= MAX_E);
	CYBOZU_TEST_ASSERT(maxe <= MAX_E);
}

template<size_t N>
void checkTable(float (*f)(float), SgFuncFloat1 g, const float (&tbl)[N])
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
	SgFuncFloat1 addr = SgGetFuncFloat1(sg, "x", "exp(x)");
	if (addr == 0) {
		CYBOZU_TEST_ASSERT(false);
		return;
	}
	const float limitTbl[] = {
		-1000, -100, -80, -5.3, -1, -FLT_MIN, 0, FLT_MIN, 0.5, 1,  5.3, 80 //, 100, 1000, FLT_MAX
	};
	checkTable(expf, addr, limitTbl);
	checkRange(expf, addr, -3, -2, 1e-4);
	checkRange(expf, addr, 0, 1, 1e-4);
	checkRange(expf, addr, 10, 11, 1e-4);
	bench("exp", expf, addr);
	SgDestroy(sg);
}

CYBOZU_TEST_AUTO(log)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1 addr = SgGetFuncFloat1(sg, "x", "log(x)");
	if (addr == 0) {
		CYBOZU_TEST_ASSERT(false);
		return;
	}
	const float limitTbl[] = {
		FLT_MIN, 0.5, 1,  5.3, 80, 100, 1000, FLT_MAX
	};
	checkTable(logf, addr, limitTbl);
	checkRange(logf, addr, FLT_MIN, 1, 1e-4);
	checkRange(logf, addr, 1 - 1e-5, 1 + 1e-5, 1e-7);
	checkRange(logf, addr, 10, 11, 1e-4);
	checkRange(logf, addr, 1000, 1000 + 1, 1e-4);
	bench("log", logf, addr);
	SgDestroy(sg);
}

float inv(float x) { return 1 / x; }

CYBOZU_TEST_AUTO(inv)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1 addr = SgGetFuncFloat1(sg, "x", "inv(x)");
	if (addr == 0) {
		CYBOZU_TEST_ASSERT(false);
		return;
	}
	const float tbl[] = {
		-FLT_MIN, -0.5, -1, -5.3, -80, -100, -1000, -FLT_MAX,
		FLT_MIN, 0.5, 1, 5.3, 80, 100, 1000, FLT_MAX
	};
	checkTable(inv, addr, tbl);
	bench("inv", inv, addr);
	SgDestroy(sg);
}

CYBOZU_TEST_AUTO(cosh)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1 addr = SgGetFuncFloat1(sg, "x", "cosh(x)");
	if (addr == 0) {
		CYBOZU_TEST_ASSERT(false);
		return;
	}
	const float tbl[] = {
		-20, -10, -3, -2, -0.3, 0, 0.4, 1, 2, 3, 10, 20
	};
	checkTable(coshf, addr, tbl);
	bench("cosh", coshf, addr);
	SgDestroy(sg);
}

CYBOZU_TEST_AUTO(red_sum)
{
	SgCode *sg = SgCreate();
	SgFuncFloat1Reduce addr = SgGetFuncFloat1Reduce(sg, "x", "red_sum(x)");
	if (addr == 0) {
		CYBOZU_TEST_ASSERT(false);
		return;
	}
	const size_t N = 40;
	float tbl[N];
	for (size_t i = 0; i < N; i++) {
		tbl[i] = i + 1;
	}
	for (size_t n = 0; n <= N; n++) {
		float r = addr(tbl, n);
		float ok = n * (n + 1) / 2;
		CYBOZU_TEST_EQUAL(r, ok);
	}
	SgDestroy(sg);
}

