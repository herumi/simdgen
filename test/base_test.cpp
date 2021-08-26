#include "parser.hpp"
#include <stdio.h>

int main(int argc, char *argv[])
{
	const char *src = argc == 1 ? "x" : argv[1];
	printf("src=%s\n", src);
	sg::TokenList tl;
	tl.setVar("x");
	sg::Parser parser;
	parser.parse(tl, src);
	tl.put();
	sg::GeneratorBase gen;
	gen.debug = true;
	gen.exec(tl);
}
