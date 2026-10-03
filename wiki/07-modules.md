# 普通 Python 模块、包与导入

[返回目录](README.md) · [编辑器](08-editor.md)

## 两类 `.py` 文件

|  | 挂载脚本 | 普通模块 |
| --- | --- | --- |
| 推荐目录 | `res://scripts/` | `res://site-packages/` |
| 作用 | 连接 Godot 对象、场景和回调 | 共享算法、业务规则和纯 Python 类型 |
| 结构 | 与文件同名的 `Extends(...)` 类 | 普通函数、类、常量或包 |
| 引用方式 | 场景挂载、`load`、`godot.scripts` | `import` |
| 编辑器入口 | FileSystem | Python 专用面板 |

内置的 `godot`、`math`、`json` 等模块由扩展或解释器提供。你放在磁盘上的普通模块由导入回调从 `res://site-packages/` 读取，不使用电脑的虚拟环境、系统 Python 或任意 `sys.path` 目录。

## 单文件模块

```python
# res://site-packages/combat_rules.py
def damage_after_armor(raw_damage, armor):
    return max(0, raw_damage - armor)
```

```python
# 在挂载脚本或另一个普通模块中
import combat_rules

damage = combat_rules.damage_after_armor(12, 3)
```

不要把它写成 `import scripts.combat_rules`，也不要把普通模块挂到节点上。

## 包与相对导入

```text
site-packages/
├── .gdignore
└── combat/
    ├── __init__.py
    └── rules.py
```

```python
# combat/rules.py
def damage_after_armor(raw_damage, armor):
    return max(0, raw_damage - armor)
```

```python
# combat/__init__.py
from .rules import damage_after_armor
```

```python
from combat import damage_after_armor
print(damage_after_armor(12, 3))
```

给包提供 `__init__.py`，不依赖 CPython 的命名空间包行为。不要用 `godot`、`json`、`math` 等已提供的模块名命名业务包。导入路径中的越界 `..` 不会让你跳出 `site-packages` 根目录。

## `.gdignore` 的职责

`site-packages/.gdignore` 阻止 Godot 按“可挂载 PythonScript”处理普通模块。它不会阻止扩展的专用导入、源码编辑或导出逻辑。

即使当前资源加载器能识别普通模块路径，也应保留 `.gdignore`，保持目录布局与打包工作流一致。普通模块在运行时不能通过 `load(...).new()` 变成节点脚本。

## 编辑是否执行模块

Python 面板使用只保存源码的 `PythonModuleSource` 文档。展开目录、双击打开、保存和设置断点不会因为编辑操作而运行模块。

但是，可挂载脚本在编辑器加载时需要执行顶层声明。如果它顶层 `import combat`，模块可能随这次导入执行。因此普通模块顶层仍应避免操作游戏场景、打开会话或写入持久数据。

## 模块缓存与修改生效

普通 `import` 会复用已加载的模块。修改磁盘源码、点击 Python 面板的 Refresh、保存脚本或重建 `.pyi`，都不等于重启运行中的模块。

**推荐的修改流程**是保存源文件，停止游戏，再重新启动游戏。升级动态库则需要关闭并重启编辑器。可挂载脚本的重载机制也不会自动递归重载它导入的全部普通模块。

### 当前版本的 `importlib.reload` 限制

pocketpy 提供 `importlib.reload(module)`，但当前 Godot 集成的导入回调会无条件写入 `size` 指针，而 pocketpy 的 reload 路径传入空指针。依据源码，这条组合路径存在无效指针访问风险；本手册没有执行它来验证。

因此本版本不将手动 `importlib.reload(...)` 作为可用的热更新步骤。需要重载时重启游戏进程，不要根据上游 pocketpy 单独使用时的说明直接套用。相关源码为 [Godot 导入回调](../src/lang/Bindings.cpp) 和 [pocketpy 模块重载](../pocketpy/src/public/ModuleSystem.c)。

## 第三方 Python 包

本项目不是 CPython 嵌入层，不支持直接把任意 pip 环境打包进去。包含 `.pyd`、CPython ABI 扩展或依赖未实现标准库的包不能直接使用。小型纯 Python 库可以在确认语法与依赖兼容后，以源码形式放进 `site-packages`；这仍不代表所有纯 Python 包都兼容。

可用内置模块见[标准库](11-standard-library.md)与[运行时模块](12-runtime-modules.md)。`.pyi` 是编辑器提示，不会让缺失模块在运行时出现。

## 导出时的区别

导出插件会收集普通模块的 `.py` 源码并按原路径写入 PCK。不会自动带上包内 JSON、图片、二进制数据或 `.pyc`。这些文件最好移到独立的 `res://data/` / `res://assets/`，并在导出预设中安排好包含规则，见[导出与发布](10-export.md)。

依据：[路径规范](../src/lang/PythonSourcePath.hpp)、[目录扫描](../src/lang/PythonSourcePath.cpp)、[普通模块文档](../src/lang/PythonModuleSource.cpp)、[普通模块资源读写](../src/lang/PythonModuleSourceIO.cpp)。
