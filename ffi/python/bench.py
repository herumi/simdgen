import simdgen
import numpy
import timeit
import math

N = 8192
C = 1000
a = numpy.arange(N, dtype='float32')
b = numpy.arange(N, dtype='float32')

for i in range(N):
	a[i] = math.sin(i * 0.1) * 3

src = "log(cosh(x))"
sg = simdgen.SgCode(src)
print("func", src)
print("simdgen", timeit.timeit(lambda: sg.calc(b, a), number=C))
print("numpy  ", timeit.timeit(lambda: numpy.log(numpy.cosh(a)), number=C))

sg.destroy()

print("result top")
c = numpy.log(numpy.cosh(a))
for i in range(10):
	print(b[i], c[i], math.fabs(b[i] - c[i]))

src = "red_sum(log(cosh(x)))"
print("func", src)
sg = simdgen.SgCode(src)
print("simdgen", timeit.timeit(lambda: sg.calc(a), number=C))
print("numpy  ", timeit.timeit(lambda: numpy.sum(numpy.log(numpy.cosh(a))), number=C))

print("result", sg.calc(a), numpy.sum(numpy.log(numpy.cosh(a))))

sg.destroy()
