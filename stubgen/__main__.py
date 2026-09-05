from stubgen.parse import *
from stubgen.map import *
from stubgen.export import *
import argparse
import os
import shutil


parser = argparse.ArgumentParser()
parser.add_argument('--with-sbx', action='store_true',
                    help='Include SBX typing stubs in the generated typings')
args = parser.parse_args()

SBX_CPP_TYPINGS = 'sbx_extension/typings/sbxcpp'
if args.with_sbx and not os.path.isdir(SBX_CPP_TYPINGS):
    parser.error('SBX typings are missing. Clone the SBX repository into sbx_extension first.')

EXTENSION_API_PATH = 'godot-cpp/gdextension/extension_api.json'

gdt_schema = parse_to_gdt_schema(EXTENSION_API_PATH)
map_result = map_gdt_to_py(gdt_schema)

TYPINGS_PATH = 'demo/addons/godot-pocketpy/typings'
GODOT_TYPINGS_PATH = f'{TYPINGS_PATH}/godot'
shutil.rmtree(TYPINGS_PATH, ignore_errors=True)
shutil.copytree('pocketpy/include/typings', TYPINGS_PATH)
os.mkdir(GODOT_TYPINGS_PATH)

# create godot/scripts.pyi
with open(f'{GODOT_TYPINGS_PATH}/scripts.pyi', 'w') as f:
    pass

export_writer(map_result.c_writer, 'src/lang/BindingsGenerated.cpp')

for path, writer in map_result.pyi_writers.items():
    export_writer(writer, f'{GODOT_TYPINGS_PATH}/{path}')

if args.with_sbx:
    shutil.copytree(SBX_CPP_TYPINGS, f'{TYPINGS_PATH}/sbxcpp')
