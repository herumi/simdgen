#pragma once
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
#include <simdgen/simdgen.h>
#include <cybozu/exception.hpp>

using namespace Xbyak;
using namespace Xbyak::util;

namespace sg {

const int freeTbl[] = {
	0, 1, 2, 3, 4,
#ifndef XBYAK64_WIN
	5, 6,
#endif
};

static const size_t maxFreeN = sizeof(freeTbl)/sizeof(freeTbl[0]);

const int saveTbl[] = {
#ifdef XBYAK64_WIN
	5, 6,
#endif
	7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
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
	Reg64 dataReg_;
	bool debug;

	Generator()
		: CodeGenerator(totalSize, DontSetProtectRWE)
		, addr_(0)
		, totalN_(0)
		, keepN_(0)
		, dataReg_(rax)
		, debug(true)
	{
		simdByte_ = 512 / 8;
		setFuncInfoTbl();
	}
	~Generator()
	{
		setProtectModeRW();
	}
	SgFuncFloat1* getAddrFloat1() const { return addr_; }
	void exec(const sg::TokenList& tl)
	{
		if (debug) puts("x64/exec");
#if 1
		Cpu cpu;
		if (!cpu.has(Xbyak::util::Cpu::tAVX512F)) {
			throw cybozu::Exception("AVX-512 is not supported");
		}
#endif
		Label dataL = L();
		updateConstIdx(tl);
		for (size_t i = 0; i < constN_; i++) {
			dd(constIdx_.getVal(i));
		}
		setSize(dataSize);
		addr_ = getCurr<SgFuncFloat1*>();
		{
			StackFrame sf(this, 3, 1 | UseRCX, keepN_ * simdByte_);
			// store regs
			for (int i = 0; i < keepN_; i++) {
				vmovups(ptr[rsp + i * simdByte_], Zmm(saveTbl[i]));
			}
			const Reg64& dst = sf.p[0];
			const Reg64& src = sf.p[1];
			const Reg64& n = sf.p[2];
			dataReg_ = sf.t[0];
			mov(dataReg_, (size_t)dataL.getAddress());
			gen_setConst();
			test(n, n);
			Label mod16, exit;
			mov(ecx, n);
			and_(n, ~15u);
			jz(mod16, T_NEAR);
			puts("execOneLoop lp");
		Label lp = L();
			vmovups(Zmm(getVarIdx(0)), ptr[src]);
			execOneLoop(tl);
			vmovups(ptr[dst], Zmm(getTmpIdx(0)));
			add(src, 64);
			add(dst, 64);
			sub(n, 16);
			jnz(lp, T_NEAR);
		L(mod16);
			puts("execOneLoop mod16");
			and_(ecx, 15);
			jz(exit, T_NEAR);
			mov(eax, 1);
			shl(eax, cl);
			sub(eax, 1);
			kmovd(k1, eax);
			vmovups(Zmm(getVarIdx(0))|k1|T_z, ptr[src]);
			execOneLoop(tl);
			vmovups(ptr[dst]|k1, Zmm(getTmpIdx(0)));
		L(exit);
			// restore regs
			for (int i = 0; i < keepN_; i++) {
				vmovups(Zmm(saveTbl[i]), ptr[rsp + i * simdByte_]);
			}
		}
		puts("setProtectModeRE");
		setProtectModeRE();
	}
	void gen_setInt(int dst, uint32_t u)
	{
		if (debug) {
			printf("mov eax, 0x%08x\n", u);
			printf("vpbroadcastd z%d, eax\n", dst);
		}
//		mov(eax, u);
//		vpbroadcastd(Zmm(dst), eax);
		vbroadcastss(Zmm(dst), ptr[dataReg_ + constIdx_.getIdx(u) * 4]);
	}
	void gen_copy(int dst, int src)
	{
		if (debug) printf("vmovaps z%d, z%d\n", dst, src);
		vmovaps(Zmm(dst), Zmm(src));
	}
	void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("vaddps z%d, z%d, z%d\n", dst, src1, src2);
		vaddps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("vsubps z%d, z%d, z%d\n", dst, src1, src2);
		vsubps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("vmulps z%d, z%d, z%d\n", dst, src1, src2);
		vmulps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("vdivps z%d, z%d, z%d\n", dst, src1, src2);
		vdivps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_inv(int inout)
	{
		if (debug) printf("inv z%d\n", inout);
		vdivps(Zmm(inout), Zmm(getConstIdx(f2u(1.0))), Zmm(inout));
	}
	void gen_exp(int inout)
	{
		if (debug) printf("exp z%d\n", inout);
		const Zmm log2(getFloatIdx(g_expTbl.log2));
		const Zmm log2_e(getFloatIdx(g_expTbl.log2_e));
		const Zmm tbl[] = {
			Zmm(getFloatIdx(g_expTbl.coef[0])),
			Zmm(getFloatIdx(g_expTbl.coef[1])),
			Zmm(getFloatIdx(g_expTbl.coef[2])),
			Zmm(getFloatIdx(g_expTbl.coef[3])),
			Zmm(getFloatIdx(g_expTbl.coef[4])),
		};
		const Zmm t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const Zmm t1(ftr.allocIdx());
		const Zmm t2(ftr.allocIdx());

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
	}
	void gen_log(int inout)
	{
		printf("gen_log %d\n", inout);
		const Zmm i127shl23(getFloatIdx(u2f(g_logTbl.i127shl23)));
		const Zmm x7fffff(getFloatIdx(u2f(g_logTbl.x7fffff)));
		const Zmm x7fffffff(getFloatIdx(u2f(g_logTbl.x7fffffff)));
		const Zmm one(getFloatIdx(1.0f));
		const Zmm f1div8(getFloatIdx(g_logTbl.f1div8));
		const Zmm f2div3(getFloatIdx(g_logTbl.f2div3));
		const Zmm log2(getFloatIdx(g_logTbl.log2));
		const Zmm log1p5(getFloatIdx(g_logTbl.log1p5));
		const Zmm tbl[] = {
			Zmm(getFloatIdx(g_logTbl.coef[0])),
			Zmm(getFloatIdx(g_logTbl.coef[1])),
			Zmm(getFloatIdx(g_logTbl.coef[2])),
			Zmm(getFloatIdx(g_logTbl.coef[3])),
			Zmm(getFloatIdx(g_logTbl.coef[4])),
			Zmm(getFloatIdx(g_logTbl.coef[5])),
			Zmm(getFloatIdx(g_logTbl.coef[6])),
			Zmm(getFloatIdx(g_logTbl.coef[7])),
			Zmm(getFloatIdx(g_logTbl.coef[8])),
		};
		const Zmm t0(inout);
		IndexRangeManager ftr(funcTmpReg_);
		const Zmm t1(ftr.allocIdx());
		const Zmm t2(ftr.allocIdx());
		const Zmm keep(ftr.allocIdx());
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
	}
	void gen_tanh(int inout)
	{
		throw cybozu::Exception("not support gen_tanh") << inout;
	}
};

} // namespace sg

