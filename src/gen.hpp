#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>
#include "tokenlist.hpp"

namespace sg {

typedef void FuncFloat1(float *dst, const float *src, size_t n);

struct FuncInfo {
	IntVec constTbl;
	uint32_t tmpN; // # of regs used temporary
	FuncInfo()
		: tmpN(0)
	{
	}
};

struct GeneratorBase {
	Index<uint32_t> constIdx_;
	FuncInfo funcInfoTbl[FuncTypeN];
	int simdByte_;
	uint32_t varN_; // # variables
	uint32_t constN_; // # constants
	uint32_t maxFuncTmpN_; // max # of regs used in functions
	uint32_t maxTmpN_; // max # of regs in evaluation
	bool print_;
	GeneratorBase()
		: simdByte_(32 / 8) // one float
		, varN_(0)
		, constN_(0)
		, maxFuncTmpN_(0)
		, maxTmpN_(0)
		, print_(false)
	{
	}
	virtual ~GeneratorBase()
	{
	}
	uint32_t getVarIdxOffset() const { return 0; }
	uint32_t getConstIdxOffset() const { return varN_; }
	uint32_t getFuncTmpOffset() const { return varN_ + constN_; }
	uint32_t getTmpOffset() const { return varN_ + constN_ + maxFuncTmpN_; }
	uint32_t getTotalNum() const { return getTmpOffset() + maxTmpN_; }

	uint32_t getConstIdx(uint32_t u) const
	{
		return getConstIdxOffset() + constIdx_.getIdx(u);
	}
	void updateConstIdx(const sg::TokenList& tl)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		for (size_t i = 0; i < vv.size(); i++) {
			if (vv[i].type == Const) {
				constIdx_.append(vv[i].v);
			}
		}
		maxFuncTmpN_ = 0;
		/*
			append const var in used functions
			set maxFuncTmpN_
		*/
		for (int i = 0; i < sg::FuncTypeN; i++) {
			if (!tl.isUsedFunc(i)) continue;
			const FuncInfo& fi = funcInfoTbl[i];
			const sg::IntVec& constTbl = fi.constTbl;
			for (size_t j = 0; j < constTbl.size(); j++) {
				constIdx_.append(constTbl[j]);
			}
			if (fi.tmpN > maxFuncTmpN_) maxFuncTmpN_ = fi.tmpN;
		}
		varN_ = tl.getVarNum();
		constN_ = constIdx_.size();
		maxTmpN_ = tl.getMaxTmpNum();
		printf("varN=%d constN=%d maxFuncTmpN=%d maxTmpN=%d\n", varN_, constN_, maxFuncTmpN_, maxTmpN_);
	}
	void gen_setConst()
	{
		for (uint32_t i = 0; i < constN_; i++) {
			gen_setInt(getConstIdxOffset() + i, constIdx_.getVal(i));
		}
	}
	virtual void gen_init(const sg::TokenList& tl)
	{
		if (print_) puts("init of GeneratorBase");
		updateConstIdx(tl);
		gen_setConst();
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
		uint32_t pos = getTmpOffset();
		for (size_t i = 0; i < n; i++) {
			const Value& v = vv[i];
			switch (v.type) {
			case Var:
				gen_copy(pos++, getVarIdxOffset() + v.v);
				break;
			case Const:
				gen_copy(pos++, getConstIdxOffset() + constIdx_.getIdx(v.v));
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
		const uint32_t varN = tl.getVarNum();
		gen_init(tl);
		puts("execOneLoop");
		// varN input
		for (uint32_t i = 0; i < varN; i++) {
			gen_loadVar(getVarIdxOffset() + i, i);
		}
		execOneLoop(tl);
		// one output
		gen_saveVar(0, getTmpOffset());
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

