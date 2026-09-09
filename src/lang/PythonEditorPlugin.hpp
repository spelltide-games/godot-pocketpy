#pragma once

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/script.hpp>

using namespace godot;

namespace pkpy {

class PythonEditorPlugin : public EditorPlugin {
	GDCLASS(PythonEditorPlugin, EditorPlugin);

protected:
	static void _bind_methods();

public:
	void _enter_tree() override;
	void _exit_tree() override;
	void _set_window_layout(const Ref<ConfigFile> &p_configuration) override;

private:
	void rebuild_index_file();

	// Gives the script currently shown in the script editor a Python
	// highlighter, unless it already has one or is not a Python script.
	void install_syntax_highlighter();
	void on_editor_script_changed(const Ref<Script> &p_script);
	void on_editor_settings_changed();
};

} //namespace pkpy
