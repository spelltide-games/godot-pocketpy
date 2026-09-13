#pragma once

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace pkpy {

inline constexpr const char *SITE_PACKAGES_PREFIX = "res://site-packages/";
inline constexpr const char *SCRIPTS_PREFIX = "res://scripts/";

// Shared by the importer, source editor and debugger. A directory boundary is
// intentional: site-packages-other is not a module root.
inline godot::String normalize_python_path(const godot::String &p_path) {
	godot::String path = p_path.replace("\\", "/");
	if (path.is_absolute_path() && !path.begins_with("res://") && !path.begins_with("user://")) {
		path = godot::ProjectSettings::get_singleton()->localize_path(path);
	}
	return path.simplify_path();
}

inline bool is_python_module_path(const godot::String &p_path) {
	const godot::String path = normalize_python_path(p_path);
	return path.begins_with(SITE_PACKAGES_PREFIX) && path.get_extension().to_lower() == "py";
}

// Resolve a pocketpy import name without allowing `..` to escape the module
// root. An empty result means the import is outside site-packages.
inline godot::String resolve_python_module_path(const godot::String &p_module) {
	const godot::String path = normalize_python_path(godot::String(SITE_PACKAGES_PREFIX) + p_module);
	return path.begins_with(SITE_PACKAGES_PREFIX) ? path : godot::String();
}

// Explicit disk traversal: EditorFileSystem intentionally hides this tree.
// Never follows directory links, so a package cannot introduce a scan cycle.
godot::PackedStringArray list_python_package_files();

godot::PackedStringArray list_python_script_files();

} // namespace pkpy
