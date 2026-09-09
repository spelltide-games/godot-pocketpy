#include "PythonEditorPlugin.hpp"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "PythonScript.hpp"
#include "PythonSyntaxHighlighter.hpp"

namespace pkpy {

namespace {

const char *TOOL_ITEM_NAME = "Python: Rebuild Scripts Index File";

ScriptEditor *get_script_editor() {
	EditorInterface *editor = EditorInterface::get_singleton();
	return editor != nullptr ? editor->get_script_editor() : nullptr;
}

Ref<EditorSettings> get_editor_settings() {
	EditorInterface *editor = EditorInterface::get_singleton();
	return editor != nullptr ? editor->get_editor_settings() : Ref<EditorSettings>();
}

CodeEdit *get_code_edit(ScriptEditorBase *p_editor) {
	if (p_editor == nullptr) {
		return nullptr;
	}
	return Object::cast_to<CodeEdit>(p_editor->get_base_editor());
}

} //namespace

void PythonEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rebuild_index_file"), &PythonEditorPlugin::rebuild_index_file);
	ClassDB::bind_method(D_METHOD("install_syntax_highlighter"), &PythonEditorPlugin::install_syntax_highlighter);
	ClassDB::bind_method(D_METHOD("on_editor_script_changed", "script"), &PythonEditorPlugin::on_editor_script_changed);
	ClassDB::bind_method(D_METHOD("on_editor_settings_changed"), &PythonEditorPlugin::on_editor_settings_changed);
}

void PythonEditorPlugin::_enter_tree() {
	add_tool_menu_item(TOOL_ITEM_NAME, Callable(this, "rebuild_index_file"));

	ScriptEditor *script_editor = get_script_editor();
	if (script_editor != nullptr) {
		script_editor->connect("editor_script_changed", Callable(this, "on_editor_script_changed"));
	}
	const Ref<EditorSettings> settings = get_editor_settings();
	if (settings.is_valid()) {
		settings->connect("settings_changed", Callable(this, "on_editor_settings_changed"));
	}

	call_deferred("install_syntax_highlighter");
}

void PythonEditorPlugin::_exit_tree() {
	remove_tool_menu_item(TOOL_ITEM_NAME);

	ScriptEditor *script_editor = get_script_editor();
	if (script_editor != nullptr) {
		Callable callable(this, "on_editor_script_changed");
		if (script_editor->is_connected("editor_script_changed", callable)) {
			script_editor->disconnect("editor_script_changed", callable);
		}
	}
	const Ref<EditorSettings> settings = get_editor_settings();
	if (settings.is_valid()) {
		Callable callable(this, "on_editor_settings_changed");
		if (settings->is_connected("settings_changed", callable)) {
			settings->disconnect("settings_changed", callable);
		}
	}
}

void PythonEditorPlugin::_set_window_layout(const Ref<ConfigFile> &p_configuration) {
	// Runs once the editor has reopened the scripts of the previous session,
	// which is the one moment `editor_script_changed` does not cover.
	call_deferred("install_syntax_highlighter");
}

void PythonEditorPlugin::rebuild_index_file() {
	PythonScript::rebuild_index_file();
}

void PythonEditorPlugin::on_editor_script_changed(const Ref<Script> &p_script) {
	// The tab is not necessarily the current one yet when this fires.
	call_deferred("install_syntax_highlighter");
}

void PythonEditorPlugin::install_syntax_highlighter() {
	ScriptEditor *script_editor = get_script_editor();
	if (script_editor == nullptr) {
		return;
	}
	const Ref<Script> script = script_editor->get_current_script();
	if (Object::cast_to<PythonScript>(script.ptr()) == nullptr) {
		return;
	}
	CodeEdit *code_edit = get_code_edit(script_editor->get_current_editor());
	if (code_edit == nullptr) {
		return;
	}
	// Reopening a tab restores the highlighter named in the session cache, which
	// is never ours, so keep checking instead of tracking what we already did.
	if (Object::cast_to<PythonSyntaxHighlighter>(code_edit->get_syntax_highlighter().ptr()) != nullptr) {
		return;
	}

	// One highlighter per editor: a SyntaxHighlighter caches per line and keeps
	// a pointer to the TextEdit it belongs to, so sharing one would mix tabs up.
	Ref<PythonSyntaxHighlighter> highlighter = memnew(PythonSyntaxHighlighter);
	highlighter->rebuild();
	code_edit->set_syntax_highlighter(highlighter);
}

void PythonEditorPlugin::on_editor_settings_changed() {
	ScriptEditor *script_editor = get_script_editor();
	if (script_editor == nullptr) {
		return;
	}
	// The theme colors are baked into the highlighter, so read them again.
	const TypedArray<ScriptEditorBase> editors = script_editor->get_open_script_editors();
	for (int i = 0; i < editors.size(); i++) {
		Object *object = editors[i];
		CodeEdit *code_edit = get_code_edit(Object::cast_to<ScriptEditorBase>(object));
		if (code_edit == nullptr) {
			continue;
		}
		PythonSyntaxHighlighter *highlighter = Object::cast_to<PythonSyntaxHighlighter>(code_edit->get_syntax_highlighter().ptr());
		if (highlighter != nullptr) {
			highlighter->rebuild();
		}
	}
}

} //namespace pkpy
