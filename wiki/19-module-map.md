# 模块覆盖与查阅索引

[返回目录](README.md)

本页把项目中的模块逐一对应到用户能力。内部 C++ 文件通常没有可供脚本直接调用的公共入口；它们通过编辑器菜单、资源加载或 Python API 发挥作用。了解它们的边界有助于定位问题，无需为使用项目去调用内部对象。

## Python 用户入口

| 模块 | 功能 | 文档与例子 |
| --- | --- | --- |
| `godot` | 辅助函数、Variant 类型、单例 | [API](04-godot-api.md)、[属性与信号](05-exports-signals.md)、[协程](06-coroutines.md) |
| `godot.classes` | 引擎类的构造和静态接口 | [API](04-godot-api.md)、[配方](13-recipes.md) |
| `godot.constants` | 全局整数常量与枚举值 | [API](04-godot-api.md)、[快速开始](02-quick-start.md) |
| `godot.scripts` | 项目已加载脚本类 | [脚本模型](03-script-model.md)、[索引工具](08-editor.md) |
| `site-packages` 中的用户模块 | 普通业务代码和包 | [导入](07-modules.md)、[存档配方](13-recipes.md) |
| `sbxcpp.leveldb` | 可选持久化键值库 | [SBX 存储](14-sbx-storage-network.md) |
| `sbxcpp.lockstep` | 可选网络传输 | [SBX 网络](14-sbx-storage-network.md) |
| `sbxcpp.space` | 可选物理、地图和查询 | [SBX 空间](15-sbx-space.md) |

类型提示中的 `godot.variants`、`godot.alias`、`godot.enums`、`godot.classes._...` 用于组织生成声明，运行时注册的公共模块以本表为准。示例通过 `godot.Vector2` 或 `godot.classes.Node` 访问实际绑定，不直接导入这些声明实现文件。

## 语言与运行时支撑模块

| 实现模块 | 用户看到的能力和边界 | 阅读位置 |
| --- | --- | --- |
| [register_types](../src/register_types.cpp) | 扩展启动时注册语言、资源和编辑器功能，关闭时清理 | [安装](01-installation.md) |
| [extensions](../src/extensions.hpp) | 决定是否注册可选 SBX | [SBX 前提](14-sbx-storage-network.md) |
| [PythonScriptLanguage](../src/lang/PythonScriptLanguage.cpp) | Python 语言、模板和 Godot 回调；部分语言服务尚未实现 | [脚本模型](03-script-model.md)、[编辑器](08-editor.md) |
| [PythonScript](../src/lang/PythonScript.cpp) | 脚本类识别、导出/信号元数据、重载、脚本索引 | [脚本模型](03-script-model.md)、[属性](05-exports-signals.md) |
| [PythonScriptInstance](../src/lang/PythonScriptInstance.cpp) | 节点与 Python 实例、方法调用、导出属性和寿命 | [跨语言配方](13-recipes.md) |
| [Bindings](../src/lang/Bindings.cpp) | export、signal、Extends、load、协程、Variant 运算 | [API](04-godot-api.md)、[协程](06-coroutines.md) |
| [BindingsHooks](../src/lang/BindingsHooks.cpp) | 属性、方法、静态方法、类常量、isinstance | [API](04-godot-api.md) |
| `BindingsGenerated.cpp`（生成文件） | 按 Godot API 注册类、常量和单例 | [生成器](17-build-and-stubs.md) |
| [Common](../src/lang/Common.cpp) | Python/Godot 类型转换与错误输出 | [数据类型](04-godot-api.md) |
| [MainThreadReloadPump](../src/lang/MainThreadReloadPump.hpp) | 编辑器将后台请求的脚本重载交回主线程；运行时后台编译有边界 | [排错](18-troubleshooting.md) |
| [PythonDebugger](../src/lang/PythonDebugger.cpp) | 断点、单步、异常和变量快照 | [调试](09-debugging.md) |
| [DebugPrint](../src/support/DebugPrint.cpp) | 扩展内部诊断输出，不是游戏日志 API | 用户脚本使用 print，见[快速开始](02-quick-start.md) |
| [custom_sname](../src/support/custom_sname.cpp) | pocketpy 名称与 Godot StringName 的内部适配 | 使用正常字符串/属性名即可，见[API](04-godot-api.md) |

这些支撑模块没有单独的用户初始化步骤，不需要在项目脚本中 import 或 new。

## 资源、编辑器和导出模块

| 模块 | 用户行为 | 对应文档 |
| --- | --- | --- |
| [PythonScriptResourceFormatLoader](../src/lang/PythonScriptResourceFormatLoader.cpp) | 加载 site-packages 以外的挂载脚本 | [脚本模型](03-script-model.md) |
| [PythonScriptResourceFormatSaver](../src/lang/PythonScriptResourceFormatSaver.cpp) | 保存挂载脚本源码 | [编辑器](08-editor.md) |
| [PythonSourcePath](../src/lang/PythonSourcePath.hpp) | 路径归一化、模块根目录和脚本扫描目录 | [模块导入](07-modules.md) |
| [PythonModuleSource](../src/lang/PythonModuleSource.cpp) | 只编辑普通模块源码，不提供可挂载实例 | [编辑器](08-editor.md) |
| [PythonModuleSourceIO](../src/lang/PythonModuleSourceIO.cpp) | 读取、缓存和保存普通模块文档 | [模块导入](07-modules.md) |
| [PythonModulesDock](../src/lang/PythonModulesDock.cpp) | Python 面板、筛选、Refresh、打开模块 | [编辑器](08-editor.md) |
| [PythonSyntaxHighlighter](../src/lang/PythonSyntaxHighlighter.cpp) | Python 语法着色与主题更新 | [编辑器](08-editor.md) |
| [PythonEditorPlugin](../src/lang/PythonEditorPlugin.cpp) | 自动安装面板、工具菜单、高亮器和导出插件 | [编辑器](08-editor.md) |
| [PythonModuleExportPlugin](../src/lang/PythonModuleExportPlugin.cpp) | 将普通模块源码加入 PCK，应用 exclude_filter | [导出](10-export.md) |

