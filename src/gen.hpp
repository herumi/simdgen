#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>
#include "tokenlist.hpp"

namespace sg {

typedef void FuncFloat1(float *dst, const float *src, size_t n);

struct GeneratorBase {
	Index<uint32_t> constIdx_;
	/*
		[0, varBegin_) ; for const values
		[varBegin_, varEnd_) ; for variables
		[varEnd_, tl.getTmpNum()] ; for tmp reges
	*/
	int regNum_;
	int varBegin_;
	int varEnd_;
	bool print_;
	GeneratorBase()
		: regNum_(0)
		, varBegin_(0)
		, varEnd_(0)
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
	void updateIdx(const sg::TokenList& tl)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		for (size_t i = 0; i < vv.size(); i++) {
			if (vv[i].type == Const) {
				constIdx_.append(vv[i].v);
			}
		}
	}
	virtual void gen_init(const sg::TokenList& tl)
	{
		if (print_) puts("init of GeneratorBase");
		updateIdx(tl);
		const uint32_t constN = constIdx_.size();
		const uint32_t varN = tl.getVarNum();
		for (uint32_t i = 0; i < constN; i++) {
			uint32_t idx = allocReg();
			gen_setInt(idx, constIdx_.getVal(i));
		}
		allocVar(varN);
	}
	virtual void gen_setInt(int dst, uint32_t u)
	{
		if (print_) printf("setImm z%d, %08x\n", dst, u);
	}
	virtual void gen_loadVar(int dst, uint32_t u)
	{
		if (print_) printf("loadVar z%d, [%u]\n", dst, u);
	}
	virtual void gen_saveVar(uint32_t u, int src)
	{
		if (print_) printf("storeVar [%u], z%d\n", u, src);
	}
	virtual void gen_copy(int dst, int src)
	{
		if (print_) printf("copy z%d, z%d\n", dst, src);
	}
	virtual void gen_add(int dst, int src1, int src2)
	{
		if (print_) printf("add z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_sub(int dst, int src1, int src2)
	{
		if (print_) printf("sub z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_mul(int dst, int src1, int src2)
	{
		if (print_) printf("mul z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_div(int dst, int src1, int src2)
	{
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
	template<class TL>
	void execOneLoop(const TL& tl)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		const size_t n = vv.size();
		uint32_t pos = getCurReg();
		for (size_t i = 0; i < n; i++) {
			const Value& v = vv[i];
			switch (v.type) {
			case Var:
				gen_copy(pos++, getVarBeginIdx() + v.v);
				break;
			case Const:
				gen_copy(pos++, constIdx_.getIdx(v.v));
				break;
			case Op:
				assert(pos > 1);
				pos--;
				switch (v.v) {
				case Add: gen_add(pos - 1, pos - 1, pos); break;
				case Sub: gen_sub(pos - 1, pos - 1, pos); break;
				case Mul: gen_mul(pos - 1, pos - 1, pos); break;
				case Div: gen_div(pos - 1, pos - 1, pos); break;
				default:
					throw cybozu::Exception("bad op") << i << v.v;
				}
				break;
			case Func:
				assert(pos > 0);
				switch (v.v) {
				case Inv: gen_inv(pos - 1); break;
				case Exp: gen_exp(pos - 1); break;
				case Log: gen_log(pos - 1); break;
				case Tanh: gen_tanh(pos - 1); break;
				default:
					throw cybozu::Exception("bad func") << i << v.v;
				}
				break;
			default:
				throw cybozu::Exception("bad type") << i << v.type;
			}
		}
	}
	void exec(const sg::TokenList& tl)
	{
		const uint32_t constN = constIdx_.size();
		const uint32_t varN = tl.getVarNum();
		const uint32_t tmpN = tl.getMaxTmpNum();
		printf("#var=%d ", varN);
		printf("#const=%d ", constN);
		printf("#tmp=%d\n", tmpN);
		gen_init(tl);
puts("execOneLoop");
		// varN input
		for (uint32_t i = 0; i < varN; i++) {
			gen_loadVar(getVarBeginIdx() + i, i);
		}
		execOneLoop(tl);
		// one output
		gen_saveVar(0, getCurReg());
		gen_end();
	}
};

struct Printer : GeneratorBase {
	Printer()
	{
		print_ = true;
	}
};

} // sg

