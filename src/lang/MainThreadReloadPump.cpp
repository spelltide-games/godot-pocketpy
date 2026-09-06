#include "MainThreadReloadPump.hpp"

#include <godot_cpp/core/error_macros.hpp>

#include "../support/DebugPrint.hpp"
#include "PythonScript.hpp"

#include <chrono>
#include <thread>

namespace pkpy {

struct MainThreadReloadPump::Request {
	Ref<PythonScript> script; // keeps the script alive across the hand-off
	Error result = ERR_UNAVAILABLE;
	bool done = false;
};

MainThreadReloadPump::~MainThreadReloadPump() {
	shutdown();
}

Error MainThreadReloadPump::request(PythonScript *p_script) {
	ERR_FAIL_NULL_V(p_script, ERR_INVALID_PARAMETER);

	// Before the wait, so a stalled hand-off names the script it is stuck on.
	// Outside the lock: fflush() must not block drain().
	debug_print(
			"=> PythonScript.reload() deferred to main thread: %s, tid=%lld\n",
			p_script->get_path().utf8().get_data(),
			(long long)std::hash<std::thread::id>()(std::this_thread::get_id()));

	// On the stack: request() cannot return while `req` is still reachable from the
	// pump. It leaves `queue` only into drain()'s batch (which sets `done`) or via
	// shutdown()'s clear (which clears `open`), and the wait below covers both.
	// Adding a timeout or a cancel would break this and need shared ownership back.
	Request req;
	req.script = Ref<PythonScript>(p_script);

	std::unique_lock<std::mutex> lock(mutex);
	if (!open) {
		return ERR_UNAVAILABLE;
	}
	queue.push_back(&req);
	cv.notify_all(); // may wake a main thread lingering in its drain window

	++waiters; // shutdown() waits for this to reach 0 before the pump is destroyed
	cv.wait(lock, [this, &req] { return req.done || !open; });
	// drain() publishes `result` and `done` in one critical section, so a request
	// it never ran still holds the ERR_UNAVAILABLE default.
	const Error result = req.result;
	if (--waiters == 0) {
		quiesced_cv.notify_all();
	}
	return result;
}

void MainThreadReloadPump::drain() {
	std::unique_lock<std::mutex> lock(mutex);
	// reload_impl() can nest a load that pumps the main loop (a progress dialog
	// calls Main::iteration()), re-entering us.
	if (draining || !open || queue.is_empty()) {
		return;
	}
	draining = true;

	const auto deadline = std::chrono::steady_clock::now() + DRAIN_BUDGET;
	while (open && !queue.is_empty()) {
		// CowData's move ctor nulls the source, so `queue` is reliably empty here --
		// unlike std::vector, whose moved-from state is unspecified.
		Vector<Request *> batch = std::move(queue);

		lock.unlock(); // reload_impl() is slow and re-enters the resource loader
		for (Request *req : batch) {
			req->result = req->script->reload_impl();
		}
		lock.lock();

		for (Request *req : batch) {
			req->done = true;
		}
		cv.notify_all();

		if (std::chrono::steady_clock::now() >= deadline) {
			break;
		}
		cv.wait_until(lock, deadline, [this] { return !queue.is_empty() || !open; });
	}

	draining = false;
}

void MainThreadReloadPump::shutdown() {
	std::unique_lock<std::mutex> lock(mutex);
	open = false;
	queue.clear();
	cv.notify_all();
	// A woken waiter still holds `mutex` until its own lock unwinds, so returning
	// now would let ~PythonScriptLanguage destroy a locked mutex. `open == false`
	// makes every waiter's predicate true, so this never depends on _frame().
	quiesced_cv.wait(lock, [this] { return waiters == 0; });
}

} //namespace pkpy
