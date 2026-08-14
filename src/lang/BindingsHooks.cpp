#include "Bindings.hpp"
#include "PythonScriptInstance.hpp"

namespace pkpy {

static void new_nativefunc_with_name(py_OutRef out, py_CFunction func, py_Name name) {
	py_newnativefunc(out, func);
	char* p_sn = out->_chars + sizeof(py_CFunction);
	memcpy(p_sn, &name, sizeof(py_Name));
	static_assert(sizeof(py_CFunction) + sizeof(py_Name) <= sizeof(out->_chars));
}

static py_Name parse_name_from_nativefunc(py_Ref f) {
	py_Name name;
	memcpy(&name, f->_chars + sizeof(py_CFunction), sizeof(py_Name));
	return name;
}

bool Variant_getattribute(py_Ref self, py_Name name) {
	Variant v = to_variant_exact(self);
	if (v.get_type() == Variant::OBJECT) {
		Object *obj = v.operator Object *();
#ifndef NDEBUG
		if (obj == nullptr) {
			return RuntimeError("Variant_getattribute(): Variant of type 'Object' is null");
		}
#endif
		if (name == pyctx()->names.script) {
			PythonScriptInstance *inst = PythonScriptInstance::attached_to_object(obj);
			if (inst != nullptr) {
				py_assign(py_retval(), &inst->py);
			} else {
				py_newnone(py_retval());
			}
			return true;
		}
	}

	bool r_valid;
	Variant res = v.get_named(python_name_to_godot(name), r_valid);
	if (!r_valid) {
		return AttributeError(self, name);
	}
	py_newvariant(py_retval(), &res);
	return true;
}

bool Variant_setattribute(py_Ref self, py_Name name, py_Ref value) {
	if (self->extra != Variant::OBJECT) {
		String name = Variant::get_type_name((Variant::Type)self->extra);
		return TypeError("Variant of type '%s' does not support attribute assignment", name.utf8().get_data());
	}
	Variant v = to_variant_exact(self);
	bool r_valid;
	v.set_named(python_name_to_godot(name), py_tovariant(value), r_valid);
	if (!r_valid) {
		return AttributeError(self, name);
	}
	return true;
}

static bool Variant_getunboundmethod_pybind(int argc, py_Ref argv) {
	py_Ref f = py_inspect_currentfunction();
	if (f == NULL || f->type != tp_nativefunc) {
		return RuntimeError("Variant_getunboundmethod(): cannot get current function");
	}
	py_CFunction inner_func = *(py_CFunction *)f->_chars;
	if (inner_func != Variant_getunboundmethod_pybind) {
		return RuntimeError("Variant_getunboundmethod(): current function mismatch");
	}

	py_Name name = parse_name_from_nativefunc(f);

	Variant v = to_variant_exact(&argv[0]);
	StringName name_sn = python_name_to_godot(name);
	bool r_valid;
	Variant named_variant = v.get_named(name_sn, r_valid);

	if (r_valid && named_variant.get_type() == Variant::CALLABLE) {
		Callable callable = named_variant;
		Variant res;
		Variant stack_args[4];
		switch(argc - 1) {
			case 0: {
				res = callable.call();
				break;
			}
			case 1:
				stack_args[0] = py_tovariant(&argv[1]);
				res = callable.call(stack_args[0]);
				break;
			case 2:
				stack_args[0] = py_tovariant(&argv[1]);
				stack_args[1] = py_tovariant(&argv[2]);
				res = callable.call(stack_args[0], stack_args[1]);
				break;
			case 3:
				stack_args[0] = py_tovariant(&argv[1]);
				stack_args[1] = py_tovariant(&argv[2]);
				stack_args[2] = py_tovariant(&argv[3]);
				res = callable.call(stack_args[0], stack_args[1], stack_args[2]);
				break;
			case 4:
				stack_args[0] = py_tovariant(&argv[1]);
				stack_args[1] = py_tovariant(&argv[2]);
				stack_args[2] = py_tovariant(&argv[3]);
				stack_args[3] = py_tovariant(&argv[4]);
				res = callable.call(stack_args[0], stack_args[1], stack_args[2], stack_args[3]);
				break;
			default: {
				Array godot_args;
				for (int i = 1; i < argc; i++) {
					godot_args.append(py_tovariant(&argv[i]));
				}
				res = callable.callv(godot_args);
				break;
			}
		}
		py_newvariant(py_retval(), &res);
		return true;
	} else {
		return AttributeError(&argv[0], name);
	}
}

bool Variant_getunboundmethod(py_Ref self, py_Name name) {
	py_OutRef out = py_retval();
	new_nativefunc_with_name(out, Variant_getunboundmethod_pybind, name);
	return true;
}

bool GDNativeClass_getattribute(py_Ref self, py_Name name) {
	StringName clazz = to_GDNativeClass(self);
	if (name == pyctx()->names.__name__) {
		py_newstring(py_retval(), String(clazz));
		return true;
	}
	// try int enum first
	StringName sn = python_name_to_godot(name);
	bool has_int_const = ClassDB::class_has_integer_constant(clazz, sn);
	if (has_int_const) {
		int64_t int_value = ClassDB::class_get_integer_constant(clazz, sn);
		py_newint(py_retval(), int_value);
		return true;
	}
	// try class constant
	String path = String(clazz) + "." + sn;
	auto ptr = pyctx()->class_constants.getptr(path);
	if (ptr) {
		py_newvariant(py_retval(), ptr);
		return true;
	}
	return py_exception(tp_AttributeError, "GDNativeClass '%n' has no attribute '%n'",
			godot_name_to_python(clazz), name);
}

static bool GDNativeClass_getunboundmethod_pybind(int argc, py_Ref argv) {
	py_Ref f = py_inspect_currentfunction();
	if (f == NULL || f->type != tp_nativefunc) {
		return RuntimeError("GDNativeClass_getunboundmethod(): cannot get current function");
	}
	py_CFunction inner_func = *(py_CFunction *)f->_chars;
	if (inner_func != GDNativeClass_getunboundmethod_pybind) {
		return RuntimeError("GDNativeClass_getunboundmethod(): current function mismatch");
	}

	py_Name name = parse_name_from_nativefunc(f);

	GDNativeClass* p_gdn = (GDNativeClass*)py_totrivial(&argv[0]);

	if (name == pyctx()->names.__call__) {
		if (p_gdn->type == Variant::OBJECT) {
			StringName clazz = python_name_to_godot(p_gdn->name);
			PY_CHECK_ARGC(1);
			if (!ClassDB::can_instantiate(clazz)) {
				return TypeError("cannot instantiate class '%n'", p_gdn->name);
			}
			Variant res = ClassDB::instantiate(clazz);
			py_newvariant(py_retval(), &res);
		} else {
			InternalArguments args(argc - 1);
			for (int i = 1; i < argc; i++) {
				args.set(i - 1, py_tovariant(&argv[i]));
			}
			Variant r_ret;
			GDExtensionCallError r_error;
			internal::gdextension_interface_variant_construct((GDExtensionVariantType)p_gdn->type, &r_ret, args.ptr(), args.size(), &r_error);
			if (!handle_gde_call_error(r_error)) {
				return false;
			}
			py_newvariant(py_retval(), &r_ret);
		}
	} else {
		Variant r_ret;
		GDExtensionCallError r_error;
		StringName method = python_name_to_godot(name);
		if (p_gdn->type == Variant::OBJECT) {
			InternalArguments args(2 + argc - 1);
			args.set(0, python_name_to_godot(p_gdn->name));
			args.set(1, method);
			for (int i = 1; i < argc; i++) {
				args.set(i + 1, py_tovariant(&argv[i]));
			}
			ClassDBSingleton *singleton = ClassDBSingleton::get_singleton();
			static GDExtensionMethodBindPtr _gde_method_bind = internal::gdextension_interface_classdb_get_method_bind(singleton->get_class_static()._native_ptr(), StringName("class_call_static")._native_ptr(), 3344196419);
			CHECK_METHOD_BIND_RET(_gde_method_bind, (Variant()));
			internal::gdextension_interface_object_method_bind_call(_gde_method_bind, singleton->_owner, args.ptr(), args.size(), &r_ret, &r_error);
		} else {
			InternalArguments args(argc - 1);
			for (int i = 1; i < argc; i++) {
				args.set(i - 1, py_tovariant(&argv[i]));
			}
			internal::gdextension_interface_variant_call_static(
					(GDExtensionVariantType)p_gdn->type, &method, args.ptr(), args.size(), &r_ret, &r_error);
		}
		if (!handle_gde_call_error(r_error)) {
			return false;
		}
		py_newvariant(py_retval(), &r_ret);
	}

	return true;
}

bool GDNativeClass_getunboundmethod(py_Ref self, py_Name name) {
	py_OutRef out = py_retval();
	new_nativefunc_with_name(out, GDNativeClass_getunboundmethod_pybind, name);
	return true;
}

bool handle_gde_call_error(GDExtensionCallError error) {
	if (error.error == GDEXTENSION_CALL_OK) {
		return true;
	}
	switch (error.error) {
		case GDEXTENSION_CALL_ERROR_INVALID_METHOD:
			return RuntimeError("GDEXTENSION_CALL_ERROR_INVALID_METHOD");
		case GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT:
			return ValueError("GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT");
		case GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS:
			return TypeError("GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS");
		case GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS:
			return TypeError("GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS");
		case GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL:
			return RuntimeError("GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL");
		case GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST:
			return RuntimeError("GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST");
		default:
			return RuntimeError("GDExtensionCallError: %d", (int)error.error);
	}
}

bool godot_isinstance_one(py_Ref obj, py_Ref type_obj) {
	if (py_istype(type_obj, tp_type)) {
		return py_isinstance(obj, py_totype(type_obj));
	}
	py_Type t1 = py_typeof(obj);
	py_Type t2 = py_typeof(type_obj);
	if (t1 == pyctx()->tp_Variant && t2 == pyctx()->tp_GDNativeClass) {
		Variant v = to_variant_exact(obj);
		GDNativeClass *p = (GDNativeClass *)py_totrivial(type_obj);
		if (Object *v_obj = v.operator Object *()) {
			return v_obj->is_class(python_name_to_godot(p->name));
		}
		return v.get_type() == p->type;
	}

	const char *t1_name = py_tpname(t1);
	const char *t2_name = py_tpname(t2);
	String error = "godot.isinstance() takes unexpected arguments: ";
	error += t1_name;
	error += " and ";
	error += t2_name;
	WARN_PRINT(error);
	return false;
}

bool godot_isinstance(int argc, py_Ref argv) {
	PY_CHECK_ARGC(2);
	if (py_istuple(py_arg(1))) {
		int length = py_tuple_len(py_arg(1));
		for (int i = 0; i < length; i++) {
			py_Ref item = py_tuple_getitem(py_arg(1), i);
			if (godot_isinstance_one(py_arg(0), item)) {
				py_newbool(py_retval(), true);
				return true;
			}
		}
		py_newbool(py_retval(), false);
		return true;
	}

	py_newbool(py_retval(), godot_isinstance_one(py_arg(0), py_arg(1)));
	return true;
}

void register_GDNativeClass(Variant::Type type, const char *name) {
	py_TValue tmp;
	py_Name sn = py_name(name);
	GDNativeClass clazz(type, sn);
	py_newtrivial(&tmp, pyctx()->tp_GDNativeClass, &clazz, sizeof(GDNativeClass));
	py_setdict(pyctx()->godot_classes, sn, &tmp);
	if(type != Variant::OBJECT) {
		py_setdict(pyctx()->godot, sn, &tmp);
	}
}

void register_GDNativeSingleton(const char *name, Object *obj) {
	Variant v(obj);
	py_OutRef out = py_emplacedict(pyctx()->godot, py_name(name));
	py_newvariant(out, &v);
}

void register_ClassConstant(const char *class_name, const char* name, Variant value) {
	String path = String(class_name) + "." + name;
	pyctx()->class_constants[path] = value;
}

void register_GlobalConstant(const char *name, py_i64 value) {
	py_TValue tmp;
	py_newint(&tmp, value);
	py_setdict(pyctx()->godot, py_name(name), &tmp);
}

} // namespace pkpy