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
	void* addr_;
	Label dataL_;
	Reg64 dataReg_;

	Generator()
		: CodeGenerator(totalSize, DontSetProtectRWE)
		, addr_(0)
		, dataReg_(rax)
	{
		simdByte_ = 512 / 8;
		setFuncInfoTbl();
	}
	~Generator()
	{
		setProtectModeRW();
	}
	SgFuncFloat1 getAddrFloat1() const { return (SgFuncFloat1)addr_; }
	SgFuncFloat1Reduce getAddrFloat1Reduce() const { return (SgFuncFloat1Reduce)addr_; }

	void gen_reduce(int red, int src)
	{
		switch (reduceFuncType_) {
		case RedSum: gen_add(red, red, src); break;
		default:
			throw cybozu::Exception("gen_reduce:bad reduceFuncType_") << reduceFuncType_;
		}
	}
	// x[0] = sum(x[0:...15]) using s
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
	void reduceAll()
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
		addr_ = getCurr<void*>();
		{
			int keepN = 0;
			if (totalN_ > maxFreeN) keepN = totalN_ - maxFreeN;
			StackFrame sf(this, 3, 1 | UseRCX, keepN * simdByte_);
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
				for (int i = 0; i < unrollN_; i++) {
					Zmm red(getReduceVarIdx() + i);
					vxorps(red, red);
				}
			}

			Label cmp1L, cmp2L, exitL;
			jmp(cmp1L, T_NEAR);
			puts("execOneLoop lp");
		Label lp1 = L(); // while (n >= 16 * unrollN_)
			for (int i = 0; i < unrollN_; i++) {
				vmovups(Zmm(getVarIdx(i)), ptr[src + i * simdByte_]);
			}
			execOneLoop(tl, unrollN_);
			for (int i = 0; i < unrollN_; i++) {
				outputOne(dst, i);
			}
			add(src, 64 * unrollN_);
			if (reduceFuncType_ < 0) add(dst, 64 * unrollN_);
			sub(n, 16 * unrollN_);
		L(cmp1L);
			cmp(n, 16  * unrollN_);
			jge(lp1, T_NEAR);
			puts("execOneLoop remain");

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

			mov(eax, 1);
			shl(eax, cl);
			sub(eax, 1);
			kmovd(k1, eax);
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
	void gen_inv(int inout, int n)
	{
		if (debug) printf("inv z%d\n", inout);
		const Zmm two(getFloatIdx(2.0));
		IndexRangeManager ftr(funcTmpReg_);
		ZmmVec t0, t1;
		for (int i = 0; i < n; i++) {
			t0.push_back(Zmm(inout + i));
			t1.push_back(Zmm(ftr.allocIdx()));
		}
		for (int i = 0; i < n; i++) vrcp14ps(t1[i], t0[i]);
		for (int i = 0; i < n; i++) vfnmadd213ps(t0[i], t1[i], two);
		for (int i = 0; i < n; i++) vmulps(t0[i], t0[i], t1[i]);
	}
	void gen_exp(int inout, int n)
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
		IndexRangeManager ftr(funcTmpReg_);
		ZmmVec t0, t1, t2;
		for (int i = 0; i < n; i++) {
			t0.push_back(Zmm(inout + i));
			t1.push_back(Zmm(ftr.allocIdx()));
			t2.push_back(Zmm(ftr.allocIdx()));
		}

		for (int i = 0; i < n; i++) vmulps(t0[i], log2_e);
		for (int i = 0; i < n; i++) vrndscaleps(t1[i], t0[i], 0); // n = round(x)
		for (int i = 0; i < n; i++) vsubps(t0[i], t1[i]); // a
		for (int i = 0; i < n; i++) vmulps(t0[i], log2);
		for (int i = 0; i < n; i++) vmovaps(t2[i], tbl[4]);
		for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[3]);
		for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[2]);
		for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[1]);
		for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[0]);
		for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[0]);
		for (int i = 0; i < n; i++) vscalefps(t0[i], t2[i], t1[i]); // t2 * 2^t1
	}
	void gen_cosh(int inout, int n)
	{
		if (debug) printf("cosh z%d\n", inout);
		const Zmm f0p5(getFloatIdx(0.5));
		const Zmm x7fffffff(getFloatIdx(u2f(0x7fffffff)));
		ZmmVec t0;
		for (int i = 0; i < n; i++) {
			t0.push_back(Zmm(inout + i));
		}
		/*
			X = exp(|x|)
			cosh(x) = (X + 1/X) * 0.5
		*/
		for (int i = 0; i < n; i++) {
			vandps(t0[i], t0[i], x7fffffff);
		}
		gen_exp(inout, n);
		IndexRangeManager ftr(funcTmpReg_);
		ZmmVec t1;
		for (int i = 0; i < n; i++) {
			t1.push_back(Zmm(ftr.allocIdx()));
			vmovaps(t1[i], t0[i]);
		}
		gen_inv(t1[0].getIdx(), n);
		for (int i = 0; i < n; i++) vaddps(t0[i], t0[i], t1[i]);
		for (int i = 0; i < n; i++) vmulps(t0[i], t0[i], f0p5);
	}
	void gen_log(int inout, int n)
	{
		if (debug) printf("gen_log %d\n", inout);
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
		IndexRangeManager ftr(funcTmpReg_);
		IndexRangeManager ftm(funcTmpMask_);
		ZmmVec t0, t1, t2, keep;
		OpmaskVec mask;
		for (int i = 0; i < n; i++) {
			t0.push_back(Zmm(inout + i));
			t1.push_back(Zmm(ftr.allocIdx()));
			t2.push_back(Zmm(ftr.allocIdx()));
			keep.push_back(Zmm(ftr.allocIdx()));
			mask.push_back(Opmask(ftm.allocIdx()));
		}

		for (int i = 0; i < n; i++) vmovaps(keep[i], t0[i]);
		for (int i = 0; i < n; i++) vpsubd(t1[i], t0[i], i127shl23);
		for (int i = 0; i < n; i++) vpsrad(t1[i], t1[i], 23); // e
		for (int i = 0; i < n; i++) vcvtdq2ps(t1[i], t1[i]); // float(e)
		for (int i = 0; i < n; i++) vpandd(t0[i], t0[i], x7fffff);
		for (int i = 0; i < n; i++) vpord(t0[i], t0[i], i127shl23); // y

		for (int i = 0; i < n; i++) vfmsub213ps(t0[i], f2div3, tbl[0]); // a
		for (int i = 0; i < n; i++) vfmadd213ps(t1[i], log2, log1p5); // e
#if 1
		for (int i = 0; i < n; i++) vsubps(t2[i], keep[i], one);
		for (int i = 0; i < n; i++) vandps(t2[i], t2[i], x7fffffff);
		for (int i = 0; i < n; i++) vcmpltps(mask[i], t2[i], f1div8);
		for (int i = 0; i < n; i++) vsubps(t0[i]|mask[i], keep[i], one);
		for (int i = 0; i < n; i++) vxorps(t1[i]|mask[i], t1[i]);
#endif

		int logN = g_logTbl.N;
		for (int i = 0; i < n; i++) vmovaps(t2[i], tbl[logN - 1]);
		for (int j = logN - 2; j >= 0; j--) {
			for (int i = 0; i < n; i++) vfmadd213ps(t2[i], t0[i], tbl[j]);
		}
		for (int i = 0; i < n; i++) vfmadd213ps(t0[i], t2[i], t1[i]);
	}
	void gen_tanh(int inout, int n)
	{
		throw cybozu::Exception("not support gen_tanh") << inout << n;
	}
};

} // namespace sg
