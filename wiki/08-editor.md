# 编辑器、语法高亮与类型补全

[返回目录](README.md) · [调试](09-debugging.md)

## Godot 内的工作流

扩展加载后会自动注册 Python 编辑器功能：附加脚本模板、`.py` 资源读写、Python 语法高亮、普通模块面板、脚本索引工具和导出插件。

可挂载脚本在 FileSystem 中打开。普通模块在 **Python** 面板中打开，因为 `site-packages/.gdignore` 会隐藏正常的 FileSystem 目录。

## Python 面板

1. 在 FileSystem 附近切换到名为 Python 的面板。
2. 展开 `site-packages` 下的包目录。
3. 双击 `.py` 文件，进入 Godot 的 Script 编辑器。
4. 编辑并保存，或点击行号区域设置断点。
5. 在外部新增、移动或删除文件后，点击 **Refresh**。

`Filter modules` 按相对路径进行不区分大小写的文本筛选。输入筛选文字只筛选缓存列表；需要重新扫描磁盘时仍要点击 Refresh。

| 面板信息 | 含义 |
| --- | --- |
| `Create a site-packages folder in your project, then refresh.` | 项目根目录没有该目录 |
| `No Python modules found.` | 没找到可显示的普通 `.py` 文件 |
| `N / M modules` | 当前筛选结果数 / 扫描总数 |
| `Cannot open Python module` | 文件不可读、已移动或已删除；先保存已有工作再刷新 |

扫描会跳过隐藏项、`__pycache__` 和符号链接。普通模块文档只允许保存到 `res://site-packages/` 下；要把它改成挂载脚本，应另建符合脚本约定的文件。

## 语法高亮不等于语言服务器

当前高亮器支持 Python 源码着色，并跟随编辑器主题颜色更新。以下能力没有完整实现：内置智能补全、符号跳转、函数查找、自动生成信号回调、完整的静态错误诊断、Python API 文档面板和 Python 函数级 Godot Profiler 数据。

没有红色错误提示不代表代码一定有效。源码实际加载错误会在输出中报告；静态补全和类型检查建议通过安装包 `.pyi` 配合外部编辑器完成。

## Pyright / Pylance 配置

在 **Godot 项目根目录** 创建 `pyrightconfig.json`。若使用仓库 demo，该位置就是 `demo/pyrightconfig.json`。

```json
{
  "pythonVersion": "3.12",
  "stubPath": "addons/godot-pocketpy/typings",
  "reportMissingModuleSource": "none",
  "reportOverlappingOverload": "none",
  "extraPaths": ["site-packages"]
}
```

这里的 `pythonVersion` 是静态工具解析 `.pyi` 时的配置，不是要求玩家安装 Python 3.12，也不是 pocketpy 完整实现 Python 3.12 的保证。

| 配置 | 用途 |
| --- | --- |
| `stubPath` | 找到 Godot 和 pocketpy 的类型提示 |
| `extraPaths` | 让业务包解析路径与运行时的 `site-packages` 对齐 |
| `reportMissingModuleSource` | 避免把“只有绑定与 stubs”视为缺少源码 |
| `reportOverlappingOverload` | 调整生成重载声明带来的诊断 |

不要将 `typings` 加入游戏模块目录去运行；`.pyi` 只描述接口。

## 项目脚本索引

菜单：**项目 → 工具 → Python: Rebuild Scripts Index File**。

输出位置：`res://addons/godot-pocketpy/typings/godot/scripts.pyi`。

例如：

```text
res://scripts/actors/Enemy.py
```

对应生成的类型声明类似：

```python
from scripts.actors.Enemy import Enemy as Enemy
```

当前实现只递归扫描 `res://scripts/`，并跳过隐藏目录和链接。放在项目根目录的脚本可以被 Godot 加载，但不会被此索引工具纳入。新增、移动、重命名脚本后重新生成；工具启动时也会尝试生成一次。

索引只提供补全和名称路径信息，不会提前执行或注册所有运行时脚本。`from godot.scripts import Enemy` 的运行时前提见[脚本模型](03-script-model.md)。

若提示 `Failed to open index file for writing`，检查扩展的 `typings/godot/` 目录是否存在、是否可写，以及是否正确安装或生成过 stubs。

## 源码保存和重载

`.py` 脚本和普通模块使用不同的加载器与保存器。它们都是文本源文件，但“保存成功”与“运行进程已使用新内容”是两个步骤。修改普通模块后停止并重新运行场景；修改库文件后重启编辑器。

依据：[编辑器插件](../src/lang/PythonEditorPlugin.cpp)、[Python 面板](../src/lang/PythonModulesDock.cpp)、[语法高亮器](../src/lang/PythonSyntaxHighlighter.cpp)、[脚本索引](../src/lang/PythonScript.cpp)、[未实现的语言服务接口](../src/lang/PythonScriptLanguage.cpp)。
