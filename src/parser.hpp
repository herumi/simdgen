#pragma once
/**
	@file
	@brief simple parser
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/
#include <string>
#include <vector>
#include <memory>
#include <assert.h>
#include <cybozu/exception.hpp>

namespace sg {

enum ValueType {
	None,
	Float,
	Var,
	Op,
};

enum OpType {
	Add,
	Sub,
	Mul,
	Div,
	Abs,
};

typedef std::vector<std::string> StrVec;
typedef std::vector<uint32_t> IntVec;

inline float u2f(uint32_t u)
{
	float f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

inline uint32_t f2u(float f)
{
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

struct Value {
	ValueType type;
	uint32_t v;
	Value()
		: type(None)
		, v(0)
	{
	}
	std::string getStr() const
	{
		char buf[32];
		switch (type) {
		case None:
			return "None";
		case Float:
			snprintf(buf, sizeof(buf), "float{%u}", v);
			break;
		case Var:
			snprintf(buf, sizeof(buf), "var{%u}", v);
			break;
		case Op:
			{
				const char *tbl[] = {
					"add",
					"sub",
					"mul",
					"div",
					"abs",
				};
				if (v >= CYBOZU_NUM_OF_ARRAY(tbl)) {
					throw cybozu::Exception("bad Op") << v;
				}
				return tbl[v];
			}
		}
		return buf;
	}
	void put() const
	{
		printf("%s\n", getStr().c_str());
	}
};

typedef std::vector<Value> ValueVec;

template<class Vec, class T>
uint32_t setAndGetIdxT(Vec& vec, const T& x)
{
	const uint32_t n = vec.size();
	for (uint32_t i = 0; i < n; i++) {
		if (vec[i] == x) return i;
	}
	vec.push_back(x);
	return n;
}

struct TokenList {
	StrVec varIdx;
	IntVec f2uIdx;;
	ValueVec vv;
	void appendFloat(float f)
	{
		Value v;
		v.type = Float;
		v.v = f2u(f);
		vv.push_back(v);
	}
	void appendOp(int kind)
	{
		Value v;
		v.type = Op;
		v.v = kind;
		vv.push_back(v);
	}
	void appendIdx(ValueType type, uint32_t idx)
	{
		Value v;
		v.type = type;
		v.v = idx;
		vv.push_back(v);
	}
	uint32_t setFloatAndGetIdx(float f)
	{
		return setAndGetIdxT(f2uIdx, f2u(f));
	}
	uint32_t setVarAndGetIdx(const std::string& s)
	{
		return setAndGetIdxT(varIdx, s);
	}
	void putFloatIdx() const
	{
		for (uint32_t i = 0; i < f2uIdx.size(); i++) {
			printf("%u:%f ", i, u2f(f2uIdx[i]));
		}
		printf("\n");
	}
	void putVarIdx() const
	{
		for (uint32_t i = 0; i < varIdx.size(); i++) {
			printf("%u:%s ", i, varIdx[i].c_str());
		}
		printf("\n");
	}
	void putValueVec() const
	{
		for (size_t i = 0; i < vv.size(); i++) {
			printf("%s ", vv[i].getStr().c_str());
		}
		printf("\n");
	}
	void put() const
	{
		printf("varIdx ");
		putVarIdx();
		printf("floatIdx ");
		putFloatIdx();
		printf("token ");
		putValueVec();
	}
	void exec() const
	{
		const size_t n = vv.size();
		std::vector<const Value*> st(n);
		size_t pos = 0;
		for (size_t i = 0; i < n; i++) {
			const Value& v = vv[i];
			switch (v.type) {
			case Float:
			case Var:
				st[++pos] = &v;
				break;
			case Op:
				switch (v.v) {
				case Add:
					assert(pos > 0);
					printf("add %s, %s\n", st[pos - 1]->getStr().c_str(), st[pos]->getStr().c_str());
					pos--;
					break;
				case Sub:
					printf("sub %s, %s\n", st[pos - 1]->getStr().c_str(), st[pos]->getStr().c_str());
					pos--;
					break;
				case Mul:
					printf("mul %s, %s\n", st[pos - 1]->getStr().c_str(), st[pos]->getStr().c_str());
					pos--;
					break;
				case Div:
					printf("div %s, %s\n", st[pos - 1]->getStr().c_str(), st[pos]->getStr().c_str());
					pos--;
					break;
				case Abs:
				default:
					throw cybozu::Exception("bad op") << i << v.v;
				}
				break;
			default:
				throw cybozu::Exception("bad type") << i << v.type;
			}
		}
	}
};

inline bool isSpace(char c) {
	return c == ' ' || c == '\t';
}

// return next pointer if success else 0
const char* parseFloat(float *f, const char *begin, const char *end)
{
	const char *p = 0;
	while (begin != end) {
		char c = *begin;
		if (p == 0 && strchr("+-0123456789", c)) {
			p = begin;
			begin++;
			continue;
		}
		if (p && strchr(".e+-0123456789", c)) {
			begin++;
			continue;
		}
		break;
	}
	if (p) {
		char *endp;
		*f = strtof(p, &endp);
		if (endp == begin) return begin;
	}
	*f = 0;
	return 0;
}

// return next pointer if success else 0
const char* parseVar(std::string& v , const char *begin, const char *end)
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
		char c = *begin;
		if (c == '(') {
			const char *next = parseAddSub(begin + 1, tl);
			if (!isEnd(next) && *next == ')') return next + 1;
			throw cybozu::Exception("bad parenthesis") << (isEnd(next) ? '_' : *next);
		}
		{
			float f;
			const char *next = parseFloat(&f, begin, end_);
			if (next) {
				uint32_t idx = tl.setFloatAndGetIdx(f);
				tl.appendIdx(Float, idx);
				return next;
			}
		}
		{
			std::string str;
			const char *next = parseVar(str, begin, end_);
			if (next) {
				uint32_t idx = tl.setVarAndGetIdx(str);
				tl.appendIdx(Var, idx);
				return next;
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
				continue;
			}
			break;
		}
		return begin;
	}
	void parse(TokenList& tl, const std::string& str)
	{
		const char *begin = str.c_str();
		end_ = begin + str.size();
		begin = parseAddSub(begin, tl);
		if (!isEnd(begin)) {
			throw cybozu::Exception("extra string") << std::string(begin, end_);
		}
	}
};

} // sg

