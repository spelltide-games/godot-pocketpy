# 源码安装、构建与类型提示生成

[返回目录](README.md) · [预编译安装](01-installation.md)

本页面向需要自建平台库、修改绑定或启用 SBX 的用户。普通用户优先使用完整安装包。**下面的命令仅为文档示例，本次工作没有执行任何构建、生成器或测试。**

## 工具与源码

- Git，用于主仓库及子模块。
- 电脑上的 Python 3.10 或更高版本，用于构建脚本和 stubgen；不是游戏运行时的 pocketpy。
- CMake 至少 3.20：虽然顶层声明 3.17，当前 pocketpy 子项目要求 3.20。CI 使用 3.26.x。
- 支持 C11 / C++17 的目标平台编译工具链。
- 平台 SDK：Android 需要 NDK，iOS/macOS 需要相应 Apple 工具链。

```sh
git clone --recursive https://github.com/pocketpy/godot-pocketpy.git
cd godot-pocketpy
```

已有非递归克隆可按需补齐：

```sh
git submodule update --init --recursive
```

主仓库子模块是 `pocketpy` 和 `godot-cpp`。SBX 是另行提供的可选目录，不在此命令的获取范围内。

## 先生成绑定和 stubs

在仓库根目录：

```sh
python -m stubgen
```

生成器读取 `godot-cpp/gdextension/extension_api.json`，输出：

| 输出 | 用途 |
| --- | --- |
| `src/lang/BindingsGenerated.cpp` | 注册引擎类、单例、类常量和全局常量 |
| `demo/addons/godot-pocketpy/typings/godot/` | Godot API 的 `.pyi` |
| `demo/addons/godot-pocketpy/typings/` 其他文件 | 从 pocketpy 复制的模块类型提示 |
| `.../typings/godot/scripts.pyi` | 起初是空文件，之后由 Godot 编辑器重建项目脚本索引 |

**生成器会删除并重建整个目标 typings 目录**。不要把手写业务文件或唯一副本保存在生成目录中；它也会覆盖之前的项目脚本索引。

生成 stubs 不会安装 Python 包、不编译动态库，也不会让任意上游模块自动进入实际构建。接口新增时需要把生成文件和原生库一起更新。

## 构建默认版

```sh
python build.py
```

默认配置为 Debug，默认 platform 参数为 `win32`。显式 Release 示例：

```sh
python build.py --config Release --platform win32
```

| 参数 | 含义 |
| --- | --- |
| `--config Debug` | 调试构建，默认值 |
| `--config Release` | Release 构建，同时设置 `GODOTCPP_TARGET=template_release` |
| `--platform win32` / `macos` | 没有专门的交叉编译配置，主要由宿主与 CMake 工具链决定 |
| `--platform android` | 配置 NDK 工具链、android-22 和 arm64-v8a |
| `--platform ios` | 配置 iOS 工具链、OS64 和部署目标 13.0 |
| `--with-sbx` | 开启可选 SBX |

`--platform` 不是跨平台编译器下载器。在 Windows 上只写 `--platform macos` 不会获得 Apple 构建环境。该脚本使用共同的 `build/` 缓存目录，切换不兼容工具链时应采用独立构建目录或妥善重新配置。

输出库默认复制到 `demo/addons/godot-pocketpy/bin/<Godot平台名>/`。Windows 的输出目录叫 `windows`，不是脚本参数中的 `win32`。

## 平台例子

### Android

设置实际安装的 NDK 路径，再运行构建。PowerShell 示例：

```powershell
$env:ANDROID_NDK_HOME = "C:\Android\Sdk\ndk\你的NDK版本"
python -m stubgen
python build.py --config Release --platform android
```

### macOS 与 iOS

在具备对应工具链的主机上使用：

```sh
python -m stubgen
python build.py --config Release --platform macos
```

或选择 iOS：

```sh
python build.py --config Release --platform ios
```

当前扩展描述文件针对 arm64；不要把宿主产生的其他架构库直接当成可分发的声明内产物。

### Linux 和定制 CMake

仓库没有 Linux CI 打包或 `.gdextension` 现成条目。使用 Linux 宿主工具链构建是定制路径，例如：

