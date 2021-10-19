#pragma once
#include <xbyak_aarch64/xbyak_aarch64.h>
#include <simdgen/simdgen.h>
#include <cybozu/exception.hpp>
#include <cmath>
#include "const.hpp"

using namespace Xbyak_aarch64;

typedef std::vector<ZRegS> ZRegSVec;
typedef std::vector<PRegS> PRegSVec;

namespace sg {

/*
	free : z0, ..., z7, z24, ..., z31
	save : z8, ..., z23
*/
static const int saveRegBegin = 8;
static const int saveRegEnd = 24;

/*
	free : p0, p1, p2, p3
	save : p4, ...
*/
static const int savePredBegin = 4;

struct Generator : CodeGenerator, sg::GeneratorBase {
	static const size_t dataSize = 4096;
	static const size_t codeSize = 8192;
	static const size_t totalSize = dataSize + codeSize;
	Label dataL_;
	XReg dataReg_;
	XReg tmpX_;
	WReg tmpW_;
	bool debug;

	Generator()
		: CodeGenerator(totalSize)
		, dataReg_(x3)
		, tmpX_(x4)
		, tmpW_(w4)
		, debug(false)
	{
		simdByte_ = 512 / 8;
	}
	// x[0] = sum(s[0:...15])
	void reduceOne_sum(int d, int s)
	{
		assert(d != s);
		faddv(SReg(d), p0, ZRegS(s));
	}
	void outputOne(const XReg& dst, int i, const XReg *tmpX = 0)
	{
		if (reduceFuncType_ >= 0) {
			int red = getReduceVarIdx() + i;
			int src = getTmpIdx(i);
			gen_reduce(red, src);
		} else {
			if (tmpX) {
				st1w(ZReg(getTmpIdx(0)).s, p1, ptr(dst, tmpX_, LSL, 2));
			} else {
				st1w(ZReg(getTmpIdx(i)).s, p0, ptr(dst, i));
			}
		}
	}
	void exec(const sg::TokenList& tl)
	{
		if (debug) puts("aarch64/exec");
		Label dataL = L();
		setupLayout(tl);
		setSize(0);
		for (uint32_t i = 0; i < constN_; i++) {
			dd(constIdx_.getVal(i));
		}
		if (getSize() > dataSize) {
			throw cybozu::Exception("bad data size") << getSize();
		}
		setSize(dataSize);
		addr_ = getCurr<void*>();

		adr(dataReg_, dataL);
		ptrue(p0.s);
		// store regs
		if (debug) printf("saveRegBegin=%d saveRegEnd=%d totalN_=%d\n", saveRegBegin, saveRegEnd, totalN_);
		const int saveN = std::min(saveRegEnd, totalN_);
		for (int i = saveRegBegin; i < saveN; i++) {
			sub(sp, sp, 64);
			st1w(ZReg(i).s, p0, ptr(sp));
		}
		const int saveMaskN = funcTmpMask_.getMax();
		for (int i = savePredBegin; i < saveMaskN; i++) {
			sub(sp, sp, 16);
			str(PReg(i).s, ptr(sp));
		}
		XReg dst = x0, src = x1, n = x2;
		if (reduceFuncType_ >= 0) {
			// dst is not used
			src = x0;
			n = x1;
		}
		gen_setConst();
		if (reduceFuncType_ >= 0) {
			LP_(i, unrollN_) {
				ZRegS red(getReduceVarIdx() + i);
				mov(red, 0);
			}
		}

		Label skipL, exitL;
		b(skipL);
	Label lp = L();
		LP_(i, unrollN_) ld1w(ZReg(getVarIdx(i)).s, p0, ptr(src, i));
		add(src, src, 64 * unrollN_);
		execOneLoop(tl, unrollN_);
		LP_(i, unrollN_) outputOne(dst, i);
		if (reduceFuncType_ < 0) add(dst, dst, 64 * unrollN_);
		sub(n, n, 16 * unrollN_);
	L(skipL);
		cmp(n, 16 * unrollN_);
		bge(lp);

		cmp(n, 0);
		beq(exitL);

		Label cond;
		mov(tmpX_, 0);
		b(cond);
	Label lp2 = L();
		ld1w(ZReg(getVarIdx(0)).s, p1, ptr(src, tmpX_, LSL, 2));
		execOneLoop(tl, 1);
		outputOne(dst, 0, &tmpX_);
		incw(tmpX_);
	L(cond);
		whilelt(p1.s, tmpX_, n);
		b_first(lp2);
	L(exitL);

		if (reduceFuncType_ >= 0) {
			reduceAll();
		}

		// restore regs
		for (int i = savePredBegin; i < saveMaskN; i++) {
			ldr(PReg(saveMaskN + savePredBegin - 1 - i).s, ptr(sp));
			add(sp, sp, 16);
		}
		for (int i = saveRegBegin; i < saveN; i++) {
			ld1w(ZReg(saveN + saveRegBegin - 1 - i).s, p0, ptr(sp));
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
	void gen_inv(int inout, int n)
	{
		if (debug) printf("inv z%d\n", inout);
		IndexRangeManager ftr(funcTmpReg_);
		ZRegSVec t0, t1, t2;
		LP_(i, n) {
			t0.push_back(ZRegS(inout + i));
			t1.push_back(ZRegS(ftr.allocIdx()));
			t2.push_back(ZRegS(ftr.allocIdx()));
		}
		LP_(i, n) frecpe(t1[i], t0[i]);
		LP_(i, n) frecps(t2[i], t0[i], t1[i]);
		LP_(i, n) fmul(t1[i], t1[i], t2[i]);

		LP_(i, n) frecps(t2[i], t0[i], t1[i]);
		LP_(i, n) fmul(t0[i], t1[i], t2[i]);
	}
	void gen_exp(int inout, int n)
	{
		if (debug) printf("exp z%d\n", inout);
		const ZRegS log2_e(getFloatIdx(g_expTbl.log2_e));
		const ZRegD not_mask17(getFloatIdx(u2f(g_expTbl.not_mask17)));
		const ZRegS one(getFloatIdx(g_expTbl.one));
		const ZRegS coeff1(getFloatIdx(g_expTbl.coeff1));
		const ZRegS coeff2(getFloatIdx(g_expTbl.coeff2));
		IndexRangeManager ftr(funcTmpReg_);
		ZRegSVec t0, t1, t2;
		LP_(i, n) {
			t0.push_back(ZRegS(inout + i));
			t1.push_back(ZRegS(ftr.allocIdx()));
			t2.push_back(ZRegS(ftr.allocIdx()));
		}

//		fmin(t0, p0, expMax.s);
//		fmax(t0, p0, expMin.s);
		LP_(i, n) fmul(t0[i], t0[i], log2_e);
		LP_(i, n) {
			movprfx(t1[i], p0, t0[i]); // clear implicit dependency
			frintm(t1[i], p0, t0[i]); // floor : float -> float
		}
		LP_(i, n) fcvtzs(t2[i], p0, t1[i]); // n = float -> int
		LP_(i, n) fsub(t1[i], t0[i], t1[i]); // a
		LP_(i, n) fadd(t0[i], t1[i], one); // b = 1 + a
		LP_(i, n) lsr(t1[i], t0[i], 17); // bL
		LP_(i, n) fexpa(t1[i], t1[i]); // c = fexpa(bL)
		LP_(i, n) fscale(t1[i], p0, t2[i]); // t[i+1] *= 2^n
		LP_(i, n) and_(ZRegD(t2[i].getIdx()), ZRegD(t0[i].getIdx()), not_mask17);
		LP_(i, n) fsub(t2[i], t0[i], t2[i]); // z
		LP_(i, n) {
			movprfx(t0[i], p0, coeff2);
			fmad(t0[i], p0, t2[i], coeff1);
		}
		LP_(i, n) fmad(t0[i], p0, t2[i], one);
		LP_(i, n) fmul(t0[i], t1[i], t0[i]);
	}
	void gen_cosh(int inout, int n)
	{
		if (debug) printf("cosh z%d\n", inout);
		ZRegSVec t0;
		LP_(i, n) t0.push_back(ZRegS(inout + i));
		/*
			X = exp(|x|)
			cosh(x) = (X + 1/X) * 0.5
		*/
		LP_(i, n) fabs(t0[i], p0, t0[i]);
		gen_exp(inout, n);
		IndexRangeManager ftr(funcTmpReg_);
		ZRegSVec t1;
		LP_(i, n) {
			t1.push_back(ZRegS(ftr.allocIdx()));
			mov(t1[i], p0, t0[i]);
		}
		gen_inv(t1[0].getIdx(), n);
		LP_(i, n) fadd(t0[i], t0[i], t1[i]);
		LP_(i, n) fmul(t0[i], p0, 0.5);
	}
	void gen_log(int inout, int n)
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
		IndexRangeManager ftr(funcTmpReg_);
		IndexRangeManager ftm(funcTmpMask_);
		ZRegSVec t0, t1, t2, t3;
		PRegSVec mask;
		LP_(i, n) {
			t0.push_back(ZRegS(inout + i));
			t1.push_back(ZRegS(ftr.allocIdx()));
			t2.push_back(ZRegS(ftr.allocIdx()));
			t3.push_back(ZRegS(ftr.allocIdx()));
			mask.push_back(PRegS(ftm.allocIdx()));
		}

		LP_(i, n) mov(t3[i], p0, t0[i]);
		LP_(i, n) sub(t1[i], t0[i], i127shl23);
		LP_(i, n) asr(t1[i], t1[i], 23);
		// int -> float
		LP_(i, n) scvtf(t1[i], p0, t1[i]);
		LP_(i, n) and_(t0[i], p0, x7fffff);
		LP_(i, n) orr(t0[i], p0, i127shl23);

		// fnmsb(a, b, c) = a * b - c
		LP_(i, n) fnmsb(t0[i], p0, f2div3, coef[0]);
		LP_(i, n) fmad(t1[i], p0, log2, log1p5);

		LP_(i, n) fsub(t2[i], t3[i], coef[0]); // x-1
		LP_(i, n) fcpy(t3[i], p0, 1.0/8);
		LP_(i, n) {
			facge(mask[i], p0, t3[i], t2[i]); // 1/8 >= abs(x-1)
			mov(t0[i], mask[i], t2[i]);
			eor(t1[i], mask[i], t1[i]);
		}
		const int logN = LogTbl::N;
		// fmad(a, b, c) ; a = a * b + c
		LP_(i, n) {
			movprfx(t2[i], p0, coef[logN - 1]);
			fmad(t2[i], p0, t0[i], coef[logN - 2]);
		}
		for (int j = logN - 3; j >= 0; j--) {
			LP_(i, n) fmad(t2[i], p0, t0[i], coef[j]);
		}
		// a * x + e
		LP_(i, n) fmad(t0[i], p0, t2[i], t1[i]);
	}
	void gen_tanh(int inout, int n)
	{
		throw cybozu::Exception("not support gen_tanh") << inout << n;
	}
};

} // namespace sg

