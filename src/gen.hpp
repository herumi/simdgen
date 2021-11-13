#pragma once
#include <simdgen/simdgen.h>
#include <stdint.h>
#include <stdio.h>
#include <cmath>
#include "tokenlist.hpp"
#include "const.hpp"
#include "opt.hpp"

#define LP_(i, n) for (int i = 0; i < n; i++)

namespace sg {

struct IndexRange {
	int offset_;
	int max_;
	int cur_;
	bool seekMode_;
	IndexRange()
		: offset_(0)
		, max_(0)
		, cur_(0)
		, seekMode_(false)
	{
	}
	void clear()
	{
		offset_ = 0;
		max_ = 0;
		cur_ = 0;
		seekMode_ = false;
	}
	void setSeekMode(bool seekMode)
	{
		seekMode_ = seekMode;
		if (seekMode) {
			max_ = 0;
		}
	}
	int alloc()
	{
		if (!seekMode_ && cur_ == max_) throw cybozu::Exception("too alloc") << max_;
		int ret = offset_ + cur_++;
		if (cur_ > max_) max_ = cur_;
		return ret;
	}
	int getCur() const { return cur_; }
	void setCur(int cur) { cur_ = cur; }
	void setSize(int n) { max_ = n; }
	int getSize() const { return max_; }
	int getMax() const { return offset_ + max_; }
	void setOffset(int offset) { offset_ = offset; }
	int getOffset() const { return offset_; }
	void put(const char *msg = "") const
	{
		printf("%s offset=%d, max=%d, cur=%d\n", msg, offset_, max_, cur_);
	}
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

struct SimdArray {
	static const int N = 16;
	static const int byteSize = N * int(sizeof(uint32_t));
private:
	uint32_t d[N];
public:
	SimdArray()
		: d()
	{
	}
	SimdArray(const void *p, size_t n)
	{
		if (n > byteSize) {
			throw cybozu::Exception("SimdArray:too large") << n;
		}
		memcpy(d, p, n);
		if (n < byteSize) {
			memset(((char *)d) + n, 0, byteSize - n);
		}
	}
	friend inline bool operator==(const SimdArray& lhs, const SimdArray& rhs)
	{
		return memcmp(lhs.d, rhs.d, byteSize) == 0;
	}
	friend inline std::ostream& operator<<(std::ostream& os, const SimdArray& x)
	{
		for (size_t i = 0; i < N; i++) {
			char buf[9];
			snprintf(buf, sizeof(buf), "%08x", x.d[i]);
			os.write(buf, 8);
			if ((i & 3) == 3) os << ':';
		}
		return os;
	}
	uint32_t get32bit(size_t i) const
	{
		if (i > N) throw cybozu::Exception("SimdArray:get32bit:bad idx") << i;
		return d[i];
	}
};

struct GeneratorBase {
	// simd memory data and preload registers
	Index<SimdArray> constTblMem_; // simd memory
	Index<uint32_t> constTblIdx_; // preload regs

