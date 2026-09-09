#pragma once

#include <godot_cpp/classes/code_highlighter.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace pkpy {

// Syntax highlighting for `.py` files opened in the script editor.
//
// Godot's built-in "Standard" highlighter already derives colors from the
// reserved words and delimiters `PythonScriptLanguage` reports, but it knows
// nothing about Python's builtins, `self`, or the helpers exported by the
// `godot` module. This subclass keeps the engine's (well tested) CodeHighlighter
// tokenizer and only paints a Python flavored color table on top of it.
class PythonSyntaxHighlighter : public CodeHighlighter {
	GDCLASS(PythonSyntaxHighlighter, CodeHighlighter);

protected:
	static void _bind_methods() {}

	// Registers one color region per `"<begin> <end>"` delimiter, the encoding
	// `ScriptLanguage` uses for comment and string delimiters.
	void add_color_regions(const PackedStringArray &p_delimiters, const Color &p_color);

public:
	// Re-reads the editor theme and rebuilds the keyword colors and color
	// regions. Safe to call again whenever the editor settings change.
	void rebuild();
};

} //namespace pkpy
