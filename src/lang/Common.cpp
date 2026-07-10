#include "Common.hpp"
#include <assert.h>

namespace pkpy {

static PythonContext *_pyctx;

PythonContext *pyctx() {
	if (!_pyctx) {
		_pyctx = new PythonContext();
	}
	return _pyctx;
}

void dispose_contexts() {
	if (_pyctx) {
		delete _pyctx;
		_pyctx = nullptr;
	}
}

static_assert(sizeof(Variant) == sizeof(py_TValue));

// static constexpr bool needs_deinit[Variant::VARIANT_MAX] = {
// 	false, //NIL,
// 	false, //BOOL,
// 	false, //INT,
// 	false, //FLOAT,
// 	true, //STRING,
// 	false, //VECTOR2,
// 	false, //VECTOR2I,
// 	false, //RECT2,
// 	false, //RECT2I,
// 	false, //VECTOR3,
// 	false, //VECTOR3I,
// 	true, //TRANSFORM2D,
// 	false, //VECTOR4,
// 	false, //VECTOR4I,
// 	false, //PLANE,
// 	false, //QUATERNION,
// 	true, //AABB,
// 	true, //BASIS,
// 	true, //TRANSFORM,
// 	true, //PROJECTION,

// 	// misc types
// 	false, //COLOR,
// 	true, //STRING_NAME,
// 	true, //NODE_PATH,
// 	false, //RID,
// 	true, //OBJECT,
// 	true, //CALLABLE,
// 	true, //SIGNAL,
// 	true, //DICTIONARY,
// 	true, //ARRAY,

// 	// typed arrays
// 	true, //PACKED_BYTE_ARRAY,
// 	true, //PACKED_INT32_ARRAY,
// 	true, //PACKED_INT64_ARRAY,
// 	true, //PACKED_FLOAT32_ARRAY,
// 	true, //PACKED_FLOAT64_ARRAY,
// 	true, //PACKED_STRING_ARRAY,
// 	true, //PACKED_VECTOR2_ARRAY,
// 	true, //PACKED_VECTOR3_ARRAY,
// 	true, //PACKED_COLOR_ARRAY,
// 	true, //PACKED_VECTOR4_ARRAY,
// };

void py_newvariant(py_OutRef out, const Variant *val) {
	switch (val->get_type()) {
		case Variant::NIL:
			py_newnone(out);
			return;
		case Variant::BOOL:
			py_newbool(out, val->operator bool());
			return;
		case Variant::INT:
			py_newint(out, val->operator int64_t());
			return;
		case Variant::FLOAT:
			py_newfloat(out, val->operator double());
			return;
		case Variant::STRING: {
			auto s = val->operator String();
			py_newstring(out, s);
			return;
		}
		case Variant::VECTOR2: {
			Vector2 v = val->operator Vector2();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector2));
			out->extra = Variant::VECTOR2;
			return;
		}
		case Variant::VECTOR2I: {
			Vector2i v = val->operator Vector2i();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector2i));
			out->extra = Variant::VECTOR2I;
			return;
		}
		case Variant::RECT2: {
			Rect2 r = val->operator Rect2();
			py_newtrivial(out, pyctx()->tp_Variant, &r, sizeof(Rect2));
			out->extra = Variant::RECT2;
			return;
		}
		case Variant::RECT2I: {
			Rect2i r = val->operator Rect2i();
			py_newtrivial(out, pyctx()->tp_Variant, &r, sizeof(Rect2i));
			out->extra = Variant::RECT2I;
			return;
		}
		case Variant::VECTOR3: {
			Vector3 v = val->operator Vector3();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector3));
			out->extra = Variant::VECTOR3;
			return;
		}
		case Variant::VECTOR3I: {
			Vector3i v = val->operator Vector3i();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector3i));
			out->extra = Variant::VECTOR3I;
			return;
		}
		case Variant::VECTOR4: {
			Vector4 v = val->operator Vector4();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector4));
			out->extra = Variant::VECTOR4;
			return;
		}
		case Variant::VECTOR4I: {
			Vector4i v = val->operator Vector4i();
			py_newtrivial(out, pyctx()->tp_Variant, &v, sizeof(Vector4i));
			out->extra = Variant::VECTOR4I;
			return;
		}
		case Variant::PLANE: {
			Plane p = val->operator Plane();
			py_newtrivial(out, pyctx()->tp_Variant, &p, sizeof(Plane));
			out->extra = Variant::PLANE;
			return;
		}
		case Variant::QUATERNION: {
			Quaternion q = val->operator Quaternion();
			py_newtrivial(out, pyctx()->tp_Variant, &q, sizeof(Quaternion));
			out->extra = Variant::QUATERNION;
			return;
		}
		case Variant::COLOR: {
			Color c = val->operator Color();
			py_newtrivial(out, pyctx()->tp_Variant, &c, sizeof(Color));
			out->extra = Variant::COLOR;
			return;
		}
		case Variant::RID: {
			RID r = val->operator RID();
			py_newtrivial(out, pyctx()->tp_Variant, &r, sizeof(RID));
			out->extra = Variant::RID;
			return;
		}
		default: {
			void *ud = py_newobject(out, pyctx()->tp_Variant, 0, sizeof(Variant));
			out->extra = Variant::OBJECT; // not only real OBJECT but also PackedStringArray like
			Variant *v = new (ud) Variant(*val);
			break;
		}
	}
}

