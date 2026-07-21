import re
from .schema_gdt import *

from . import enum
from . import converters
from .writer import Writer

# ===============================
# 入口
# ===============================

FLOAT_PATTERN = re.compile('[-]?([0-9]*[.])?[0-9]+')
INT_PATTERN = re.compile('[-]?[0-9]+')

def parse_default(arg_type, val):
    not_typed = arg_type is None
    if INT_PATTERN.fullmatch(val) and (arg_type == 'int' or not_typed or 'Enum.' in arg_type):
        return val
    elif (arg_type == 'float' or not_typed) and FLOAT_PATTERN.fullmatch(val):
        return val
    elif (arg_type == 'str' or not_typed) and len(val) >= 2 and val[0] == val[-1] == '"':
        return val
    elif (arg_type == 'bool' or not_typed) and val == 'false':
        return False
    elif (arg_type == 'bool' or not_typed) and val == 'true':
        return True
    # elif val == 'null':
    #     return None
    else:
        return f"default({val!r})"

@dataclass
class MapResult:
    c_writer: Writer
    pyi_writers: dict[str, Writer]


def fill_converters(gdt_all_in_one: GodotInOne):
    """
    使用gdt_all_in_one的信息初始化converters模块下的全局变量
    """
    converters.BUILTIN_TYPES.update(
        set(cls.name for cls in gdt_all_in_one.builtin_classes)
    )

    converters.NATIVE_TYPES.update(set(cls.name for cls in gdt_all_in_one.classes))

    for cls_data in gdt_all_in_one.classes + gdt_all_in_one.builtin_classes:
        if not cls_data.enums:
            continue
        cls_name = cls_data.name
        # enum_cls_name = f"{cls_name}Enum"
        enum_cls_name = cls_name

        for enum_data in cls_data.enums:
            for v in enum_data.values or []:
                # HTTPRequest, HTTPRequest.Result, HTTPRequestEnum, Result, RESULT_SUCCESS, 0
                # HTTPRequest, HTTPRequest.Result, HTTPRequestEnum, Result, RESULT_CHUNKED_BODY_SIZE_MISMATCH, 1
                # HTTPRequest, HTTPRequest.Result, HTTPRequestEnum, Result, ..., ...
                converters.append_records(
                    converters.CLASS_ENUM_DATA,
                    {
                        "cls_name": cls_name,
                        "orign_enum_name": cls_name + "." + enum_data.name,
                        "cls_enum_name": enum_cls_name,
                        "enum_name": enum_data.name,
                        "enum_constant_name": v.name,
                        "constant_value": v.value,
                    },
                )

    for enum in gdt_all_in_one.global_enums:
        if "." in enum.name:
            cls_name, enum_name = enum.name.split(".")
            for v in enum.values:
                # Variant.Type, Variant_Type, TYPE_NIL, 0
                # Variant.Type, Variant_Type, TYPE_BOOL, 1
                # Variant.Type, Variant_Type, ..., ...
                converters.append_records(
                    converters.GLOBAL_ENUMS_DATA,
                    {
                        "orign_enum_name": enum.name,
                        "converted_enum_name": cls_name + "_" + enum_name,
                        "enum_constant_name": v.name,
                        "constant_value": v.value,
                    },
                )

        else:
            for v in enum.values:
                # MethodFlags, MethodFlags, METHOD_FLAG_NORMAL, 1
                # MethodFlags, MethodFlags, METHOD_FLAG_EDITOR, 2
                # MethodFlags, MethodFlags, ..., ...
                converters.append_records(
                    converters.GLOBAL_ENUMS_DATA,
                    {
                        "orign_enum_name": enum.name,
                        "converted_enum_name": enum.name,
                        "enum_constant_name": v.name,
                        "constant_value": v.value,
                    },
                )

    for builtin_class in gdt_all_in_one.builtin_classes:
        cls_name = converters.convert_class_name(builtin_class.name)
        if builtin_class.operators:
            for op in builtin_class.operators:
                if converters.is_supported_operator(op.name):
                    op_name = converters.convert_operator_to_method_name(op.name)
                    converters.append_records(
                        converters.BUILTIN_CLASSES_SUPPORTED_OPERATOR_DATA,
                        {
                            "orign_cls_name": builtin_class.name,  # 原始的类名
                            "cls_name": cls_name,  # 转换后的类名
                            "orign_op_name": op.name,  # 原始的运算符名称
                            "op_name": op_name,  # 转换后的运算符名称
                        },
                    )

    for clazz in gdt_all_in_one.singletons:
        converters.SINGLETON_CLASS_NAMES.add(clazz.name)

