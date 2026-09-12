#pragma once

#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/resource_format_saver.hpp>

using namespace godot;

namespace pkpy {

class PythonModuleSourceLoader : public ResourceFormatLoader {
	GDCLASS(PythonModuleSourceLoader, ResourceFormatLoader);

protected:
	static void _bind_methods() {}

public:
	PackedStringArray _get_recognized_extensions() const override;
	bool _recognize_path(const String &p_path, const StringName &p_type) const override;
	bool _handles_type(const StringName &p_type) const override;
	// Explicit loads work, but the filesystem scanner must not classify modules
	// as project resources, even if .gdignore is accidentally removed.
	String _get_resource_type(const String &p_path) const override { return {}; }
	bool _exists(const String &p_path) const override;
	Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const override;
};

class PythonModuleSourceSaver : public ResourceFormatSaver {
	GDCLASS(PythonModuleSourceSaver, ResourceFormatSaver);

protected:
	static void _bind_methods() {}

public:
	Error _save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) override;
	bool _recognize(const Ref<Resource> &p_resource) const override;
	PackedStringArray _get_recognized_extensions(const Ref<Resource> &p_resource) const override;
};

void register_module_source_io();
void unregister_module_source_io();

} // namespace pkpy
