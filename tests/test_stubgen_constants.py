"""Run with: python -m unittest discover -s tests -p test_stubgen_constants.py"""

import contextlib
from dataclasses import replace
import io
import json
from pathlib import Path
import re
import unittest

from stubgen.map import gen_c_writer, gen_constants_pyi_writer, map_gdt_to_py
from stubgen.parse import parse_to_gdt_schema
from stubgen.schema_gdt import GlobalConstant
from stubgen.writer import Writer


API_PATH = Path(__file__).resolve().parents[1] / 'godot-cpp/gdextension/extension_api.json'


def stub_values(writer):
    namespace = {}
    exec(str(writer), namespace)
    return {name: value for name, value in namespace.items() if not name.startswith('_')}


def binding_values(writer):
    return {
        name: int(value)
        for name, value in re.findall(r'register_GlobalConstant\("(\w+)", (-?\d+)\);', str(writer))
    }


class ConstantsGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        api = json.loads(API_PATH.read_text(encoding='utf-8'))
        cls.expected = {constant['name']: constant['value'] for constant in api['global_constants']}
        cls.expected.update({
            value['name']: value['value']
            for enum in api['global_enums']
            for value in enum['values']
        })
        cls.schema = parse_to_gdt_schema(str(API_PATH))
        with contextlib.redirect_stdout(io.StringIO()):
            cls.result = map_gdt_to_py(cls.schema)

    def test_constants_stub_exports_all_api_values(self):
        self.assertEqual(stub_values(self.result.pyi_writers['constants.pyi']), self.expected)
        self.assertEqual(binding_values(self.result.c_writer), self.expected)

    def test_root_excludes_constants_and_keeps_enum_types_separate(self):
        root = str(self.result.pyi_writers['__init__.pyi'])
        self.assertNotIn('constants', root)
        self.assertNotIn('from .constants import', root)
        root_assignments = set(re.findall(r'^(\w+)\s*=', root, re.MULTILINE))
        self.assertFalse(root_assignments & self.expected.keys())
        self.assertIn('Key = Literal[', str(self.result.pyi_writers['enums.pyi']))
        self.assertIn('Variant_Type = Literal[', str(self.result.pyi_writers['enums.pyi']))
        self.assertIn('from .variants import Vector2 as Vector2', root)
        self.assertIn('Input: classes.Input', root)

    def test_standalone_global_constants_preserve_signed_64_bit_values(self):
        expected = {'LARGE_FLAG': 1 << 40, 'NEGATIVE_VALUE': -(1 << 40)}
        schema = replace(
            self.schema,
            global_constants=[GlobalConstant(name, value, False, None) for name, value in expected.items()],
            global_enums=[],
            builtin_classes=[],
            classes=[],
            singletons=[],
        )
        constants = gen_constants_pyi_writer(schema, Writer())
        bindings = Writer()
        gen_c_writer(schema, bindings)
        self.assertEqual(stub_values(constants), expected)
        self.assertEqual(binding_values(bindings), expected)


if __name__ == '__main__':
    unittest.main()
