#pragma once
#include <cybozu/atoi.hpp>
#include <sstream>
#include <fstream>

struct SgOpt {
	int unrollN;
	bool debug;
	bool disableLogp1;
	std::string dumpName;
	SgOpt()
		: unrollN(1)
		, debug(false)
		, disableLogp1(false)
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
			if (k == "unroll") {
				unrollN = cybozu::atoi(v);
				if (unrollN < 1) throw cybozu::Exception("bad unroll") << unrollN;
				if (debug) printf("unrollN=%d\n", unrollN);
			}
			if (k == "dump") {
				dumpName = v;
				if (debug) printf("dumpName=%s\n", dumpName.c_str());
			} else
			if (k == "disableLogp1") {
				disableLogp1 = v == "1";
				if (debug) printf("disableLogp1=%d\n", disableLogp1);
			} else
			{
				// none
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

