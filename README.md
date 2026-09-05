# godot-pocketpy


## How to use
Download the artifact `godot-pocketpy` from Github Actions page.
https://github.com/pocketpy/godot-pocketpy/actions

Unzip it into your Godot project's `addons` folder, and then open the Godot editor. Make sure your Godot version is v4.4 or later.

Create a special folder `site-packages` in your project root with a `.gdignore` file. This is where you can put your Python modules.
When you run `import` statements, `site-packages` will be searched.

You may also want to create a `scripts` folder to store Godot-based Python scripts, which can be attached to nodes in your Godot scenes.
Please note that any Python scripts outside of `site-packages` will be compiled by Godot, so they must derive from `godot.Node` and the class name must match the filename.

Python scripts in `site-packages` must not be compiled, which is why you need to add a `.gdignore` file in the `site-packages` folder.

The basic structure of your project should look like this:

```
- addons
    - godot-pocketpy
        - bin
        - typings
- site-packages
    - .gdignore
    - some_library
        - __init__.py
        - test.py
- scripts
    - MyClass.py
- project.godot
```

To create a Godot-based Python script, open "Attach Node Script" dialog, select Python as the language with the "Node: Default" template, and enter the script name (e.g., `MyClass.py`).

![attach_node_script](docs/assets/attach_node_script.png)

![demo_script](docs/assets/demo_script.png)

## Dev Instructions

To generate the stub files, run:

```
python -m stubgen
```

To build the project in debug mode, run:

```
python build.py
```

To test the debug build, you need to open `demo/addons/godot-pocketpy/godot-pocketpy.gdextension`,
find your platform and replace `template_release` with `template_debug`.
In this way, the Godot Editor can load the debug version of the extension.

### Optional native extensions

SBX is disabled by default for new builds. Enable it to use `sbxcpp.*`, the
`MessagePack` and `SpaceDebugDraw` Godot classes, and the SBX demo scenes:

```sh
git submodule update --init --recursive sbx_extension
python -m stubgen --with-sbx
python build.py --cmake-arg=-DGODOT_POCKETPY_WITH_SBX=ON
```

The equivalent CMake commands, after running stubgen, are:

```sh
cmake -S . -B build -DGODOT_POCKETPY_WITH_SBX=ON
cmake --build build --config Debug
```

CMake remembers options in the build directory. To disable SBX in an existing build:

```sh
python -m stubgen
python build.py --cmake-arg=-DGODOT_POCKETPY_WITH_SBX=OFF
```

When disabled, the build does not require the `sbx_extension` submodule and excludes
its sources, LevelDB, KCP, and runtime registrations. PocketPy's own LZ4 and msgpack
modules remain controlled by `PK_BUILD_MODULE_LZ4` and `PK_BUILD_MODULE_MSGPACK`;
both must be enabled when using SBX.

Run stubgen before building. Pass `--with-sbx` to include the `sbxcpp` typings.
Without this flag, stubgen regenerates only the base typings and removes any
previously copied `sbxcpp` typings. Typing generation and the CMake build option
are controlled separately.
CI uses `GODOT_POCKETPY_WITH_SBX` (`ON` by default) to select both the compiled
extension and its typings.

`--cmake-arg` can be repeated to pass additional CMake configure arguments. Quote
the whole argument when its value contains spaces, for example
`"--cmake-arg=-DGODOT_PROJECT_DIR=demo project"`.

To add another native extension, define an option and conditionally add/link its
target in the root CMake file, then add its guarded registration calls to
`src/extensions.hpp`. Keep the Godot and Python registration stages separate.
Extensions with typing packages can add an explicit option to `stubgen`.
