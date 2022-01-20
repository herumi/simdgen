#pragma once
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
#include <simdgen/simdgen.h>
#include <cybozu/exception.hpp>

using namespace Xbyak;
using namespace Xbyak::util;

typedef std::vector<Zmm> ZmmVec;
typedef std::vector<Opmask> OpmaskVec;

namespace sg {

#ifdef XBYAK64_WIN
// zmm0, ..., zmm4 are free
const int maxFreeN = 5;
#else
// zmm0, ..., zmm6 are free
const int maxFreeN = 7;
#endif

struct Generator : CodeGenerator, sg::GeneratorBase {
	static const size_t dataSize = 4096;
	static const size_t codeSize = 8192;
	static const size_t totalSize = dataSize + codeSize;
	Reg64 dataReg_;
	Reg32 tmp32_;
	Reg64 tmp64_;

	Generator()
		: CodeGenerator(totalSize, DontSetProtectRWE)
		, dataReg_(rdx)
		, tmp32_(eax)
		, tmp64_(rax)
	{
		simdByte_ = 512 / 8;
		maxSimdRegN_ = 32;
	}
	~Generator()
	{
		setProtectModeRW();
	}
	// x[0] = sum(s[0:...15])
	void reduceOne_sum(int d, int s)
	{
		assert(d != s);
		vextractf64x4(Ymm(d), Zmm(s), 1);
		vaddps(Ymm(d), Ymm(s), Ymm(d));
		vextractf128(Xmm(s), Ymm(d), 1);
		vaddps(Xmm(d), Xmm(s), Xmm(d));
		vpermilps(Xmm(s), Xmm(d), 0x4e);
		vaddps(Xmm(s), Xmm(s), Xmm(d));
		vmovaps(Xmm(d), Xmm(s));
		vshufps(Xmm(s), Xmm(s), Xmm(s), 0x55);
		vaddss(Xmm(d), Xmm(d), Xmm(s));
	}
	void outputOne(const Reg64& dst, int i, const Opmask& k = util::k0)
	{
		if (reduceFuncType_ >= 0) {
			int red = getReduceVarIdx() + i;
			int src = getTmpIdx(i);
			gen_reduce(red, src);
		} else {
			vmovups(ptr[dst + i * simdByte_]|k, Zmm(getTmpIdx(i)));
		}
	}
	void exec(const sg::TokenList& tl)
	{
		if (debug) puts("x64/exec");
		Cpu cpu;
		if (!cpu.has(Xbyak::util::Cpu::tAVX512F)) {
			throw cybozu::Exception("AVX-512 is not supported");
		}
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
		if (opt.break_point) int3();
		{
			int keepN = 0;
			if (totalN_ > maxFreeN) keepN = totalN_ - maxFreeN;
			StackFrame sf(this, 3, 1 | UseRCX | UseRDX, keepN * simdByte_);
			// store regs
			for (int i = 0; i < keepN; i++) {
				vmovups(ptr[rsp + i * simdByte_], Zmm(maxFreeN + i));
			}
			Reg64 dst, src, n;
			if (reduceFuncType_ >= 0) {
				src = sf.p[0];
				n = sf.p[1];
			} else {
				dst = sf.p[0];
				src = sf.p[1];
				n = sf.p[2];
			}
			dataReg_ = sf.t[0];
			mov(dataReg_, (size_t)dataL.getAddress());
			gen_setConst();
			if (reduceFuncType_ >= 0) {
				LP_(i, unrollN_) {
					Zmm red(getReduceVarIdx() + i);
					vxorps(red, red);
				}
			}

			Label cmp1L, cmp2L, exitL;
			jmp(cmp1L, T_NEAR);
		Label lp1 = L(); // while (n >= 16 * unrollN_)
			LP_(i, unrollN_) vmovups(Zmm(getVarIdx(i)), ptr[src + i * simdByte_]);
			execOneLoop(tl, unrollN_);
			LP_(i, unrollN_) outputOne(dst, i);
			add(src, 64 * unrollN_);
			if (reduceFuncType_ < 0) add(dst, 64 * unrollN_);
			sub(n, 16 * unrollN_);
		L(cmp1L);
			cmp(n, 16  * unrollN_);
			jge(lp1, T_NEAR);

			if (unrollN_ > 1) {
				jmp(cmp2L, T_NEAR);
			Label lp2 = L();
				vmovups(Zmm(getVarIdx(0)), ptr[src]);
				execOneLoop(tl, 1);
				outputOne(dst, 0);
				add(src, 64);
				if (reduceFuncType_ < 0) add(dst, 64);
				sub(n, 16);
			L(cmp2L);
				cmp(n, 16);
				jge(lp2, T_NEAR);
			}

			mov(ecx, n);
			test(ecx, ecx);
			jz(exitL, T_NEAR);

			mov(tmp32_, 1);
			shl(tmp32_, cl);
			sub(tmp32_, 1);
			kmovd(k1, tmp32_);
			vmovups(Zmm(getVarIdx(0))|k1|T_z, ptr[src]);
			execOneLoop(tl, 1);
			outputOne(dst, 0, k1);
		L(exitL);
			if (reduceFuncType_ >= 0) {
				reduceAll();
			}
			// restore regs
			for (int i = 0; i < keepN; i++) {
				vmovups(Zmm(maxFreeN + i), ptr[rsp + i * simdByte_]);
			}
		}
		if (debug) putLayout();
		setProtectModeRE();
	}
	ZmmVec getInputRegVec(int pos, int n)
	{
		ZmmVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(Zmm(pos + i));
		}
		return t;
	}
	ZmmVec getTmpRegVec(IndexRangeManager& irm, int n)
	{
		ZmmVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(Zmm(irm.allocIdx()));
		}
		return t;
	}
	OpmaskVec getTmpMaskVec(IndexRangeManager& irm, int n)
	{
		OpmaskVec t;
		for (int i = 0; i < n; i++) {
			t.push_back(Opmask(irm.allocIdx()));
		}
		return t;
	}
	void gen_setInt(int dst, uint32_t u)
	{
		vbroadcastss(Zmm(dst), ptr[dataReg_ + getConstOffsetToDataReg(u)]);
	}
	// set int u to z using tmp32_ instead of memory
	void setInt(const Zmm& z, uint32_t u)
	{
		mov(tmp32_, u);
		vpbroadcastd(z, tmp32_);
	}
	// set float u to z using tmp32_ instead of memory
	void setFloat(const Zmm& z, float f)
	{
		setInt(z, f2u(f));
	}
	void gen_fullLoad(int dst, uint32_t offset)
	{
		vmovups(Zmm(dst), ptr[dataReg_ + offset]);
	}
	void gen_copy(int dst, int src)
	{
		vmovaps(Zmm(dst), Zmm(src));
	}
	void gen_add(int dst, int src1, int src2)
	{
		vaddps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_sub(int dst, int src1, int src2)
	{
		vsubps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_mul(int dst, int src1, int src2)
	{
		vmulps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_div(int dst, int src1, int src2)
	{
		vdivps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_neg(int inout, int n)
	{
		IndexRangeManager ftr(funcTmpReg_);
		const ZmmVec t = getInputRegVec(inout, n);
		const Zmm sign(ftr.allocIdx());
		setInt(sign, 1u << 31);
		LP_(i, n) vxorps(t[i], sign);
	}
	void gen_inv(int inout, int n)
	{
		const Zmm two(getFloatIdx(2.0));
		IndexRangeManager ftr(funcTmpReg_);
		const ZmmVec t0 = getInputRegVec(inout, n);
		const ZmmVec t1 = getTmpRegVec(ftr, n);
		LP_(i, n) vrcp14ps(t1[i], t0[i]);
		LP_(i, n) vfnmadd213ps(t0[i], t1[i], two);
		LP_(i, n) vmulps(t0[i], t0[i], t1[i]);
	}
	void gen_exp(int inout, int n)
	{
		IndexRangeManager ftr(funcTmpReg_);
		const ZmmVec t0 = getInputRegVec(inout, n);
		const ZmmVec t1 = getTmpRegVec(ftr, n);
		const ZmmVec t2 = getTmpRegVec(ftr, n);

		if (opt.use_mem) {
			Zmm c1(ftr.allocIdx());
			setFloat(c1, g_expTbl.log2_e);
			LP_(i, n) vmulps(t0[i], c1);
			LP_(i, n) vrndscaleps(t1[i], t0[i], 0); // n = round(x)
			LP_(i, n) vsubps(t0[i], t1[i]); // a
			setFloat(c1, g_expTbl.log2);
			LP_(i, n) vmulps(t0[i], c1);
			setFloat(c1, g_expTbl.coef[4]);
			LP_(i, n) vmovaps(t2[i], c1);
			setFloat(c1, g_expTbl.coef[3]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			setFloat(c1, g_expTbl.coef[2]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			setFloat(c1, g_expTbl.coef[1]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			setFloat(c1, g_expTbl.coef[0]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			LP_(i, n) vscalefps(t0[i], t2[i], t1[i]); // t2 * 2^t1
		} else {
			const Zmm log2(getFloatIdx(g_expTbl.log2));
			const Zmm log2_e(getFloatIdx(g_expTbl.log2_e));
			const Zmm tbl[] = {
				Zmm(getFloatIdx(g_expTbl.coef[0])),
				Zmm(getFloatIdx(g_expTbl.coef[1])),
				Zmm(getFloatIdx(g_expTbl.coef[2])),
				Zmm(getFloatIdx(g_expTbl.coef[3])),
				Zmm(getFloatIdx(g_expTbl.coef[4])),
			};
			LP_(i, n) vmulps(t0[i], log2_e);
			LP_(i, n) vrndscaleps(t1[i], t0[i], 0); // n = round(x)
			LP_(i, n) vsubps(t0[i], t1[i]); // a
			LP_(i, n) vmulps(t0[i], log2);
			LP_(i, n) vmovaps(t2[i], tbl[4]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[3]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[2]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[1]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[0]);
			LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[0]);
			LP_(i, n) vscalefps(t0[i], t2[i], t1[i]); // t2 * 2^t1
		}
	}
	void gen_cosh(int inout, int n)
	{
		const Zmm f0p5(getFloatIdx(0.5));
		const Zmm x7fffffff(getFloatIdx(u2f(0x7fffffff)));
		const ZmmVec t0 = getInputRegVec(inout, n);
		/*
			X = exp(|x|)
			cosh(x) = (X + 1/X) * 0.5
		*/
		LP_(i, n) vandps(t0[i], t0[i], x7fffffff);
		gen_exp(inout, n);
		IndexRangeManager ftr(funcTmpReg_);
		const ZmmVec t1 = getTmpRegVec(ftr, n);
		LP_(i, n) vmovaps(t1[i], t0[i]);
		gen_inv(t1[0].getIdx(), n);
		LP_(i, n) vaddps(t0[i], t0[i], t1[i]);
		LP_(i, n) vmulps(t0[i], t0[i], f0p5);
	}
	void gen_log(int inout, int n)
	{
		int logN = g_logTbl.N;
		ZmmVec tbl;
		int offset = 0;
		if (opt.log_use_mem) {
			offset = getConstTblOffsetToDataReg(g_logTbl.coef, logN * 4);
		} else {
			for (int i = 0; i < logN; i++) {
				tbl.push_back(Zmm(getFloatIdx(g_logTbl.coef[i])));
			}
		}

		IndexRangeManager ftr(funcTmpReg_);
		IndexRangeManager ftm(funcTmpMask_);
		const ZmmVec t0 = getInputRegVec(inout, n);
		const ZmmVec t1 = getTmpRegVec(ftr, n);
		const ZmmVec t2 = getTmpRegVec(ftr, n);
		ZmmVec keep;
		if (opt.logp1) {
			keep = getTmpRegVec(ftr, n);
			LP_(i, n) vmovaps(keep[i], t0[i]);
		}

		if (opt.log_use_mem) {
			Zmm c1(ftr.allocIdx());
			Zmm c2(ftr.allocIdx());
			setInt(c2, 127 << 23);
			LP_(i, n) vpsubd(t1[i], t0[i], c2);
			LP_(i, n) vpsrad(t1[i], t1[i], 23); // e
			LP_(i, n) vcvtdq2ps(t1[i], t1[i]); // float(e)
			setInt(c1, 0x7fffff);
			LP_(i, n) vpandd(t0[i], t0[i], c1);
			LP_(i, n) vpord(t0[i], t0[i], c2); // y
			setFloat(c1, 2.0f / 3);
			setFloat(c2, 1.0f);
			LP_(i, n) vfmsub213ps(t0[i], c1, c2); // a
			setFloat(c1, log(1.5f));
			setFloat(c2, log(2.0f));
			LP_(i, n) vfmadd213ps(t1[i], c2, c1); // e
		} else {
			const Zmm log1p5(getFloatIdx(log(1.5)));
			const Zmm i127shl23(getFloatIdx(u2f(g_logTbl.i127shl23)));
			const Zmm x7fffff(getFloatIdx(u2f(g_logTbl.x7fffff)));
			const Zmm f2div3(getFloatIdx(g_logTbl.f2div3));
			const Zmm log2(getFloatIdx(g_logTbl.log2));
			const Zmm one(getFloatIdx(1.0f));
			LP_(i, n) vpsubd(t1[i], t0[i], i127shl23);
			LP_(i, n) vpsrad(t1[i], t1[i], 23); // e
			LP_(i, n) vcvtdq2ps(t1[i], t1[i]); // float(e)
			LP_(i, n) vpandd(t0[i], t0[i], x7fffff);
			LP_(i, n) vpord(t0[i], t0[i], i127shl23); // y
			LP_(i, n) vfmsub213ps(t0[i], f2div3, one); // a
			LP_(i, n) vfmadd213ps(t1[i], log2, log1p5); // e
		}

		if (opt.logp1) {
			OpmaskVec mask = getTmpMaskVec(ftm, n);
			const Zmm f1div8(getFloatIdx(g_logTbl.f1div8));
			const Zmm x7fffffff(getFloatIdx(u2f(g_logTbl.x7fffffff)));
			const Zmm one(getFloatIdx(1.0f));
			LP_(i, n) vsubps(t2[i], keep[i], one);
			LP_(i, n) vandps(t2[i], t2[i], x7fffffff);
			LP_(i, n) vcmpltps(mask[i], t2[i], f1div8);
			LP_(i, n) vsubps(t0[i]|mask[i], keep[i], one);
			LP_(i, n) vxorps(t1[i]|mask[i], t1[i]);
		}

		if (opt.log_use_mem) {
			Zmm c1(ftr.allocIdx());
			vbroadcastss(c1, ptr[dataReg_ + offset + (logN - 1) * 4]);
			LP_(i, n) vmovaps(t2[i], c1);
			for (int j = logN - 2; j >= 0; j--) {
				vbroadcastss(c1, ptr[dataReg_ + offset + j * 4]);
				LP_(i, n) vfmadd213ps(t2[i], t0[i], c1);
			}
		} else {
			LP_(i, n) vmovaps(t2[i], tbl[logN - 1]);
			for (int j = logN - 2; j >= 0; j--) {
				LP_(i, n) vfmadd213ps(t2[i], t0[i], tbl[j]);
			}
		}
		LP_(i, n) vfmadd213ps(t0[i], t2[i], t1[i]);
	}
	void gen_debugFunc(int inout, int n)
	{
		if (debug) printf("debugFunc z%d (%d)\n", inout, n);
		static const float tbl[] = {
			1, 3, 5, 7, 9, 11, 13, 15, 17
		};
		const ZmmVec t0 = getInputRegVec(inout, n);
#if 1
		LP_(i, n) {
			vmovups(t0[i], ptr[dataReg_ + getConstTblOffsetToDataReg(tbl, sizeof(tbl))]);
		}
#else
		const Zmm t(getConstTblIdx(tbl, sizeof(tbl)));
		LP_(i, n) vmovaps(t0[i], t);
#endif
	}
	void gen_tanh(int inout, int n)
	{
		throw cybozu::Exception("not support gen_tanh") << inout << n;
	}
};

} // namespace sg
