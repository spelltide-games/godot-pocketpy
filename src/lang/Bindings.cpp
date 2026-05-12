#include "Bindings.hpp"
#include "PythonScriptInstance.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/callable.hpp>

#include "sbx.hpp"

namespace pkpy {

void setup_bindings_generated();

static bool call_next_for_coroutine(Object* owner, IdGenerator::T id);

static void call_next_for_coroutine_no_error(Object* owner, IdGenerator::T id) {
	py_Ref p0 = py_peek(0);
	bool ok = call_next_for_coroutine(owner, id);
	if(!ok) log_python_error_and_clearexc(p0);
}

static bool call_next_for_coroutine(Object* owner, IdGenerator::T id) {
	std::thread::id current_thread_id = std::this_thread::get_id();
	if (current_thread_id != pyctx()->main_thread_id) {
		ERR_PRINT("coroutine can only be resumed in the main thread");
		std::abort();
	}

	PythonScriptInstance *instance = PythonScriptInstance::attached_to_object(owner);
	if(instance == NULL) {
		py_newint(py_retval(), id);
		return true;
	}
	py_ItemRef gen = instance->coroutines.getptr(id);
	if(gen == NULL) {
		py_newint(py_retval(), id);
		return true;
	}

	int res = py_next(gen);
	if(res == 1) {
		if(py_retval()->type != pyctx()->tp_Variant) {
			return TypeError("coroutine yielded value must be 'godot.Signal', got '%t'", py_typeof(py_retval()));
		}
		Variant v = to_variant_exact(py_retval());
		if(v.get_type() != Variant::SIGNAL) {
			CharString type_name = Variant::get_type_name(v.get_type()).utf8();
			return TypeError("coroutine yielded value must be 'godot.Signal', got '%s'", type_name.get_data());
		}
		Signal signal = v;
		Callable callable = callable_mp_static(call_next_for_coroutine_no_error);
		signal.connect(callable.bind(owner, id), Object::CONNECT_ONE_SHOT | Object::CONNECT_DEFERRED);
		py_newint(py_retval(), id);
		return true;
	} else if (res == -1) {
		instance->coroutines.erase(id);
		return false;	// error
	} else {
		// generator finished
		instance->coroutines.erase(id);
		py_newint(py_retval(), id);
		return true;
	}
}

static void setup_awaitables() {
	py_bindmethod(pyctx()->tp_Script, "__new__", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(1);
		py_Type cls = py_totype(&argv[0]);
		PythonScript *script = PythonScript::runtime_type_to_script.get(cls);
		StringName node_cls = script->meta.extends;
		if(!ClassDB::can_instantiate(node_cls)) {
			py_Name node_cls_py = godot_name_to_python(node_cls);
			return TypeError("cannot instantiate script that extends '%n'", node_cls_py);
		}
		Variant v = ClassDB::instantiate(node_cls);
		Node* node = Object::cast_to<Node>(v);
		if(node == NULL) {
			return RuntimeError("Object::cast_to<Node> failed");
		}

		Ref<Script> arg(script);
		node->set_script(arg);
		py_newvariant(py_retval(), &v);
		return true;
	});
	
	py_bindmethod(pyctx()->tp_Script, "start_coroutine", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(2);
		PY_CHECK_ARG_TYPE(1, tp_generator);
		PythonScriptInstance *instance = (PythonScriptInstance *)py_touserdata(argv);
		IdGenerator::T id = instance->coroutine_id_gen.next();
		instance->coroutines.insert(id, argv[1]);
		return call_next_for_coroutine(instance->owner, id);
	});

	py_bindmethod(pyctx()->tp_Script, "stop_coroutine", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(2);
		PY_CHECK_ARG_TYPE(1, tp_int);
		PythonScriptInstance *instance = (PythonScriptInstance *)py_touserdata(argv);
		IdGenerator::T id = (IdGenerator::T)py_toint(&argv[1]);
		bool removed = instance->coroutines.erase(id);
		py_newbool(py_retval(), removed);
		return true;
	});

	py_bindmethod(pyctx()->tp_Script, "stop_all_coroutines", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(1);
		PythonScriptInstance *instance = (PythonScriptInstance *)py_touserdata(argv);
		instance->coroutines.clear();
		py_newnone(py_retval());
		return true;
	});
}

