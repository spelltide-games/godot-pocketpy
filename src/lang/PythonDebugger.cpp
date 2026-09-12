// These two pocketpy internals are the only ones this file needs; everything
// else goes through the public API. They are reachable because pocketpy exports
// its whole `include/` tree (`target_include_directories(... INTERFACE
// include)`), the same way custom_sname.cpp already reaches
// `pocketpy/common/name.h`.
//
// THEY MUST COME FIRST, ahead of every other include, for two reasons:
//
//  1. `pocketpy.h` -- the public shim -- defines PK_IS_PUBLIC_INCLUDE, which
//     makes `pocketpy/pocketpy.h` declare an opaque `py_TValue`. The real one
//     lives in `objects/base.h`, and the two collide. Reaching
//     `pocketpy/pocketpy.h` through an internal header first (so without that
//     macro) leaves the opaque definition out, and its `#pragma once` then
//     makes the later public include a no-op.
//  2. pocketpy has a `RefCounted` struct and so does godot-cpp. Once
//     `using namespace godot` is in scope the name is ambiguous, and these
//     headers use it unqualified.
//
// What is missing upstream is exactly two accessors -- a `py_Frame_back()` and
// a `py_Frame_funcname()`. If those land, delete both includes, drop the two
// helpers under "pocketpy internals" below, and this ordering constraint goes
// away with them.
#include "pocketpy/interpreter/frame.h"
#include "pocketpy/objects/codeobject.h"
#include "pocketpy/objects/exception.h"

#include "PythonDebugger.hpp"

#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/print_string.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "Common.hpp"
#include "PythonScriptInstance.hpp"
#include "PythonScriptLanguage.hpp"

namespace pkpy {

namespace {

/* ------------------------------- pocketpy internals ----------------------- */

py_Frame *frame_parent(py_Frame *p_frame) {
	return p_frame->f_back;
}

String frame_funcname(py_Frame *p_frame) {
	const c11_string *name = p_frame->co->name;
	return String::utf8(name->data, name->size);
}

/* ------------------------------------ state ------------------------------- */

// Godot's break reason for an ordinary breakpoint. Two separate pieces of the
// engine key off this exact literal, so it is not a cosmetic string:
//
//   - GDScriptLanguage::debug_break() derives `is_error_breakpoint` from
//     `p_error != "Breakpoint"`, and RemoteDebugger::debug() returns without
//     breaking when `is_skipping_breakpoints() && !is_error_breakpoint`. Get it
//     wrong and the debugger's "skip breakpoints" toggle stops working.
//   - DebugAdapterProtocol::on_debug_breaked() compares the reason against
//     "Breakpoint" to decide between a `stopped(breakpoint)` and a
//     `stopped(exception)` DAP event. Get it wrong and VS Code reports every
//     breakpoint as a crash.
constexpr const char *BREAK_REASON_BREAKPOINT = "Breakpoint";

// One stack level as the debugger serves it.
//
// Both kinds of break are flattened into this so the accessors have a single
// shape to read. A breakpoint snapshots the live frame chain at the moment it
// stops; an exception reads the one pocketpy recorded as it propagated, because
// by the time anyone knows it went unhandled every frame is already gone. It
// also means no raw py_Frame outlives the break that produced it.
struct StackLevel {
	String file; // res:// path
	String func;
	int line = 0;
	int locals_slot = -1; // index into the scope list; globals is the next one
};

// GC-rooted holder for the locals and globals dicts the levels point into.
// Register 12 is reserved for precisely this ("for debugger", pocketpy.h) and
// every register is walked by the collector (VM__gc_mark) -- which is what these
// need, since they have to survive the repr() calls made while serving a break,
// and in the exception case the whole unwind before that.
inline py_Ref scope_list() { return py_sysr0(); }

// Allocated by initialize(), released by finalize(), non-null exactly while the
// trace hook is installed. Heap-allocated rather than static storage because it
// holds a StringName: a static one would be destroyed after Godot has torn its
// string table down.
struct DebuggerState {
	bool broken = false;
	String break_reason;

	// Innermost first, which is Godot's level 0.
	Vector<StackLevel> levels;

	// py_Frame_sourceloc() hands back the SourceData's own filename buffer,
	// whose address is stable for as long as the compiled script lives. This
	// one-entry memo on that pointer keeps StringName construction -- a hash
	// plus a lookup in the engine's global table -- off a path that runs once
	// per executed Python line.
	const char *cached_source_ptr = nullptr;
	StringName cached_source_name;

