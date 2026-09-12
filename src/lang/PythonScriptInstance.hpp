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

	// Called by Godot through `free_func` when the owner Object is destroyed or
	// when the script is detached from it (Object::~Object, Object::set_script,
	// Object::set_script_instance). This is the only notification we ever get
	// about the owner's death: `~PythonScriptInstance()` is driven by pocketpy's
	// GC, and the GC can never collect this object while `known_instances` still
	// roots it from `gc_mark_instances()`.
	void detach_from_owner();

	static GDExtensionScriptInstanceInfo3 *get_script_instance_info();
	static PythonScriptInstance *attached_to_object(Object *owner);

	Object *owner;
	uint64_t owner_id;
	Ref<PythonScript> script;

	// The engine-side ScriptInstanceExtension that wraps this object, as handed
	// back by script_instance_create3(). Kept because the debugger has to return
	// it from _debug_get_stack_level_instance(): RemoteDebugger abandons its
	// whole debug loop -- silently resuming the game -- when a break reports no
	// instance and the user types a watch expression.
	void *engine_instance;

	py_TValue py;

	IdGenerator coroutine_id_gen;
	HashMap<IdGenerator::T, py_TValue> coroutines;

	static void gc_mark_instances(void (*f)(py_Ref val, void *ctx), void *ctx);

private:
	// Node instance ID -> PythonScriptInstance
	static HashMap<uint64_t, PythonScriptInstance *> known_instances;
};

} //namespace pkpy