## pocketpy 模块全表

| 分组 | 模块 | 文档 |
| --- | --- | --- |
| 基础与注解 | `builtins`、`typing` | [标准库](11-standard-library.md) |
| 数学与采样 | `math`、`cmath`、`random`、`long_v1` | [标准库](11-standard-library.md) |
| 数据结构 | `bisect`、`heapq`、`collections`、`functools`、`operator`、`enum`、`dataclasses` | [标准库](11-standard-library.md) |
| 时间与文本 | `time`、`datetime`、`unicodedata` | [标准库](11-standard-library.md) |
| 序列化 | `json`、`pickle`、`base64` | [标准库](11-standard-library.md) |
| 反射与诊断 | `sys`、`traceback`、`inspect`、`dis`、`importlib` | [标准库](11-standard-library.md)；reload 限制见[模块页](07-modules.md) |
| 游戏计算 | `vmath`、`array2d`、`easing` | [运行时模块](12-runtime-modules.md) |
| 二进制格式 | `lz4`、`msgpack` | [运行时模块](12-runtime-modules.md) |
| VM 与底层 | `gc`、`pkpy`、`stdc`、`picoterm` | [运行时模块](12-runtime-modules.md) |
| 默认不提供 / 未注册 | `os`、`io`、`conio`、`colorcvt`、`cute_png`、`periphery` | [编译条件与替代接口](12-runtime-modules.md) |

`pocketpy/include/pybind11/`、C API、FFI 生成器、Flutter 插件和上游 web 示例属于解释器自身的嵌入与其他宿主集成资料，不是本 Godot 插件另外要求安装的功能。使用 Godot 脚本时只需本手册描述的语言层与绑定；需要扩展原生能力时可阅读 [pocketpy 上游资料](../pocketpy/README.md)。

## 可选 SBX 的内部组成

| 模块 | 用户用途 | 文档 |
| --- | --- | --- |
| `sbx.cpp` / `sbx.hpp` | 注册原生子模块和 SpaceDebugDraw | [可选组件前提](14-sbx-storage-network.md) |
| `LevelDB` | 字符串键到二进制值的持久化 | [存储](14-sbx-storage-network.md) |
| `LockstepGoNetwork` | WebSocket / KCP 传输 | [网络](14-sbx-storage-network.md) |
| `MessagePack` | 安装 Godot Variant 的 msgpack ext 回调 | [序列化](14-sbx-storage-network.md) |
| `Space`、`Body`、`SpaceBindings` | 运动、盒子形状、接触与 Python API | [物理](15-sbx-space.md) |
| `Tilemap`、`Torus`、`Euclidean` | 八层地图、环绕和几何计算 | [地图与查询](15-sbx-space.md) |
| `Config` | 像素尺度、层数、模拟容差和砖高 | [空间约束](15-sbx-space.md) |
| `ToVariant` | Space / BodyID 等快照转换 | [快照](15-sbx-space.md) |
| `SpaceDebugDraw` | 原生物理数据的 Godot 可视化 | [调试绘制](15-sbx-space.md) |
| `index_vector`、`extern_c`、`pkpy.hpp`、`system/snappy.h` | ID 容器、数学和依赖适配，无独立用户配置 | 通过公开 Space / 存储接口使用 |
| `PixelQuad3D.hpp` | 当前仅看到类型声明，注册入口未提供该类 | 不作为可用的公开模块 |
| `leveldb/`、`kcp/` | 存储和传输的底层依赖 | 用户通过 `sbxcpp` 使用，无需另行编写这些库的调用 |

SBX 源码不随默认主仓库检出提供，因此本表解释组件作用，不要求所有用户都具有这些目录。

## 演示与构建工具

| 模块 | 入口与用途 |
| --- | --- |
| `demo` 核心示例与 `bugs/` | [示例导航](16-demo-tools.md) |
| `demo/site-packages/sbx` | 物理 playground 的 Python 业务层，区别于原生 sbxcpp |
| `SlidingWindow`、`FrameRateWindow`、`TimeDeltaWindow`、`FrameStabilizer` | [GDScript 辅助工具](16-demo-tools.md) |
| `immediate_gizmos` | [独立 2D/3D 绘制插件](16-demo-tools.md) |
| `stubgen` 全部子模块 | [类型生成器模块表](17-build-and-stubs.md) |
| `build.py`、CMake、CI | [源码安装](17-build-and-stubs.md) |
| `tests/test_stubgen_constants.py` | 维护者检查全局常量生成规则 |
| `tests/debugger/` | 调试协议、启动夹具、断点/单步/异常回归 |
| `tests/module_sources/` | 普通模块编辑、运行、导出集成回归 |
| `scripts/count_class_lines.py` | 维护者统计工具，含旧硬编码路径，不是正常用户安装步骤 |
| `docs/assets/` | README 使用的操作截图 |

测试目录用于描述和维护预期行为，不是用户项目必须携带的运行依赖。本次文档任务未执行其中的代码。
