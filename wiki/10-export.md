# 游戏导出与发布

[返回目录](README.md) · [安装与平台](01-installation.md)

扩展随 Godot 的导出工作流发布。准备目标平台的 Godot 导出模板和匹配架构的原生库，再配置项目导出预设。预编译包的具体平台范围见[安装页](01-installation.md)。

## 普通 Python 模块如何进入 PCK

虽然 `site-packages` 被 `.gdignore` 隐藏，编辑器自动注册的 Python 导出插件仍会扫描并添加其中的 `.py` 文件。文件保持原来的 `res://site-packages/...` 路径，运行时通过 Godot FileAccess 从导出包读取。

| 内容 | 自动模块导出行为 |
| --- | --- |
| `site-packages/helpers.py` | 包含 |
| `site-packages/pkg/__init__.py`、子模块 | 包含 |
| `.gdignore`、点开头的文件和目录 | 跳过 |
| `__pycache__/` | 跳过 |
| `.pyc`、`.pyo`、`.pyi` | 不作为普通模块源码加入 |
| 符号链接文件和目录 | 扫描跳过 |
| JSON、CSV、图片、二进制数据 | 不自动加入 |
| 位于 `scripts/` 的挂载脚本 | 由正常的 Godot 资源导出流程处理 |

文件级的自动包含不分析“是否实际被 import”。开发用 `.py` 也可能进入包，应使用排除规则控制。

## 排除开发模块

导出插件读取预设的 `exclude_filter`，以逗号分隔模式，并对相对项目路径与 `res://` 路径作不区分大小写的匹配。

例如，排除 `site-packages/devtools/` 下的 Python 文件：

```ini
exclude_filter="site-packages/devtools/*"
```

可以在 Godot 导出预设界面配置相应排除字段。上例只是过滤项，不是完整的 `export_presets.cfg`。排除后仍需保证运行脚本不会无条件导入该目录中的模块。

## 包里的非 Python 数据

推荐把数据放在普通资源目录：

```text
res://data/item_rules.json
res://site-packages/items.py
```

在导出预设的非资源文件包含规则中安排 `.json`，并通过 Godot FileAccess 读取：

```python
import json
from godot.classes import FileAccess

def read_item_rules():
    text = FileAccess.get_file_as_string("res://data/item_rules.json")
    return json.loads(text)
```

不要把 `site-packages` 中出现的 JSON 文件认作导出插件必然打包的依赖。导出后的 `res://` 面向随游戏发布的资源；存档使用可写的 `user://`，见[存档配方](13-recipes.md)。

## 动态加载的脚本与场景

采用“只导出选中场景/资源”时，需要额外留意通过字符串 `load("res://...")` 才使用的资源。Python 脚本资源加载器没有完整提供静态依赖分析，不应假定所有动态路径都能被自动推导。

初次发布可选择完整项目资源导出，再根据实际依赖缩小范围。确保脚本、场景、材质、非资源数据和对应平台库都有明确包含方式。

## 原生库配置

确认 `.gdextension` 中目标平台右侧的库路径真实存在。源码检出的 Windows 配置默认引用 Debug 库，CI 打包配置则替换为 Release；发布包应与实际构建产物一致。

当前不提供 Linux、Web 或 Intel macOS 的即用库声明。不能只新增一个预设名称就获得这些平台支持。

SBX 项目还需要启用 SBX 的目标平台库；`.pyi`、GDScript 辅助资源或 Python 业务模块都不能代替缺失的原生模块。

## 发布前的人工确认项

- 记录 `GIT_COMMIT_HASH.txt`、Godot 版本及目标平台架构。
- 检查导出预设包含动态加载的脚本和场景。
- 检查 `site-packages` 的开发模块排除规则。
- 检查业务读取的非 `.py` 文件已被打包。
- 调试性能与正式运行行为需在合适的调试设置下分别确认。
- 在目标设备确认资源加载、导入和存档路径。

上述是读者发布时的操作建议；本次文档编写没有导出或启动任何游戏。

依据：[普通模块导出插件](../src/lang/PythonModuleExportPlugin.cpp)、[扫描规则](../src/lang/PythonSourcePath.cpp)、[脚本资源加载器](../src/lang/PythonScriptResourceFormatLoader.cpp)、[CI 打包](../.github/workflows/main.yml)。
