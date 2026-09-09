#pragma once

#include <godot_cpp/classes/code_highlighter.hpp>
#include <godot_cpp/classes/editor_syntax_highlighter.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace pkpy {

// Syntax highlighting for `.py` files opened in the script editor.
//
// Godot's built-in "Standard" highlighter already derives colors from the
// reserved words and delimiters `PythonScriptLanguage` reports, but it knows
// nothing about Python's builtins, `self`, or the helpers exported by the
// `godot` module. So this fills in a Python flavored color table and leaves the
// tokenizing to an inner CodeHighlighter, the same split the editor's own
// EditorStandardSyntaxHighlighter uses.
//
// Extending EditorSyntaxHighlighter is not optional: CodeTextEditor::
// get_edit_state() downcasts whatever highlighter a script's CodeEdit holds and
// calls `_get_name()` on it without a null check, so anything else crashes the
// editor when it saves the session.
class PythonSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(PythonSyntaxHighlighter, EditorSyntaxHighlighter);

	// Does the actual tokenizing. It needs to be bound to the same TextEdit we
	// are; see PythonEditorPlugin::install_syntax_highlighter().
	Ref<CodeHighlighter> highlighter;

protected:
	static void _bind_methods() {}

	// Re-reads the editor theme and rebuilds the keyword colors and color
	// regions of the inner highlighter.
	void rebuild();

	// Registers one color region per `"<begin> <end>"` delimiter, the encoding
	// `ScriptLanguage` uses for comment and string delimiters.
	void add_color_regions(const PackedStringArray &p_delimiters, const Color &p_color);

public:
	PythonSyntaxHighlighter();

	String _get_name() const override;
	PackedStringArray _get_supported_languages() const override;
	Dictionary _get_line_syntax_highlighting(int32_t p_line) const override;
	void _update_cache() override;
	void _clear_highlighting_cache() override;

	// The tokenizer, exposed only so the plugin can bind it to the CodeEdit.
	Ref<CodeHighlighter> get_inner_highlighter() const { return highlighter; }
};

} //namespace pkpy
