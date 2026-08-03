#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "PythonScript.hpp"
#include "Common.hpp"

using namespace godot;

namespace pkpy {

struct PythonScriptInstance {
	PythonScriptInstance(Object *owner, Ref<PythonScript> script);
	~PythonScriptInstance();

	static GDExtensionScriptInstanceInfo3 *get_script_instance_info();
	static PythonScriptInstance *attached_to_object(Object *owner);

	Object *owner;
	Ref<PythonScript> script;

	py_TValue py;

	IdGenerator coroutine_id_gen;
	HashMap<IdGenerator::T, py_TValue> coroutines;

	static void gc_mark_instances(void (*f)(py_Ref val, void *ctx), void *ctx);

private:
	// Node instance ID -> PythonScriptInstance
	static HashMap<uint64_t, PythonScriptInstance *> known_instances;
};

} //namespace pkpy