static void setup_exports() {
	// export
	pyctx()->tp_DefineStatement = py_newtype("_DefineStatement", tp_object, pyctx()->godot, [](void *ud) {
		DefineStatement *self = (DefineStatement *)ud;
		self->~DefineStatement();
	});

	py_bind(pyctx()->godot, "export(cls, default=None)", [](int argc, py_Ref argv) -> bool {
		auto ctx = &pyctx()->reloading_context;
		StringName type_name;

		if (py_istype(&argv[0], tp_type)) {
			py_Type type = py_totype(&argv[0]);
			switch (type) {
				case tp_int:
					type_name = "int";
					break;
				case tp_float:
					type_name = "float";
					break;
				case tp_bool:
					type_name = "bool";
					break;
				case tp_str:
					type_name = "String";
					break;
				default:
					return TypeError("cannot export type '%t'", type);
			}
		} else if (py_istype(&argv[0], pyctx()->tp_GDNativeClass)) {
			PY_CHECK_ARG_TYPE(0, pyctx()->tp_GDNativeClass);
			type_name = to_GDNativeClass(&argv[0]);
		} else {
			return TypeError("expected 'type' or 'GDNativeClass', got '%t'", py_typeof(&argv[0]));
		}

		ExportStatement *ud = (ExportStatement *)py_newobject(py_retval(), pyctx()->tp_DefineStatement, 0, sizeof(ExportStatement));
		new (ud) ExportStatement(ctx->next_index());
		ud->template_ = "@export var ?: " + type_name;
		ud->default_value = py_tovariant(&argv[1]);
		return true;
	});

	py_bind(pyctx()->godot, "export_range(min, max, step, *extra_hints, default=None)", [](int argc, py_Ref argv) -> bool {
		auto ctx = &pyctx()->reloading_context;
		ExportStatement *ud = (ExportStatement *)py_newobject(py_retval(), pyctx()->tp_DefineStatement, 0, sizeof(ExportStatement));
		new (ud) ExportStatement(ctx->next_index());
		Variant min = py_tovariant(&argv[0]);
		Variant max = py_tovariant(&argv[1]);
		Variant step = py_tovariant(&argv[2]);
		bool any_is_float = min.get_type() == Variant::FLOAT || max.get_type() == Variant::FLOAT || step.get_type() == Variant::FLOAT;
		ud->template_ = String("@export_range({0}, {1}, {2}) var ?").format(Array::make(min, max, step));
		ud->default_value = py_tovariant(&argv[4]);
		if (any_is_float && ud->default_value.get_type() == Variant::INT) {
			ud->default_value = (double)ud->default_value;
		}
		return true;
	});

	py_bindfunc(pyctx()->godot, "signal", [](int argc, py_Ref argv) -> bool {
		auto ctx = &pyctx()->reloading_context;
		SignalStatement *ud = (SignalStatement *)py_newobject(py_retval(), pyctx()->tp_DefineStatement, 0, sizeof(SignalStatement));
		new (ud) SignalStatement(ctx->next_index());
		for (int i = 0; i < argc; i++) {
			PY_CHECK_ARG_TYPE(i, tp_str);
			const char *arg_cstr = py_tostr(py_arg(i));
			ud->arguments.append(String::utf8(arg_cstr));
		}
		return true;
	});
}

