#pragma once

#include "godot_cpp/classes/gd_script.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
#include <godot_cpp/classes/script_extension.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>

#include "Common.hpp"
#include "PythonScriptLanguage.hpp"

using namespace godot;

namespace pkpy {

struct PythonScriptMeta {
	Ref<GDScript> gds;
	py_Type type;
	StringName class_name;
	StringName extends;
	HashMap<StringName, Variant> default_values;
	HashMap<StringName, int> methods;
	HashMap<StringName, PackedStringArray> signals;
	bool is_valid;

	PythonScriptMeta() :
			gds(nullptr),
			type(0),
			class_name(),
			extends(),
			default_values(),
			methods(),
			signals(),
			is_valid(false) {
	}
};

class PythonScript : public ScriptExtension {
	GDCLASS(PythonScript, ScriptExtension);

public:
	PythonScript();
	~PythonScript();

	bool _editor_can_reload_from_file() override;
	void _placeholder_erased(void *p_placeholder) override;
	bool _can_instantiate() const override;
	Ref<Script> _get_base_script() const override;
	StringName _get_global_name() const override;
	bool _inherits_script(const Ref<Script> &p_script) const override;
	StringName _get_instance_base_type() const override;
	void *_instance_create(Object *p_for_object) const override;
	void *_placeholder_instance_create(Object *p_for_object) const override;
	bool _instance_has(Object *p_object) const override;
	bool _has_source_code() const override;
	String _get_source_code() const override;
	void _set_source_code(const String &p_code) override;
	Error _reload(bool p_keep_state) override;
	TypedArray<Dictionary> _get_documentation() const override;
	String _get_class_icon_path() const override;
	bool _has_method(const StringName &p_method) const override;
	bool _has_static_method(const StringName &p_method) const override;
	Variant _get_script_method_argument_count(const StringName &p_method) const override;
	Dictionary _get_method_info(const StringName &p_method) const override;
	bool _is_tool() const override;
	bool _is_valid() const override;
	bool _is_abstract() const override;
	ScriptLanguage *_get_language() const override;
	bool _has_script_signal(const StringName &p_signal) const override;
	TypedArray<Dictionary> _get_script_signal_list() const override;
	bool _has_property_default_value(const StringName &p_property) const override;
	Variant _get_property_default_value(const StringName &p_property) const override;
	void _update_exports() override;
	TypedArray<Dictionary> _get_script_method_list() const override;
	TypedArray<Dictionary> _get_script_property_list() const override;
	int32_t _get_member_line(const StringName &p_member) const override;
	Dictionary _get_constants() const override;
	TypedArray<StringName> _get_members() const override;
	bool _is_placeholder_fallback_enabled() const override;
	Variant _get_rpc_config() const override;

	// Script methods
	Variant _new(const Variant **args, GDExtensionInt arg_count, GDExtensionCallError &error);

	PythonScriptMeta meta;

	static Variant eval(String code) {
		py_StackRef p0 = py_peek(0);
		bool ok = py_eval(code.utf8().get_data(), NULL);
		if (!ok) {
			log_python_error_and_clearexc(p0);
			return Variant();
		}
		return py_tovariant(py_retval());
	}

	static void rebuild_index_file();
	static HashMap<py_Type, PythonScript *> runtime_type_to_script;

	static void dispose() {
		known_classes.clear();
		runtime_type_to_script.clear();
	}

protected:
	static void _bind_methods();
	virtual String _to_string() const;

	void _update_placeholder_exports(void *placeholder) const;

	String source_code;

	bool placeholder_fallback_enabled;

	// TODO: use instance member instead of static map if "_placeholder_instance_create" is changed to be non-const
	static HashMap<const PythonScript *, HashSet<void *>> placeholders;

private:
	static HashMap<StringName, String> known_classes;
};

} //namespace pkpy
