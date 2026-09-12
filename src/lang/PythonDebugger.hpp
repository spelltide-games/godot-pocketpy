#pragma once

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "pocketpy.h"

using namespace godot;

namespace pkpy {

// Connects pocketpy's per-line trace hook to Godot's script debugger, so that a
// breakpoint set on a `.py` file suspends the Python VM the way GDScript
// suspends. Both debugger front ends come out of this one bridge: the editor's
// own Debugger dock, and any DAP client (VS Code) attached to the editor's
// debug adapter server, because that server is a translator in front of the
// same engine-side protocol and carries no GDScript assumptions.
//
// The whole flow lives on a single call stack:
//
//   pocketpy ceval loop
//     -> trace_func(frame, TRACE_EVENT_LINE)              [this file]
//        -> EngineDebugger::script_debug(language)        [blocks]
//           -> RemoteDebugger::debug() message loop       [engine]
//              -> PythonScriptLanguage::_debug_*          [this file]
//           <- "next" / "step" / "continue"
//     <- ceval loop resumes
//
// Because `script_debug()` blocks *inside* the trace hook, the VM is frozen for
// its duration: every `py_Frame` reachable from the breaking frame is still
// alive, so the `_debug_*` readers can walk them safely. Those pointers are
// valid only while `is_broken()` -- nothing may cache one past a resume.
//
// Unhandled exceptions arrive on their own, through the debugger callbacks in
// py_appcallbacks() that initialize() claims: pocketpy records the frames and
// their variables as the exception propagates, then hands it over. Nothing
// outside this file has to call anything for that to work.
//
// Stepping is not tracked here. The engine owns it, as two thread-local
// counters on `ScriptDebugger` (`lines_left`, `depth`) that the editor sets from
// the step/next/continue commands; this file only reads them and keeps `depth`
// in step with Python's call depth. That is the same contract the GDScript VM
// implements in `OPCODE_LINE` and `enter_function`/`exit_function`.
struct PythonDebugger {
	// Project setting that gates all of this. Off by default -- arming the hook
	// costs roughly 4x on Python-heavy code, so debugging is opted into rather
	// than paid for on every run. Registered (so it shows up under Project
	// Settings) even when the debugger will not arm, because the editor process
	// is exactly where someone goes to turn it on.
	static constexpr const char *ENABLED_SETTING = "python/debugger/enabled";

	// Installs the trace hook -- but only when the setting above is on AND the
	// process was started with a remote debugger attached (the editor's Play
	// button, or `--remote-debug`). A standalone game, an exported build, and
	// the editor process itself stay untraced and pay nothing. Safe to call more
	// than once.
	static void initialize();
	static void finalize();

	// Drops the per-line breakpoint memo. Called once a frame from
	// PythonScriptLanguage::_frame(), which is what bounds how long a newly set
	// breakpoint can take to be noticed.
	static void flush_breakpoint_cache();

	// Backs `PythonScriptLanguage::_debug_*`. Level 0 is the innermost frame,
	// matching GDScript.
	static String get_error();
	static int32_t get_stack_level_count();
	static int32_t get_stack_level_line(int32_t p_level);
	static String get_stack_level_function(int32_t p_level);
	static String get_stack_level_source(int32_t p_level);
	static Dictionary get_stack_level_locals(int32_t p_level, int32_t p_max_subitems);
	static Dictionary get_stack_level_members(int32_t p_level, int32_t p_max_subitems);
	static void *get_stack_level_instance(int32_t p_level);
	static Dictionary get_globals(int32_t p_max_subitems);
	static TypedArray<Dictionary> get_current_stack_info();
};

} //namespace pkpy
