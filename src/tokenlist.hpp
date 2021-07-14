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
	IntVec f2uIdx;
	ValueVec vv;
	int maxTmpN_;
	static const size_t funcN = CYBOZU_NUM_OF_ARRAY(funcNameTbl);
	bool usedFuncTbl_[funcN];
	TokenList()
		: maxTmpN_(0)
		, usedFuncTbl_()
	{
	}
	size_t getVarNum() const { return varIdx.size(); }
	size_t getConstNum() const { return f2uIdx.size(); }
	int getMaxTmpNum() const { return maxTmpN_; }
	void updateMaxTmpNum(int x)
	{
		if (x > maxTmpN_) maxTmpN_ = x;
	}
	void clear()
	{
		maxTmpN_ = 0;
		for (size_t i = 0; i < funcN; i++) {
			usedFuncTbl_[i] = false;
		}
	}
	const ValueVec& getValueVec() const { return vv; }
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
		usedFuncTbl_[kind] = true;
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
	uint32_t getConstVal(size_t idx) const
	{
		return f2uIdx[idx];
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
};

} // namespace sp

