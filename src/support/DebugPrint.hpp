#pragma once

namespace pkpy {

// Terminal-only debug output, flushed on every call.
//
// Not print_line(): that goes through OS::print(), where the editor installs
// EditorLog, so the message would also land in the Output panel. Plain stdio
// bypasses the engine's logger.
//
// Same mechanism Godot uses for the terminal -- StdLogger::logv() in
// core/io/logger.cpp is vprintf() + fflush(stdout) -- except it gates the flush on
// application/run/flush_stdout_on_print. We always flush: the point of a trace is
// that the last line before a hang made it out. stdout rather than stderr, so
// these stay ordered with the engine's own print_line().
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void debug_print(const char *p_format, ...);

} //namespace pkpy