void setup_python_bindings() {
	pyctx()->main_thread_id = std::this_thread::get_id();
	// pyctx()->lock.clear();
	pyctx()->names.__init__ = py_name("__init__");
	pyctx()->names.__name__ = py_name("__name__");
	pyctx()->names.__call__ = py_name("__call__");
	pyctx()->names.script = py_name("script");
	pyctx()->names.owner = py_name("owner");

	py_callbacks()->gc_mark = PythonScriptInstance::gc_mark_instances;
	py_callbacks()->print = [](const char *msg) {
		size_t length = strlen(msg);
		if (msg[length - 1] == '\n') {
			length--;
		}
		String s = String::utf8(msg, length);
		print_line(s);
	};
	py_callbacks()->flush = []() {
		// No-op, Godot's print is already flushed.
	};
	py_callbacks()->importfile = [](const char *path_cstr, int* size) -> char * {
		String path = String::utf8(path_cstr);
		path = "res://site-packages/" + path;
		bool exists = FileAccess::file_exists(path);
		if (!exists) {
			return nullptr;
		}
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::ModeFlags::READ);
		if (!file->is_open()) {
			String msg = "cannot open file '" + path + "' when importing '" + path_cstr + "' module";
			ERR_PRINT(msg);
			return nullptr;
		}
		CharString content = file->get_as_text().utf8();
		char *dup = (char *)PK_MALLOC(content.length() + 1);
		memcpy(dup, content.get_data(), content.length());
		dup[content.length()] = '\0';
		*size = (int)content.length();
		return dup;
	};

	py_GlobalRef godot = pyctx()->godot = py_newmodule("godot");
	pyctx()->godot_classes = py_newmodule("godot.classes");
	pyctx()->godot_scripts = py_newmodule("godot.scripts");

	py_bindfunc(py_getmodule("builtins"), "isinstance", godot_isinstance);

	// load()
	py_bindfunc(godot, "load", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(1);
		PY_CHECK_ARG_TYPE(0, tp_str);
		String path = String::utf8(py_tostr(&argv[0]));
		Ref<Resource> res = ResourceLoader::get_singleton()->load(path);
		if (!res.is_valid()) {
			return RuntimeError("failed to load resource '%s'", path.utf8().get_data());
		}
		Variant var(res);
		py_newvariant(py_retval(), &var);
		return true;
	});

	// cast()
	py_bindfunc(godot, "cast", [](int argc, py_Ref argv) -> bool {
		PY_CHECK_ARGC(2);
		if(!godot_isinstance_one(py_arg(0), py_arg(1))) {
			return TypeError("cast(): !godot_isinstance_one");
		}
		py_assign(py_retval(), py_arg(0));
		return true;
	});

	// Script
	pyctx()->tp_Script = py_newtype("PythonScriptInstance", tp_object, godot, [](void *ud) {
		auto *self = (PythonScriptInstance *)ud;
		self->~PythonScriptInstance();
	});

	// GDNativeClass
	pyctx()->tp_GDNativeClass = py_newtype("GDNativeClass", tp_object, godot, NULL);

	py_tphookattributes(pyctx()->tp_GDNativeClass, GDNativeClass_getattribute, NULL, NULL, GDNativeClass_getunboundmethod);

	// Extends
	py_bindfunc(godot, "Extends", [](int argc, py_Ref argv) -> bool {
		auto ctx = &pyctx()->reloading_context;
		PY_CHECK_ARGC(1);
		PY_CHECK_ARG_TYPE(0, pyctx()->tp_GDNativeClass);
		StringName nativeClass = to_GDNativeClass(argv);
		ctx->extends = nativeClass;
		py_assign(py_retval(), py_tpobject(pyctx()->tp_Script));
		return true;
	});

	setup_exports();

	// Variant
	py_Type type = pyctx()->tp_Variant = py_newtype("Variant", tp_object, pyctx()->godot, [](void *ud) {
		Variant *v = static_cast<Variant *>(ud);
		v->~Variant();
	});

	py_tpsetfinal(type);
	py_tphookattributes(type, Variant_getattribute, Variant_setattribute, NULL, Variant_getunboundmethod);

	py_bindmethod(type, "__call__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		if (self.get_type() != Variant::CALLABLE) {
			return TypeError("Variant type is not Variant::CALLABLE");
		}
		Callable callable(self);
		Array godot_args;
		for (int i = 1; i < argc; i++) {
			godot_args.push_back(py_tovariant(&argv[i]));
		}
		Variant res = callable.callv(godot_args);
		py_newvariant(py_retval(), &res);
		return true;
	});

	py_bindmethod(type, "__getitem__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		Variant key = py_tovariant(&argv[1]);
		bool r_valid;
		Variant value = self.get_keyed(key, r_valid);
		if (r_valid) {
			py_newvariant(py_retval(), &value);
			return true;
		}
		return RuntimeError("!r_valid");
	});

	py_bindmethod(type, "__setitem__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		Variant key = py_tovariant(&argv[1]);
		Variant value = py_tovariant(&argv[2]);
		bool r_valid;
		self.set_keyed(key, value, r_valid);
		if (r_valid) {
			py_newnone(py_retval());
			return true;
		}
		return RuntimeError("!r_valid");
	});

	py_bindmethod(type, "__bool__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		bool res = self.booleanize();
		py_newbool(py_retval(), res);
		return true;
	});

	py_bindmethod(type, "__hash__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		py_newint(py_retval(), self.hash());
		return true;
	});

	py_bindmethod(type, "__repr__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		String type_name = Variant::get_type_name(self.get_type());
		String r = "<godot.Variant " + type_name + ">";
		py_newstring(py_retval(), r);
		return true;
	});

	py_bindmethod(type, "__str__", [](int argc, py_Ref argv) -> bool {
		Variant self = to_variant_exact(&argv[0]);
		py_newstring(py_retval(), self.stringify());
		return true;
	});