	// float/uint32_t data and preload registers
	Index<uint32_t> constMem_; // 4 byte float memory
	Index<uint32_t> constIdx_; // preload regs
	int simdByte_;
	int unrollN_;
	void* addr_;
	/*
		[0, varN_] ; var
		varN_ + [0, constN_] ; const
		varN_ + constN_ + [0, funcTmpReg_.max()] ; tmp reg in func
		varN_ + constN_ + funcTmpReg_.max() + [0, maxTmpN_] ; stack tmp reg
	*/
	uint32_t varN_; // # variables
	uint32_t constN_; // # constants
	IndexRange funcTmpReg_;
	IndexRange funcTmpMask_;
	uint32_t maxTmpN_; // max # of regs in evaluation
	int totalN_;
	uint32_t curMaskTmpIdx_;
	int reduceFuncType_;
	bool debug;
	SgOpt opt;
	GeneratorBase()
		: simdByte_(32 / 8) // one float
		, unrollN_(0)
		, addr_(0)
		, varN_(0)
		, constN_(0)
		, maxTmpN_(0)
		, totalN_(0)
		, curMaskTmpIdx_(0)
		, reduceFuncType_(-1)
		, debug(false)
	{
		opt.getEnv();
		debug = opt.debug;
		unrollN_ = opt.unrollN;
	}
	virtual ~GeneratorBase()
	{
	}
	SgFuncFloat1 getAddrFloat1() const { return (SgFuncFloat1)addr_; }
	SgFuncFloat1Reduce getAddrFloat1Reduce() const { return (SgFuncFloat1Reduce)addr_; }
	int getVarIdxOffset() const { return 0; }
	int getVarIdx(int i) const { return getVarIdxOffset() + i; }
	int getReduceVarIdx() const { return getVarIdxOffset() + varN_ - unrollN_; }
	uint32_t getConstTblIdx0() const { return varN_; }
	uint32_t getConstIdx0() const { return varN_ + constTblIdx_.size(); }
	uint32_t getTmpOffset() const { return varN_ + constN_ + funcTmpReg_.getSize(); }
	int getTmpIdx(int i) const { return getTmpOffset() + i; }
	uint32_t getTotalNum() const { return getTmpOffset() + maxTmpN_; }
	void putLayout() const
	{
		puts("--- Layout ---");
		printf("var       %d, ..., %d\n", 0, varN_);
		printf("const     %d, ..., %d\n", varN_, varN_ + constN_);
		printf("funcTmp   %d, ..., %d\n", funcTmpReg_.getOffset(), funcTmpReg_.getOffset() + funcTmpReg_.getSize());
		printf("stack reg %d, ..., %d\n", getTmpOffset(), getTotalNum());
		printf("funcTmpReg_.getMax()=%d\n", funcTmpReg_.getMax());
		printf("funcTmpMask_.getMax()=%d\n", funcTmpMask_.getMax());
		puts("---");
	}
	// return byte offset to dataReg_
	int getConstTblOffsetToDataReg(const void *p, size_t byteSize) const
	{
		SimdArray u(p, byteSize);
		return constTblMem_.getIdx(u) * SimdArray::byteSize;
	}
	uint32_t getConstOffsetToDataReg(uint32_t u) const
	{
		return constTblMem_.size() * SimdArray::byteSize + constMem_.getIdx(u) * 4;
	}
	uint32_t getConstTblIdx(const void *p, size_t byteSize) const
	{
		SimdArray u(p, byteSize);
		uint32_t idx = constTblMem_.getIdx(u);
		return getConstTblIdx0() + constTblIdx_.getIdx(idx);
	}
	uint32_t getConstIdx(uint32_t u) const
	{
		uint32_t idx = constMem_.getIdx(u);
		return getConstIdx0() + constIdx_.getIdx(idx);
	}
	int getFloatIdx(float f) const
	{
		return getConstIdx(f2u(f));
	}
	/*
		setup registers and const variables
	*/
	bool setupLayout(const sg::TokenList& tl, int unrollN)
	{
		unrollN_ = unrollN;
		// set constMem_ by consts used in tl
		const sg::ValueVec& vv = tl.getValueVec();
		for (size_t i = 0; i < vv.size(); i++) {
			if (vv[i].type == Const) {
				uint32_t idx = constMem_.append(vv[i].v);
				constIdx_.append(idx);
			}
		}
		/*
			try execOneLoop with seekMode and
			estimate the max num of regs and constants
			funcTmpReg_ ; # of registers temporarily used in functions
			funcTmpMask ; # of mask registers
		*/
		funcTmpReg_.setSeekMode(true);
		funcTmpMask_.setSeekMode(true);
		constMem_.setSeekMode(true);
		constIdx_.setSeekMode(true);
		constTblMem_.setSeekMode(true);
		constTblIdx_.setSeekMode(true);

		execOneLoop(tl, unrollN_);

		// set ordinary mode
		funcTmpReg_.setSeekMode(false);
		funcTmpMask_.setSeekMode(false);
		constMem_.setSeekMode(false);
		constIdx_.setSeekMode(false);
		constTblMem_.setSeekMode(false);
		constTblIdx_.setSeekMode(false);

		reduceFuncType_ = tl.getReduceFuncType();

		varN_ = tl.getVarNum() * unrollN_;
		if (reduceFuncType_ >= 0) {
			varN_ += unrollN_;
		}
		constN_ = constIdx_.size() + constTblIdx_.size();
		funcTmpReg_.setOffset(varN_ + constN_);
		funcTmpMask_.setOffset(1 + 1); // mask0 and mask1 are reserved
		maxTmpN_ = tl.getMaxTmpNum() * unrollN_;
		totalN_ = varN_ + constN_ + funcTmpReg_.getSize() + maxTmpN_;
		if (debug) printf("varN=%d constN=%d funcTmpReg.max=%d maxTmpN=%d\n", varN_, constN_, funcTmpReg_.getSize(), maxTmpN_);
		return totalN_ <= 32;
	}
	void detectUnrollN(const sg::TokenList& tl)
	{
		const int maxTryUnrollN = 5;
		if (unrollN_ > 0) {
			if (!setupLayout(tl, unrollN_)) {
				throw cybozu::Exception("can't unrollN") << unrollN_;
			}
		} else {
			int unrollN = maxTryUnrollN;
			while (unrollN > 0) {
				if (setupLayout(tl, unrollN)) {
					break;
				}
				unrollN--;
			}
			if (unrollN == 0) {
				throw cybozu::Exception("too complex expression");
			}
		}
		if (debug) printf("unrollN_=%d\n", unrollN_);
	}
	void gen_setConst()
	{
		for (uint32_t i = 0; i < constTblIdx_.size(); i++) {
			gen_fullLoad(getConstTblIdx0() + i, constTblIdx_.getIdx(i) * SimdArray::byteSize);
		}
		for (uint32_t i = 0; i < constIdx_.size(); i++) {
			gen_setInt(getConstIdx0() + i, constMem_.getVal(constIdx_.getVal(i)));
		}
	}
	virtual void exec(const sg::TokenList& tl)
	{
		if (debug) puts("init of GeneratorBase");
		setupLayout(tl, 1);
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
	virtual void gen_fullLoad(int dst, uint32_t offset)
	{
		if (debug) printf("fullLoad z%d, [%08x]\n", dst, offset);
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
	virtual void gen_debugFunc(int inout, int n)
	{
		if (debug) printf("debugFunc z%d (%d)\n", inout, n);
	}
	virtual void reduceOne_sum(int d, int s)
	{
		if (debug) printf("reduceOne_sum z%d (%d)\n", d, s);
	}
	void gen_reduce(int red, int src) // not virtual
	{
		switch (reduceFuncType_) {
		case RedSum: gen_add(red, red, src); break;
		default:
			throw cybozu::Exception("gen_reduce:bad reduceFuncType_") << reduceFuncType_;
		}
	}
	void reduceAll() // not virtual
	{
		int red = getReduceVarIdx();
		for (int i = 1; i < unrollN_; i++) {
			gen_reduce(red, red + i);
		}
		switch (reduceFuncType_) {
		case RedSum: reduceOne_sum(0, red); break;
		default:
			throw cybozu::Exception("reduce:bad reduceFuncType_") << reduceFuncType_;
		}
	}
	template<class TL>
	void execOneLoop(const TL& tl, int unrollN)
	{
		const sg::ValueVec& vv = tl.getValueVec();
		const size_t n = vv.size();
		int stack[32];
		int stackPos = 0;
		const int tmpMin = getTmpOffset();
		int tmpPos = tmpMin;
		for (size_t j = 0; j < n; j++) {
			const Value& v = vv[j];
			switch (v.type) {
			case Var:
				LP_(i, unrollN) stack[stackPos++] = getVarIdxOffset() + v.v + i;
				break;
			case Const:
				LP_(i, unrollN) stack[stackPos++] = getConstIdx(v.v);
				break;
			case Op:
				LP_(i, unrollN) {
					int dst = 0;
					int src1 = stack[stackPos - unrollN * 2 + i];
					int src2 = stack[stackPos - unrollN + i];
					if (src1 < tmpMin) {
						if (src2 < tmpMin) {
							dst = tmpPos++;
						} else {
							dst = src2;
						}
					} else {
						if (src2 < tmpMin) {
							dst = src1;
							tmpPos--;
						} else {
							dst = src1;
						}
					}
					stack[stackPos - unrollN * 2 + i] = dst;
					switch (v.v) {
					case Add: gen_add(dst, src1, src2); break;
					case Sub: gen_sub(dst, src1, src2); break;
					case Mul: gen_mul(dst, src1, src2); break;
					case Div: gen_div(dst, src1, src2); break;
					default:
						throw cybozu::Exception("bad op") << j << v.v;
					}
				}
				stackPos -= unrollN;
				break;
			case Func:
				{
					int pos = stack[stackPos - unrollN];
					if (pos < tmpMin) {
						LP_(i, unrollN) {
							gen_copy(tmpPos, pos + i);
							stack[stackPos - unrollN + i] = tmpPos;
							tmpPos++;
						}
						pos = stack[stackPos - unrollN];
					}
					switch (v.v) {
					case Inv: gen_inv(pos, unrollN); break;
					case Exp: gen_exp(pos, unrollN); break;
					case Log: gen_log(pos, unrollN); break;
					case Cosh: gen_cosh(pos, unrollN); break;
					case Tanh: gen_tanh(pos, unrollN); break;
					case DebugFunc: gen_debugFunc(pos, unrollN); break;
					case RedSum: /* nothing */ break;
					default:
						throw cybozu::Exception("bad func") << j << pos << v.v;
					}
				}
				break;
			default:
				throw cybozu::Exception("bad type") << j << stackPos << v.type;
			}
		}
	}
};

} // sg

