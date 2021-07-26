#include "parser.hpp"
#ifdef SG_X64
#include "x64/main.hpp"
#endif
#include <stdio.h>

struct SgCode {
	sg::Generator gen;
};

SgCode* SgCreate()
{
	return new SgCode();
}

void SgDestroy(SgCode *sg)
{
	delete sg;
}

const SgFuncFloat1* SgGetFuncFloat1(SgCode *sg, const char *varName, const char *src)
	try
{
	sg::TokenList tl;
	tl.setVar(varName);
	sg::Parser parser;
	parser.parse(tl, src);
	sg->gen.exec(tl);
	return sg->gen.getAddrFloat1();
} catch (std::exception& e) {
	fprintf(stderr, "SgGetFuncFloat1 %s\n", e.what());
	return 0;
}


