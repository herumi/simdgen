#pragma once
/**
	@file
	@brief token list
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <assert.h>
#include <cybozu/exception.hpp>

namespace sg {

typedef std::vector<std::string> StrVec;
typedef std::vector<uint32_t> IntVec;

enum ValueType {
	None,
	Const,
	Var,
	Op,
	Func,
	ValueTypeN
};

enum OpType {
	Add,
	Sub,
	Mul,
	Div,
	OpTypeN
};

enum FuncType {
	Inv,
	Exp,
	Log,
	Tanh,
	FuncTypeN
};

inline const char* getFuncName(size_t i)
{
	static const char *tbl[] = {
		"inv",
		"exp",
		"log",
		"tanh",
	};
	if (i >= CYBOZU_NUM_OF_ARRAY(tbl)) {
		throw cybozu::Exception("bad Func") << i;
	}
	return tbl[i];
}

inline int getFuncKind(const std::string& str)
{
	for (int i = 0; i < FuncTypeN; i++) {
		if (str == getFuncName(i)) {
			return i;
		}
	}
	throw cybozu::Exception("getFuncKind:bad name") << str;
}


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
	uint32_t append(const T& s)
	{
		int idx = getIdx(s, false);
		if (idx < 0) {
			tbl.push_back(s);
			idx = (int)tbl.size() - 1;
		}
		return idx;
	}
	void put() const
	{
		for (size_t i = 0; i < tbl.size(); i++) {
			std::cout << i << ":" << tbl[i] << ' ';
		}
		std::cout << std::endl;
	}
	const T& getVal(uint32_t idx) const { return tbl[idx]; }
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
	// index if type == Var else value
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
		case Const:
			snprintf(buf, sizeof(buf), "float{%f(0x%08x)}", u2f(v), v);
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
				};
				if (v >= CYBOZU_NUM_OF_ARRAY(tbl)) {
					throw cybozu::Exception("bad Op") << v;
				}
				return tbl[v];
			}
		case Func:
			return getFuncName(v);
		default:
			throw cybozu::Exception("bad type") << type;
		}
		return buf;
	}
	void put() const
	{
		printf("%s\n", getStr().c_str());
	}
};

typedef std::vector<Value> ValueVec;

struct TokenList {
	Index<std::string> varIdx_;
	ValueVec vv;
	int maxRegStackN_;
	bool usedFuncTbl_[FuncTypeN];
	TokenList()
		: maxRegStackN_(0)
		, usedFuncTbl_()
	{
	}
	// treat s as the index 0, 1, 2, ... accoring to the order to call setVar(s)
	void setVar(const std::string& s)
	{
		varIdx_.append(s);
	}
	size_t getVarNum() const { return varIdx_.size(); }
	const ValueVec& getValueVec() const { return vv; }
	int getMaxTmpNum() const { return maxRegStackN_; }
	void updateMaxRegStackNum(int x)
	{
		if (x > maxRegStackN_) maxRegStackN_ = x;
	}
	void useFunc(int kind)
	{
		usedFuncTbl_[kind] = true;
	}
	bool isUsedFunc(int kind) const
	{
		return usedFuncTbl_[kind];
	}
	void clear()
	{
		maxRegStackN_ = 0;
		for (size_t i = 0; i < FuncTypeN; i++) {
			usedFuncTbl_[i] = false;
		}
	}
	void appendConst(float f)
	{
		Value v;
		v.type = Const;
		v.v = f2u(f);
		vv.push_back(v);
	}
	void appendVar(const std::string& s)
	{
		Value v;
		v.type = Var;
		v.v = varIdx_.getIdx(s);
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
		useFunc(kind);
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
		printf("var ");
		varIdx_.put();
		printf("token ");
		putValueVec();
	}
};

} // namespace sp

