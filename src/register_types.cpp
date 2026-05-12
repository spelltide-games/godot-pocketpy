#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "lang/Common.hpp"
#include "lang/PythonEditorPlugin.hpp"
#include "lang/PythonScript.hpp"
#include "lang/PythonScriptLanguage.hpp"
#include "lang/PythonScriptResourceFormatLoader.hpp"
#include "lang/PythonScriptResourceFormatSaver.hpp"

#include "sbx.hpp"

using namespace godot;
using namespace pkpy;

static void initialize(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		printf("==> initializing pocketpy...\n");

		py_initialize();

		printf("==> registering pocketpy classes...\n");

		ClassDB::register_abstract_class<PythonScript>();
		ClassDB::register_abstract_class<PythonScriptLanguage>();
		PythonScriptLanguage::get_or_create_singleton();
		ClassDB::register_class<PythonScriptResourceFormatLoader>();
		ClassDB::register_class<PythonScriptResourceFormatSaver>();
		PythonScriptResourceFormatLoader::register_in_godot();
		PythonScriptResourceFormatSaver::register_in_godot();

		printf("==> pocketpy initialized.\n");

		setup_sbx_godot_classes();
		
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<PythonEditorPlugin>();
		EditorPlugins::add_by_type<PythonEditorPlugin>();
	}
}

static void uninitialize(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		printf("==> unregistering pocketpy classes...\n");
		PythonScriptResourceFormatSaver::unregister_in_godot();
		PythonScriptResourceFormatLoader::unregister_in_godot();
		PythonScriptLanguage::delete_singleton();

		PythonScript::dispose();
		printf("==> disposing contexts...\n");
		dispose_contexts();

		printf("==> finalizing pocketpy...\n");
		py_finalize();
		printf("==> pocketpy uninitialized.\n");
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<PythonEditorPlugin>();
	}
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT godot_pocketpy_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize);
	init_obj.register_terminator(uninitialize);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}