#pragma once

#include <godot_cpp/classes/editor_export_plugin.hpp>

using namespace godot;

namespace pkpy {

class PythonModuleExportPlugin : public EditorExportPlugin {
	GDCLASS(PythonModuleExportPlugin, EditorExportPlugin);

protected:
	static void _bind_methods() {}

public:
	String _get_name() const override { return "Python"; }
	void _export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path, uint32_t p_flags) override;
};

} // namespace pkpy
