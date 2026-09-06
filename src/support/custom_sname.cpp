#include "pocketpy/common/name.h"
#include <cstdio>
#include <string.h>
#include <atomic>
#include <map>
#include <thread>

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include "DebugPrint.hpp"

using namespace godot;

static struct CachedNames {
	std::map<py_Name, CharString> map; // must use std::map to ensure iterators are stable
	HashMap<String, StringName> keepalive;
	std::atomic_flag lock;
} *cached_names;

struct CachedNamesLock {
	CachedNamesLock() {
		while (cached_names->lock.test_and_set()) {
			std::this_thread::yield();
		}
	}

	~CachedNamesLock() {
		cached_names->lock.clear();
	}
};

extern "C" {

#define MAGIC_METHOD(x) py_Name x;
#include "pocketpy/xmacros/magics.h"
#undef MAGIC_METHOD

void pk_names_initialize() {
	cached_names = new CachedNames;
	cached_names->lock.clear();

#define MAGIC_METHOD(x) x = py_name(#x);
#include "pocketpy/xmacros/magics.h"
#undef MAGIC_METHOD

	py_Name t1 = py_name("__init__");
	py_Name t2 = py_name("__init__");
	if (t1 != t2) {
		pkpy::debug_print("py_name() is buggy: %p != %p\n", t1, t2);
		std::abort();
	}
}

void pk_names_finalize() {
	delete cached_names;
}

py_Name py_name(const char *name) {
	c11_sv name_sv;
	name_sv.data = name;
	name_sv.size = (int)strlen(name);
	return py_namev(name_sv);
}

py_Name py_namev(c11_sv name) {
	CachedNamesLock lock;
	String key = String::utf8(name.data, name.size);
	auto it = cached_names->keepalive.find(key);
	StringName sn;
	if (it == cached_names->keepalive.end()) {
		sn = StringName(key);
		cached_names->keepalive.insert(key, sn);
	} else {
		sn = it->value;
	}
	const py_Name &retval = (const py_Name &)sn;
	return retval;
}

c11_sv py_name2sv(py_Name index) {
	CachedNamesLock lock;
	const StringName &sn = (const StringName &)(index);
	auto it = cached_names->map.find(index);
	if (it == cached_names->map.end()) {
		CharString cs = String(sn).utf8();
		it = cached_names->map.insert({ index, cs }).first;
	}
	c11_sv sv;
	sv.data = it->second.get_data();
	sv.size = (int)it->second.length();
	return sv;
}

const char *py_name2str(py_Name index) {
	return py_name2sv(index).data;
}
}