def gen_c_writer(gdt_all_in_one: GodotInOne, c_writer: Writer) -> list[str]:
    global_variant_classes = []

    c_writer.write(
        """\
#include "Bindings.hpp"
"""
    )
    for clazz in gdt_all_in_one.singletons:
        if clazz.name == "ClassDB":
            header_name = "class_db_singleton"
        else:
            s1 = re.sub("(.)([A-Z][a-z0-9]+)", r"\1_\2", clazz.name)
            header_name = re.sub("([a-z])([A-Z])", r"\1_\2", s1).lower()
        c_writer.write(f"#include <godot_cpp/classes/{header_name}.hpp>")

    c_writer.write("")
    c_writer.write("namespace pkpy {")
    c_writer.write("")
    c_writer.write("void setup_bindings_generated() {")
    c_writer.indent()

    c_writer.write('const float inf = std::numeric_limits<float>::infinity();')

    for clazz in gdt_all_in_one.singletons:
        cpp_name = clazz.name
        if cpp_name == "ClassDB":
            cpp_name = "ClassDBSingleton"

        stmt = f'register_GDNativeSingleton("{clazz.name}", {cpp_name}::get_singleton());'
        if clazz.name != 'EditorInterface':
            c_writer.write(stmt)
        else:
            c_writer.write('if (Engine::get_singleton()->is_editor_hint()) {')
            c_writer.indent()
            c_writer.write(stmt)
            c_writer.dedent()
            c_writer.write('}')


    for clazz in gdt_all_in_one.builtin_classes + gdt_all_in_one.classes:
        if clazz.name in converters.SINGLETON_CLASS_NAMES:
            continue
        if clazz.name in converters.BLACKLIST_CLASS_NAMES:
            continue

        variant_type = converters.CLASS_TO_VARIANT_TYPE.get(
            clazz.name, "Variant::OBJECT"
        )
        c_writer.write(f'register_GDNativeClass({variant_type}, "{clazz.name}");')
        if variant_type != "Variant::OBJECT":
            global_variant_classes.append(clazz.name)

        for c in clazz.constants or []:
            const_name = converters.convert_keyword_name(c.name)
            c_writer.write(f'register_ClassConstant("{clazz.name}", "{const_name}", {c.value});')

    for enum in gdt_all_in_one.global_enums:
        for v in enum.values:
            c_writer.write(f'register_GlobalConstant("{v.name}", {v.value});')

    c_writer.dedent()
    c_writer.write("}")
    c_writer.write("")
    c_writer.write("} // namespace pkpy")

    return global_variant_classes


