#include "PythonSyntaxHighlighter.hpp"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "PythonScriptLanguage.hpp"

namespace pkpy {

namespace {

// Types pocketpy exposes in `builtins`, including the exception hierarchy.
const char *BUILTIN_TYPES[] = {
	"bool", "bytes", "classmethod", "complex", "dict", "float", "int", "list",
	"object", "property", "range", "set", "slice", "staticmethod", "str",
	"super", "tuple", "type",
	"AssertionError", "AttributeError", "BaseException", "Exception",
	"ImportError", "IndexError", "KeyError", "KeyboardInterrupt", "NameError",
	"NotImplementedError", "OSError", "PermissionError", "RecursionError",
	"RuntimeError", "StopIteration", "SyntaxError", "SystemExit",
	"TimeoutError", "TypeError", "UnboundLocalError", "ValueError",
	"ZeroDivisionError",
	nullptr
};

// Functions pocketpy exposes in `builtins`. Without these they would fall back
// to the generic "call" color, which makes them look like user code.
const char *BUILTIN_FUNCTIONS[] = {
	"__import__", "abs", "all", "any", "bin", "callable", "chr", "compile",
	"delattr", "dir", "divmod", "enumerate", "eval", "exec", "exit", "filter",
	"getattr", "globals", "hasattr", "hash", "help", "hex", "id", "input",
	"isinstance", "issubclass", "iter", "len", "locals", "map", "max", "min",
	"next", "ord", "print", "repr", "reversed", "round", "setattr", "sorted",
	"sum", "zip",
	nullptr
};

// Not keywords, but they name the instance/class in every method body and read
// as one, so they get the same color members do.
const char *SOFT_NAMES[] = { "self", "cls", nullptr };

// Helpers from the `godot` module that shape a script's interface. They are the
// closest thing this language has to GDScript's annotations.
const char *SCRIPT_HELPERS[] = { "Extends", "export", "export_range", "signal", nullptr };

Color theme_color(const Ref<EditorSettings> &p_settings, const char *p_name, const Color &p_fallback) {
	if (p_settings.is_null()) {
		return p_fallback;
	}
	const String key = String("text_editor/theme/highlighting/") + p_name;
	if (!p_settings->has_setting(key)) {
		return p_fallback;
	}
	const Variant value = p_settings->get_setting(key);
	if (value.get_type() != Variant::COLOR) {
		return p_fallback;
	}
	return value;
}

} //namespace

PythonSyntaxHighlighter::PythonSyntaxHighlighter() {
	highlighter.instantiate();
}

String PythonSyntaxHighlighter::_get_name() const {
	return "Python";
}

PackedStringArray PythonSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray languages;
	languages.push_back("Python");
	return languages;
}

Dictionary PythonSyntaxHighlighter::_get_line_syntax_highlighting(int32_t p_line) const {
	// Returns an empty map while the inner highlighter has no TextEdit, which
	// costs us the colors on that editor but never crashes.
	return highlighter->get_line_syntax_highlighting(p_line);
}

void PythonSyntaxHighlighter::_update_cache() {
	// Called by the editor whenever the theme or the text changes.
	rebuild();
}

void PythonSyntaxHighlighter::_clear_highlighting_cache() {
	highlighter->clear_highlighting_cache();
}

