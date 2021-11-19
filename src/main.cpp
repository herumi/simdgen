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

const void* SgGetFuncAddr(SgCode *sg, const char *src, const char *varName)
	try
{
	if (sg == 0) return 0;
	sg::TokenList tl;
	tl.setVar(varName);
	sg::Parser parser;
	parser.parse(tl, src);
	sg->gen.exec(tl);
	sg->gen.opt.dump(sg->gen.addr_, sg->gen.getSize() - ((const uint8_t*)sg->gen.addr_ - (const uint8_t*)sg->gen.getCode()));
	return sg->gen.getAddrFloat1();
} catch (std::exception& e) {
	if (sg->gen.opt.debug) {
		fprintf(stderr, "SgGetFuncAddr %s\n", e.what());
	}
	return 0;
}

