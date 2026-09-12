#pragma once

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

using namespace godot;

namespace pkpy {

class PythonModulesDock : public VBoxContainer {
	GDCLASS(PythonModulesDock, VBoxContainer);

	LineEdit *filter = nullptr;
	Tree *tree = nullptr;
	Label *status = nullptr;
	AcceptDialog *error_dialog = nullptr;
	PackedStringArray module_paths;

	void refresh();
	void rebuild_tree(const String &p_filter);
	void open_selected();

protected:
	static void _bind_methods();

public:
	PythonModulesDock();
	void _ready() override;
};

} // namespace pkpy
