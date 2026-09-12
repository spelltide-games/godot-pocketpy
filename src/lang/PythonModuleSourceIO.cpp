#include "PythonModuleSourceIO.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include "PythonModuleSource.hpp"
#include "PythonSourcePath.hpp"

namespace pkpy {

static Ref<PythonModuleSourceLoader> module_loader;
static Ref<PythonModuleSourceSaver> module_saver;

PackedStringArray PythonModuleSourceLoader::_get_recognized_extensions() const {
	return Array::make("py");
}

bool PythonModuleSourceLoader::_recognize_path(const String &p_path, const StringName &p_type) const {
	return is_python_module_path(p_path) && (p_type.is_empty() || _handles_type(p_type));
}

bool PythonModuleSourceLoader::_handles_type(const StringName &p_type) const {
	return p_type == Script::get_class_static() || p_type == PythonModuleSource::get_class_static();
}

bool PythonModuleSourceLoader::_exists(const String &p_path) const {
	return is_python_module_path(p_path) && FileAccess::file_exists(p_path);
}

Variant PythonModuleSourceLoader::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	ERR_FAIL_COND_V(!is_python_module_path(p_path), ERR_FILE_UNRECOGNIZED);
	const String path = normalize_python_path(p_original_path.is_empty() ? p_path : p_original_path);
	if (p_cache_mode == CACHE_MODE_REUSE) {
		Ref<PythonModuleSource> cached = ResourceLoader::get_singleton()->get_cached_ref(path);
		if (cached.is_valid()) {
			return cached; // Never overwrite an unsaved editor buffer on a debugger lookup.
		}
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return FileAccess::get_open_error();
	}
	Ref<PythonModuleSource> source;
	source.instantiate();
	source->set_source_code(file->get_as_text());
	// ResourceLoader assigns the path/cache according to p_cache_mode. In
	// particular, an IGNORE load used for external-edit checks stays separate.
	return source;
}

bool PythonModuleSourceSaver::_recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<PythonModuleSource>(p_resource.ptr()) != nullptr;
}

PackedStringArray PythonModuleSourceSaver::_get_recognized_extensions(const Ref<Resource> &p_resource) const {
	return _recognize(p_resource) ? PackedStringArray(Array::make("py")) : PackedStringArray();
}

Error PythonModuleSourceSaver::_save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<PythonModuleSource> source = p_resource;
	ERR_FAIL_COND_V(source.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(!is_python_module_path(p_path), ERR_INVALID_PARAMETER,
			"Python modules must be saved under res://site-packages/.");
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(file.is_null(), FileAccess::get_open_error());
	file->store_string(source->get_source_code());
	const Error error = file->get_error();
	return error == ERR_FILE_EOF ? OK : error;
}

void register_module_source_io() {
	module_loader.instantiate();
	module_saver.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(module_loader, true);
	ResourceSaver::get_singleton()->add_resource_format_saver(module_saver, true);
}

void unregister_module_source_io() {
	ResourceSaver::get_singleton()->remove_resource_format_saver(module_saver);
	ResourceLoader::get_singleton()->remove_resource_format_loader(module_loader);
	module_saver.unref();
	module_loader.unref();
}

} // namespace pkpy
