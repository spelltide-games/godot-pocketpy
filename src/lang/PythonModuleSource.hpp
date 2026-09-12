#pragma once

#include <godot_cpp/classes/script_extension.hpp>
#include <godot_cpp/classes/script_language.hpp>

using namespace godot;

namespace pkpy {

// Editor-only source document. Godot's built-in debugger requires a Script to
// navigate to a stack frame, but importing/executing this file belongs solely
// to pocketpy. None of the editor callbacks below may execute Python.
class PythonModuleSource : public ScriptExtension {
	GDCLASS(PythonModuleSource, ScriptExtension);

	String source_code;

protected:
	static void _bind_methods() {}

public:
	bool _editor_can_reload_from_file() override { return true; }
	void _placeholder_erased(void *p_placeholder) override {}
	bool _can_instantiate() const override { return false; }
	bool _is_abstract() const override { return true; }
	Ref<Script> _get_base_script() const override { return {}; }
	StringName _get_global_name() const override { return {}; }
	bool _inherits_script(const Ref<Script> &p_script) const override { return false; }
	StringName _get_instance_base_type() const override { return {}; }
	void *_instance_create(Object *p_for_object) const override { return nullptr; }
	void *_placeholder_instance_create(Object *p_for_object) const override { return nullptr; }
	bool _instance_has(Object *p_object) const override { return false; }
	bool _has_source_code() const override { return true; } // Empty __init__.py is a valid document.
	String _get_source_code() const override { return source_code; }
	void _set_source_code(const String &p_code) override { source_code = p_code; }
	Error _reload(bool p_keep_state) override { return OK; }
	TypedArray<Dictionary> _get_documentation() const override { return {}; }
	String _get_class_icon_path() const override { return {}; }
	bool _has_method(const StringName &p_method) const override { return false; }
	bool _has_static_method(const StringName &p_method) const override { return false; }
	Variant _get_script_method_argument_count(const StringName &p_method) const override { return {}; }
	Dictionary _get_method_info(const StringName &p_method) const override { return {}; }
	bool _is_tool() const override { return false; }
	bool _is_valid() const override { return true; }
	ScriptLanguage *_get_language() const override;
	bool _has_script_signal(const StringName &p_signal) const override { return false; }
	TypedArray<Dictionary> _get_script_signal_list() const override { return {}; }
	bool _has_property_default_value(const StringName &p_property) const override { return false; }
	Variant _get_property_default_value(const StringName &p_property) const override { return {}; }
	void _update_exports() override {}
	TypedArray<Dictionary> _get_script_method_list() const override { return {}; }
	TypedArray<Dictionary> _get_script_property_list() const override { return {}; }
	int32_t _get_member_line(const StringName &p_member) const override { return -1; }
	Dictionary _get_constants() const override { return {}; }
	TypedArray<StringName> _get_members() const override { return {}; }
	bool _is_placeholder_fallback_enabled() const override { return false; }
	Variant _get_rpc_config() const override { return {}; }
};

} // namespace pkpy
