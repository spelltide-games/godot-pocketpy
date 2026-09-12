"""Integration tests in an isolated, real Godot editor and exported PCK.

    GODOT_BIN=/path/to/godot python tests/module_sources/run.py

Keeps the generated project and logs under build/ for inspection. --prepare-only
creates a small project suitable for manually checking the dock and breakpoints.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def write(project, path, text):
    target = project / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def prepare():
    (ROOT / "build").mkdir(exist_ok=True)
    project = Path(tempfile.mkdtemp(prefix="module-sources-", dir=ROOT / "build"))
    shutil.copytree(
        ROOT / "demo/addons/godot-pocketpy",
        project / "addons/godot-pocketpy",
        ignore=shutil.ignore_patterns("~*", "*.pdb", "typings"),
    )
    write(project, "project.godot", '''config_version=5
[application]
config/name="Python Modules Integration"
run/main_scene="res://runner.tscn"
[rendering]
renderer/rendering_method="gl_compatibility"
[editor_plugins]
enabled=PackedStringArray("res://addons/module_probe/plugin.cfg")
[python]
debugger/enabled=true
''')
    write(project, "addons/module_probe/plugin.cfg", '''[plugin]
name="Module source integration tests"
description="Runs only when requested on the command line."
author="godot-pocketpy"
version="1.0"
script="editor_probe.gd"
''')
    shutil.copy2(HERE / "editor_probe.gd", project / "addons/module_probe/editor_probe.gd")
    shutil.copy2(HERE / "runtime_probe.gd", project / "runtime_probe.gd")
    files = {
        "site-packages/.gdignore": "",
        "site-packages/math_helpers.py": "def add(a, b):\n    return a + b\n",
        "site-packages/nested/math_helpers.py": "answer = 100\n",
        "site-packages/nested/__init__.py": "",
        "site-packages/nested/data.json": '{"answer":42}',
        "site-packages/empty/__init__.py": "",
        "site-packages/broken.py": "def broken(\n",
        "site-packages/raise_on_open.py": 'raise RuntimeError("MODULE_SOURCE_MUST_NOT_EXECUTE")\n',
        "site-packages/excluded.py": "answer = -1\n",
        "site-packages/__pycache__/ignored.py": "answer = -1\n",
        "exported.marker": "export probe\n",
        "ModuleRunner.py": '''from godot import *
from godot.classes import Node
import math_helpers

class ModuleRunner(Extends(Node)):
    def compute(self):
        return math_helpers.add(20, 22)

    def _ready(self):
        print("MODULE_GAME_RESULT", self.compute())
''',
        "runner.tscn": '''[gd_scene load_steps=2 format=3]
[ext_resource type="Script" path="res://ModuleRunner.py" id="1"]
[node name="ModuleRunner" type="Node"]
script = ExtResource("1")
''',
        "export_presets.cfg": '''[preset.0]
name="Module Pack"
platform="Windows Desktop"
runnable=true
export_filter="all_resources"
include_filter="exported.marker"
exclude_filter="site-packages/excluded.py"
export_path=""
[preset.0.options]
binary_format/architecture="x86_64"
''',
    }
    for path, content in files.items():
        write(project, path, content)
    return project


def run(godot, project, name, args, marker=None, cwd=None):
    log = project.parent / f"{project.name}-{name}.log"
    command = [godot, "--headless", *args]
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run(command, cwd=cwd or project, stdout=output,
                                stderr=subprocess.STDOUT, timeout=90)
    text = log.read_text(encoding="utf-8", errors="replace")
    # set_script() intentionally emits an engine error for abstract scripts.
    start = text.find("MODULE_EXPECTED_ATTACH_REJECTION_BEGIN")
    end = text.find("MODULE_EXPECTED_ATTACH_REJECTION_END")
    checked = text if start < 0 or end < 0 else text[:start] + text[end:]
    errors = [line for line in checked.splitlines()
              if "ERROR:" in line or "MODULE_CHECK FAIL" in line]
    if result.returncode != 0 or errors or (marker and marker not in text):
        print(text, flush=True)
        raise AssertionError(f"{name} failed (exit={result.returncode}); log: {log}")
    print(f"PASS {name}: {log}", flush=True)
    for line in text.splitlines():
        if line.startswith("MODULE_EDITOR_RESULT") or line.startswith("MODULE_RUNTIME_RESULT"):
            print(line, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", default=os.environ.get("GODOT_BIN"))
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    project = prepare()
    print(f"Project: {project}", flush=True)
    if args.prepare_only:
        return
    if not args.godot:
        parser.error("set GODOT_BIN or pass --godot")
    godot = str(Path(args.godot).resolve())
    run(godot, project, "import", ["--path", str(project), "--editor", "--import"])
    run(godot, project, "editor", ["--path", str(project), "--editor", "--", "--module-source-tests"], "MODULE_EDITOR_RESULT")
    run(godot, project, "restore", ["--path", str(project), "--editor", "--", "--module-source-restore-tests"], "MODULE_EDITOR_RESULT")
    run(godot, project, "runtime", ["--path", str(project), "--script", "res://runtime_probe.gd"], "MODULE_RUNTIME_RESULT PASS")
    pack = project.parent / f"{project.name}.pck"
    run(godot, project, "export", ["--path", str(project), "--editor", "--export-pack", "Module Pack", str(pack)])
    # An empty working directory prevents falling back to the source project.
    isolated = project.parent / f"{project.name}-packed-runtime"
    isolated.mkdir()
    # --export-pack only writes the PCK. Stage native libraries separately,
    # preserving the relative paths from the extension manifest; no .py source
    # is copied to this directory.
    shutil.copytree(project / "addons/godot-pocketpy/bin", isolated / "addons/godot-pocketpy/bin")
    run(godot, project, "pack-runtime", ["--path", str(isolated), "--main-pack", str(pack), "--script", "res://runtime_probe.gd", "--", "--packed"], "MODULE_RUNTIME_RESULT PASS", cwd=isolated)


if __name__ == "__main__":
    main()
