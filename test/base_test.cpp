#define CYBOZU_TEST_DISABLE_AUTO_RUN
#include <cybozu/test.hpp>
#include "parser.hpp"
#include <stdio.h>

const char *g_src;

CYBOZU_TEST_AUTO(print)
{
	printf("src=%s\n", g_src);
	sg::TokenList tl;
	tl.setVar("x");
	sg::Parser parser;
	parser.parse(tl, g_src);
	tl.put();
	sg::GeneratorBase gen;
	gen.debug = true;
	gen.exec(tl);
	gen.putLayout();
}

int main(int argc, char *argv[])
{
	g_src = argc == 1 ? "x" : argv[1];
	return cybozu::test::autoRun.run(argc, argv);
}
