# simdgen ; a simple code generator for x64 AVX-512/AArch64 SVE (beta version)

## Abstract

simdgen evaluates a given string and generates a code to apply it to an array.

## Supported OS
- Windows 10 (x64) + Visual Studio
- Linux (x64/A64FX) + gcc/clang

## How to build a library

On Linux
```
make test
```

On Windows
```
mklib ; make a static library
mk -s test/parser_test.cpp
```

## How to use

```
>cat t.c
#include <simdgen/simdgen.h>
#include <stdio.h>
int main(int arg, char *argv[])
{
	const char *src = argc == 1 ? "x+3" : argv[1];
	SgCode *sg = SgCreate();
	SgFuncFloat1* addr = SgGetFuncFloat1(sg, "x", src);
	const size_t N = 40;
	float x[N], y[N];
	for (size_t i = 0; i < N; i++) {
		x[i] = i;
	}
	addr(y, x, N);
	for (size_t i = 0; i < N; i++) {
		printf("%5.3f ", y[i]);
		if (((i + 1) % 8) == 0) putchar('\n');
	}
	printf("\n");
	SgDestroy(sg);
}

gcc t.c -I ./src -L ./lib -lsimdgen
./a.out
# compute x+3
3.000 4.000 5.000 6.000 7.000 8.000 9.000 10.000
11.000 12.000 13.000 14.000 15.000 16.000 17.000 18.000
19.000 20.000 21.000 22.000 23.000 24.000 25.000 26.000
27.000 28.000 29.000 30.000 31.000 32.000 33.000 34.000
35.000 36.000 37.000 38.000 39.000 40.000 41.000 42.000
./a.out "exp(log(x+1)+x)"
2.718 5.437 8.155 10.873 13.591 16.310 19.028 21.746
24.465 27.183 29.901 32.619 35.338 38.056 40.774 43.493
46.211 48.929 51.647 54.366 57.084 59.802 62.520 65.239
67.957 70.675 73.394 76.112 78.830 81.548 84.267 86.985
89.703 92.422 95.140 97.858 100.576 103.295 106.013 108.731
```

## API

### `SGcode* SgCreate()`
- create an instance of Sgcode and return the pointer.
- return null if fail.

### `void SgDestroy(SgCreate *sg)`
- destroy an instance of `sg`.

### `SgFuncFloat1* SgGetFuncFloat1(Sgcode *sg, const char *varName, const char *src)`
- `sg` generates a code accoring to `varName` and src`.
- `varName` is a variable name such as `x`.
- `src` is a single function of `varName` such as `log(exp(x)+1`.

## Support functions

- arithmetic operations (`+`, `-`, `\*`, `/`)
- inv
- exp
- log
- cosh

## License

modified new BSD License
http://opensource.org/licenses/BSD-3-Clause

## History
2021/Jul/29 public

## Author
MITSUNARI Shigeo(herumi@nifty.com)

## Sponsors welcome
[GitHub Sponsor](https://github.com/sponsors/herumi)
