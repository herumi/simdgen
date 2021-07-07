#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>

namespace sg {

typedef void FuncFloat1(float *dst, const float *src, size_t n);

struct GeneratorBase {
	int regNum_;
	GeneratorBase()
		: regNum_(0)
	{
	}
	virtual ~GeneratorBase()
	{
	}
	virtual void reset()
	{
		regNum_ = 0;
	}
	int allocReg()
	{
		return regNum_++;
	}
	int getCurReg() const { return regNum_; }
};

struct Printer : GeneratorBase {
	void gen_init()
	{
		puts("init");
	}
	void gen_setInt(int dst, uint32_t u)
	{
		printf("setImm z%d, %08x\n", dst, u);
	}
	void gen_loadVar(int dst, uint32_t u)
	{
		printf("loadVar z%d, [%u]\n", dst, u);
	}
	void gen_saveVar(uint32_t u, int src)
	{
		printf("storeVar [%u], z%d\n", u, src);
	}
	void gen_copy(int dst, int src)
	{
		printf("copy z%d, z%d\n", dst, src);
	}
	void gen_add(int dst, int src1, int src2)
	{
		printf("add z%d, z%d, z%d\n", dst, src1, src2);
	}
	void gen_sub(int dst, int src1, int src2)
	{
		printf("sub z%d, z%d, z%d\n", dst, src1, src2);
	}
	void gen_mul(int dst, int src1, int src2)
	{
		printf("mul z%d, z%d, z%d\n", dst, src1, src2);
	}
	void gen_div(int dst, int src1, int src2)
	{
		printf("div z%d, z%d, z%d\n", dst, src1, src2);
	}
	void gen_inv(int inout)
	{
		printf("inv z%d\n", inout);
	}
	void gen_exp(int inout)
	{
		printf("exp z%d\n", inout);
	}
	void gen_log(int inout)
	{
		printf("log z%d\n", inout);
	}
	void gen_tanh(int inout)
	{
		printf("tanh z%d\n", inout);
	}
	void gen_end()
	{
		puts("end");
	}
};


} // sg