#define DEF_UNARY_OP(__name, __op)                                     \
	py_bindmethod(type, __name, [](int argc, py_Ref argv) -> bool {    \
		PY_CHECK_ARGC(1);                                              \
		Variant self = to_variant_exact(&argv[0]);                     \
		Variant other;                                                 \
		Variant r_ret;                                                 \
		bool r_valid;                                                  \
		Variant::evaluate(Variant::__op, self, other, r_ret, r_valid); \
		if (r_valid) {                                                 \
			py_newvariant(py_retval(), &r_ret);                        \
			return true;                                               \
		}                                                              \
		return RuntimeError("!r_valid");                               \
	});

#define DEF_BINARY_OP(__name, __op)                                    \
	py_bindmethod(type, __name, [](int argc, py_Ref argv) -> bool {    \
		PY_CHECK_ARGC(2);                                              \
		Variant self = to_variant_exact(&argv[0]);                     \
		Variant other = py_tovariant(&argv[1]);                        \
		Variant r_ret;                                                 \
		bool r_valid;                                                  \
		Variant::evaluate(Variant::__op, self, other, r_ret, r_valid); \
		if (r_valid) {                                                 \
			py_newvariant(py_retval(), &r_ret);                        \
			return true;                                               \
		}                                                              \
		return RuntimeError("!r_valid");                               \
	});

	DEF_BINARY_OP("__eq__", OP_EQUAL)
	DEF_BINARY_OP("__ne__", OP_NOT_EQUAL)
	DEF_BINARY_OP("__lt__", OP_LESS)
	DEF_BINARY_OP("__le__", OP_LESS_EQUAL)
	DEF_BINARY_OP("__gt__", OP_GREATER)
	DEF_BINARY_OP("__ge__", OP_GREATER_EQUAL)

	DEF_BINARY_OP("__add__", OP_ADD)
	DEF_BINARY_OP("__sub__", OP_SUBTRACT)
	DEF_BINARY_OP("__mul__", OP_MULTIPLY)
	DEF_BINARY_OP("__truediv__", OP_DIVIDE)
	DEF_BINARY_OP("__mod__", OP_MODULE)
	DEF_BINARY_OP("__pow__", OP_POWER)
	DEF_BINARY_OP("__lshift__", OP_SHIFT_LEFT)
	DEF_BINARY_OP("__rshift__", OP_SHIFT_RIGHT)
	DEF_BINARY_OP("__and__", OP_BIT_AND)
	DEF_BINARY_OP("__or__", OP_BIT_OR)
	DEF_BINARY_OP("__xor__", OP_BIT_XOR)

	DEF_BINARY_OP("__contains__", OP_IN)
#undef DEF_BINARY_OP

	DEF_UNARY_OP("__neg__", OP_NEGATE)
	// DEF_UNARY_OP("__pos__", OP_POSITIVE)
	DEF_UNARY_OP("__invert__", OP_BIT_NEGATE)
#undef DEF_UNARY_OP

	setup_bindings_generated();

	setup_awaitables();

	setup_sbx_python_modules();

	printf("==> setup_python_bindings() done!\n");
}

} //namespace pkpy