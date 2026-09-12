#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>

#include "PythonScript.hpp"
#include "PythonScriptLanguage.hpp"
#include "PythonScriptResourceFormatLoader.hpp"

namespace pkpy {

static Ref<PythonScriptResourceFormatLoader> py_loader;

PackedStringArray PythonScriptResourceFormatLoader::_get_recognized_extensions() const {
	return Array::make("py");
}

bool PythonScriptResourceFormatLoader::_handles_type(const StringName &p_type) const {
	return p_type == Script::get_class_static() || p_type == PythonScript::get_class_static();
}

bool PythonScriptResourceFormatLoader::_recognize_path(const String &p_path, const StringName &p_type) const {
	return p_path.get_extension().to_lower() == "py" && !is_python_module_path(p_path) &&
			(p_type.is_empty() || _handles_type(p_type));
}

String PythonScriptResourceFormatLoader::_get_resource_type(const String &p_path) const {
	if (_recognize_path(p_path, StringName())) {
		return PythonScript::get_class_static();
	} else {
		return "";
	}
}

bool PythonScriptResourceFormatLoader::_exists(const String &p_path) const {
	return _recognize_path(p_path, StringName()) && FileAccess::file_exists(p_path);
}

Variant PythonScriptResourceFormatLoader::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	ERR_FAIL_COND_V(is_python_module_path(p_path), ERR_FILE_UNRECOGNIZED);
	Ref<PythonScript> script = ResourceLoader::get_singleton()->get_cached_ref(p_path);
	if (!script.is_valid()) {
		script = PythonScriptLanguage::get_singleton()->_create_script();
		script->set_path(p_path);
	}
	script->set_source_code(FileAccess::get_file_as_string(p_path));
	Error status = script->reload();
	if (status == OK) {
		return script;
	} else {
		return status;
	}
}

void PythonScriptResourceFormatLoader::register_in_godot() {
	py_loader.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(py_loader);
}

void PythonScriptResourceFormatLoader::unregister_in_godot() {
	ResourceLoader::get_singleton()->remove_resource_format_loader(py_loader);
	py_loader.unref();
}

void PythonScriptResourceFormatLoader::_bind_methods() {
}

} //namespace pkpy