	// Memo of EngineDebugger::is_breakpoint() answers, keyed by the source
	// pointer and line that produced them.
	//
	// Worth the bookkeeping because that query is a call across the GDExtension
	// boundary and a loop re-executes the same handful of lines thousands of
	// times per frame. Breakpoints are pushed in by the editor with no
	// notification we can hook, so the memo is simply dropped every frame --
	// which is also the worst-case latency for a breakpoint the user just set.
	HashMap<uint64_t, bool> breakpoint_memo;

	// Drives the two periodic jobs at the end of the line handler; see
	// LINE_POLL_INTERVAL and MEMO_FLUSH_INTERVAL.
	uint32_t line_tick = 0;
};

// Call EngineDebugger::line_poll() every 16th line rather than every one.
// line_poll() only does work on every 2048th call (a counter plus a
// poll_events(), see engine_debugger.h) and exists to keep the debugger
// answerable inside a runaway loop; ordinary responsiveness comes from the
// per-frame idle poll. So this makes the engine poll every ~32k Python lines
// instead of every ~2k, which is still well inside a frame, and buys back
// fifteen sixteenths of a boundary call per line.
constexpr uint32_t LINE_POLL_INTERVAL = 16;

// Second flush path for breakpoint_memo, on top of the per-frame one. A loop
// that never yields back to _frame() would otherwise hold a stale memo forever,
// so a breakpoint set inside a runaway loop would never be noticed -- which is
// exactly the situation someone reaches for a breakpoint in. Both intervals are
// powers of two so one counter and two masks cover it.
constexpr uint32_t MEMO_FLUSH_INTERVAL = 2048;

DebuggerState *state = nullptr;

/* ------------------------------ value formatting -------------------------- */

String sv_to_string(c11_sv p_sv) {
	return String::utf8(p_sv.data, p_sv.size);
}

// Godot keys breakpoints -- and opens files -- by res:// path, so every source
// name leaving this file has to be one.
//
// Scripts already are: PythonScript::_reload hands py_exec() the script's own
// res:// path. Imported modules are not: pocketpy builds their filename itself,
// as the relative path it probed for ("test.py", or "pkg\__init__.py" with a
// platform separator), and the importfile callback only gets to supply the
// bytes. So rebuild the path that callback resolved -- the prefix is shared with
// it via SITE_PACKAGES_PREFIX precisely so the two cannot drift.
String source_to_res_path(const char *p_source) {
	String path = String::utf8(p_source);
	if (path.begins_with("<") && path.ends_with(">")) {
		return path; // eval/embedded sources are not files under site-packages.
	}
	if (path.begins_with("res://")) {
		return normalize_python_path(path);
	}
	return resolve_python_module_path(path);
}

// Runs Python, so only legal with the trace hook off -- which is the case for
// everything reached from a break.
String to_repr_string(py_Ref p_val) {
	py_StackRef p0 = py_peek(0);
	if (!py_repr(p_val)) {
		py_clearexc(p0);
		return String("<") + py_tpname(py_typeof(p_val)) + ">";
	}
	return sv_to_string(py_tosv(py_retval()));
}

// The editor renders debugger values through the Variant inspector, so every
// Python object has to become one. Anything without a Variant counterpart is
// shown as its repr(), which is what a Python programmer expects to read
// anyway.
//
// py_tovariant_checked() rather than py_tovariant(): a miss is the normal case
// here, not a bug, and the loud version would put a line in the output log for
// most variables in a frame.
Variant to_display_variant(py_Ref p_val) {
	Variant out;
	if (py_tovariant_checked(p_val, &out)) {
		return out;
	}
	if (py_issubclass(py_typeof(p_val), pyctx()->tp_Script)) {
		// A script instance reads best as its owner node: the inspector can
		// expand that, and `self` is already handed to the editor that way by
		// RemoteDebugger (from _debug_get_stack_level_instance).
		PythonScriptInstance *instance = (PythonScriptInstance *)py_touserdata(p_val);
		if (instance->owner != nullptr) {
			return instance->owner;
		}
	}
	return to_repr_string(p_val);
}

// Things a module namespace holds because of an `import`, not because they are
// state worth looking at while stopped.
//
// `from godot import *` supplies engine singletons, built-in types and helpers.
// Filter names in that root module with a plain namedict lookup. Global
// constants live in `godot.constants` and are outside this filter's scope.
bool is_import_noise(py_Name p_name, py_Ref p_val) {
	c11_sv name = py_name2sv(p_name);
	if (name.size >= 2 && name.data[0] == '_' && name.data[1] == '_') {
		return true;
	}
	switch (py_typeof(p_val)) {
		case tp_type:
		case tp_module:
		case tp_function:
		case tp_nativefunc:
		case tp_boundmethod:
		case tp_staticmethod:
		case tp_classmethod:
			return true;
		default:
			break;
	}
	// `from godot.classes import Node, ...` leaves GDNativeClass handles behind.
	// They are not tp_type, and they live in `godot.classes` rather than `godot`,
	// so neither test above catches them -- but a class handle is never state.
	if (py_typeof(p_val) == pyctx()->tp_GDNativeClass) {
		return true;
	}
	return py_getdict(pyctx()->godot, p_name) != nullptr;
}

// Flattens a mapping into the parallel name/value arrays Godot wants.
//
// Goes through `list(mapping.items())` instead of py_dict_apply()/py_applydict()
// on purpose: converting a value can call repr(), which allocates and may
// trigger a GC, and that is not safe to do from inside those walks. Evaluating
// items() up front also means one code path covers both shapes
// py_Frame_newglobals() can return (a dict, or a namedict view over a module).
// The list sits on the VM stack while it is read, so the GC keeps every element
// reachable.
void collect_mapping(py_Ref p_mapping, int32_t p_max_subitems, bool p_drop_imports, PackedStringArray &r_names, Array &r_values) {
	py_StackRef p0 = py_peek(0);
	if (!py_smarteval("list(_0.items())", nullptr, p_mapping)) {
		py_clearexc(p0);
		return;
	}
	py_Ref items = py_pushtmp();
	py_assign(items, py_retval());

	int count = py_list_len(items);
	for (int i = 0; i < count; i++) {
		if (p_max_subitems >= 0 && r_names.size() >= p_max_subitems) {
			break;
		}
		py_Ref pair = py_list_getitem(items, i);
		py_Ref key = py_tuple_getitem(pair, 0);
		py_Ref val = py_tuple_getitem(pair, 1);
		if (!py_istype(key, tp_str)) {
			r_names.push_back(to_repr_string(key));
			r_values.push_back(to_display_variant(val));
			continue;
		}
		if (p_drop_imports && is_import_noise(py_namev(py_tosv(key)), val)) {
			continue;
		}
		r_names.push_back(sv_to_string(py_tosv(key)));
		r_values.push_back(to_display_variant(val));
	}

	py_pop(); // items
}

Dictionary mapping_to_scope(py_Ref p_mapping, const char *p_names_key, int32_t p_max_subitems, bool p_drop_imports = false) {
	PackedStringArray names;
	Array values;
	collect_mapping(p_mapping, p_max_subitems, p_drop_imports, names, values);

	Dictionary out;
	out[p_names_key] = names;
	out["values"] = values;
	return out;
}

/* -------------------------------- stack levels ---------------------------- */

void levels_reset() {
	state->levels.clear();
	py_newlist(scope_list());
}

// Appends one live frame as the next level, capturing its locals and globals
// into the scope list. Callers append innermost first.
//
// The dicts are built straight into their list slots rather than into a local
// and copied in. A py_TValue on the C stack is not a GC root, and
// py_Frame_newlocals() allocates -- so this shape is safe by construction
// instead of by an argument about what does and does not collect. It is also
// why snapshot_exception_stack() cannot share this helper: it already has the
// dicts and only needs to reference them.
void levels_push_frame(py_Frame *p_frame) {
	StackLevel level;
	int line = 0;
	level.file = source_to_res_path(py_Frame_sourceloc(p_frame, &line));
	level.line = line;
	level.func = frame_funcname(p_frame);
	level.locals_slot = py_list_len(scope_list());
	// Two appends, read back by slot; the second may reallocate the list's
	// storage, so the first ref is finished with before it is taken.
	py_Frame_newlocals(p_frame, py_list_emplace(scope_list()));
	py_Frame_newglobals(p_frame, py_list_emplace(scope_list()));
	state->levels.push_back(level);
}

const StackLevel *level_at(int32_t p_level) {
	if (state == nullptr || !state->broken || p_level < 0 || p_level >= state->levels.size()) {
		return nullptr;
	}
	return state->levels.ptr() + p_level;
}

py_Ref scope_at(int p_slot) {
	if (p_slot < 0 || p_slot >= py_list_len(scope_list())) {
		return nullptr;
	}
	return py_list_getitem(scope_list(), p_slot);
}

// The `self` in a level's locals, or nullptr for a plain function or module
// body.
PythonScriptInstance *instance_from_locals(py_Ref p_locals) {
	if (p_locals == nullptr) {
		return nullptr;
	}
	py_StackRef p0 = py_peek(0);
	int found = py_dict_getitem_by_str(p_locals, "self");
	if (found == -1) {
		py_clearexc(p0);
		return nullptr;
	}
	if (found != 1 || !py_issubclass(py_typeof(py_retval()), pyctx()->tp_Script)) {
		return nullptr;
	}
	// Read straight out of retval: the next VM call would overwrite it.
	return (PythonScriptInstance *)py_touserdata(py_retval());
}

// Snapshots the live chain for a stop that happens while the frames still
// exist, i.e. a breakpoint or a step.
void snapshot_live_stack(py_Frame *p_top) {
	levels_reset();
	for (py_Frame *frame = p_top; frame != nullptr; frame = frame_parent(frame)) {
		levels_push_frame(frame);
	}
}

// Snapshots the frames pocketpy recorded on an exception as it propagated.
//
// py_BaseException__stpush() fills these in at every frame the exception passes
// through, innermost first -- which is already the order Godot wants -- and
// captures each frame's locals and globals while that frame still exists. That
// is the whole reason not to rebuild any of it here: this is the same walk, at
// the same moment, and pocketpy already does it.
//
// Only the dicts are referenced, never copied, and they are parked in the
// GC-rooted scope list so they outlive py_clearexc().
bool snapshot_exception_stack(py_Ref p_exc) {
	// Non-const only because py_list_append() takes a mutable ref; nothing here
	// writes to the exception.
	BaseException *ud = (BaseException *)py_touserdata(p_exc);
	if (ud->stacktrace.length == 0) {
		return false; // no python frames were involved, e.g. a compile error
	}
	levels_reset();
	for (int i = 0; i < ud->stacktrace.length; i++) {
		BaseExceptionFrame *dump = c11__at(BaseExceptionFrame, &ud->stacktrace, i);
		StackLevel level;
		level.file = source_to_res_path(dump->src->filename->data);
		level.line = dump->lineno;
		level.func = dump->name != nullptr ? String::utf8(dump->name->data, dump->name->size) : String();
		level.locals_slot = py_list_len(scope_list());
		py_list_append(scope_list(), &dump->locals);
		py_list_append(scope_list(), &dump->globals);
		state->levels.push_back(level);
	}
	return true;
}

/* --------------------------------- trace hook ----------------------------- */

void trace_func(py_Frame *p_frame, enum py_TraceEvent p_event);

void break_at(py_Frame *p_frame, const String &p_reason) {
	// Only the main thread has an editor-facing message loop to resume it; a
	// break on a worker would block with nothing able to let it go. GDScript
	// gates debug_break_parse() the same way.
	if (std::this_thread::get_id() != pyctx()->main_thread_id) {
		return;
	}
	if (state->broken) {
		return; // already suspended; do not nest
	}

	// Everything the editor asks for between here and the return below runs
	// Python -- repr(), items() walks -- so the hook has to be off for the
	// duration or it would recurse into itself. reset=false keeps
	// TraceInfo::prev_loc pointing at the line we stopped on, which is what
	// stops the same line from firing again the instant we resume.
	py_sys_settrace(nullptr, false);

	// A live frame means a breakpoint or a step, so the chain is still there to
	// walk. Null means an exception, whose levels snapshot_exception_stack() has
	// already put in place.
	if (p_frame != nullptr) {
		snapshot_live_stack(p_frame);
	}
	state->broken = true;
	state->break_reason = p_reason;

	// Blocks until the editor answers step/next/continue. The engine derives
	// `is_error_breakpoint` from the reason exactly as GDScript does, so that
	// "skip breakpoints" suppresses ordinary breakpoints but never real errors.
	bool is_error_breakpoint = p_reason != String(BREAK_REASON_BREAKPOINT);
	EngineDebugger::get_singleton()->script_debug(PythonScriptLanguage::get_singleton(), true, is_error_breakpoint);

	state->broken = false;
	state->break_reason = String();
	// Drop the snapshot so the captured locals and globals stop being reachable
	// the moment the break ends.
	levels_reset();

	py_sys_settrace(trace_func, false);
}

/* ----------------------------- pocketpy callbacks ------------------------- */

// py_appcallbacks()->debugger_status. pocketpy asks this to decide how much
// detail an exception is worth recording: at 1 it gives a frame dump 31 levels
// deep with each frame's locals and globals, at anything else 7 levels and no
// variables (py_BaseException__stpush).
//
// The three states are pocketpy's, and `broken` maps onto them exactly. A break
// runs python of its own -- repr() over the captured scopes -- and reporting 2
// there is what stops that from being recorded as if it were the program's own
// exception, as well as what keeps the callback below from re-entering.
int debugger_status_cb() {
	if (state == nullptr) {
		return 0; // detached
	}
	return state->broken ? 2 : 1;
}

// py_appcallbacks()->debugger_exceptionbreakpoint. Called from py_formatexc(),
// which is the moment a host reports an exception it is not going to handle --
// so pocketpy has already decided this one is worth stopping for, and the
// caught ones never arrive here.
void debugger_exceptionbreakpoint_cb(py_Ref p_exc) {
	// No re-entrancy check needed: pocketpy only calls this while
	// debugger_status_cb() answers 1, which it does not do during a break. The
	// null check is for the window where the callback is still installed but the
	// state is gone.
	if (state == nullptr) {
		return;
	}
	if (!snapshot_exception_stack(p_exc)) {
		return; // no python frames were involved, e.g. a compile error
	}

	// Detached for the rest of this: building the reason runs __str__, and a
	// line event out of that must not be able to trip a breakpoint and nest a
	// second break inside this one. break_at() re-attaches on its way out.
	py_sys_settrace(nullptr, false);

	// p_exc aliases vm->unhandled_exc, so it reads as nil the moment the
	// exception is cleared. Park a reference in the scope list -- which is
	// GC-rooted, and which snapshot_exception_stack() has just reset -- so the
	// object survives both the clear and the collections the break may cause.
	int exc_slot = py_list_len(scope_list());
	py_list_append(scope_list(), p_exc);

	// Cleared before running any python, the same way pocketpy's own DAP hook
	// does it (c11_debugger_exception_on_trace): a break evaluates repr() over
	// the captured scopes, and that must not happen with an exception pending.
	py_clearexc(nullptr);

	// "ValueError: kaboom". The reason is what the editor shows as the break
	// cause, and anything other than the literal "Breakpoint" counts as an error
	// break -- which is what turns this into a stopped(exception) DAP event, and
	// what puts it under the debugger's "Ignore Error Breaks" toggle rather than
	// "Skip Breakpoints".
	py_Ref exc = scope_at(exc_slot);
	String reason = py_tpname(py_typeof(exc));
	if (py_str(exc)) {
		reason += String(": ") + sv_to_string(py_tosv(py_retval()));
	} else {
		py_clearexc(nullptr); // __str__ raised; the type name alone will do
	}

	// Null frame: the levels came from the exception, not a live chain to walk.
	break_at(nullptr, reason);
}

void trace_func(py_Frame *p_frame, enum py_TraceEvent p_event) {
	EngineDebugger *dbg = EngineDebugger::get_singleton();

	if (p_event != TRACE_EVENT_LINE) {
		// Keep `depth` in step with Python's call depth, but only while a step
		// is pending -- that is the condition under which the counter means
		// anything. It is what lets "next" tell "a deeper frame" (do not stop)
		// from "back at my level" (stop). Mirrors GDScript's
		// enter_function()/exit_function().
		if (dbg->get_lines_left() > 0 && dbg->get_depth() >= 0) {
			dbg->set_depth(dbg->get_depth() + (p_event == TRACE_EVENT_PUSH ? 1 : -1));
		}
		return;
	}

	int line = 0;
	const char *source = py_Frame_sourceloc(p_frame, &line);

	bool do_break = false;

	// A pending step, counted down one line at a time.
	int32_t lines_left = dbg->get_lines_left();
	if (lines_left > 0) {
		if (dbg->get_depth() <= 0) {
			lines_left--;
			dbg->set_lines_left(lines_left);
		}
		if (lines_left <= 0) {
			do_break = true;
		}
	}

	// Breakpoints live in the engine: the editor pushes them into the game's
	// ScriptDebugger keyed by res:// path, so that is what has to be looked up.
	// `is_skipping_breakpoints` is deliberately NOT checked here -- it would
	// cost another boundary call on every line, and RemoteDebugger::debug()
	// already returns immediately when it is set.
	if (!do_break) {
		// The source pointer is stable per compiled script and lines fit in 32
		// bits, so the pair packs into one key with no hashing of strings.
		uint64_t key = (uint64_t)(uintptr_t)source ^ ((uint64_t)(uint32_t)line << 48);
		if (const bool *memo = state->breakpoint_memo.getptr(key)) {
			do_break = *memo;
		} else {
			if (source != state->cached_source_ptr) {
				state->cached_source_ptr = source;
				state->cached_source_name = StringName(source_to_res_path(source));
			}
			do_break = dbg->is_breakpoint(line, state->cached_source_name);
			state->breakpoint_memo.insert(key, do_break);
		}
	}

	if (do_break) {
		break_at(p_frame, BREAK_REASON_BREAKPOINT);
	}

	state->line_tick++;
	if ((state->line_tick & (LINE_POLL_INTERVAL - 1)) == 0) {
		dbg->line_poll();
		if ((state->line_tick & (MEMO_FLUSH_INTERVAL - 1)) == 0) {
			state->breakpoint_memo.clear();
		}
	}
}

} //namespace

/* --------------------------------- public API ----------------------------- */

// Off by default: arming the trace hook costs roughly 4x on Python-heavy code,
// and about two thirds of that is pocketpy's own per-bytecode trace check,
// which nothing on this side can reduce. Most runs are not debugging runs, so
// the default is the cheap one and debugging is opted into.
constexpr bool ENABLED_DEFAULT = false;

// Declares ENABLED_SETTING so it appears under Project Settings, and returns
// what it is set to. Runs in the editor too -- that is where someone goes to
// turn it on -- so it must not be behind the is_active() check below.
static bool register_and_read_enabled_setting() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings->has_setting(PythonDebugger::ENABLED_SETTING)) {
		settings->set_setting(PythonDebugger::ENABLED_SETTING, ENABLED_DEFAULT);
	}
	// Godot only writes a setting into project.godot once it differs from its
	// initial value, so declaring the default here keeps an untouched project
	// file clean.
	settings->set_initial_value(PythonDebugger::ENABLED_SETTING, ENABLED_DEFAULT);
	settings->set_as_basic(PythonDebugger::ENABLED_SETTING, true);

	Dictionary info;
	info["name"] = PythonDebugger::ENABLED_SETTING;
	info["type"] = Variant::BOOL;
	settings->add_property_info(info);

	return settings->get_setting_with_override(PythonDebugger::ENABLED_SETTING);
}

