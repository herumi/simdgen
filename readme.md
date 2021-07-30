# simdgen ; a simple code generator for x64 AVX-512/AArch64 SVE

## Abstract

simdgen evaluates a given string and generates a code to apply it to an array.

## Supported OS
- Windows 10 (x64) + Visual Studio
- Linux (x64/A64FX) + gcc/clang

## How to build

On Linux
```
make test
```

On Windows
```
mklib ; make a static library
mk -s test/parser_test.cpp
```

## License

modified new BSD License
http://opensource.org/licenses/BSD-3-Clause

## History

## Author
MITSUNARI Shigeo(herumi@nifty.com)

## Sponsors welcome
[GitHub Sponsor](https://github.com/sponsors/herumi)
