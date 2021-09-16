#define SG_DLL_EXPORT
#include "parser.hpp"
#ifdef SG_X64
#include "x64/main.hpp"
#endif
#ifdef SG_AARCH64
#include "aarch64/main.hpp"
#endif
#include <stdio.h>

const sg::ExpTbl sg::g_expTbl;
const sg::LogTbl sg::g_logTbl;

struct SgCode {
	sg::Generator gen;
};

SgCode* SgCreate()
	try
{
	return new SgCode();
} catch (std::exception& e) {
	return 0;
}

void SgDestroy(SgCode *sg)
{
	delete sg;
}

static void setup(SgCode *sg, const char *varName, const char *src)
	try
{
	sg::TokenList tl;
	tl.setVar(varName);
	sg::Parser parser;
	parser.parse(tl, src);
	sg->gen.exec(tl);
	sg->gen.opt.dump(sg->gen.addr_, sg->gen.getSize());
} catch (std::exception& e) {
	fprintf(stderr, "SgGetFuncFloat1 %s\n", e.what());
}


SgFuncFloat1 SgGetFuncFloat1(SgCode *sg, const char *varName, const char *src)
{
	setup(sg, varName, src);
	return sg->gen.getAddrFloat1();
}

SgFuncFloat1Reduce SgGetFuncFloat1Reduce(SgCode *sg, const char *varName, const char *src)
{
	setup(sg, varName, src);
	return sg->gen.getAddrFloat1Reduce();
}