void PythonDebugger::initialize() {
	if (state != nullptr) {
		return;
	}
	bool enabled = register_and_read_enabled_setting();

	// is_active() is decided once, in EngineDebugger::initialize() while Main
	// parses `--remote-debug`, and never flips afterwards -- so this one check
	// at startup is enough, and a game launched without a debugger never pays
	// for the hook. The editor process has no remote debugger of its own, so
	// `@tool` scripts there also stay untraced.
	if (!EngineDebugger::get_singleton()->is_active() || !enabled) {
		return;
	}

	state = memnew(DebuggerState);
	py_sys_settrace(trace_func, true);
	// pocketpy routes its debugger hooks through these, defaulting them to its
	// own DAP implementation when PK_ENABLE_OS is on and leaving them null
	// otherwise -- which is this build. Claiming them is what makes pocketpy
	// record exception stacks in full detail (see debugger_status_cb) and hand
	// unhandled exceptions over to us (debugger_exceptionbreakpoint_cb).
	py_AppCallbacks *app = py_appcallbacks();
	app->debugger_status = debugger_status_cb;
	app->debugger_exceptionbreakpoint = debugger_exceptionbreakpoint_cb;
	print_line("=> Python debugger enabled");
}

void PythonDebugger::flush_breakpoint_cache() {
	if (state != nullptr && !state->breakpoint_memo.is_empty()) {
		state->breakpoint_memo.clear();
	}
}

