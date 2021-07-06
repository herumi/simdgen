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

struct UsedReg {
	size_t pos;
	UsedReg()
		: pos(0)
	{
	}
	int allocRegIdx()
	{
		if (pos < maxFreeN) {
			return freeTbl[pos++];
		}
		if (pos < maxFreeN + maxSaveN) {
			return saveTbl[pos++ - maxFreeN];
		}
		throw std::runtime_error("allocRegIdx");
	}
	int getKeepNum() const
	{
		if (pos < maxFreeN) return 0;
		return pos - maxFreeN;
	}
	size_t getPos() const { return pos; }
};

struct Env {
	StackFrame *sf;
	Label lp;
	Label exit;
	Env()
		: sf(0)
	{
	}
	~Env()
	{
		delete sf;
	}
};

struct Code : sg::GeneratorBase, CodeGenerator {
	static const size_t dataSize = 4096;
	static const size_t codeSize = 8192;
	MIE_ALIGN(4096) uint8_t buf_[dataSize + codeSize];
	Env env;
	FuncFloat1 *addr;
	Code()
		: CodeGenerator(sizeof(buf_), DontSetProtectRWE)
		, env()
		, addr(0)
	{
	}
	void gen_init()
	{
#if 0
		Cpu cpu;
		if (!cpu.has(Xbyak::util::Cpu::tAVX512F)) {
			throw cybozu::Exception("AVX-512 is not supported");
		}
#endif
		setSize(dataSize);
		addr = getCurr<FuncFloat1*>();
		env.sf = new StackFrame(this, 3);
		const Reg64& n = env.sf->p[2];
	L(env.lp);
		test(n, n);
		jz(env.exit);
	}
	void gen_setInt(int dst, uint32_t u)
	{
		mov(eax, u);
		vmovd(Xmm(dst), eax);
	}
	void gen_loadVar(int dst, uint32_t u)
	{
		vmovss(Xmm(dst), ptr[env.sf->p[1] + u * 4]);
	}
	void gen_saveVar(uint32_t u, int src)
	{
		vmovss(ptr[env.sf->p[0] + u * 4], Xmm(src));
	}
	void gen_copy(int dst, int src)
	{
		vmovss(Xmm(dst), Xmm(src));
	}
	void gen_add(int dst, int src1, int src2)
	{
		vaddss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_sub(int dst, int src1, int src2)
	{
		vsubss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_mul(int dst, int src1, int src2)
	{
		vmulss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_div(int dst, int src1, int src2)
	{
		vdivss(Xmm(dst), Xmm(src1), Xmm(src2));
	}
	void gen_end()
	{
		add(env.sf->p[1], 4);
		sub(env.sf->p[2], 1);
		jnz(env.lp, T_NEAR);
	L(env.exit);
		env.sf->close();
		setProtectModeRE();
	}
	~Code()
	{
		delete env.sf;
		setProtectModeRW();
	}
};

} // namespace sg

