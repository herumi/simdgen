#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>
#include <cmath>
#include "tokenlist.hpp"
#include "const.hpp"
#include <cybozu/atoi.hpp>

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
	int n_;
	int cur_;
	IndexRange()
		: offset_(0)
		, n_(0)
		, cur_(0)
	{
	}
	int alloc()
	{
		if (cur_ == n_) throw cybozu::Exception("too alloc") << n_;
		return offset_ + cur_++;
	}
	int getCur() const { return cur_; }
	void setCur(int cur) { cur_ = cur; }
	void setSize(int n) { n_ = n; }
	int getSize() const { return n_; }
	int getMax() const { return offset_ + n_; }
	void setOffset(int offset) { offset_ = offset; }
};

struct IndexRangeManager {
	IndexRange& ir_;
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
	int unrollN_;
	uint32_t varN_; // # variables
	uint32_t constN_; // # constants
	IndexRange funcTmpReg_;
	IndexRange funcTmpMask_;
	uint32_t maxTmpN_; // max # of regs in evaluation
	int totalN_;
	uint32_t curMaskTmpIdx_;
	bool debug;
	GeneratorBase()
		: simdByte_(32 / 8) // one float
		, unrollN_(1)
		, varN_(0)
		, constN_(0)
		, maxTmpN_(0)
		, totalN_(0)
		, curMaskTmpIdx_(0)
		, debug(false)
	{
		const char *env = getenv("SG_OPT");
		if (env == 0) return;
		std::istringstream iss(env);
		std::string kv;
		while (iss >> kv) {
			size_t pos = kv.find('=');
			if (pos == std::string::npos) continue;
			std::string k = kv.substr(0, pos);
			std::string v = kv.substr(pos + 1);
			if (k == "unroll") {
				unrollN_ = cybozu::atoi(v);
				if (unrollN_ < 1) throw cybozu::Exception("bad unroll") << unrollN_;
				printf("unrollN=%d\n", unrollN_);
			}
		}
	}
	virtual ~GeneratorBase()
	{
	}
	int getVarIdxOffset() const { return 0; }
	int getVarIdx(int i) const { return getVarIdxOffset() + i; }
	uint32_t getConstIdxOffset() const { return varN_; }
	uint32_t getTmpOffset() const { return varN_ + constN_ + funcTmpReg_.getSize(); }
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
		funcTmpReg_.n_ = 0;
		/*
			append const var in used functions
			set funcTmpReg_
		*/
		int regN = 0;
		int maskN = 0;
		for (int i = 0; i < sg::FuncTypeN; i++) {
			if (!tl.isUsedFunc(i)) continue;
			const FuncInfo& fi = funcInfoTbl[i];
			const sg::IntVec& constTbl = fi.constTbl;
			for (size_t j = 0; j < constTbl.size(); j++) {
				constIdx_.append(constTbl[j]);
			}
			if (fi.tmpRegN > regN) regN = fi.tmpRegN;
			if (fi.tmpMaskN > maskN) maskN = fi.tmpMaskN;
		}
		funcTmpReg_.setSize(regN * unrollN_);
		funcTmpMask_.setSize(maskN * unrollN_);

