#pragma once
#include <xbyak_aarch64/xbyak_aarch64.h>
#include <simdgen/simdgen.h>
#include <cybozu/exception.hpp>
#include <cmath>
#include "const.hpp"

using namespace Xbyak_aarch64;

namespace sg {

const int freeTbl[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 24, 25, 26, 27, 28, 29, 30, 31
};

static const size_t maxFreeN = sizeof(freeTbl)/sizeof(freeTbl[0]);

const int saveTbl[] = {
	8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};
static const size_t maxSaveN = sizeof(saveTbl)/sizeof(saveTbl[0]);

inline bool isKeepReg(uint32_t idx)
{
	return idx < maxFreeN;
}

inline uint32_t getKeepNum(uint32_t maxIdx)
{
	if (maxIdx < maxFreeN) return 0;
	return maxIdx - maxFreeN;
}

struct Generator : CodeGenerator, sg::GeneratorBase {
	static const size_t dataSize = 4096;
	static const size_t codeSize = 8192;
	static const size_t totalSize = dataSize + codeSize;
	SgFuncFloat1 *addr_;
	Label dataL_;
	int totalN_;
	int keepN_;
	XReg dataReg_;
	XReg tmpX_;
	WReg tmpW_;
	bool debug;

	Generator()
		: CodeGenerator(totalSize)
		, addr_(0)
		, totalN_(0)
		, keepN_(0)
		, dataReg_(x3)
		, tmpX_(x4)
		, tmpW_(w4)
		, debug(true)
	{
		simdByte_ = 512 / 8;
		setFuncInfoTbl();
	}
	SgFuncFloat1* getAddrFloat1() const { return addr_; }
	void exec(const sg::TokenList& tl)
	{
		if (debug) puts("aarch64/exec");
		Label dataL = L();
		updateConstIdx(tl);
		for (size_t i = 0; i < constN_; i++) {
			dd(constIdx_.getVal(i));
		}
		setSize(dataSize);
		addr_ = getCurr<SgFuncFloat1*>();

		adr(dataReg_, dataL);
		ptrue(p0.s);
		// store regs
		for (int i = 0; i < keepN_; i++) {
			sub(sp, sp, 64);
			st1w(ZReg(saveTbl[i]).s, p0, ptr(sp));
		}
		const XReg& dst = x0;
		const XReg& src = x1;
		const XReg& n = x2;
		gen_setConst();

		Label skip;
		b(skip);
	Label lp = L();
		ld1w(ZReg(getVarIdx(0)).s, p0, ptr(src));
		add(src, src, 64);
		execOneLoop(tl);
		st1w(ZReg(getTmpIdx(0)).s, p0, ptr(dst));
		add(dst, dst, 64);
		sub(n, n, 16);
	L(skip);
		cmp(n, 16);
		bge(lp);

		Label cond;
		mov(tmpX_, 0);
		b(cond);
	Label lp2 = L();
		ld1w(ZReg(getVarIdx(0)).s, p1, ptr(src, tmpX_, LSL, 2));
		execOneLoop(tl);
		st1w(ZReg(getTmpIdx(0)).s, p1, ptr(dst, tmpX_, LSL, 2));
		incd(tmpX_);
	L(cond);
		whilelt(p1.s, tmpX_, n);
		b_first(lp2);

		// restore regs
		for (int i = 0; i < keepN_; i++) {
			ld1w(ZReg(saveTbl[i]).s, p0, ptr(sp));
			add(sp, sp, 64);
		}
		ret();
		ready();
	}
	void gen_setInt(int dst, uint32_t u)
	{
		if (debug) {
			printf("mov tmpW_, 0x%08x\n", u);
			printf("cpy z%d, tmpW_\n", dst);
		}
		mov(tmpW_, u);
		cpy(ZReg(dst).s, p0, tmpW_);
	}
	void gen_copy(int dst, int src)
	{
		if (debug) printf("mov z%d, z%d\n", dst, src);
		mov(ZReg(dst).s, p0, ZReg(src).s);
	}
	void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("fadd z%d, z%d, z%d\n", dst, src1, src2);
		fadd(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("fsub z%d, z%d, z%d\n", dst, src1, src2);
		fsub(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("fmul z%d, z%d, z%d\n", dst, src1, src2);
		fmul(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("fdiv z%d, z%d, z%d\n", dst, src1, src2);
		movprfx(ZReg(dst), ZReg(src1));
		fdiv(ZReg(dst).s, p0, ZReg(src2).s);
	}
	void gen_inv(int inout)
	{
		if (debug) printf("inv z%d\n", inout);
		IndexRangeManager ftr(funcTmpReg_);
		const ZReg t1(ftr.allocIdx());
		fcpy(t1.s, p0, 1.0);
		fdivr(ZReg(inout).s, p0, t1.s);
	}
	void gen_exp(int inout)
	{
		if (debug) printf("exp z%d\n", inout);
		const ZRegS log2_e(getFloatIdx(g_expTbl.log2_e));
		const ZRegD not_mask17(getFloatIdx(u2f(g_expTbl.not_mask17)));
		const ZRegS one(getFloatIdx(g_expTbl.one));
		const ZRegS coeff1(getFloatIdx(g_expTbl.coeff1));
		const ZRegS coeff2(getFloatIdx(g_expTbl.coeff2));
		const ZRegS t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegS t1(ftr.allocIdx());
		const ZRegS t2(ftr.allocIdx());

//		fmin(t0, p0, expMax.s);
//		fmax(t0, p0, expMin.s);
		fmul(t0, t0, log2_e);
		movprfx(t1, p0, t0); // clear implicit dependency
		frintm(t1, p0, t0); // floor : float -> float
		fcvtzs(t2, p0, t1); // n = float -> int
		fsub(t1, t0, t1); // a
		fadd(t0, t1, one); // b = 1 + a
		lsr(t1, t0, 17); // bL
		fexpa(t1, t1); // c = fexpa(bL)
		fscale(t1, p0, t2); // t[i+1] *= 2^n
		and_(ZRegD(t2.getIdx()), ZRegD(t0.getIdx()), not_mask17);
		fsub(t2, t0, t2); // z
		movprfx(t0, p0, coeff2);
		fmad(t0, p0, t2, coeff1);
		fmad(t0, p0, t2, one);
		fmul(t0, t1, t0);
	}
	void gen_log(int inout)
	{
		printf("gen_log %d\n", inout);
		const ZRegS i127shl23(getFloatIdx(u2f(g_logTbl.i127shl23)));
		const ZRegS x7fffff(getFloatIdx(u2f(g_logTbl.x7fffff)));
		const ZRegS log2(getFloatIdx(g_logTbl.log2));
		const ZRegS f2div3(getFloatIdx(g_logTbl.f2div3));
		const ZRegS log1p5(getFloatIdx(g_logTbl.log1p5));
		const ZRegS coef[] = {
			ZRegS(getFloatIdx(g_logTbl.coef[0])),
			ZRegS(getFloatIdx(g_logTbl.coef[1])),
			ZRegS(getFloatIdx(g_logTbl.coef[2])),
			ZRegS(getFloatIdx(g_logTbl.coef[3])),
			ZRegS(getFloatIdx(g_logTbl.coef[4])),
			ZRegS(getFloatIdx(g_logTbl.coef[5])),
			ZRegS(getFloatIdx(g_logTbl.coef[6])),
			ZRegS(getFloatIdx(g_logTbl.coef[7])),
			ZRegS(getFloatIdx(g_logTbl.coef[8])),
		};
		const ZRegS t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegS t1(ftr.allocIdx());
		const ZRegS t2(ftr.allocIdx());
		const ZRegS t3(ftr.allocIdx());
		IndexRangeManager ftm(funcTmpMask_);
		const PRegS mask(ftm.allocIdx());

		mov(t3, p0, t0);
		sub(t1, t0, i127shl23);
		asr(t1, t1, 23);
		// int -> float
		scvtf(t1, p0, t1);
		and_(t0, p0, x7fffff);
		orr(t0, p0, i127shl23);

		// fnmsb(a, b, c) = a * b - c
		fnmsb(t0, p0, f2div3, coef[0]);
		fmad(t1, p0, log2, log1p5);

		fsub(t2, t3, coef[0]); // x-1
		fcpy(t3, p0, 1.0/8);
		facge(mask, p0, t3, t2); // 1/8 >= abs(x-1)
		mov(t0, mask, t2);
		eor(t1, mask, t1);
		const int logN = LogTbl::N;
		// fmad(a, b, c) ; a = a * b + c
		movprfx(t2, p0, coef[logN - 1]);
		fmad(t2, p0, t0, coef[logN - 2]);
		for (int j = logN - 3; j >= 0; j--) {
			fmad(t2, p0, t0, coef[j]);
		}
		// a * x + e
		fmad(t0, p0, t2, t1);
	}
	void gen_tanh(int inout)
	{
		throw cybozu::Exception("not support gen_tanh") << inout;
	}
};

} // namespace sg

