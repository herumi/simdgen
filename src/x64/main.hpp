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

#if 0
struct LogGen {
	int constN;
	int tmpN;
	int constIdx[5];
	int tmpIdx[4];
	LogGen()
		: constN(3)
		, tmpN(2)
	{
	}
	void init(const sg::TokenList&)
	{
		// determin useConstNum, useTmpNum
	}
	uint32_t getConstNum() const
	{
		return constN;
	}
	void main(GeneratorBase *gb, int inout) const
	{
		(void)gb;
		(void)inout;
		// generate code for inout using constIdx, tmpN
	}
};
#endif

struct Env {
	StackFrame *sf;
	Env()
		: sf(0)
	{
	}
	~Env()
	{
		delete sf;
	}
};

struct Generator : CodeGenerator, sg::GeneratorBase {
	static const size_t dataSize = 4096;
	static const size_t codeSize = 8192;
	MIE_ALIGN(4096) uint8_t buf_[dataSize + codeSize];
	Env env;
	FuncFloat1 *addr_;
	int totalN_;
	int keepN_;
	bool debug;

	Generator()
		: CodeGenerator(sizeof(buf_), DontSetProtectRWE)
		, env()
		, addr_(0)
		, totalN_(0)
		, keepN_(0)
		, debug(true)
	{
		simdByte_ = 512 / 8;
		setFuncInfoTbl();
	}
	~Generator()
	{
		setProtectModeRW();
	}
	const FuncFloat1* getAddrFloat1() const { return addr_; }
	void setFuncInfoTbl()
	{
		for (size_t i = 0; i < FuncTypeN; i++) {
			FuncInfo& fi = funcInfoTbl[i];
			if (i == Inv) {
				fi.constTbl.push_back(f2u(1.0));
				fi.tmpN = 1;
			}
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
		// setup constatns
		updateConstIdx(tl);
		// start to generate code
		setSize(dataSize);
		addr_ = getCurr<FuncFloat1*>();
		totalN_ = getTotalNum();
		keepN_ = getKeepNum(totalN_);
		printf("keepN=%d\n", keepN_);
		env.sf = new StackFrame(this, 3, keepN_ * simdByte_);
		for (int i = 0; i < keepN_; i++) {
			vmovups(ptr[rsp + i * simdByte_], Zmm(saveTbl[i]));
		}
		gen_setConst();
		const Reg64& n = env.sf->p[2];
		test(n, n);
		Label lpL, exitL;
		jz(exitL, T_NEAR);
	L(lpL);
		for (uint32_t i = 0; i < varN_; i++) {
			vmovss(Xmm(getVarIdx(0)), ptr[env.sf->p[1]]);
		}
		execOneLoop(tl);
		vmovss(ptr[env.sf->p[0]], Xmm(getTmpIdx(0)));
		add(env.sf->p[1], 4);
		add(env.sf->p[0], 4);
		sub(env.sf->p[2], 1);
		jnz(lpL, T_NEAR);
	L(exitL);
		for (int i = 0; i < keepN_; i++) {
			vmovups(Zmm(saveTbl[i]), ptr[rsp + i * simdByte_]);
		}
		env.sf->close();
		setProtectModeRE();
	}
	void gen_setInt(int dst, uint32_t u)
	{
		if (debug) {
			printf("mov eax, 0x%08x(%f)\n", u, u2f(u));
			printf("vmovd z%d, eax\n", dst);
		}
		mov(eax, u);
		vmovd(Xmm(dst), eax);
	}
	void gen_loadVar(int dst, uint32_t u)
	{
		if (u != 0) throw cybozu::Exception("gen_loadVar") << u;
		if (debug) printf("vmovss z%d, [%s]\n", dst, env.sf->p[1].toString());
		vmovss(Xmm(dst), ptr[env.sf->p[1]]);
	}
	void gen_saveVar(uint32_t u, int src)
	{
		if (u != 0) throw cybozu::Exception("gen_saveVar") << u;
		if (debug) printf("vmovss [%s], z%d\n", env.sf->p[0].toString(), src);
		vmovss(ptr[env.sf->p[0]], Xmm(src));
	}
	void gen_copy(int dst, int src)
	{
		if (debug) printf("vmovss z%d, z%d\n", dst, src);
		vmovaps(Zmm(dst), Zmm(src));
	}
	void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("vaddss z%d, z%d, z%d\n", dst, src1, src2);
		vaddps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("vsubss z%d, z%d, z%d\n", dst, src1, src2);
		vsubps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("vmulss z%d, z%d, z%d\n", dst, src1, src2);
		vmulps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("vdivss z%d, z%d, z%d\n", dst, src1, src2);
		vdivps(Zmm(dst), Zmm(src1), Zmm(src2));
	}
	void gen_inv(int inout)
	{
		if (debug) printf("inv z%d\n", inout);
		vdivps(Zmm(inout), Zmm(getConstIdx(f2u(1.0))), Zmm(inout));
//		throw cybozu::Exception("not support gen_inv") << inout;
	}
	void gen_exp(int inout)
	{
		throw cybozu::Exception("not support gen_exp") << inout;
	}
	void gen_log(int inout)
	{
		printf("gen_log %d\n", inout);
//		throw cybozu::Exception("not support gen_log") << inout;
	}
	void gen_tanh(int inout)
	{
		throw cybozu::Exception("not support gen_tanh") << inout;
	}
};

} // namespace sg

