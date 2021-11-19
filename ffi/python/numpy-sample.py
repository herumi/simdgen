import simdgen
import numpy

a = numpy.array([1,2,3,4,5,6,7], dtype='float32')
b = numpy.arange(len(a), dtype='float32')

sg = simdgen.SgCode("x+0.5")

print(a)
sg.calc(b, a)
print(b)

sg.destroy()

sg = simdgen.SgCode("red_sum(x)")
print("sum=", sg.calc(b))
