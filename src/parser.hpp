#pragma once
/**
	@file
	@brief simple parser
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/
#include <memory>
#include <assert.h>
#include <cybozu/exception.hpp>
#include "tokenlist.hpp"
#include "gen.hpp"

namespace sg {

inline bool isSpace(char c) {
	return c == ' ' || c == '\t';
}

// return next pointer if success else 0
inline const char* parseFloat(float *f, const char *begin, const char *end)
{
//	printf("parseFloat1=[%s]\n", std::string(begin, end).c_str());
	/*
		digits::=digit+
		(sign)((digits).digists|digits(.))(e(sign)digits)
		  1                 2              3  4    5
	*/
	const char *sign = "+-";
	const char *digit = "0123456789.";
	const char *p = 0;
	int status = 0;
	while (begin != end) {
		char c = *begin;
		if (p == 0) {
			if (strchr(sign, c)) {
				p = begin++;
				status = 1;
				continue;
			}
			if (strchr(digit, c)) {
				p = begin++;
				status = 2;
				continue;
			}
			break;
		}
		switch (status) {
		case 1:
			if (strchr(digit, c)) {
				begin++;
				status = 2;
				continue;
			} else {
				goto EXIT;
			}
		case 2:
			if (strchr(digit, c)) {
				begin++;
				continue;
			} else if (c == 'e' || c == 'E') {
				begin++;
				status = 3;
				continue;
			} else {
				goto EXIT;
			}
		case 3:
		case 4:
			if (status == 3 && strchr(sign, c)) {
				begin++;
				status = 4;
				continue;
			}
			if (strchr(digit, c)) {
				begin++;
				status = 4;
				continue;
			} else {
				goto EXIT;
			}
		default:
			goto EXIT;
		}
	}
EXIT:;
	if (p) {
//		printf("parseFloat2=[%s]\n", std::string(p, begin).c_str());
		char *endp;
		*f = strtof(p, &endp);
		if (endp == begin) return begin;
	}
	*f = 0;
	return 0;
}

// return next pointer if success else 0
inline const char* parseVar(std::string& v , const char *begin, const char *end)
{
	const char *tbl = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
	const char *top = tbl + 10; // exclude [0-9]
	const char *p = 0;
	while (begin != end) {
		char c = *begin;
		if (p == 0 && strchr(top, c)) {
			p = begin;
			begin++;
			continue;
		}
		if (p && strchr(tbl, c)) {
			begin++;
			continue;
		}
		break;
	}
	if (p == 0) return 0;
	v.assign(p, begin);
	return begin;
}

/*
	var = [a-zA-Z_]([a-zA-Z_0-9]*)
	num = float
	term = var|num|(addSub)
	addSub = mulDiv ('+'|'-' mulDiv)*
	mulDiv = expr ('*'|'/' expr)
*/
struct Parser {
	const char *end_;
	int nest_;
	Parser()
		: end_(0)
		, nest_(0)
	{
	}
	const char *skipSpace(const char *begin)
	{
		while (begin != end_) {
			if (!isSpace(*begin)) break;
			begin++;
		}
		return begin;
	}
	bool isEnd(const char *begin) const { return begin == end_; }
	const char *parseTerm(const char *begin, TokenList& tl)
	{
		begin = skipSpace(begin);
		if (isEnd(begin)) throw cybozu::Exception("num empty");
		{
			float f;
			const char *next = parseFloat(&f, begin, end_);
			if (next) {
				tl.appendConst(f);
				nest_++;
				tl.updateMaxRegStackNum(nest_);
				return next;
			}
		}
		{
			std::string str;
			const char *next = parseVar(str, begin, end_);
			if (next && *(next = skipSpace(next)) == '(') {
				int kind = getFuncKind(str);
				const char *next2 = parseAddSub(next + 1, tl);
				if (!isEnd(next2) && *next2 == ')') {
					tl.appendFunc(kind);
					return next2 + 1;
				}
				throw cybozu::Exception("bad func") << str;
			}
			if (next) {
				tl.appendVar(str);
				nest_++;
				tl.updateMaxRegStackNum(nest_);
				return next;
			}
			char c = *begin;
			if (c == '(') {
				const char *next = parseAddSub(begin + 1, tl);
				if (!isEnd(next) && *next == ')') {
					return next + 1;
				}
				throw cybozu::Exception("bad parenthesis") << (isEnd(next) ? '_' : *next);
			}
		}
		throw cybozu::Exception("bad syntax") << std::string(begin, end_);
	}
	const char *parseMulDiv(const char *begin, TokenList& tl)
	{
		begin = parseTerm(begin, tl);
		while (!isEnd(begin)) {
			begin = skipSpace(begin);
			if (isEnd(begin)) break;
			char c = *begin;
			if (c == '*' || c == '/') {
				begin = parseTerm(begin + 1, tl);
				tl.appendOp(c == '*' ? Mul : Div);
				nest_--;
				continue;
			}
			break;
		}
		return begin;
	}
	const char *parseAddSub(const char *begin, TokenList& tl)
	{
		begin = parseMulDiv(begin, tl);
		while (!isEnd(begin)) {
			begin = skipSpace(begin);
			if (isEnd(begin)) break;
			char c = *begin;
			if (c == '+' || c == '-') {
				begin = parseMulDiv(begin + 1, tl);
				tl.appendOp(c == '+' ? Add : Sub);
				nest_--;
				continue;
			}
			break;
		}
		return begin;
	}
	void parse(TokenList& tl, const std::string& str)
	{
		const char *begin = str.c_str();
		printf("src=%s\n", begin);
		end_ = begin + str.size();
		nest_ = 0;
		tl.clear();
		begin = parseAddSub(begin, tl);
		if (!isEnd(begin)) {
			throw cybozu::Exception("extra string") << std::string(begin, end_);
		}
	}
};

} // sg