void PythonSyntaxHighlighter::rebuild() {
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	EditorInterface *editor = EditorInterface::get_singleton();
	const Ref<EditorSettings> settings = editor != nullptr ? editor->get_editor_settings() : Ref<EditorSettings>();

	// Fallbacks are the defaults of the editor's dark theme; they only matter if
	// the settings are somehow unavailable.
	const Color symbol_color = theme_color(settings, "symbol_color", Color(0.67, 0.79, 1.0));
	const Color function_color = theme_color(settings, "function_color", Color(0.34, 0.70, 1.0));
	const Color number_color = theme_color(settings, "number_color", Color(0.63, 1.0, 0.88));
	const Color member_variable_color = theme_color(settings, "member_variable_color", Color(0.74, 0.88, 1.0));
	const Color keyword_color = theme_color(settings, "keyword_color", Color(1.0, 0.44, 0.52));
	const Color control_flow_keyword_color = theme_color(settings, "control_flow_keyword_color", Color(1.0, 0.55, 0.80));
	const Color base_type_color = theme_color(settings, "base_type_color", Color(0.56, 1.0, 0.86));
	const Color engine_type_color = theme_color(settings, "engine_type_color", Color(0.56, 1.0, 0.86));
	const Color user_type_color = theme_color(settings, "user_type_color", Color(0.78, 1.0, 0.93));
	const Color comment_color = theme_color(settings, "comment_color", Color(0.80, 0.81, 0.82, 0.5));
	const Color doc_comment_color = theme_color(settings, "doc_comment_color", Color(0.60, 0.70, 0.80, 0.8));
	const Color string_color = theme_color(settings, "string_color", Color(1.0, 0.93, 0.63));
	const Color global_function_color = theme_color(settings, "gdscript/global_function_color", function_color);
	const Color annotation_color = theme_color(settings, "gdscript/annotation_color", Color(1.0, 0.70, 0.70));

	highlighter->set_symbol_color(symbol_color);
	highlighter->set_function_color(function_color);
	highlighter->set_number_color(number_color);
	highlighter->set_member_variable_color(member_variable_color);

	// Engine classes go in first: everything added below may legitimately
	// shadow them, and the last color registered for a word wins.
	const PackedStringArray engine_types = ClassDBSingleton::get_singleton()->get_class_list();
	for (int i = 0; i < engine_types.size(); i++) {
		const String name = engine_types[i];
		highlighter->add_keyword_color(name.begins_with("_") ? name.substr(1) : name, engine_type_color);
	}

	const TypedArray<Dictionary> global_classes = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < global_classes.size(); i++) {
		const Dictionary entry = global_classes[i];
		const String name = entry.get("class", String());
		if (!name.is_empty()) {
			highlighter->add_keyword_color(name, user_type_color);
		}
	}

	for (const char **it = BUILTIN_TYPES; *it != nullptr; it++) {
		highlighter->add_keyword_color(*it, base_type_color);
	}
	for (const char **it = BUILTIN_FUNCTIONS; *it != nullptr; it++) {
		highlighter->add_keyword_color(*it, global_function_color);
	}
	for (const char **it = SCRIPT_HELPERS; *it != nullptr; it++) {
		highlighter->add_keyword_color(*it, annotation_color);
	}
	for (const char **it = SOFT_NAMES; *it != nullptr; it++) {
		highlighter->add_keyword_color(*it, member_variable_color);
	}

	// Keywords last, so a real keyword always beats a shadowed builtin.
	PythonScriptLanguage *language = PythonScriptLanguage::get_singleton();
	if (language != nullptr) {
		const PackedStringArray reserved_words = language->_get_reserved_words();
		for (int i = 0; i < reserved_words.size(); i++) {
			const String word = reserved_words[i];
			highlighter->add_keyword_color(word, language->_is_control_flow_keyword(word) ? control_flow_keyword_color : keyword_color);
		}

		// Same "<begin> <end>" encoding the editor's standard highlighter uses;
		// an absent end key means the region stops at the end of the line.
		// CodeHighlighter itself tries longer start keys first, so `"""` wins
		// over `"` no matter which order they are added in.
		add_color_regions(language->_get_string_delimiters(), string_color);
		add_color_regions(language->_get_doc_comment_delimiters(), doc_comment_color);
		add_color_regions(language->_get_comment_delimiters(), comment_color);
	}

	highlighter->clear_highlighting_cache();
}

void PythonSyntaxHighlighter::add_color_regions(const PackedStringArray &p_delimiters, const Color &p_color) {
	for (int i = 0; i < p_delimiters.size(); i++) {
		const String delimiter = p_delimiters[i];
		const String begin = delimiter.get_slice(" ", 0);
		// CodeHighlighter aborts on an empty or duplicated start key.
		if (begin.is_empty() || highlighter->has_color_region(begin)) {
			continue;
		}
		const String end = delimiter.get_slice_count(" ") > 1 ? delimiter.get_slice(" ", 1) : String();
		highlighter->add_color_region(begin, end, p_color, end.is_empty());
	}
}

} //namespace pkpy