def gen_typings_pyi_writers(gdt_all_in_one: GodotInOne):
    writer_v = Writer()
    writer_v.write(
        """\
from typing import overload, Literal, Any
from .classes._Object import Object
from . import alias

intptr = int
def default(gdt_expr: str) -> Any: ...

"""
    )

    # -----手动定义Variant
    writer_v.write("class Variant:")
    writer_v.indent()
    writer_v.write("...")
    writer_v.dedent()
    writer_v.write("")

    all_writers = {}

    for clazz in gdt_all_in_one.builtin_classes + gdt_all_in_one.classes:
        writer: Writer
        is_empty_class: bool = True
        # ------class xxx(xxx, xxxEnum):
        class_name: str = converters.convert_class_name(clazz.name)

        if class_name in converters.BLACKLIST_CLASS_NAMES:
            continue
        
        # 获取 super class
        if isinstance(clazz, BuiltinClass):
            super_class_name = "Variant"
            writer = writer_v
        else:
            super_class_name = (
                converters.convert_class_name(clazz.inherits)
                if clazz.inherits
                else None
            )
            
            writer = Writer()
            writer.write(
        """\
from typing import Literal, Any, Callable as typing_Callable
from ..enums import *
from ._init import *

"""
            )
            all_writers[f"classes/_{class_name}.pyi"] = writer

        # 组装 class xxx(xxx)
        if super_class_name:
            writer.writefmt("class {0}({1}):", 
                class_name, 
                super_class_name,
            )
        else:
            writer.writefmt("class {0}:", 
                class_name
            )

        writer.indent()

        # 填充 enum
        if clazz.enums:
            for enum_data in clazz.enums:
                if isinstance(enum_data, ClassesEnum):
                    enum.gen_class_enum(writer, enum_data)
                else:
                    enum.gen_builtin_class_enum(writer, enum_data)

        # ------init
        if isinstance(clazz, BuiltinClass):
            if clazz.constructors:
                for constructor in clazz.constructors:
                    
                    method = constructor
                    
                    # ------Arguments
                    arg_expr = []
                    arg_expr.append("self")
                    
                    for arg in method.arguments or []:
                    
                        arg_name = converters.convert_keyword_name(arg.name)
                        arg_type = converters.convert_type_name(arg.type)
                        # arg_name += arg_type
                        arg_type_is_alias = arg_type in list(
                            converters.ALIAS_CLASS_DATA.loc[:, "cls_name"]
                        )

                        arg_default_value = None
                    
                        if arg_type_is_alias:
                            alias_module_path = "alias"
                            expr = f"{arg_name}: {alias_module_path}.{arg_type}"
                        else:
                            expr = f"{arg_name}: {arg_type}"
                    
                        arg_expr.append(expr)
                    
                    # ------Method
                    if len(clazz.constructors) > 1:
                        writer.write("@overload")
                    method_name = "__init__"
                    writer.writefmt(
                        "def {0}({1}): ...",
                        method_name,
                        ", ".join(arg_expr)
                    )
        writer.write('')

        # ------Member variables
        if isinstance(clazz, BuiltinClass):
            member_vars = clazz.members
        else:
            member_vars = clazz.properties

        if member_vars:
            for member_data in member_vars:

                member_name: str = converters.convert_keyword_name(member_data.name)
                member_type: str = converters.convert_type_name(member_data.type)

                writer.writefmt("{0}: {1}", member_name, member_type)

            writer.write("")
            is_empty_class = False

        # ------Signals
        if isinstance(clazz, BuiltinClass):
            signals = None
        else:
            signals = clazz.signals

            if signals:

                for signal in signals:
                    signal_name = converters.convert_keyword_name(signal.name)
                    signal_args = signal.arguments or []

                    arg_comment_expr_list = []
                    arg_expr_list = []
                    for signal_arg in signal_args:

                        arg_name = converters.convert_keyword_name(signal_arg.name)
                        arg_type = converters.convert_type_name(signal_arg.type)

                        arg_comment_expr_list.append(arg_name + ":" + arg_type)
                        arg_expr_list.append(arg_type)

                    writer.writefmt(
                        "{0}: Signal[typing_Callable[[{1}], None]]  # {2}",
                        signal_name,
                        ", ".join(arg_expr_list),
                        (
                            ", ".join(arg_comment_expr_list)
                            if arg_comment_expr_list
                            else "no arguments"
                        ),
                    )

                writer.write("")
                is_empty_class = False

        # ------Class variables
        if isinstance(clazz, BuiltinClass):
            class_consts = clazz.constants
        else:
            class_consts = clazz.constants

        if class_consts:
            for const in class_consts:
                const_name = converters.convert_keyword_name(const.name)
                value = str(const.value)
                writer.write(f"{const_name} = {parse_default(None, value)}")

            writer.write("")
            is_empty_class = False

        # ------Operators
        if isinstance(clazz, BuiltinClass):
            operators = clazz.operators
        else:
            operators = None

        if operators:
            for operator in operators:
                orign_operator_name = operator.name

                if converters.is_supported_operator(orign_operator_name):
                    operator_name = converters.convert_operator_to_method_name(
                        orign_operator_name
                    )
                    return_type = operator.return_type
                    return_type = converters.convert_type_name(return_type)

                    right_type = operator.right_type
                    if right_type:
                        right_type = converters.convert_type_name(right_type)
                        
                    right_type_is_alias = right_type in list(
                        converters.ALIAS_CLASS_DATA.loc[:, "cls_name"]
                    )

                    if converters.is_overload_operator(class_name, operator_name):
                        writer.write("@overload")

                    if right_type:
                        alias_module_path = "alias"
                        if right_type_is_alias:
                            writer.write(
                                f"def {operator_name}(self, right: {alias_module_path}.{right_type}) -> {return_type}: ..."
                            )
                        else:
                            writer.write(
                                f"def {operator_name}(self, right: {right_type}) -> {return_type}: ..."
                            )
                    else:
                        writer.write(
                            f"def {operator_name}(self) -> {return_type}: ..."
                        )

            writer.write("")
            is_empty_class = False        
        
        # ------Class methods
        if clazz.methods:
            for method in clazz.methods:
                ret_t = "None"
                if isinstance(method, (ClassesMethod, ClassesMethodVirtual)):
                    if method.return_value:
                        ret_t = converters.convert_type_name(method.return_value.type)
                else:
                    ret_t = (
                        converters.convert_type_name(method.return_type)
                        if method.return_type
                        else "None"
                    )

                # ------Arguments
                arg_expr = []
                if method.is_static:
                    writer.write("@staticmethod")
                else:
                    arg_expr.append("self")

                for arg in method.arguments or []:

                    arg_name = converters.convert_keyword_name(arg.name)
                    arg_type = converters.convert_type_name(arg.type)
                    arg_type_is_alias = arg_type in list(
                        converters.ALIAS_CLASS_DATA.loc[:, "cls_name"]
                    )

                    if arg.default_value:
                        arg_default_value = parse_default(arg_type, arg.default_value)
                    else:
                        arg_default_value = None

                    if arg_type_is_alias:
                        alias_module_path = "alias"
                        expr = f"{arg_name}: {alias_module_path}.{arg_type}"
                    else:
                        expr = f"{arg_name}: {arg_type}"

                    if arg.default_value is not None:
                        expr += f" = {arg_default_value}"
                    arg_expr.append(expr)

                if method.is_vararg:
                    arg_expr.append(f"*args")

                # ------Method
                method_name = converters.convert_keyword_name(method.name)
                writer.writefmt(
                    "def {0}({1}) -> {2}: ...",
                    method_name,
                    ", ".join(arg_expr),
                    ret_t,
                )

            writer.write("")
            is_empty_class = False
        
        if clazz.name == "Object":
            writer.write("@property")
            writer.write("def script(self) -> Any: ...")
            writer.write("")
            is_empty_class = False

        if is_empty_class:
            writer.write("...")
        writer.dedent()
        writer.write("")

    return writer_v, all_writers


