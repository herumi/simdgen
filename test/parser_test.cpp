#include <cybozu/test.hpp>
#include <cybozu/inttype.hpp>
#include "parser.hpp"

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
		tl.setVarAndGetIdx(varTbl[i]);
	}
	sg::Parser parser;
//	const char *src = "x + 1/(2 + exp(x / y)) -1.3 + 1/z";
	const char *src = "log(exp(x)+tanh(y)*1.2)";
	printf("src=%s\n", src);
	parser.parse(tl, src);
	tl.put();
	tl.execPrinter();
}

CYBOZU_TEST_AUTO(x64)
{
}