		varN_ = tl.getVarNum() * unrollN_;
		constN_ = constIdx_.size();
		funcTmpReg_.setOffset(varN_ + constN_);
		funcTmpMask_.setOffset(1 + 1); // mask0 and mask1 are reserved
		maxTmpN_ = tl.getMaxTmpNum() * unrollN_;
		totalN_ = varN_ + constN_ + funcTmpReg_.getSize() + maxTmpN_;
		if (debug) printf("varN=%d constN=%d funcTmpReg.max=%d maxTmpN=%d\n", varN_, constN_, funcTmpReg_.getSize(), maxTmpN_);
		if (totalN_ > 32) {
			throw cybozu::Exception("too many registers");
		}
	}
	void gen_setConst()
	{
		for (uint32_t i = 0; i < constN_; i++) {
			gen_setInt(getConstIdxOffset() + i, constIdx_.getVal(i));
		}
	}
	virtual void exec(const sg::TokenList& tl)
	{
		if (debug) puts("init of GeneratorBase");
		updateConstIdx(tl);
		gen_setConst();
		puts("execOneLoop");
		for (uint32_t i = 0; i < varN_; i++) {
			gen_loadVar(getVarIdx(i), i);
		}
		execOneLoop(tl, 1);
//		gen_saveVar(0, getTmpOffset());
	}
	virtual void gen_setInt(int dst, uint32_t u)
	{
		if (debug) printf("setImm z%d, %08x\n", dst, u);
	}
	virtual void gen_loadVar(int dst, uint32_t u)
	{
		if (debug) printf("loadVar z%d, [%u]\n", dst, u);
	}
	virtual void gen_copy(int dst, int src)
	{
		if (debug) printf("copy z%d, z%d\n", dst, src);
	}
	virtual void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("add z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("sub z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("mul z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("div z%d, z%d, z%d\n", dst, src1, src2);
	}
	virtual void gen_inv(int inout, int n)
	{
		if (debug) printf("inv z%d (%d)\n", inout, n);
	}
	virtual void gen_exp(int inout, int n)
	{
		if (debug) printf("exp z%d (%d)\n", inout, n);
	}
	virtual void gen_log(int inout, int n)
	{
		if (debug) printf("log z%d (%d)\n", inout, n);
	}
	virtual void gen_cosh(int inout, int n)
	{
		if (debug) printf("cosh z%d (%d)\n", inout, n);
	}
	virtual void gen_tanh(int inout, int n)
	{
		if (debug) printf("tanh z%d (%d)\n", inout, n);
	}
	template<class TL>
	void execOneLoop(const TL& tl, int unrollN)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		const size_t n = vv.size();
		uint32_t pos = getTmpOffset();
		for (size_t i = 0; i < n; i++) {
			const Value& v = vv[i];
			switch (v.type) {
			case Var:
				for (int i = 0; i < unrollN; i++) {
					gen_copy(pos + i, getVarIdxOffset() + v.v + i);
				}
				pos += unrollN;
				break;
			case Const:
				for (int i = 0; i < unrollN; i++) {
					gen_copy(pos + i, getConstIdxOffset() + constIdx_.getIdx(v.v));
				}
				pos += unrollN;
				break;
			case Op:
				assert(pos > unrollN);
				pos -= unrollN;
				for (int i = 0; i < unrollN; i++) {
					int dst = pos - unrollN + i;
					int src = pos + i;
					switch (v.v) {
					case Add: gen_add(dst, dst, src); break;
					case Sub: gen_sub(dst, dst, src); break;
					case Mul: gen_mul(dst, dst, src); break;
					case Div: gen_div(dst, dst, src); break;
					default:
						throw cybozu::Exception("bad op") << i << v.v;
					}
				}
				break;
			case Func:
				assert(pos > 0);
				switch (v.v) {
				case Inv: gen_inv(pos - unrollN, unrollN); break;
				case Exp: gen_exp(pos - unrollN, unrollN); break;
				case Log: gen_log(pos - unrollN, unrollN); break;
				case Cosh: gen_cosh(pos - unrollN, unrollN); break;
				case Tanh: gen_tanh(pos - unrollN, unrollN); break;
				default:
					throw cybozu::Exception("bad func") << i << pos << v.v;
				}
				break;
			default:
				throw cybozu::Exception("bad type") << i << pos << v.type;
			}
		}
	}
	void setFuncInfoTbl()
	{
		for (size_t i = 0; i < FuncTypeN; i++) {
			FuncInfo& fi = funcInfoTbl[i];
			switch (i) {
			case Inv:
#ifdef SG_X64
				fi.constTbl.push_back(f2u(2.0));
				fi.tmpRegN = 1;
#else
				fi.tmpRegN = 2;
#endif
				fi.tmpMaskN = 0;
				break;
			case Exp:
			case Cosh:
				fi.constTbl.push_back(f2u(g_expTbl.log2_e));
#ifdef SG_X64
				fi.constTbl.push_back(f2u(g_expTbl.log2));
				for (int j = 0; j < ExpTbl::N; j++) {
					fi.constTbl.push_back(f2u(g_expTbl.coef[j]));
				}
#else
				fi.constTbl.push_back(g_expTbl.not_mask17);
				fi.constTbl.push_back(f2u(g_expTbl.one));
				fi.constTbl.push_back(f2u(g_expTbl.coeff1));
				fi.constTbl.push_back(f2u(g_expTbl.coeff2));
#endif
				fi.tmpRegN = ExpTbl::tmpRegN;
				fi.tmpMaskN = ExpTbl::tmpMaskN;
				if (i == Exp) break;
				// cosh(x) = (e^x + e^(-x))*0.5
				fi.constTbl.push_back(f2u(0.5));
				// for inv
#ifdef SG_X64
				fi.constTbl.push_back(f2u(2.0));
				fi.constTbl.push_back(0x7fffffff);
#endif
				break;
			case Log:
				fi.constTbl.push_back(g_logTbl.i127shl23);
				fi.constTbl.push_back(g_logTbl.x7fffff);
#ifdef SG_X64
				fi.constTbl.push_back(g_logTbl.x7fffffff);
				fi.constTbl.push_back(f2u(g_logTbl.one));
				fi.constTbl.push_back(f2u(g_logTbl.f1div8));
#endif
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

} // sg