def gen_alias_pyi_writer(gdt_all_in_one: GodotInOne, pyi_writer: Writer) -> Writer:

    modules = map(str, set(converters.ALIAS_CLASS_DATA.loc[:, "module_abs_path"]))
    modules = [module for module in modules if module != ""]
    
    pyi_writer.write(
        f"""\
import {', '.join(modules)}
from . import variants
"""
    )

    # 只有builtin_classes才有替用的类
    for clazz in gdt_all_in_one.builtin_classes:
        cls_name = converters.convert_class_name(clazz.name)
        found_records = converters.find_records(
            converters.ALIAS_CLASS_DATA, {"cls_name": cls_name}
        )
        if len(found_records) > 0:

            alternative_cls_with_module_exprs: list[str] = (
                []
            )  # ["vmath.vec2", "vmath.vec3", ...]
            for _, record in found_records.iterrows():
                module = record.loc["module_abs_path"] if record.loc["module_abs_path"] != "" else None
                
                if module is None:
                    alternative_cls_with_module_exprs.append(
                        record.loc["alternative_cls_name"]
                    )
                else:
                    alternative_cls_with_module_exprs.append(
                        record.loc["module_abs_path"]
                        + "."
                        + record.loc["alternative_cls_name"]
                    )

            pyi_writer.writefmt(
                "{0} = variants.{1} | {2}",
                cls_name,
                cls_name,
                " | ".join(alternative_cls_with_module_exprs),
            )

    return pyi_writer


