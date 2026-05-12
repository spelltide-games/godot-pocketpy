#include "CubePhysics.hpp"
#include "LevelDB.hpp"
#include <godot_cpp/core/defs.hpp>

namespace pkpy {

using namespace godot;

void setup_sbx_godot_classes() {
	ClassDB::register_class<cube_physics::CubePhysicsSpace>();
	ClassDB::register_class<cube_physics::CubePhysicsBody>();
}

void setup_sbx_python_modules() {
	setup_leveldb_module();
}

} // namespace pkpy