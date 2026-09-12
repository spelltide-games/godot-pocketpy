#include "PythonModuleSource.hpp"
#include "PythonScriptLanguage.hpp"

namespace pkpy {

ScriptLanguage *PythonModuleSource::_get_language() const {
	return PythonScriptLanguage::get_singleton();
}

} // namespace pkpy
