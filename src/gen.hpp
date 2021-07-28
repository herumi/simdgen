#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>
#include <cmath>
#include "tokenlist.hpp"
#include "const.hpp"

namespace sg {

struct FuncInfo {
	IntVec constTbl;
	int tmpRegN; // # of regs used temporary
	int tmpMaskN; // # of mask regs used temporary
	FuncInfo()
		: tmpRegN(0)
		, tmpMaskN(0)
	{
	}
};

struct IndexRange {
	int offset_;
	int max_;
	int cur_;
	IndexRange()
		: offset_(0)
		, max_(0)
		, cur_(0)
	{
	}
	int alloc()
	{
		if (cur_ == max_) throw cybozu::Exception("too alloc") << max_;
		return offset_ + cur_++;
	}
	int getCur() const { return cur_; }
	void setCur(int cur) { cur_ = cur; }
	void updateMax(int max)
	{
		if (max > max_) max_ = max;
	}
	int getMax() const { return max_; }
	void setOffset(int offset) { offset_ = offset; }
};

struct IndexRangeManager {
	IndexRange ir_;
	int cur_;
	IndexRangeManager(IndexRange& ir)
		 : ir_(ir)
		 , cur_(ir_.getCur())
	{
	}
	~IndexRangeManager()
	{
		ir_.setCur(cur_);
	}
	int allocIdx()
	{
		return ir_.alloc();
	}
};

struct GeneratorBase {
	Index<uint32_t> constIdx_;
	FuncInfo funcInfoTbl[FuncTypeN];
	int simdByte_;
	uint32_t varN_; // # variables
	uint32_t constN_; // # constants
	IndexRange funcTmpReg_;
	IndexRange funcTmpMask_;
	uint32_t maxTmpN_; // max # of regs in evaluation
	uint32_t curMaskTmpIdx_;
	bool print_;
	GeneratorBase()
		: simdByte_(32 / 8) // one float
		, varN_(0)
		, constN_(0)
		, maxTmpN_(0)
		, curMaskTmpIdx_(0)
		, print_(false)
	{
	}
	virtual ~GeneratorBase()
	{
	}
	int getVarIdxOffset() const { return 0; }
	int getVarIdx(int i) const { return getVarIdxOffset() + i; }
	uint32_t getConstIdxOffset() const { return varN_; }
	uint32_t getTmpOffset() const { return varN_ + constN_ + funcTmpReg_.getMax(); }
	int getTmpIdx(int i) const { return getTmpOffset() + i; }
	uint32_t getTotalNum() const { return getTmpOffset() + maxTmpN_; }

	uint32_t getConstIdx(uint32_t u) const
	{
		return getConstIdxOffset() + constIdx_.getIdx(u);
	}
	int getFloatIdx(float f) const
	{
		return getConstIdx(f2u(f));
	}
	void updateConstIdx(const sg::TokenList& tl)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		for (size_t i = 0; i < vv.size(); i++) {
			if (vv[i].type == Const) {
				constIdx_.append(vv[i].v);
			}
		}
		funcTmpReg_.max_ = 0;
		/*
			append const var in used functions
			set funcTmpReg_
		*/
		for (int i = 0; i < sg::FuncTypeN; i++) {
			if (!tl.isUsedFunc(i)) continue;
			const FuncInfo& fi = funcInfoTbl[i];
			const sg::IntVec& constTbl = fi.constTbl;
			for (size_t j = 0; j < constTbl.size(); j++) {
				constIdx_.append(constTbl[j]);
			}
			funcTmpReg_.updateMax(fi.tmpRegN);
			funcTmpMask_.updateMax(fi.tmpMaskN);
		}
		varN_ = tl.getVarNum();
		constN_ = constIdx_.size();
		funcTmpReg_.setOffset(varN_ + constN_);
		funcTmpMask_.setOffset(2); // mask0 and mask1 are reserved
		maxTmpN_ = tl.getMaxTmpNum();
		printf("varN=%d constN=%d funcTmpReg.max=%d maxTmpN=%d\n", varN_, constN_, funcTmpReg_.getMax(), maxTmpN_);
	}
	void gen_setConst()
	{
		for (uint32_t i = 0; i < constN_; i++) {
			gen_setInt(getConstIdxOffset() + i, constIdx_.getVal(i));
		}
	}
	virtual void exec(const sg::TokenList& tl)
	{
		if (print_) puts("init of GeneratorBase");
		updateConstIdx(tl);
		gen_setConst();
		puts("execOneLoop");
		for (uint32_t i = 0; i < varN_; i++) {
			gen_loadVar(getVarIdx(i), i);
		}
		execOneLoop(tl);
		// one output
		gen_saveVar(0, getTmpOffset());
		gen_end();
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
	void setFuncInfoTbl()
	{
		for (size_t i = 0; i < FuncTypeN; i++) {
			FuncInfo& fi = funcInfoTbl[i];
			switch (i) {
			case Inv:
				fi.constTbl.push_back(f2u(1.0));
				fi.tmpRegN = 1;
				fi.tmpMaskN = 0;
				break;
			case Exp:
				fi.constTbl.push_back(f2u(g_expTbl.log2));
				fi.constTbl.push_back(f2u(g_expTbl.log2_e));
				for (int j = 0; j < ExpTbl::N; j++) {
					fi.constTbl.push_back(f2u(g_expTbl.coef[j]));
				}
				fi.tmpRegN = ExpTbl::tmpRegN;
				fi.tmpMaskN = ExpTbl::tmpMaskN;
				break;
			case Log:
				fi.constTbl.push_back(g_logTbl.i127shl23);
				fi.constTbl.push_back(g_logTbl.x7fffff);
				fi.constTbl.push_back(g_logTbl.x7fffffff);
				fi.constTbl.push_back(f2u(g_logTbl.one));
				fi.constTbl.push_back(f2u(g_logTbl.f1div8));
				fi.constTbl.push_back(f2u(g_logTbl.log2));
				fi.constTbl.push_back(f2u(g_logTbl.f2div3));
				fi.constTbl.push_back(f2u(g_logTbl.log1p5));
				for (int j = 0; j < LogTbl::N; j++) {
					fi.constTbl.push_back(f2u(g_logTbl.coef[j]));
				}
				fi.tmpRegN = LogTbl::tmpRegN;
				fi.tmpMaskN = LogTbl::tmpMaskN;
			default:
				break;
			}
		}
	}
};

struct Printer : GeneratorBase {
	Printer()
	{
		print_ = true;
	}
};

} // sg