void PythonDebugger::finalize() {
	if (state == nullptr) {
		return;
	}
	py_sys_settrace(nullptr, true);
	py_AppCallbacks *app = py_appcallbacks();
	app->debugger_status = nullptr;
	app->debugger_exceptionbreakpoint = nullptr;
	memdelete(state);
	state = nullptr;
}

String PythonDebugger::get_error() {
	return state != nullptr ? state->break_reason : String();
}

int32_t PythonDebugger::get_stack_level_count() {
	// RemoteDebugger::debug() reports `count > 0` to the editor as
	// "has stack dump"; at zero it never asks for one and both front ends show
	// an empty call stack.
	if (state == nullptr || !state->broken) {
		return 0;
	}
	return state->levels.size();
}

int32_t PythonDebugger::get_stack_level_line(int32_t p_level) {
	const StackLevel *level = level_at(p_level);
	return level != nullptr ? level->line : 0;
}

String PythonDebugger::get_stack_level_function(int32_t p_level) {
	const StackLevel *level = level_at(p_level);
	return level != nullptr ? level->func : String();
}

String PythonDebugger::get_stack_level_source(int32_t p_level) {
	const StackLevel *level = level_at(p_level);
	return level != nullptr ? level->file : String();
}

Dictionary PythonDebugger::get_stack_level_locals(int32_t p_level, int32_t p_max_subitems) {
	const StackLevel *level = level_at(p_level);
	if (level == nullptr) {
		return {};
	}
	py_Ref locals = scope_at(level->locals_slot);
	return locals != nullptr ? mapping_to_scope(locals, "locals", p_max_subitems) : Dictionary();
}

