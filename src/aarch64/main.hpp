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
	XReg dataReg_;
	XReg tmp64_;
	WReg tmp32_;
	XReg loop_i_;

	Generator()
		: CodeGenerator(totalSize)
		, dataReg_(x3)
		, tmp64_(x4)
		, tmp32_(w4)
		, loop_i_(x5)
	{
#ifdef SG_NEON
		simdByte_ = 128 / 8;
		maxSimdRegN_ = 32;
#else
		simdByte_ = 512 / 8;
		maxSimdRegN_ = 32;
#endif
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
				st1w(ZReg(getTmpIdx(0)).s, p1, ptr(dst, *tmpX, LSL, 2));
			} else {
				str(ZReg(getTmpIdx(i)), ptr(dst, i));
			}
		}
	}
	void exec(const sg::TokenList& tl)
	{
		Label dataL = L();
		detectUnrollN(tl);
		setSize(0);
		for (uint32_t i = 0; i < constTblMem_.size(); i++) {
			const SimdArray& v = constTblMem_.getVal(i);
			for (size_t j = 0; j < v.N; j++) {
				dd(v.get32bit(j));
			}
		}
		for (uint32_t i = 0; i < constMem_.size(); i++) {
			dd(constMem_.getVal(i));
		}
		if (getSize() > dataSize) {
			throw cybozu::Exception("bad data size") << getSize();
		}
		setSize(dataSize);
		addr_ = getCurr<void*>();
		if (opt.break_point) brk(0);

		adr(dataReg_, dataL);
#ifdef SG_SVE
		ptrue(p0.s);
#endif
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
		LP_(i, unrollN_) ldr(ZReg(getVarIdx(i)), ptr(src, i));
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
		mov(loop_i_, 0);
		b(cond);
	Label lp2 = L();
		ld1w(ZReg(getVarIdx(0)).s, p1, ptr(src, loop_i_, LSL, 2));
		execOneLoop(tl, 1);
		outputOne(dst, 0, &loop_i_);
		incw(loop_i_);
	L(cond);
		whilelt(p1.s, loop_i_, n);
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
	ZRegSVec getInputRegVec(int pos, int n)
	{
		ZRegSVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(ZRegS(pos + i));
		}
		return t;
	}
	ZRegSVec getTmpRegVec(IndexRangeManager& irm, int n)
	{
		ZRegSVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(ZRegS(irm.allocIdx()));
		}
		return t;
	}
	PRegSVec getTmpMaskVec(IndexRangeManager& irm, int n)
	{
		PRegSVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(PRegS(irm.allocIdx()));
		}
		return t;
	}
	void gen_setInt(int dst, uint32_t u)
	{
#if 0
		ldr(tmp32_, ptr(dataReg_, getConstOffsetToDataReg(u)));
		cpy(ZRegS(dst), p0, tmp32_);
#else
		ld1rw(ZRegS(dst), p0, ptr(dataReg_, (int)getConstOffsetToDataReg(u)));
#endif
	}
	void setInt(const ZRegS& z, uint32_t u)
	{
		mov(tmp32_, u);
		dup(z, tmp32_);
	}
	void setFloat(const ZRegS& z, float f)
	{
		if (f2u(f) == f2u(1.0f)) {
			fcpy(z, p0, f);
		} else {
			setInt(z, f2u(f));
		}
	}
	void gen_fullLoad(int dst, uint32_t offset)
	{
		ld1w(ZRegS(dst), p0, ptr(dataReg_, int(offset / SimdArray::byteSize)));
	}
	void gen_copy(int dst, int src)
	{
		mov(ZReg(dst).s, p0, ZReg(src).s);
	}
	void gen_add(int dst, int src1, int src2)
	{
		fadd(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_sub(int dst, int src1, int src2)
	{
		fsub(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_mul(int dst, int src1, int src2)
	{
		fmul(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_div(int dst, int src1, int src2)
	{
		movprfx(ZReg(dst), ZReg(src1));
		fdiv(ZReg(dst).s, p0, ZReg(src2).s);
	}
	void gen_neg(int inout, int n)
	{
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegSVec t = getInputRegVec(inout, n);
		const ZRegS sign(ftr.allocIdx());
		setInt(sign, 1u << 31);
		LP_(i, n) eor(t[i], p0, sign);
	}
	void gen_inv(int inout, int n)
	{
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegSVec t0 = getInputRegVec(inout, n);
		const ZRegSVec t1 = getTmpRegVec(ftr, n);
		const ZRegSVec t2 = getTmpRegVec(ftr, n);

		LP_(i, n) frecpe(t1[i], t0[i]);
		LP_(i, n) frecps(t2[i], t0[i], t1[i]);
		LP_(i, n) fmul(t1[i], t1[i], t2[i]);

		LP_(i, n) frecps(t2[i], t0[i], t1[i]);
		LP_(i, n) fmul(t0[i], t1[i], t2[i]);
	}
	void gen_exp(int inout, int n)
	{
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegSVec t0 = getInputRegVec(inout, n);
		const ZRegSVec t1 = getTmpRegVec(ftr, n);
		const ZRegSVec t2 = getTmpRegVec(ftr, n);

		if (opt.use_mem) {
			const ZRegS c1(ftr.allocIdx());
			const ZRegS c2(ftr.allocIdx());

	//		fmin(t0, p0, expMax.s);
	//		fmax(t0, p0, expMin.s);
			setFloat(c1, g_expTbl.log2_e);
			LP_(i, n) fmul(t0[i], t0[i], c1);
			LP_(i, n) {
				movprfx(t1[i], p0, t0[i]); // clear implicit dependency
				frintm(t1[i], p0, t0[i]); // floor : float -> float
			}
			LP_(i, n) fcvtzs(t2[i], p0, t1[i]); // n = float -> int
			LP_(i, n) fsub(t1[i], t0[i], t1[i]); // a
			setFloat(c1, 1.0f);
			LP_(i, n) fadd(t0[i], t1[i], c1); // b = 1 + a
			LP_(i, n) lsr(t1[i], t0[i], 17); // bL
			LP_(i, n) fexpa(t1[i], t1[i]); // c = fexpa(bL)
			LP_(i, n) fscale(t1[i], p0, t2[i]); // t[i+1] *= 2^n
			setInt(c1, g_expTbl.not_mask17);
			LP_(i, n) and_(ZRegD(t2[i].getIdx()), ZRegD(t0[i].getIdx()), ZRegD(c1.getIdx()));
			LP_(i, n) fsub(t2[i], t0[i], t2[i]); // z

			setFloat(c1, g_expTbl.coeff1);
			setFloat(c2, g_expTbl.coeff2);
			LP_(i, n) {
				movprfx(t0[i], p0, c2);
				fmad(t0[i], p0, t2[i], c1);
			}
			setFloat(c1, 1.0f);
			LP_(i, n) fmad(t0[i], p0, t2[i], c1);
			LP_(i, n) fmul(t0[i], t1[i], t0[i]);
		} else {
			const ZRegS log2_e(getFloatIdx(g_expTbl.log2_e));
			const ZRegD not_mask17(getFloatIdx(u2f(g_expTbl.not_mask17)));
			const ZRegS one(getFloatIdx(1.0));
			const ZRegS coeff1(getFloatIdx(g_expTbl.coeff1));
			const ZRegS coeff2(getFloatIdx(g_expTbl.coeff2));

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
	}
	void gen_cosh(int inout, int n)
	{
		const ZRegSVec t0 = getInputRegVec(inout, n);
		/*
			X = exp(|x|)
			cosh(x) = (X + 1/X) * 0.5
		*/
		LP_(i, n) fabs(t0[i], p0, t0[i]);
		gen_exp(inout, n);
		IndexRangeManager ftr(funcTmpReg_);
		const ZRegSVec t1 = getTmpRegVec(ftr, n);
		LP_(i, n) mov(t1[i], p0, t0[i]);
		gen_inv(t1[0].getIdx(), n);
		LP_(i, n) fadd(t0[i], t0[i], t1[i]);
		LP_(i, n) fmul(t0[i], p0, 0.5);
	}
	void gen_log(int inout, int n)
	{
		const int logN = LogTbl::N;
		ZRegSVec tbl;
		int offset = 0;
		if (opt.log_use_mem) {
			offset = getConstTblOffsetToDataReg(g_logTbl.coef, logN * 4);
		} else {
			for (int i = 0; i < logN; i++) {
				tbl.push_back(ZRegS(getFloatIdx(g_logTbl.coef[i])));
			}
		}

		IndexRangeManager ftr(funcTmpReg_);
		IndexRangeManager ftm(funcTmpMask_);

		const ZRegSVec t0 = getInputRegVec(inout, n);
		const ZRegSVec t1 = getTmpRegVec(ftr, n);
		const ZRegSVec t2 = getTmpRegVec(ftr, n);
		ZRegSVec keep;
		if (opt.logp1) {
			keep = getTmpRegVec(ftr, n);
			LP_(i, n) mov(keep[i], p0, t0[i]);
		}

		if (opt.log_use_mem) {
			const ZRegS c1(ftr.allocIdx());
			const ZRegS c2(ftr.allocIdx());
			setInt(c2, 127 << 23);
			LP_(i, n) sub(t1[i], t0[i], c2);
			LP_(i, n) asr(t1[i], t1[i], 23);
			setInt(c1, 0x7fffff);
			// int -> float
			LP_(i, n) scvtf(t1[i], p0, t1[i]);
			LP_(i, n) and_(t0[i], p0, c1);
			LP_(i, n) orr(t0[i], p0, c2);
			setFloat(c1, 2.0f / 3);
			setFloat(c2, 1.0f);
			// fnmsb(a, b, c) = a * b - c
			LP_(i, n) fnmsb(t0[i], p0, c1, c2);
			setFloat(c1, log(1.5f));
			setFloat(c2, log(2.0f));
			LP_(i, n) fmad(t1[i], p0, c2, c1);
		} else {
			const ZRegS i127shl23(getConstIdx(127 << 23));
			const ZRegS x7fffff(getConstIdx(0x7fffff));
			const ZRegS log2(getFloatIdx(g_logTbl.log2));
			const ZRegS f2div3(getFloatIdx(g_logTbl.f2div3));
			const ZRegS log1p5(getFloatIdx(g_logTbl.log1p5));
			const ZRegS one(getFloatIdx(1.0));
			LP_(i, n) sub(t1[i], t0[i], i127shl23);
			LP_(i, n) asr(t1[i], t1[i], 23);
			// int -> float
			LP_(i, n) scvtf(t1[i], p0, t1[i]);
			LP_(i, n) and_(t0[i], p0, x7fffff);
			LP_(i, n) orr(t0[i], p0, i127shl23);
			// fnmsb(a, b, c) = a * b - c
			LP_(i, n) fnmsb(t0[i], p0, f2div3, one);
			LP_(i, n) fmad(t1[i], p0, log2, log1p5);
		}

		if (opt.logp1) {
			const ZRegS c1(ftr.allocIdx());
			fcpy(c1, p0, 1.0f);
			LP_(i, n) fsub(t2[i], keep[i], c1); // x-1

			const PRegSVec mask = getTmpMaskVec(ftm, n);
			fcpy(c1, p0, 1.0f/8);
			LP_(i, n) facge(mask[i], p0, c1, t2[i]); // 1/8 >= abs(x-1)
			LP_(i, n) mov(t0[i], mask[i], t2[i]);
			LP_(i, n) eor(t1[i], mask[i], t1[i]);
		}
		// fmad(a, b, c) ; a = a * b + c
		if (opt.log_use_mem) {
			const ZRegS c1(ftr.allocIdx());
			const ZRegS c2(ftr.allocIdx());
			ldr(tmp32_, ptr(dataReg_, offset + (logN - 1) * 4));
			dup(c1, tmp32_);
			ldr(tmp32_, ptr(dataReg_, offset + (logN - 2) * 4));
			dup(c2, tmp32_);

			LP_(i, n) {
				movprfx(t2[i], p0, c1);
				fmad(t2[i], p0, t0[i], c2);
			}
			for (int j = logN - 3; j >= 0; j--) {
				ldr(tmp32_, ptr(dataReg_, offset + j * 4));
				dup(c1, tmp32_);
				LP_(i, n) fmad(t2[i], p0, t0[i], c1);
			}
		} else {
			LP_(i, n) {
				movprfx(ZReg(t2[i].getIdx()), ZReg(tbl[logN - 1].getIdx()));
				fmad(t2[i], p0, t0[i], tbl[logN - 2]);
			}
			for (int j = logN - 3; j >= 0; j--) {
				LP_(i, n) fmad(t2[i], p0, t0[i], tbl[j]);
			}
		}
		// a * x + e
		LP_(i, n) fmad(t0[i], p0, t2[i], t1[i]);
	}
	void gen_debugFunc(int inout, int n)
	{
		if (debug) printf("debugFunc z%d (%d)\n", inout, n);
		static const float tbl[] = {
			1, 3, 5, 7, 9, 11, 13, 15, 17
		};
		const ZRegSVec t0 = getInputRegVec(inout, n);
#if 0
		LP_(i, n) {
			ld1w(t0[i], p0, ptr(dataReg_, getConstTblOffsetToDataReg(tbl, sizeof(tbl)) / SimdArray::byteSize));
		}
#else
		const ZRegS t(getConstTblIdx(tbl, sizeof(tbl)));
printf("idx=%d\n", getConstTblIdx(tbl, sizeof(tbl)));
		LP_(i, n) mov(t0[i], p0, t);
#endif
	}
	void gen_tanh(int inout, int n)
	{
		throw cybozu::Exception("not support gen_tanh") << inout << n;
	}
};

} // namespace sg

