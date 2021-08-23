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

int main()
{
	const char *src = "log(exp(x) + 3)";
	SgCode *sg = SgCreate();
	SgFuncFloat1* addr = SgGetFuncFloat1(sg, "x", src);
	if (addr == 0) {
		return 1;
	}
	clock_t begin, end;
	const size_t N = 4000;
	const size_t C = 10000;
	static float x[N], y1[N], y2[N];
	for (size_t i = 0; i < N; i++) {
		x[i] = ((int)i - 100) / (N * 0.1);
	}
	begin = clock();
	for (size_t i = 0; i < C; i++) {
		applyC(y1, x, N);
	}
	end = clock();
	printf("C  %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
	begin = clock();
	for (size_t i = 0; i < C; i++) {
		addr(y2, x, N);
	}
	end = clock();
	printf("sg %6.2f usec\n", (end - begin) / (double)CLOCKS_PER_SEC / C * 1e6);
	for (size_t i = 0; i < 20; i++) {
		printf("%zd x=%f C=%f sg=%f diff=%e\n", i, x[i], y1[i], y2[i], fabs(y1[i] - y2[i]));
	}
	SgDestroy(sg);
}
