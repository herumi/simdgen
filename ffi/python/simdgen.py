import os
import sys
import platform
from ctypes import *
#from ctypes.util import find_library

g_lib = None
g_numpy_exists = True
try:
	import numpy
except:
	g_numpy_exists = False

def init():
	global g_lib
	name = platform.system()
	if name == 'Linux':
		libName = 'libsimdgen.so'
	elif name == 'Darwin':
		libName = 'libsimdgen.dylib'
	elif name == 'Windows':
		libName = 'simdgen.dll'
	else:
		raise RuntimeError("not support yet", name)
	g_lib = cdll.LoadLibrary(libName)

def is_c_float(a):
	return isinstance(a, Array) and isinstance(a[0], float)

def convert_ndarray_to_ctypes(a):
	if not g_numpy_exists:
		return 0
	if not isinstance(a[0], numpy.float32):
		raise RuntimeError("bad numpy type")
	return a.ctypes.data_as(POINTER(c_float * len(a))).contents

class SgCode(Structure):
	def __init__(self, src, varName="x"):
		if not g_lib:
			init()
		self.p = c_void_p(g_lib.SgCreate())
		if self.p == 0:
			raise RuntimeError("SgCreate")
		self.reduce = src.find("red_sum") >= 0
		if self.reduce:
			g_lib.SgGetFuncAddr.restype = CFUNCTYPE(c_float, POINTER(c_float), c_size_t)
		else:
			g_lib.SgGetFuncAddr.restype = CFUNCTYPE(c_void_p, POINTER(c_float), POINTER(c_float), c_size_t)
		self.addr = g_lib.SgGetFuncAddr(self.p, c_char_p(src.encode()), c_char_p(varName.encode()))
		if not self.addr:
			raise RuntimeError("bad param", src, varName)
	def destroy(self):
		if g_lib:
			g_lib.SgDestroy(self.p)
		self.p = 0
	def calc(self, p1, p2=None):
		if self.reduce:
			# float func(const float *src, size_t n)
			if g_numpy_exists and isinstance(p1, numpy.ndarray):
				p1 = convert_ndarray_to_ctypes(p1)
			if not is_c_float(p1) or p2:
				raise RuntimeError("bad type", type(p1), type(p2))
			return self.addr(cast(byref(p1), POINTER(c_float)), len(p1))
		else:
			# void func(float *dst, const float *src, size_t n)
			if len(p1) != len(p2):
				raise RuntimeError("bad length", len(p1), len(p2))
			if g_numpy_exists:
				if isinstance(p1, numpy.ndarray):
					p1 = convert_ndarray_to_ctypes(p1)
				if isinstance(p2, numpy.ndarray):
					p2 = convert_ndarray_to_ctypes(p2)
			if not is_c_float(p1) or not is_c_float(p2):
				raise RuntimeError("bad type", type(p1), type(p2))
			self.addr(cast(byref(p1), POINTER(c_float)), cast(byref(p2), POINTER(c_float)), len(p1))
			return 0

if __name__ == '__main__':
	print("len=", len(sys.argv))
	if len(sys.argv) > 1:
		funcStr = sys.argv[1]
	else:
		funcStr = "exp(x*x*0.1+0.3)"
	print(f"funcStr={funcStr}")
	sg = SgCode(funcStr)

	import math
	def f(x):
		return eval(funcStr.replace("red_sum", ""), {"x":x, "exp":math.exp, "log":math.log, "cosh":math.cosh})
	N = 1024
	if sg.reduce:
		a = 0
		src = (c_float * N)()
		for i in range(N):
			src[i] = math.sin(i) * 2.4
			a = a + f(src[i])
		b = sg.calc(src)
		print('ok', a)
		print('my', b)
	else:
		dst = (c_float * N)()
		src = (c_float * N)()
		for i in range(N):
			src[i] = i * 0.1
		sg.calc(dst, src)
		for i in range(min(N, 16)):
			a = f(src[i])
			b = dst[i]
			if math.fabs(a - b) > 1e-5:
				print(f"err a={a} b={b}")
	print("ok")
	sg.destroy()