Dictionary PythonDebugger::get_stack_level_members(int32_t p_level, int32_t p_max_subitems) {
	const StackLevel *level = level_at(p_level);
	if (level == nullptr) {
		return {};
	}
	PythonScriptInstance *instance = instance_from_locals(scope_at(level->locals_slot));
	if (instance == nullptr) {
		return {};
	}
	// The instance's own attributes. RemoteDebugger prepends `self` itself,
	// from _debug_get_stack_level_instance, so it is deliberately not repeated.
	py_StackRef p0 = py_peek(0);
	py_Ref members = py_pushtmp();
	if (!py_smarteval("_0.__dict__", nullptr, &instance->py)) {
		// Captured before pushtmp(): a failing smarteval leaves its own temporaries
		// on the stack too, so the unwind point must predate the push.
		py_clearexc(p0);
		return {};
	}
	py_assign(members, py_retval());
	Dictionary out = mapping_to_scope(members, "members", p_max_subitems);
	py_pop();
	return out;
}

void *PythonDebugger::get_stack_level_instance(int32_t p_level) {
	// Not decoration. RemoteDebugger's `evaluate` command bails out of the whole
	// debug loop when this is null -- which silently resumes the game the first
	// time anyone types in a watch expression.
	const StackLevel *level = level_at(p_level);
	if (level == nullptr) {
		return nullptr;
	}
	PythonScriptInstance *instance = instance_from_locals(scope_at(level->locals_slot));
	return instance != nullptr ? instance->engine_instance : nullptr;
}

