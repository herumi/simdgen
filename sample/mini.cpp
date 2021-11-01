/*
	make bin/mini.exe CFLAGS_USER="-DUSE_RED_SUM" for red_sum test
*/

#include <simdgen/simdgen.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

float funcC(float x)
{
	return log(exp(x) + 3);
}

void applyC(float *dst, const float *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		dst[i] = funcC(src[i]);
	}
}

float sum_log_cosh(const float *src, size_t n)
{
	float sum = 0;
	for (size_t i = 0; i < n; i++) {
//printf("src[%zd]=%f cosh=%f log=%f\n", i, src[i], cosh(src[i]), log(cosh(src[i])));
		sum += log(cosh(src[i]));
	}
	return sum;
}

double sum_log_cosh_double(const double *src, size_t n)
{
	double sum = 0;
	for (size_t i = 0; i < n; i++) {
//printf("src[%zd]=%f cosh=%f log=%f\n", i, src[i], cosh(src[i]), log(cosh(src[i])));
		sum += log(cosh(src[i]));
	}
	return sum;
}

int main()
{
	SgCode *sg = SgCreate();
	if (sg == 0) {
		printf("init err\n");
		return 1;
	}
#ifdef USE_RED_SUM
	const char *src = "red_sum(log(cosh(x)))";
	SgFuncFloat1Reduce addr = SgGetFuncFloat1Reduce(sg, "x", src);
#else
	const char *src = "log(exp(x) + 3)";
	SgFuncFloat1 addr = SgGetFuncFloat1(sg, "x", src);
#endif
	if (addr == 0) {
		return 1;
	}
	clock_t begin, end;
	const size_t N = 1000;
	const size_t C = 10000;
	static float x[N];
#ifdef USE_RED_SUM
	static double xd[N];
#else
	static float y1[N], y2[N];
#endif
	for (size_t i = 0; i < N; i++) {
#ifdef USE_RED_SUM
		xd[i] = (abs(sin(i * 0.1)) * 26) - 4; /* [-4, 22] */
		x[i] = (abs(sin(i * 0.1f)) * 26) - 4; /* [-4, 22] */
#else
		x[i] = ((int)i - 100) / (N * 0.1);
#endif
	}
	begin = clock();
#ifdef USE_RED_SUM
	float sum1 = 0;
	for (size_t i = 0; i < C; i++) {
		sum1 += sum_log_cosh(x, N);
	}
#else
	for (size_t i = 0; i < C; i++) {
		applyC(y1, x, N);
	}
#endif
	end = clock();
	printf("C  %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
	begin = clock();
#ifdef USE_RED_SUM
	float sum2 = 0;
	for (size_t i = 0; i < C; i++) {
		sum2 += addr(x, N);
	}
#else
	for (size_t i = 0; i < C; i++) {
		addr(y2, x, N);
	}
#endif
	end = clock();
	printf("sg %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
#ifndef USE_RED_SUM
	for (size_t i = 0; i < (N >= 20 ? 20 : N); i++) {
		printf("%zd x=%f C=%f sg=%f diff=%e\n", i, x[i], y1[i], y2[i], fabs(y1[i] - y2[i]));
	}
#endif
#ifdef USE_RED_SUM
	double sum3 = sum_log_cosh_double(xd, N);
	printf("sum1=%f sum2=%f sum3=%f\n", sum1/C, sum2/C, sum3);
#endif
	SgDestroy(sg);
}
