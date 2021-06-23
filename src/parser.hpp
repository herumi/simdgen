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

struct Node {
	NodeType type;
	uint32_t v;
	NodeVec child;
};

inline bool isSpace(char c) {
	return c == ' ' || c == '\t';
}

bool parseFloat(float *f, const char *(&begin), const char *end)
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
	if (p == 0) return false;
	char *endp;
	*f = strtof(p, &endp);
	return endp == begin;
}

bool parseVar(std::string& v , const char *(&begin), const char *end)
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
	if (p == 0) return false;
	v.assign(p, begin);
	return true;
}

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

	void parse(const std::string& str)
	{
		begin_ = str.c_str();
		end_ = begin_ + str.size();
	}
};

} // sp