def gen_init_pyi_writer(gdt_all_in_one: GodotInOne, pyi_writer: Writer, global_variant_classes: list[str]) -> Writer:
    pyi_writer.write(
        '''\
from . import classes

class PythonScriptInstance[T: classes.Object]:
    owner: T

    def __new__(cls) -> T: ...

    def start_coroutine(self, gen) -> int:
        """Start a coroutine. The argument should be a `generator` object.

        - To await a `godot.Signal`, use `yield your_signal`
        - To await another coroutine, use `yield from another_coroutine()`
        """

    def stop_coroutine(self, id: int) -> bool: ...
    def stop_all_coroutines(self) -> None: ...

def Extends[T: classes.Object](cls: type[T]) -> type[PythonScriptInstance[T]]: ...

def export[T](cls: type[T], default=None) -> T: ...
def export_range[T: int | float](min: T, max: T, step: T, default: T | None = None) -> T: ...
def signal(*args: str) -> classes.Signal: ...
def load(path: str) -> classes.Resource: ...
def cast[T](obj, cls: type[T]) -> T: ...

def var_to_bytes(v: Variant) -> PackedByteArray: ...
def bytes_to_var(b: PackedByteArray) -> Variant: ...
'''
    )

    writer = pyi_writer

    for clazz in gdt_all_in_one.singletons:
        writer.writefmt('{}: classes.{}', clazz.name, clazz.type)
    writer.write('')

    writer.write('from .variants import Variant as Variant')
    for clazz in global_variant_classes:
        writer.writefmt('from .variants import {0} as {0}', clazz)
    writer.write('')

    enum.gen_global_enums(pyi_writer, gdt_all_in_one.global_enums, with_types=False, with_values=True)
    return pyi_writer


def gen_enums_pyi_writer(gdt_all_in_one: GodotInOne, pyi_writer: Writer) -> Writer:
    pyi_writer.write(
        """\
from typing import Literal
"""
    )
    # --- global enum ---
    enum.gen_global_enums(pyi_writer, gdt_all_in_one.global_enums, with_types=True, with_values=False)
    return pyi_writer


def map_gdt_to_py(gdt_all_in_one: GodotInOne) -> MapResult:

    map_result = MapResult(c_writer=Writer(), pyi_writers={
        '__init__.pyi': Writer(),
        'alias.pyi': Writer(),
        'enums.pyi': Writer(),
    })

    print('fill_converters')
    fill_converters(gdt_all_in_one)

    print("gen_c_writer")
    global_variant_classes = gen_c_writer(gdt_all_in_one, map_result.c_writer)

    print('gen_alias_pyi_writer')
    gen_alias_pyi_writer(gdt_all_in_one, map_result.pyi_writers['alias.pyi'])
    print('gen_enums_pyi_writer')
    gen_enums_pyi_writer(gdt_all_in_one, map_result.pyi_writers['enums.pyi'])
    print('gen_init_pyi_writer')
    gen_init_pyi_writer(gdt_all_in_one, map_result.pyi_writers['__init__.pyi'], global_variant_classes)

    print('gen_typings_pyi_writer')
    writer_v, writers = gen_typings_pyi_writers(gdt_all_in_one)
    
    map_result.pyi_writers['classes/_init.pyi'] = Writer()
    for path in writers.keys():
        class_name = path.split("/")[-1].split(".")[0]
        map_result.pyi_writers['classes/_init.pyi'].write(f"from .{class_name} import {class_name[1:]} as {class_name[1:]}")
    map_result.pyi_writers['classes/_init.pyi'].write(f'from ..variants import *')

    map_result.pyi_writers['classes/__init__.pyi'] = Writer().write('from ._init import *\n')

    map_result.pyi_writers.update(writers)
    map_result.pyi_writers['variants.pyi'] = writer_v
    return map_result
