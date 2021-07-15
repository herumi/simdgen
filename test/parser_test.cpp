#define CYBOZU_TEST_DISABLE_AUTO_RUN
#include <cybozu/test.hpp>
#include <cybozu/inttype.hpp>
#include "parser.hpp"
#include <iostream>

CYBOZU_TEST_AUTO(parseFloat)
{
	const struct {
		const char *src;
		bool ok;
		float v;
	} tbl[] = {
		{ "0", true, 0 },
		{ "0.0", true, 0 },
		{ "1.5", true, 1.5 },
		{ "-1", true, -1 },
		{ "+1", true, 1 },
		{ "1e10", true, 1e10 },
		{ "1e-10", true, 1e-10 },
		{ "1e+10", true, 1e+10 },
		{ "1.23456", true, 1.23456 },

		{ "++1", false, 0 },
		{ "+1-", false, 0 },
		{ "e10", false, 0 },
		{ "1e1e5", false, 0 },
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		const char *begin = tbl[i].src;
		const char *end = begin + strlen(begin);
		float f;
		const char *next = sg::parseFloat(&f, begin, end);
		bool ok = next == end;
		CYBOZU_TEST_EQUAL(ok, tbl[i].ok);
		if (ok) {
			CYBOZU_TEST_EQUAL(sg::f2u(f), sg::f2u(tbl[i].v));
		}
	}
}

CYBOZU_TEST_AUTO(parseVar)
{
	const struct {
		const char *src;
		bool ok;
		const char *v;
	} tbl[] = {
		{ "a", true, "a" },
		{ "abc", true, "abc" },
		{ "abc_EF5z", true, "abc_EF5z" },
		{ "-1", false, 0 },
		{ "0", false, 0 },
		{ "0123", false, 0 },
		{ "a+b", false, 0 },
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		const char *begin = tbl[i].src;
		const char *end = begin + strlen(begin);
		std::string v;
		const char *next = sg::parseVar(v, begin, end);
		bool ok = next == end;
		CYBOZU_TEST_EQUAL(ok, tbl[i].ok);
		if (ok) {
			CYBOZU_TEST_EQUAL(v, tbl[i].v);
		}
	}
}

CYBOZU_TEST_AUTO(parse)
{
	sg::TokenList tl;
	const char *varTbl[] = {
		"x",
		"y",
		"z",
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(varTbl); i++) {
		tl.setVar(varTbl[i]);
	}
//	const char *src = "x + 1/(2 + exp(x / y)) -1.3 + 1/z";
//	const char *src = "log(exp(x)+tanh(y)*1.2)";
	const char *src = "x*2-3";
	printf("src=%s\n", src);
	sg::Parser parser;
	parser.parse(tl, src);
	tl.put();
	sg::Printer printer;
	printer.exec(tl);
}

std::string g_src;

#ifdef SG_X64
#include "x64/main.hpp"
CYBOZU_TEST_AUTO(x64)
{
	const char *src = "x*2e3-3.1415";
	if (!g_src.empty()) {
		src = g_src.c_str();
	}
	sg::TokenList tl;
	tl.setVar("x");
	sg::Parser parser;
	parser.parse(tl, src);
	sg::Generator gen;
	gen.exec(tl);
	float x[5] = { 1, 2, 3, 4, 5 };
	float y[5];
	gen.addr(y, x, 5);
	for (size_t i = 0; i < 5; i++) {
		printf("%zd %f %f\n", i, x[i], y[i]);
	}
}
#endif

int main(int argc, char *argv[])
{
	if (argc > 1) g_src = argv[1];
	return cybozu::test::autoRun.run(argc, argv);
}

