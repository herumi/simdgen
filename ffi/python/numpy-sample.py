import simdgen
import numpy
import ctypes

a = numpy.array([1,2,3,4,5,6,7], dtype='float32')
b = numpy.arange(len(a), dtype='float32')

sg = simdgen.SgCode("exp(x)+4")

aa = a.ctypes.data_as(ctypes.POINTER(ctypes.c_float * len(a))).contents
bb = b.ctypes.data_as(ctypes.POINTER(ctypes.c_float * len(b))).contents
print(a)
sg.calc(bb, aa)
print(b)
