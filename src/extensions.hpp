#pragma once

#ifdef GODOT_POCKETPY_WITH_SBX
#include "sbx.hpp"
#endif

namespace pkpy::extensions {

inline void setup_godot_classes() {
#ifdef GODOT_POCKETPY_WITH_SBX
	sbx::setup_sbx_godot_classes();
#endif
}

inline void setup_python_modules() {
#ifdef GODOT_POCKETPY_WITH_SBX
	sbx::setup_sbx_python_modules();
#endif
}

} // namespace pkpy::extensions
