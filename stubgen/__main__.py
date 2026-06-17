from stubgen.parse import *
from stubgen.map import *
from stubgen.export import *
import os
import shutil


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

# create leveldb
with open(f'{TYPINGS_PATH}/leveldb.pyi', 'w') as f:
    f.write('''
from typing import Iterator

class DB:
    def __new__(
            cls,
            path: str,
            create_if_missing=False,
            error_if_exists=False,
            paranoid_checks=False,
            bloom_filter_policy_bits=0,
        ): ...
    def get(self, key: str, verify_checksums=False) -> bytes | None: ...
    def put(self, key: str, value: bytes, sync=False) -> None: ...
    def delete(self, key: str, sync=False) -> None: ...
    def close(self) -> None: ...

    def write(self, ops: dict[str, bytes | None], sync=False) -> None: ...

    def iter(self, start: str | None = None, end: str | None = None, verify_checksums=False) -> Iterator[tuple[str, bytes]]: ...

''')
    
# create lockstepgo

with open(f'{TYPINGS_PATH}/lockstepgo.pyi', 'w') as f:
    f.write('''
class LockstepGoNetwork:
    @property
    def ws_opened(self) -> bool: ...
    @property
    def ws_closed(self) -> bool: ...
    @property
    def kcp_opened(self) -> bool: ...

    def connect_ws(self, host: str, port: int) -> int: ...
    def connect_kcp(self, host: str, port: int, conv: int, udp_redundancy: int) -> int: ...
    def send_ws(self, data: bytes) -> int: ...
    def send_kcp(self, data: bytes) -> int: ...
    def poll(self) -> None: ...
    def dispose(self) -> None: ...

    def on_ws_data(self, data: bytes) -> None: ...
    def on_kcp_data(self, data: bytes) -> None: ...
''')
