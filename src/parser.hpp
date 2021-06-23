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
#include <stack>
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

struct Node;

typedef std::vector<std::unique_ptr<Node>> NodeVec;
typedef std::vector<std::string> StrVec;

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
			snprintf(buf, sizeof(buf), "float{%f}", u2f(v));
			break;
		case Var:
			snprintf(buf, sizeof(buf), "var{%d}", v);
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

struct Node {
	Value v;
	NodeVec child;
	void put() const
	{
		printf("%s", v.getStr().c_str());
		if (child.empty()) return;
		printf(" [");
		for (size_t i = 0; i < child.size(); i++) {
			if (i > 0) printf(",");
			child[i]->put();
		}
		printf("]");
	}
};

typedef std::vector<Value> ValueVec;

struct TokenList {
	StrVec varIdx;
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
	void appendVar(int idx)
	{
		Value v;
		v.type = Var;
		v.v = idx;
		vv.push_back(v);
	}
	uint32_t setVarAndGetIdx(const std::string& s)
	{
		const uint32_t n = varIdx.size();
		for (uint32_t i = 0; i < n; i++) {
			if (varIdx[i] == s) return i;
		}
		varIdx.push_back(s);
		return n;
	}
	void putVarIdx() const
	{
		for (size_t i = 0; i < varIdx.size(); i++) {
			printf("%s:%d\n", varIdx[i].c_str(), (int)i);
		}
	}
	void putValueVec() const
	{
		for (size_t i = 0; i < vv.size(); i++) {
			vv[i].put();
		}
	}
	void put() const
	{
		putVarIdx();
		putValueVec();
	}
};

inline void cvtValueVecToNode(Node& root, const ValueVec& vv)
{
	root.child.clear();
	for (size_t i = 0; i < vv.size(); i++) {
		std::unique_ptr<Node> n(new Node());
		n->v = vv[i];
		root.child.push_back(std::move(n));
	}
}

inline void vecToTree(NodeVec& stack)
{
#if 0
	size_t i = 0;
	while (i < stack.size()) {
		if (stack[i].child.size() == 2) {
			Value v = stack[i].v;
			if (v.type == Op) {
				NodeVec t;
				t.
				continue;
			}
		}
		i++;
	}
#endif
}

struct Ast {
	TokenList tl;
	Node root;
	void put() const
	{
		puts("TokenList");
		tl.put();
		puts("root");
		root.put();
		puts("");
	}
	void makeTree()
	{
		cvtValueVecToNode(root, tl.vv);
		vecToTree(root.child);
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
			if (!isEnd(next) && *next == ')') return next;
			throw cybozu::Exception("bad parenthesis") << (isEnd(next) ? '_' : *next);
		}
		{
			float f;
			const char *next = parseFloat(&f, begin, end_);
			if (next) {
				tl.appendFloat(f);
				return next;
			}
		}
		{
			std::string str;
			const char *next = parseVar(str, begin, end_);
			if (next) {
				uint32_t idx = tl.setVarAndGetIdx(str);
				tl.appendVar(idx);
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
				tl.appendOp('+' ? Mul : Div);
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
				tl.appendOp('+' ? Add : Sub);
				continue;
			}
			break;
		}
		return begin;
	}
	void parse(Ast& ast, const std::string& str)
	{
		const char *begin = str.c_str();
		end_ = begin + str.size();
		begin = parseAddSub(begin, ast.tl);
		if (!isEnd(begin)) {
			throw cybozu::Exception("extra string") << std::string(begin, end_);
		}
	}
};

} // sg

