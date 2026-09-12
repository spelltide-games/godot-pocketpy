#include "PythonModulesDock.hpp"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "PythonModuleSource.hpp"
#include "PythonSourcePath.hpp"

namespace pkpy {

void PythonModulesDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &PythonModulesDock::refresh);
	ClassDB::bind_method(D_METHOD("rebuild_tree", "filter"), &PythonModulesDock::rebuild_tree);
	ClassDB::bind_method(D_METHOD("open_selected"), &PythonModulesDock::open_selected);
}

PythonModulesDock::PythonModulesDock() {
	set_name("Python");
	HBoxContainer *toolbar = memnew(HBoxContainer);
	add_child(toolbar);
	filter = memnew(LineEdit);
	filter->set_name("Filter");
	filter->set_placeholder("Filter modules");
	filter->set_clear_button_enabled(true);
	filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter->connect("text_changed", Callable(this, "rebuild_tree"));
	toolbar->add_child(filter);
	Button *refresh_button = memnew(Button);
	refresh_button->set_text("Refresh");
	refresh_button->set_tooltip_text("Rescan site-packages for added or removed modules.");
	refresh_button->connect("pressed", Callable(this, "refresh"));
	toolbar->add_child(refresh_button);

	tree = memnew(Tree);
	tree->set_name("Modules");
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_hide_root(false);
	tree->connect("item_activated", Callable(this, "open_selected"));
	add_child(tree);
	status = memnew(Label);
	status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	add_child(status);
	error_dialog = memnew(AcceptDialog);
	error_dialog->set_title("Cannot open Python module");
	add_child(error_dialog);
}

void PythonModulesDock::_ready() {
	refresh();
}

void PythonModulesDock::refresh() {
	module_paths.clear();
	for (const String &path : list_python_package_files()) {
		if (is_python_module_path(path)) {
			module_paths.push_back(path);
		}
	}
	rebuild_tree(filter->get_text());
}

void PythonModulesDock::rebuild_tree(const String &p_filter) {
	// Filtering uses the cached names; typing never traverses the disk again.
	tree->clear();
	TreeItem *root = tree->create_item();
	root->set_text(0, "site-packages");
	root->set_metadata(0, SITE_PACKAGES_PREFIX);
	root->set_tooltip_text(0, "res://site-packages/ — double-click a module to edit it and set breakpoints.");
	root->set_icon(0, get_theme_icon("Folder", "EditorIcons"));
	HashMap<String, TreeItem *> folders;
	folders[""] = root;
	int visible = 0;
	const String query = p_filter.strip_edges();
	for (const String &path : module_paths) {
		const String relative = path.trim_prefix(SITE_PACKAGES_PREFIX);
		if (!query.is_empty() && relative.findn(query) < 0) {
			continue;
		}
		TreeItem *parent = root;
		String directory;
		const PackedStringArray parts = relative.split("/");
		for (int i = 0; i < parts.size() - 1; i++) {
			directory = directory.path_join(parts[i]);
			if (!folders.has(directory)) {
				TreeItem *folder = tree->create_item(parent);
				folder->set_text(0, parts[i]);
				folder->set_metadata(0, String(SITE_PACKAGES_PREFIX) + directory);
				folder->set_icon(0, get_theme_icon("Folder", "EditorIcons"));
				folder->set_collapsed(query.is_empty());
				folders[directory] = folder;
			}
			parent = folders[directory];
		}
		TreeItem *item = tree->create_item(parent);
		item->set_text(0, parts[parts.size() - 1]);
		item->set_metadata(0, path);
		item->set_tooltip_text(0, path);
		item->set_icon(0, get_theme_icon("Script", "EditorIcons"));
		visible++;
	}
	if (!DirAccess::dir_exists_absolute(SITE_PACKAGES_PREFIX)) {
		status->set_text("Create a site-packages folder in your project, then refresh.");
	} else if (module_paths.is_empty()) {
		status->set_text("No Python modules found.");
	} else {
		status->set_text(itos(visible) + " / " + itos(module_paths.size()) + " modules");
	}
}

void PythonModulesDock::open_selected() {
	TreeItem *item = tree->get_selected();
	if (item == nullptr) {
		return;
	}
	const String path = item->get_metadata(0);
	if (!is_python_module_path(path)) {
		item->set_collapsed(!item->is_collapsed());
		return;
	}
	Ref<PythonModuleSource> source = ResourceLoader::get_singleton()->load(path);
	if (source.is_null()) {
		error_dialog->set_text("Could not read " + path + ".\nRefresh the module list if the file was moved or deleted.");
		error_dialog->popup_centered();
		return;
	}
	EditorInterface::get_singleton()->edit_script(source);
	EditorInterface::get_singleton()->set_main_screen_editor("Script");
}

} // namespace pkpy
