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
		bool ret = sp::parseFloat(&f, begin, end);
		CYBOZU_TEST_EQUAL(ret, tbl[i].ok);
		if (ret) {
			CYBOZU_TEST_EQUAL(f, tbl[i].v);
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
		{ "ab+", true, "ab" },
		{ "abc_EF5z@", true, "abc_EF5z" },
		{ "-1", false, 0 },
		{ "0", false, 0 },
		{ "0123", false, 0 },
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		const char *begin = tbl[i].src;
		const char *end = begin + strlen(begin);
		std::string v;
		bool ret = sp::parseVar(v, begin, end);
		CYBOZU_TEST_EQUAL(ret, tbl[i].ok);
		if (ret) {
			CYBOZU_TEST_EQUAL(v, tbl[i].v);
		}
	}
}

