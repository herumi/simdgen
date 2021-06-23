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

