#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>

namespace sg {

typedef void FuncFloat1(float *dst, const float *src, size_t n);

struct GeneratorBase {
	int regNum_;
	int varBegin_;
	int varEnd_;
	int maxDst_;
	bool print_;
	GeneratorBase()
		: regNum_(0)
		, varBegin_(0)
		, varEnd_(0)
		, maxDst_(0)
		, print_(false)
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
	void allocVar(size_t n)
	{
		varBegin_ = regNum_;
		varEnd_ = regNum_ + n;
		regNum_ += n;
	}
	int getVarBeginIdx() const { return varBegin_; }
	int getVarEndIdx() const { return varEnd_; }
	int getCurReg() const { return regNum_; }
	int getMaxDst() const { return maxDst_; }

	virtual void gen_init()
	{
		if (print_) puts("init");
	}
	virtual void gen_setInt(int dst, uint32_t u)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("setImm z%d, %08x\n", dst, u);
	}
	virtual void gen_loadVar(int dst, uint32_t u)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("loadVar z%d, [%u]\n", dst, u);
	}
	virtual void gen_saveVar(uint32_t u, int src)
	{
		if (src > maxDst_) maxDst_ = src;
		if (print_) printf("storeVar [%u], z%d\n", u, src);
	}
	virtual void gen_copy(int dst, int src)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("copy z%d, z%d\n", dst, src);
	}
	virtual void gen_add(int dst, int src1, int src2)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("add z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_sub(int dst, int src1, int src2)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("sub z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_mul(int dst, int src1, int src2)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("mul z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_div(int dst, int src1, int src2)
	{
		if (dst > maxDst_) maxDst_ = dst;
		if (print_) printf("div z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_inv(int inout)
	{
		if (print_) printf("inv z%d\n", inout);
	}
	virtual void gen_exp(int inout)
	{
		if (print_) printf("exp z%d\n", inout);
	}
	virtual void gen_log(int inout)
	{
		if (print_) printf("log z%d\n", inout);
	}
	virtual void gen_tanh(int inout)
	{
		if (print_) printf("tanh z%d\n", inout);
	}
	virtual void gen_end()
	{
		if (print_) puts("end");
	}
};

struct Printer : GeneratorBase {
	Printer()
	{
		print_ = true;
	}
};


} // sg

