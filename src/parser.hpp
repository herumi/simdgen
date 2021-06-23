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
#include <unordered_map>
#include <stack>
#include <cybozu/exception.hpp>

namespace sp {

enum NodeType {
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

struct Node;

typedef std::vector<std::unique_ptr<Node>> NodeVec;
typedef std::unordered_map<std::string, uint32_t> VarIdxTbl;

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
	NodeType type;
	uint32_t v;
};

struct Node {
	Value v;
	NodeVec child;
};

typedef std::vector<Value> ValueVec;

struct Ast {
	std::unique_ptr<Node> root;
	VarIdxTbl varIdxTbl;
	ValueVec vv;
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
	const char *begin_;
	const char *end_;
	void skipSpace()
	{
		while (begin_ != end_) {
			if (!isSpace(*begin_)) break;
			begin_++;
		}
	}
	bool isEnd() const { return begin_ == end_; }
	void parseTerm(Ast& ast)
	{
		skipSpace();
		if (isEnd()) throw cybozu::Exception("num empty");
		char c = *begin_;
		if (c == '(') {
			parseAddSub(ast);
			if (!isEnd() && *begin_ == ')') return;
			throw cybozu::Exception("bad parenthesis") << (isEnd() ? '_' : *begin_);
		}
		{
			float f;
		}
	}
	void parseMulDiv(Ast& ast)
	{
		parseTerm(ast);
		while (!isEnd()) {
			skipSpace();
			if (isEnd()) return;
			char c = *begin_;
			if (c == '*' || c == '/') {
				begin_++;
				parseTerm(ast);
				Value v;
				v.type = Op;
				v.v = c == '+' ? Mul : Div;
				ast.vv.push_back(v);
			}
		}
	}
	void parseAddSub(Ast& ast)
	{
		parseMulDiv(ast);
		while (!isEnd()) {
			skipSpace();
			if (isEnd()) return;
			char c = *begin_;
			if (c == '+' || c == '-') {
				begin_++;
				parseMulDiv(ast);
				Value v;
				v.type = Op;
				v.v = c == '+' ? Add : Sub;
				ast.vv.push_back(v);
			}
		}
	}
	void parse(Ast& ast, const std::string& str)
	{
		begin_ = str.c_str();
		end_ = begin_ + str.size();
		parseAddSub(ast);
		if (isEnd()) {
			throw cybozu::Exception("extra string") << std::string(begin_, end_);
		}
	}
};

} // sp

