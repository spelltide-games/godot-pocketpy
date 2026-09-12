#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "lang/Common.hpp"
#include "lang/PythonDebugger.hpp"
#include "lang/PythonEditorPlugin.hpp"
#include "lang/PythonModuleSource.hpp"
#include "lang/PythonModuleSourceIO.hpp"
#include "lang/PythonScript.hpp"
#include "lang/PythonScriptLanguage.hpp"
#include "lang/PythonScriptResourceFormatLoader.hpp"
#include "lang/PythonScriptResourceFormatSaver.hpp"
#include "lang/PythonSyntaxHighlighter.hpp"

#include "extensions.hpp"
#include "support/DebugPrint.hpp"

using namespace godot;
using namespace pkpy;

static void initialize(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		debug_print("==> initializing pocketpy...\n");

		py_initialize();

		debug_print("==> registering pocketpy classes...\n");

		ClassDB::register_abstract_class<PythonScript>();
		ClassDB::register_abstract_class<PythonScriptLanguage>();
		PythonScriptLanguage::get_or_create_singleton();
		ClassDB::register_class<PythonScriptResourceFormatLoader>();
		ClassDB::register_class<PythonScriptResourceFormatSaver>();
		PythonScriptResourceFormatLoader::register_in_godot();
		PythonScriptResourceFormatSaver::register_in_godot();

		debug_print("==> pocketpy initialized.\n");

		// After the language singleton: registering it runs _init(), which is
		// what sets up pyctx(), and the trace hook reads pyctx() when it breaks.
		PythonDebugger::initialize();

		extensions::setup_godot_classes();
		
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Editor binaries also reach this initialization level when running a
		// game. Source adapters and their loaders belong to the editor process only.
		if (!Engine::get_singleton()->is_editor_hint()) {
			return;
		}
		ClassDB::register_internal_class<PythonModuleSource>();
		ClassDB::register_internal_class<PythonModuleSourceLoader>();
		ClassDB::register_internal_class<PythonModuleSourceSaver>();
		register_module_source_io();
		ClassDB::register_internal_class<PythonModulesDock>();
		ClassDB::register_internal_class<PythonModuleExportPlugin>();
		ClassDB::register_internal_class<PythonSyntaxHighlighter>();
		ClassDB::register_internal_class<PythonEditorPlugin>();
		EditorPlugins::add_by_type<PythonEditorPlugin>();
	}
}

static void uninitialize(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		debug_print("==> unregistering pocketpy classes...\n");
		// Before py_finalize(): the hook must not be able to fire while the VM
		// is tearing frames down.
		PythonDebugger::finalize();
		PythonScriptResourceFormatSaver::unregister_in_godot();
		PythonScriptResourceFormatLoader::unregister_in_godot();
		PythonScriptLanguage::delete_singleton();

		PythonScript::dispose();
		debug_print("==> disposing contexts...\n");
		dispose_contexts();

		debug_print("==> finalizing pocketpy...\n");
		py_finalize();
		debug_print("==> pocketpy uninitialized.\n");
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<PythonEditorPlugin>();
		unregister_module_source_io();
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
