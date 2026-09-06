#include "DebugPrint.hpp"

#include <cstdarg>
#include <cstdio>

namespace pkpy {

void debug_print(const char *p_format, ...) {
	va_list args;
	va_start(args, p_format);
	vfprintf(stdout, p_format, args);
	va_end(args);
	fflush(stdout);
}

} //namespace pkpy
