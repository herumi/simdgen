#pragma once
#include <cybozu/atoi.hpp>
#include <sstream>
#include <fstream>

struct SgOpt {
	int unrollN;
	bool debug;
	bool break_point;
	bool logp1;
	bool log_use_mem;
	bool use_mem;
	std::string varName;
	std::string dumpName;
	SgOpt()
		: unrollN(0)
		, debug(false)
		, break_point(false)
		, logp1(true)
		, log_use_mem(true)
		, use_mem(true)
		, varName("x")
		, dumpName("")
	{
	}
	void getEnv()
	{
		const char *env = getenv("SG_OPT");
		if (env == 0) return;
		std::istringstream iss(env);
		std::string kv;
		while (iss >> kv) {
			size_t pos = kv.find('=');
			if (pos == std::string::npos) continue;
			std::string k = kv.substr(0, pos);
			std::string v = kv.substr(pos + 1);
			if (k == "debug") {
				debug = v == "1";
				if (debug) printf("debug=%d\n", debug);
			} else
			if (k == "break_point") {
				break_point = v == "1";
				if (debug) printf("break_point=%d\n", break_point);
			} else
			if (k == "unroll") {
				unrollN = cybozu::atoi(v);
				if (unrollN < 0) throw cybozu::Exception("bad unroll") << unrollN;
				if (debug) printf("unrollN=%d\n", unrollN);
			} else
			if (k == "var") {
				varName = v;
				if (debug) printf("varName=%s\n", varName.c_str());
			} else
			if (k == "dump") {
				dumpName = v;
				if (debug) printf("dumpName=%s\n", dumpName.c_str());
			} else
			if (k == "logp1") {
				logp1 = v == "1";
				if (debug) printf("logp1=%d\n", logp1);
			} else
			if (k == "log_use_mem") {
				log_use_mem = v == "1";
				if (debug) printf("log_use_mem=%d\n", log_use_mem);
			} else
			if (k == "use_mem") {
				use_mem = v == "1";
				if (use_mem) {
					log_use_mem = true;
				}
				if (debug) printf("use_mem=%d\n", log_use_mem);
			} else
			{
				throw cybozu::Exception("bad option") << k << v;
			}
		}
	}
	void dump(const void *addr, size_t size)
	{
		if (dumpName.empty()) return;
		std::ofstream ofs(dumpName.c_str(), std::ios::binary);
		ofs.write((const char*)addr, size);
	}
};

