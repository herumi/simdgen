import os
import sys
import platform
from ctypes import *
#from ctypes.util import find_library

g_lib = None

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

class SgCode(Structure):
	def __init__(self):
		self.p = 0
		self.addr = 0
	def init(self):
		if not self.p:
			if not g_lib:
				init()
			self.p = c_void_p(g_lib.SgCreate())
			if self.p == 0:
				raise RuntimeError("SgCreate")
			print(f"self.p={self.p}")
	def destroy(self):
		g_lib.SgDestroy(self.p)
		self.p = 0
	def getFuncAddr(self, src, varName):
		if self.addr:
			raise RuntimeError("already getFunc")
		g_lib.SgGetFuncAddr.restype = CFUNCTYPE(c_void_p, POINTER(c_float), POINTER(c_float), c_size_t)
		self.addr = g_lib.SgGetFuncAddr(self.p, c_char_p(src.encode()), c_char_p(varName.encode()))
		if not self.addr:
			raise RuntimeError("bad param", varName, src)
		return self.addr
	def calc(self, dst, src):
		self.addr(cast(byref(dst), POINTER(c_float)), cast(byref(src), POINTER(c_float)), len(src))

if __name__ == '__main__':
	print("len=", len(sys.argv))
	if len(sys.argv) > 1:
		funcStr = sys.argv[1]
	else:
		funcStr = "log(cosh(x))+100"
	print(f"funcStr={funcStr}")
	sg = SgCode()
	sg.init()
	sg.getFuncAddr(funcStr, "x")

	dst = (c_float * 16)()
	src = (c_float * 16)()
	for i in range(len(dst)):
		src[i] = i * 0.1
	sg.calc(dst, src)
	print("dst")
	import math
	for i in range(len(dst)):
		a = dst[i]
		if funcStr == "log(cosh(x))+100":
			b = math.log(math.cosh(src[i]))+100
			print(a, b, math.fabs(a - b))
		else:
			print(a)

	sg.destroy()


