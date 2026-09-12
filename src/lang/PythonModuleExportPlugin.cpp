#include "PythonModuleExportPlugin.hpp"

#include <godot_cpp/classes/editor_export_platform.hpp>
#include <godot_cpp/classes/editor_export_preset.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include "PythonSourcePath.hpp"

namespace pkpy {

void PythonModuleExportPlugin::_export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path, uint32_t p_flags) {
	const Ref<EditorExportPreset> preset = get_export_preset();
	const PackedStringArray excludes = preset.is_valid() ? preset->get_exclude_filter().split(",", false) : PackedStringArray();
	for (const String &path : list_python_package_files()) {
		const String relative = path.trim_prefix("res://");
		bool include = is_python_module_path(path);
		for (const String &pattern : excludes) {
			const String trimmed = pattern.strip_edges();
			if (!trimmed.is_empty() && (relative.matchn(trimmed) || path.matchn(trimmed))) {
				include = false;
			}
		}
		if (!include) {
			continue;
		}
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) {
			get_export_platform()->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "Python", "Cannot read " + path);
			continue;
		}
		// .gdignore keeps these out of the normal export traversal. Add the bytes
		// at their original paths so the runtime FileAccess importer also works in PCKs.
		add_file(path, file->get_buffer(file->get_length()), false);
	}
}

} // namespace pkpy
