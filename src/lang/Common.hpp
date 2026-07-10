#pragma once

#include "gdextension_interface.h"
#include "godot_cpp/variant/callable.hpp"
#include "pocketpy.h"

#include <atomic>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <thread>

using namespace godot;

namespace pkpy {

struct IdGenerator {
	using T = py_i64;
	T _counter;
	IdGenerator() : _counter(1) {}
	T next() { return _counter++; }
	void reset() { _counter = 1; }
};

struct PythonScriptReloadingContext {
	StringName class_name;
	StringName extends;
	IdGenerator id_gen;

	PythonScriptReloadingContext() :
			class_name(), extends(), id_gen() {}

	inline void reset() {
		class_name = StringName();
		extends = StringName();
		id_gen.reset();
	}

	inline IdGenerator::T next_index() {
		return id_gen.next();
	}
};

struct InternalArguments {
	union {
		struct {
			Vector<Variant> _args;
			Vector<GDExtensionConstVariantPtr> _pointers;
		} dyn;

		struct {
			Variant _args[4];
			GDExtensionConstVariantPtr _pointers[4];
		} stc;
	};

	int length;

	InternalArguments(int length) : length(length) {
		if (length > 4) {
			dyn._args.resize(length);
			dyn._pointers.resize(length);
			for (int i = 0; i < length; i++) {
				dyn._pointers.write[i] = &dyn._args[i];
			}
		} else {
			std::memset(stc._args, 0, sizeof(Variant) * 4);
			for (int i = 0; i < length; i++) {
				stc._pointers[i] = &stc._args[i];
			}
		}
	}

	~InternalArguments() {
		if (length > 4) {
			using DynType = decltype(dyn);
			dyn.~DynType();
		} else {
			using StcType = decltype(stc);
			stc.~StcType();
		}
	}

	inline void set(int index, const Variant &val) {
		if (length > 4) {
			dyn._args.write[index] = val;
		} else {
			stc._args[index] = val;
		}
	}

	inline const GDExtensionConstVariantPtr *ptr() {
		return length > 4 ? dyn._pointers.ptr() : stc._pointers;
	}

	inline int size() const {
		return length;
	}
};

struct PythonContext {
	py_GlobalRef godot;
	py_GlobalRef godot_classes;
	py_GlobalRef godot_scripts;
	py_Type tp_Script;
	py_Type tp_GDNativeClass;
	py_Type tp_Variant;
	// internals
	py_Type tp_DefineStatement;
	std::thread::id main_thread_id;
	// std::atomic_flag lock;
	PythonScriptReloadingContext reloading_context;
	HashMap<String, Variant> class_constants;
	struct {
		py_Name __init__;
		py_Name __name__;
		py_Name __call__;
		py_Name script;
		py_Name owner;
	} names;
};

struct GDNativeClass {
	Variant::Type type;
	py_Name name;

	GDNativeClass() :
			type(Variant::NIL), name(NULL) {}
	GDNativeClass(Variant::Type type, py_Name clazz) :
			type(type), name(clazz) {}
};

struct DefineStatement {
	IdGenerator::T index;
	String name;

	DefineStatement(IdGenerator::T index) :
			index(index), name() {}

	bool operator<(const DefineStatement &other) const {
		return index < other.index;
	}

	virtual bool is_signal() const = 0;
	virtual ~DefineStatement() = default;
};

struct ExportStatement : DefineStatement {
	String template_;
	Variant default_value;

	using DefineStatement::DefineStatement;

	bool is_signal() const override {
		return false;
	}
};

struct SignalStatement : DefineStatement {
	PackedStringArray arguments;

	using DefineStatement::DefineStatement;

	bool is_signal() const override {
		return true;
	}
};

///////////////////

PythonContext *pyctx();

inline py_Name godot_name_to_python(StringName name) {
	const py_Name &retval = (const py_Name &)name;
	return retval;
}

inline StringName python_name_to_godot(py_Name name) {
	const StringName &sn = (const StringName &)name;
	return sn;
}

void log_python_error_and_clearexc(py_StackRef p0);

void py_newvariant(py_OutRef out, const Variant *val);
void py_newstring(py_OutRef out, String val);

Variant py_tovariant(py_Ref val);
Variant to_variant_exact(py_Ref val);
void dispose_contexts();

} // namespace pkpy