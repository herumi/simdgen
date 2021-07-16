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
	void prolog(GeneratorBase *gb)
	{
		for (int i = 0; i < constN; i++) {
			constIdx[i] = gb->allocReg();
		}
		for (int i = 0; i < tmpN; i++) {
			tmpIdx[i] = gb->allocReg();
		}
		// set gb
	}
	void main(GeneratorBase *gb, int inout) const
	{
		(void)gb;
		(void)inout;
		// generate code for inout using constIdx, tmpN
	}
};

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
	FuncFloat1 *addr;
	Label lpL;
	Label exitL;
	bool debug;
	sg::IntVec funcConstTbl[sg::FuncTypeN];
	int funcRegNumTbl[sg::FuncTypeN];

	Generator()
		: CodeGenerator(sizeof(buf_), DontSetProtectRWE)
		, env()
		, addr(0)
		, debug(true)
		, funcRegNumTbl()
	{
	}
	~Generator()
	{
		setProtectModeRW();
	}
	const sg::IntVec& getFuncConstTbl(int funcType) const
	{
		return funcConstTbl[funcType];
	}
	int getFuncRegNum(int funcType) const
	{
		return funcRegNumTbl[funcType];
	}
	void gen_init(const sg::TokenList& tl)
	{
		if (debug) puts("gen_init");
#if 0
		sg::IntVec constIdx = tl.getIntVec();
		// append constVar to constIdx
		for (uint32_t kind = 0; kind < sg::FuncEnd; kind++) {
			if (tl.isUsedFunc(kind)) {
				printf("used %s\n", sg::funcNameTbl[kind]);
			}
		}
#endif
#if 0
		Cpu cpu;
		if (!cpu.has(Xbyak::util::Cpu::tAVX512F)) {
			throw cybozu::Exception("AVX-512 is not supported");
		}
#endif
		updateConstIdx(tl);
		const uint32_t constN = constIdx_.size();
		const uint32_t varN = tl.getVarNum();
//		uint32_t totalN = constN + varN;
		setSize(dataSize);
		addr = getCurr<FuncFloat1*>();
		env.sf = new StackFrame(this, 3);
		for (uint32_t i = 0; i < constN; i++) {
			uint32_t idx = allocReg();
			gen_setInt(idx, constIdx_.getVal(i));
		}
		allocVar(varN);
		const Reg64& n = env.sf->p[2];
		test(n, n);
		jz(exitL, T_NEAR);
	L(lpL);
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
		vmovss(Xmm(dst), Xmm(src));
	}
	void gen_add(int dst, int src1, int src2)
	{
		if (debug) printf("vaddss z%d, z%d, z%d\n", dst, src1, src2);
		vaddss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_sub(int dst, int src1, int src2)
	{
		if (debug) printf("vsubss z%d, z%d, z%d\n", dst, src1, src2);
		vsubss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_mul(int dst, int src1, int src2)
	{
		if (debug) printf("vmulss z%d, z%d, z%d\n", dst, src1, src2);
		vmulss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_div(int dst, int src1, int src2)
	{
		if (debug) printf("vdivss z%d, z%d, z%d\n", dst, src1, src2);
		vdivss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_inv(int inout)
	{
		throw cybozu::Exception("not support gen_inv") << inout;
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
	void gen_end()
	{
		if (debug) puts("gen_end");
		add(env.sf->p[1], 4);
		add(env.sf->p[0], 4);
		sub(env.sf->p[2], 1);
		jnz(lpL, T_NEAR);
	L(exitL);
		env.sf->close();
		setProtectModeRE();
	}
};

} // namespace sg

