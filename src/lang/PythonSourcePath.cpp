#include "PythonSourcePath.hpp"

#include <godot_cpp/classes/dir_access.hpp>

namespace pkpy {

namespace {

void collect_files(const godot::String &p_path, godot::PackedStringArray &r_files) {
	using namespace godot;
	Ref<DirAccess> dir = DirAccess::open(p_path);
	ERR_FAIL_COND_MSG(dir.is_null(), "Cannot read Python package directory: " + p_path);
	for (const String &name : dir->get_files()) {
		if (!name.begins_with(".") && !dir->is_link(name)) {
			r_files.push_back(p_path.path_join(name));
		}
	}
	for (const String &name : dir->get_directories()) {
		if (!name.begins_with(".") && name != "__pycache__" && !dir->is_link(name)) {
			collect_files(p_path.path_join(name), r_files);
		}
	}
}

} // namespace

godot::PackedStringArray list_python_package_files() {
	godot::PackedStringArray files;
	if (godot::DirAccess::dir_exists_absolute(SITE_PACKAGES_PREFIX)) {
		collect_files(SITE_PACKAGES_PREFIX, files);
		files.sort();
	}
	return files;
}

godot::PackedStringArray list_python_script_files() {
	godot::PackedStringArray files;
	if (godot::DirAccess::dir_exists_absolute(SCRIPTS_PREFIX)) {
		collect_files(SCRIPTS_PREFIX, files);
		files.sort();
	}
	return files;
}

} // namespace pkpy