Variant to_variant_exact(py_Ref val) {
	bool is_convertible = py_istype(val, pyctx()->tp_Variant);
	ERR_FAIL_COND_V(!is_convertible, Variant());
	switch (val->extra) {
		case Variant::VECTOR2: {
			return *(Vector2 *)py_totrivial(val);
		}
		case Variant::VECTOR2I: {
			return *(Vector2i *)py_totrivial(val);
		}
		case Variant::RECT2: {
			return *(Rect2 *)py_totrivial(val);
		}
		case Variant::RECT2I: {
			return *(Rect2i *)py_totrivial(val);
		}
		case Variant::VECTOR3: {
			return *(Vector3 *)py_totrivial(val);
		}
		case Variant::VECTOR3I: {
			return *(Vector3i *)py_totrivial(val);
		}
		case Variant::VECTOR4: {
			return *(Vector4 *)py_totrivial(val);
		}
		case Variant::VECTOR4I: {
			return *(Vector4i *)py_totrivial(val);
		}
		case Variant::PLANE: {
			return *(Plane *)py_totrivial(val);
		}
		case Variant::QUATERNION: {
			return *(Quaternion *)py_totrivial(val);
		}
		case Variant::COLOR: {
			return *(Color *)py_totrivial(val);
		}
		case Variant::RID: {
			return *(RID *)py_totrivial(val);
		}
		case Variant::OBJECT: {
			assert(val->is_ptr);
			void *ud = py_touserdata(val);
			return *static_cast<Variant *>(ud);
		}
		default: {
			int extra = val->extra;
			String msg = "py_tovariant: unknown Variant type: " + String::num_int64(extra);
			ERR_PRINT(msg);
			return Variant();
		}
	}
}

Variant py_tovariant(py_Ref val) {
	switch (py_typeof(val)) {
		case tp_NoneType:
			return Variant();
		case tp_bool:
			return py_tobool(val);
		case tp_int:
			return py_toint(val);
		case tp_float:
			return py_tofloat(val);
		case tp_str: {
			c11_sv sv = py_tosv(val);
			return String::utf8(sv.data, sv.size);
		}
		case tp_vec2: {
			c11_vec2 v = py_tovec2(val);
			return Vector2(v.x, v.y);
		}
		case tp_vec2i: {
			c11_vec2i v = py_tovec2i(val);
			return Vector2i(v.x, v.y);
		}
		case tp_vec3: {
			c11_vec3 v = py_tovec3(val);
			return Vector3(v.x, v.y, v.z);
		}
		case tp_vec3i: {
			c11_vec3i v = py_tovec3i(val);
			return Vector3i(v.x, v.y, v.z);
		}
		case tp_vec4i: {
			c11_vec4i v = py_tovec4i(val);
			return Vector4i(v.x, v.y, v.z, v.w);
		}
		case tp_color32: {
			c11_color32 c = py_tocolor32(val);
			float r = c.r / 255.0f;
			float g = c.g / 255.0f;
			float b = c.b / 255.0f;
			float a = c.a / 255.0f;
			return Color(r, g, b, a);
		}
		default: {
			return to_variant_exact(val);
		}
	}
}

void py_newstring(py_OutRef out, String val) {
	auto s = val.utf8();
	c11_sv sv;
	sv.data = s.get_data();
	sv.size = (int)s.length();
	py_newstrv(out, sv);
}

void log_python_error_and_clearexc(py_StackRef p0) {
	char *msg = py_formatexc();
	print_error(String(msg));
	PK_FREE(msg);
	py_clearexc(p0);
}

} // namespace pkpy