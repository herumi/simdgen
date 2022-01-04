# `exp(x)`

## Approximate calculation of `exp(x)`

```
e^x = 2^(x log_2(e))
```
Let y = x log_2(e) and split y into an integer and a fraction.

```
y = n + a where n is an integer and |a| <= 0.5
```
We can get `e^x = e^ (n + a) = 2^n 2^a`.
`2^n` can be easily computed then we consider `2^a`.

```
2^a = e^(a log(2)) = e^b where b = a log(2)
```
`|a| <= 0.5` and `log(2) = 0.693...` then `|b| = |a log(2)| <= 0.346`.

Here, we use a Maclaurin series of `exp(x) = 1 + x + x^2/2 + x^3/6 + ...`.

The term of degree 6 is about `0.346^6/6! = 2.4e-6`, which is close to the resolution of `float`.

```
e^b = 1 + b + b^2/2! + b^3/3! + b^4/4! + b^5/5!
```
Then we take the following algorithm:

```
input : x
output : e^x

1. x = x * log_2(e)
2. n = round(x)
3. a = x - n
4. b = a * log(2)
5. z = 1 + b(1 + b(1/2! + b(1/3! + b(1/4! + b/5!))))
6. return z * 2^n
```

## AVX-512 implementation

Let zm0 be an input value.

```
vmulps(zm0, log2_e); // x *= log_2(e)
vrndscaleps(zm1, zm0, 0); // n = round(x)
vsubps(zm0, zm1); // a = x - n
vmulps(zm0, log2); // a *= log2
```

Compute Maclaurin series.

`vfmadd213ps(x, y, z)` means `x = x * y + z`.

```
vmovaps(zm2, expCoeff[4]); // 1/5!
vfmadd213ps(zm2, zm0, expCoeff[3]); // b * (1/5!) + 1/4!
vfmadd213ps(zm2, zm0, expCoeff[2]); // b(b/5! + 1/4!) + 1/3!
vfmadd213ps(zm2, zm0, expCoeff[1]); // b(b(b/5! + 1/4!) + 1/3!) + 1/2!
vfmadd213ps(zm2, zm0, expCoeff[0]); // b(b(b(b/5! + 1/4!) + 1/3!) + 1/2!) + 1
vfmadd213ps(zm2, zm0, expCoeff[0]); // b(b(b(b(b/5! + 1/4!) + 1/3!) + 1/2!) + 1) + 1
```

`vscalefps` is an instruction for only AVX-512.

```
vscalefps(zm0, zm2, zm1); // zm2 * 2^zm1
```
