#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/script.hpp>

#include "PythonScript.hpp"
#include "PythonScriptLanguage.hpp"
#include "PythonScriptResourceFormatSaver.hpp"

using namespace godot;

namespace pkpy {

static Ref<PythonScriptResourceFormatSaver> py_saver;

Error PythonScriptResourceFormatSaver::_save(const Ref<Resource> &resource, const String &p_path, uint32_t flags) {
	Ref<PythonScript> py_script = resource;
	ERR_FAIL_COND_V(py_script.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(is_python_module_path(p_path), ERR_INVALID_PARAMETER,
			"site-packages contains importable Python modules, not attachable scripts.");

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::ModeFlags::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), FileAccess::get_open_error(), "Failed to save file at " + p_path);

	file->store_string(py_script->get_source_code());
	Error error = file->get_error();
	switch (error) {
		case OK:
		case ERR_FILE_EOF: {
			py_script->reload();
			return OK;
		}
		default:
			return error;
	}
}

bool PythonScriptResourceFormatSaver::_recognize(const Ref<Resource> &resource) const {
	Ref<PythonScript> py_script = resource;
	return py_script.is_valid();
}

PackedStringArray PythonScriptResourceFormatSaver::_get_recognized_extensions(const Ref<Resource> &resource) const {
	PackedStringArray extensions;
	if (_recognize(resource)) {
		extensions.push_back("py");
	}
	return extensions;
}

void PythonScriptResourceFormatSaver::register_in_godot() {
	py_saver.instantiate();
	ResourceSaver::get_singleton()->add_resource_format_saver(py_saver);
}

void PythonScriptResourceFormatSaver::unregister_in_godot() {
	ResourceSaver::get_singleton()->remove_resource_format_saver(py_saver);
	py_saver.unref();
}

void PythonScriptResourceFormatSaver::_bind_methods() {
}

} //namespace pkpy
