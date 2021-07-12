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
#include <map>
#include <memory>
#include <iostream>
#include <assert.h>
#include <cybozu/exception.hpp>
#include "gen.hpp"

namespace sg {

enum ValueType {
	None,
	Float,
	Var,
	Op,
	Func,
};

enum OpType {
	Add,
	Sub,
	Mul,
	Div,
};

enum FuncType {
	Inv,
	Exp,
	Log,
	Tanh,
};

const char *funcNameTbl[] = {
	"inv",
	"exp",
	"log",
	"tanh",
};

typedef std::vector<std::string> StrVec;
typedef std::vector<uint32_t> IntVec;

template<class T>
struct Index {
	std::vector<T> tbl;
	/*
		return index of s
		return -1 if not found
	*/
	int getIdx(const T& s, bool doThrow = true) const
	{
		for (int i = 0; i < (int)tbl.size(); i++) {
			if (tbl[i] == s) return i;
		}
		if (doThrow) throw cybozu::Exception("getIdx") << s;
		return -1;
	}
	void append(const T& s)
	{
		int idx = getIdx(s, false);
		if (idx < 0) tbl.push_back(s);
	}
	void put() const
	{
		for (size_t i = 0; i < tbl.size(); i++) {
			std::cout << i << ":" << tbl[i] << ' ';
		}
		std::cout << std::endl;
	}
	size_t size() const { return tbl.size(); }
};

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
				};
				if (v >= CYBOZU_NUM_OF_ARRAY(tbl)) {
					throw cybozu::Exception("bad Op") << v;
				}
				return tbl[v];
			}
		case Func:
			if (v >= CYBOZU_NUM_OF_ARRAY(funcNameTbl)) {
				throw cybozu::Exception("bad Func") << v;
			}
			return funcNameTbl[v];
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
	Index<std::string> varIdx;
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
	void appendFunc(int kind)
	{
		Value v;
		v.type = Func;
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
	void appendVar(const std::string& s)
	{
		varIdx.append(s);
	}
	uint32_t getVarIdx(const std::string& s)
	{
		return varIdx.getIdx(s);
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
		varIdx.put();
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
	template<class Generator>
	void setFloatConst(Generator& gen) const
	{
		for (size_t i = 0; i < f2uIdx.size(); i++) {
			uint32_t idx = gen.allocReg();
			gen.gen_setInt(idx, f2uIdx[i]);
		}
	}
	template<class Generator>
	void loadVar(Generator& gen) const
	{
		for (uint32_t i = 0; i < varIdx.size(); i++) {
			gen.gen_loadVar(gen.getVarBeginIdx() + i, i);
		}
	}
	template<class Generator>
	void execOneLoop(Generator& gen) const
	{
		const size_t n = vv.size();
		uint32_t pos = gen.getCurReg();
		for (size_t i = 0; i < n; i++) {
			const Value& v = vv[i];
			switch (v.type) {
			case Var:
				gen.gen_copy(pos++, gen.getVarBeginIdx() + v.v);
				break;
			case Float:
				gen.gen_copy(pos++, v.v);
				break;
			case Op:
				assert(pos > 1);
				pos--;
				switch (v.v) {
				case Add: gen.gen_add(pos - 1, pos - 1, pos); break;
				case Sub: gen.gen_sub(pos - 1, pos - 1, pos); break;
				case Mul: gen.gen_mul(pos - 1, pos - 1, pos); break;
				case Div: gen.gen_div(pos - 1, pos - 1, pos); break;
				default:
					throw cybozu::Exception("bad op") << i << v.v;
				}
				break;
			case Func:
				assert(pos > 0);
				switch (v.v) {
				case Inv: gen.gen_inv(pos - 1); break;
				case Exp: gen.gen_exp(pos - 1); break;
				case Log: gen.gen_log(pos - 1); break;
				case Tanh: gen.gen_tanh(pos - 1); break;
				default:
					throw cybozu::Exception("bad func") << i << v.v;
				}
				break;
			default:
				throw cybozu::Exception("bad type") << i << v.type;
			}
		}
	}
	template<class Generator>
	void exec(Generator& gen) const
	{
		gen.gen_init();
		setFloatConst(gen);
		gen.allocVar(varIdx.size());
puts("execOneLoop");
		loadVar(gen);
		execOneLoop(gen);
		gen.gen_saveVar(0, gen.getCurReg());
		gen.gen_end();
	}
	void execPrinter() const
	{
		Printer printer;
		exec(printer);
	}
};

inline bool isSpace(char c) {
	return c == ' ' || c == '\t';
}

// return next pointer if success else 0
const char* parseFloat(float *f, const char *begin, const char *end)
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

int getFuncKind(const std::string& str)
{
	for (uint32_t i = 0; i < CYBOZU_NUM_OF_ARRAY(funcNameTbl); i++) {
		if (str == funcNameTbl[i]) {
			return i;
		}
	}
	throw cybozu::Exception("getFuncKind:bad name") << str;
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
	Parser()
		: end_(0)
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
				uint32_t idx = tl.setFloatAndGetIdx(f);
				tl.appendIdx(Float, idx);
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
				uint32_t idx = tl.getVarIdx(str);
				tl.appendIdx(Var, idx);
				return next;
			}
			char c = *begin;
			if (c == '(') {
				const char *next = parseAddSub(begin + 1, tl);
				if (!isEnd(next) && *next == ')') return next + 1;
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
		printf("src=%s\n", begin);
		end_ = begin + str.size();
		begin = parseAddSub(begin, tl);
		if (!isEnd(begin)) {
			throw cybozu::Exception("extra string") << std::string(begin, end_);
		}
	}
};

} // sg