```sh
python -m stubgen
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release
cmake --build build-linux --config Release
```

之后还需检查实际输出名称，并在 `.gdextension` 中添加匹配平台和 CPU 的库条目。本手册未验证这条路径，不提供捏造的最终库文件名。

## 让编辑器找到你构建的库

打开 `demo/addons/godot-pocketpy/godot-pocketpy.gdextension`，检查右侧库路径与产物相同。

Windows 源码配置目前引用 `template_debug`。若构建 Release，则需对应改为 `template_release`；其他平台也按实际库名核对。`.debug` / `.release` 左侧条件和右侧 `template_debug` / `template_release` 文件名不是同一个开关。

关闭 Godot 后替换库，再重新打开项目。该扩展不支持动态库热重载。

## 启用 SBX

确保 `sbx_extension/CMakeLists.txt`、其依赖和 `sbx_extension/typings/sbxcpp/` 已由有权限的来源提供。然后同时启用代码和类型提示：

```sh
python -m stubgen --with-sbx
python build.py --config Release --platform win32 --with-sbx
```

对应 CMake 开关为 `GODOT_POCKETPY_WITH_SBX=ON`。它要求 `lz4` 和 `msgpack` 目标存在。只运行 stubgen 的 `--with-sbx` 不会把 SBX 编译进库；只构建 SBX 而不提供类型提示则可能运行可用但 IDE 没有补全。

当前 CI 的 `WITH_SBX=OFF`，其打包步骤会移除 `addons/godot-pocketpy/sbx_extension/` 资源目录。制作 SBX 安装包时需将代码、类型提示与所需辅助资源配套提供。

## 配置开关对用户的影响

| 顶层默认值 | 影响 |
| --- | --- |
| `PK_ENABLE_OS=OFF` | 使用 Godot 文件系统接口 |
| `PK_ENABLE_THREADS=OFF` | 不提供默认的 pocketpy 计算线程工作流 |
| `PK_ENABLE_DLL=OFF` | 不通过动态库导入 Python 扩展模块 |
| `PK_ENABLE_DETERMINISM=ON` | 使用该构建的确定性数学相关配置；不是所有游戏逻辑完全确定的保证 |
| `PK_ENABLE_WATCHDOG=OFF` | 看门狗接口不可按默认功能使用 |
| `PK_ENABLE_CUSTOM_SNAME=ON` | 与 Godot 名称系统对接的内部选项 |
| `PK_ENABLE_MIMALLOC=OFF` | 不默认启用该分配器 |
| `PK_BUILD_STATIC_LIB=ON` | pocketpy 静态链接进扩展 |
| `PK_BUILD_MODULE_LZ4=ON` / `MSGPACK=ON` | 提供相应压缩和编码模块 |
| `PK_BUILD_MODULE_CUTE_PNG=OFF` | 不默认提供 cute_png |

## 构建相关模块各做什么

| 模块 | 用户可见作用 |
| --- | --- |
| `stubgen.__main__` | 命令入口，决定输入、输出和 SBX 复制 |
| `schema_gdt` | 读取 Godot API JSON 的数据结构 |
| `parse` | 解析到生成器使用的模型 |
| `converters` | 类型名、保留字、Variant 映射和别名转换 |
| `enum` | 枚举声明生成 |
| `map` | 生成绑定注册及 `.pyi` 内容 |
| `writer` | 组织生成文本和缩进 |
| `export` | 写出生成文件 |
| `build.py` | 调用 CMake，配置平台与构建模式 |
| `CMakeLists.txt` | 整合 godot-cpp、pocketpy 和可选 SBX |
| `.github/workflows/main.yml` | 多平台构建与合并安装包 |

这些模块在电脑的 Python 构建环境使用，不要复制进游戏 `site-packages`。

项目另有 `tests/` 和 `scripts/count_class_lines.py`。前者用于维护者的集成/回归验证，后者含旧的硬编码路径，不是安装必需步骤。本次工作未运行它们。

依据：[生成器入口](../stubgen/__main__.py)、[构建脚本](../build.py)、[主 CMake](../CMakeLists.txt)、[pocketpy CMake](../pocketpy/CMakeLists.txt)、[CI](../.github/workflows/main.yml)。
