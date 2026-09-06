#pragma once

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/templates/vector.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>

using namespace godot;

namespace pkpy {

class PythonScript;

// Runs PythonScript::reload() on the main thread on behalf of a worker thread.
// Owned by PythonScriptLanguage, driven from its _frame().
//
// The pocketpy VM is a single instance only ever entered from the main thread, so
// instead of locking it (GDScript's approach) we hand the work over and block.
// Blocking is required, not just convenient: once _load() returns,
// ResourceLoader::_run_load_task() writes to the same script from the worker
// (set_path / set_edited / set_last_modified_time), racing reload_impl()'s
// get_path(). Returning early would also publish an uncompiled script into the
// ResourceCache, which CACHE_MODE_REUSE would hand out forever without retrying.
//
// Editor only. At runtime the main thread is often blocked waiting on the very
// thread that would wait on it (Thread.wait_to_finish, load_threaded_get,
// wait_for_task_completion), so an off-thread reload fails fast instead.
//
// The wait is unbounded. That is safe because the only main-thread join on the
// requesting thread -- EditorHelp::regenerate_script_doc_cache() waiting on
// loader_thread -- is reachable once per session. Re-check when upgrading Godot.
class MainThreadReloadPump {
public:
	MainThreadReloadPump() = default;
	~MainThreadReloadPump();

	MainThreadReloadPump(const MainThreadReloadPump &) = delete;
	MainThreadReloadPump &operator=(const MainThreadReloadPump &) = delete;

	// Worker threads only. Blocks until the main thread has run the reload.
	Error request(PythonScript *p_script);

	// Main thread only; driven by PythonScriptLanguage::_frame().
	void drain();

	// Refuses new requests and waits for every waiter to leave -- this object's
	// mutex dies with the language singleton right after. Idempotent.
	void shutdown();

private:
	// Defined in the .cpp. Each Request lives on its requester's stack and the
	// queue only borrows it; see request().
	struct Request;

	std::mutex mutex;
	std::condition_variable cv;
	std::condition_variable quiesced_cv; // only shutdown() waits on this
	Vector<Request *> queue;
	int waiters = 0;
	bool open = true;
	bool draining = false;

	// EditorHelp submits one script at a time, so draining once per frame would
	// cost a frame per script. Linger this long for the next request instead.
	static constexpr std::chrono::milliseconds DRAIN_BUDGET{ 8 };
};

} //namespace pkpy
