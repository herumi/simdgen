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
		const ZRegS arg0 = ZReg(getVarIdx(0)).s;
		gen_setConst();
		Label skip;
		b(skip);
	Label lp = L();
		ld1w(arg0, p0, ptr(src));
		execOneLoop(tl);
		st1w(arg0, p0, ptr(dst));
		add(dst, dst, 64);
		sub(n, n, 16);
	L(skip);
		cmp(n, 16);
		bge(lp);

		Label cond;
		mov(tmpX_, 0);
		b(cond);
	Label lp2 = L();
		ld1w(arg0, p1, ptr(src, tmpX_, LSL, 2));
		execOneLoop(tl);
		st1w(arg0, p1, ptr(dst, tmpX_, LSL, 2));
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
			printf("mov eax, 0x%08x\n", u);
			printf("vpbroadcastd z%d, eax\n", dst);
		}
		mov(tmpW_, u);
		cpy(ZReg(dst).s, p0, tmpW_);
	}
	void gen_copy(int dst, int src)
	{
		if (debug) printf("vmovaps z%d, z%d\n", dst, src);
		mov(ZReg(dst).s, p0, ZReg(src).s);
	}
	void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("vaddps z%d, z%d, z%d\n", dst, src1, src2);
		fadd(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("vsubps z%d, z%d, z%d\n", dst, src1, src2);
		fsub(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("vmulps z%d, z%d, z%d\n", dst, src1, src2);
		fmul(ZReg(dst).s, ZReg(src1).s, ZReg(src2).s);
	}
	void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("vdivps z%d, z%d, z%d\n", dst, src1, src2);
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
#if 0
		const ZReg log2(getFloatIdx(g_expTbl.log2));
		const ZReg log2_e(getFloatIdx(g_expTbl.log2_e));
		const ZReg tbl[] = {
			ZReg(getFloatIdx(g_expTbl.coef[0])),
			ZReg(getFloatIdx(g_expTbl.coef[1])),
			ZReg(getFloatIdx(g_expTbl.coef[2])),
			ZReg(getFloatIdx(g_expTbl.coef[3])),
			ZReg(getFloatIdx(g_expTbl.coef[4])),
		};
		const ZReg t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const ZReg t1(ftr.allocIdx());
		const ZReg t2(ftr.allocIdx());

		vmulps(t0, log2_e);
		vrndscaleps(t1, t0, 0); // n = round(x)
		vsubps(t0, t1); // a
		vmulps(t0, log2);
		vmovaps(t2, tbl[4]);
		vfmadd213ps(t2, t0, tbl[3]);
		vfmadd213ps(t2, t0, tbl[2]);
		vfmadd213ps(t2, t0, tbl[1]);
		vfmadd213ps(t2, t0, tbl[0]);
		vfmadd213ps(t2, t0, tbl[0]);
		vscalefps(t0, t2, t1); // t2 * 2^t1
#endif
	}
	void gen_log(int inout)
	{
		printf("gen_log %d\n", inout);
#if 0
		const ZReg i127shl23(getFloatIdx(u2f(g_logTbl.i127shl23)));
		const ZReg x7fffff(getFloatIdx(u2f(g_logTbl.x7fffff)));
		const ZReg x7fffffff(getFloatIdx(u2f(g_logTbl.x7fffffff)));
		const ZReg one(getFloatIdx(1.0f));
		const ZReg f1div8(getFloatIdx(g_logTbl.f1div8));
		const ZReg f2div3(getFloatIdx(g_logTbl.f2div3));
		const ZReg log2(getFloatIdx(g_logTbl.log2));
		const ZReg log1p5(getFloatIdx(g_logTbl.log1p5));
		const ZReg tbl[] = {
			ZReg(getFloatIdx(g_logTbl.coef[0])),
			ZReg(getFloatIdx(g_logTbl.coef[1])),
			ZReg(getFloatIdx(g_logTbl.coef[2])),
			ZReg(getFloatIdx(g_logTbl.coef[3])),
			ZReg(getFloatIdx(g_logTbl.coef[4])),
			ZReg(getFloatIdx(g_logTbl.coef[5])),
			ZReg(getFloatIdx(g_logTbl.coef[6])),
			ZReg(getFloatIdx(g_logTbl.coef[7])),
			ZReg(getFloatIdx(g_logTbl.coef[8])),
		};
		const ZReg t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const ZReg t1(ftr.allocIdx());
		const ZReg t2(ftr.allocIdx());
		const ZReg keep(ftr.allocIdx());
		IndexRangeManager ftm(funcTmpMask_);
		const Opmask mask(ftm.allocIdx());

		vmovaps(keep, t0);
		vpsubd(t1, t0, i127shl23);
		vpsrad(t1, t1, 23); // e
		vcvtdq2ps(t1, t1); // float(e)
		vpandd(t0, t0, x7fffff);
		vpord(t0, t0, i127shl23); // y

		vfmsub213ps(t0, f2div3, tbl[0]); // a
		vfmadd213ps(t1, log2, log1p5); // e
#if 1
		vsubps(t2, keep, one);
		vandps(t2, t2, x7fffffff);
		vcmpltps(mask, t2, f1div8);
		vsubps(t0|mask, keep, one);
		vxorps(t1|mask, t1);
#endif

		int logN = g_logTbl.N;
		vmovaps(t2, tbl[logN - 1]);
		for (int i = logN - 2; i >= 0; i--) {
			vfmadd213ps(t2, t0, tbl[i]);
		}
		vfmadd213ps(t0, t2, t1);
#endif
	}
	void gen_tanh(int inout)
	{
		throw cybozu::Exception("not support gen_tanh") << inout;
	}
};

} // namespace sg

