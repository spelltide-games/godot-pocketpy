# godot-pocketpy 用户手册

godot-pocketpy 让你在 Godot 4 中使用 Python 编写游戏逻辑：把 `.py` 脚本挂到节点上，在 Inspector 中编辑属性，连接信号，并调用 Godot 的节点、资源和单例 API。它内嵌的是 **pocketpy**，使用安装包时不需要在玩家电脑上安装 Python。

这份 Wiki 面向游戏开发者和插件使用者。内容依据仓库提交 `d3bf233` 的源码、配置和示例整理，编写日期为 **2026-10-02**；可选 SBX 的说明依据本工作区另行提供的 `sbx_extension/` 源码。本文档未通过运行项目、示例、构建或测试验证。文中的输出均为预期行为，命令只供读者按需使用。

## 从这里开始

1. [安装与升级](01-installation.md)：选择平台、安装扩展、确认加载成功。
2. [第一个 Python 场景](02-quick-start.md)：从空项目创建可操作的示例。
3. [脚本、实例与生命周期](03-script-model.md)：理解 `Extends`、`self.owner` 和脚本继承。
4. [Godot API 与数据类型](04-godot-api.md)：查找类、常量、资源、容器和类型转换。
5. [导出属性与信号](05-exports-signals.md)、[协程](06-coroutines.md)：连接编辑器与游戏逻辑。

## 完整目录

| 页面 | 解决的问题 |
| --- | --- |
| [01 安装与升级](01-installation.md) | Godot 版本、平台架构、安装包结构、升级与卸载 |
| [02 快速开始](02-quick-start.md) | 场景结构、脚本创建、Inspector、输入和预期表现 |
| [03 脚本模型](03-script-model.md) | 命名、生命周期、实例创建、继承、内存与跨语言调用 |
| [04 Godot API](04-godot-api.md) | `godot`、`godot.classes`、`godot.constants`、`godot.scripts` 与 Variant |
| [05 属性与信号](05-exports-signals.md) | `export`、`export_range`、`signal`、`Callable` |
| [06 协程](06-coroutines.md) | 等待信号、嵌套任务、取消任务、带参数信号 |
| [07 普通模块与包](07-modules.md) | `site-packages`、`.gdignore`、导入、缓存与重载边界 |
| [08 编辑器与补全](08-editor.md) | Python 面板、源码编辑、语法高亮、Pyright 和脚本索引 |
| [09 调试](09-debugging.md) | 开启调试器、断点、单步、异常、变量和监视表达式 |
| [10 导出与发布](10-export.md) | 原生库、PCK、普通模块、排除规则、非 Python 数据 |
| [11 标准库模块](11-standard-library.md) | 数据结构、数学、时间、序列化、反射与 Python 兼容性 |
| [12 游戏与运行时模块](12-runtime-modules.md) | `vmath`、`array2d`、`easing`、`gc`、`pkpy` 等 |
| [13 实用配方](13-recipes.md) | 移动角色、生成场景、JSON 存档、GDScript 互调 |
| [14 SBX 存储与网络](14-sbx-storage-network.md) | 可选的 LevelDB、WebSocket/KCP、MessagePack 扩展 |
| [15 SBX 空间与瓦片](15-sbx-space.md) | 可选的环形地图、体素碰撞、查询、快照与调试绘制 |
| [16 示例与辅助工具](16-demo-tools.md) | demo 导航、滑动窗口、帧稳定器、Immediate Gizmos |
| [17 源码安装与类型生成](17-build-and-stubs.md) | 构建参数、平台配置、stubgen、可选 SBX 的启用 |
| [18 常见问题](18-troubleshooting.md) | 按症状定位安装、脚本、导入、调试和数据转换问题 |
| [19 模块覆盖索引](19-module-map.md) | 每个项目模块的用途、入口、对应文档与实现位置 |

## 四个最重要的约定

```text
res://
├── addons/godot-pocketpy/     # 扩展与类型提示
├── scripts/                  # 挂到节点上的 Python 脚本
│   └── Player.py             # 声明 class Player(Extends(...))
├── site-packages/             # 通过 import 使用的普通模块
│   ├── .gdignore             # 空文件，必须保留
│   └── gameplay/
│       ├── __init__.py
│       └── rules.py
├── scenes/
│   └── main.tscn
├── pyrightconfig.json
└── project.godot
```

- 可挂载脚本的文件名与类名一致，脚本类名在项目内唯一。
- Godot 节点 API 通过 `self.owner` 访问；`self` 保存 Python 脚本状态。
- 普通文件模块从 `res://site-packages/` 导入；内置模块由解释器和扩展提供。
- Godot 的 `Array`、`Dictionary`、`PackedByteArray` 与 Python 的 `list`、`dict`、`bytes` 是不同类型。

`res://` 表示 Godot 项目根目录；在仓库演示项目里，它对应 `demo/`，不是仓库根目录。Wiki 的代码块默认在 Godot 内的 pocketpy 环境中使用，标有 `gdscript` 的代码属于 GDScript，构建章节中的命令属于电脑上的终端。

## 默认版与可选组件

标准构建提供 Python 脚本支持、Godot API 绑定、普通模块编辑与导出、调试桥接和一部分 pocketpy 模块。当前主项目开启 `lz4`、`msgpack`，关闭 pocketpy 的 OS、线程和动态库加载功能。

`sbxcpp.leveldb`、`sbxcpp.lockstep`、`sbxcpp.space` 需要另行启用 SBX。当前 CI 配置 `WITH_SBX=OFF`，普通安装包不应被当成包含这些模块。Immediate Gizmos 是演示项目中的独立插件。

完整性以“每个对用户有意义的功能模块”为单位：Godot 数千个引擎方法采用统一调用规则，签名由随包 `.pyi` 提供；模块索引也介绍构建工具、内部支撑模块及未启用组件，避免把源码中存在的文件误认为可直接导入的 API。