Dictionary PythonDebugger::get_globals(int32_t p_max_subitems) {
	// Godot asks for globals without a level, so this answers for the innermost
	// frame: module globals, minus what `import` put there. GDScript answers
	// with engine-wide constants and autoloads; for Python the enclosing module
	// is closer to what the name means, but only once the imports are filtered
	// out -- see is_import_noise().
	const StackLevel *level = level_at(0);
	if (level == nullptr) {
		return {};
	}
	py_Ref globals = scope_at(level->locals_slot + 1);
	return globals != nullptr ? mapping_to_scope(globals, "globals", p_max_subitems, true) : Dictionary();
}

TypedArray<Dictionary> PythonDebugger::get_current_stack_info() {
	// Unlike everything above, this one is asked outside a break too: Godot
	// calls it while building an error report, to attach the script stack to an
	// engine error raised from script.
	TypedArray<Dictionary> out;
	if (state == nullptr) {
		return out;
	}
	if (state->broken) {
		for (const StackLevel &level : state->levels) {
			Dictionary entry;
			entry["file"] = level.file;
			entry["line"] = level.line;
			entry["func"] = level.func;
			out.push_back(entry);
		}
		return out;
	}
	for (py_Frame *frame = py_inspect_currentframe(); frame != nullptr; frame = frame_parent(frame)) {
		int line = 0;
		const char *source = py_Frame_sourceloc(frame, &line);
		Dictionary entry;
		entry["file"] = source_to_res_path(source);
		entry["line"] = line;
		entry["func"] = frame_funcname(frame);
		out.push_back(entry);
	}
	return out;
}

} //namespace pkpy
