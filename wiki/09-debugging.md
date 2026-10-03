# 断点、单步与异常调试

[返回目录](README.md) · [常见问题](18-troubleshooting.md)

## 开启调试器

Python 调试器默认关闭。在 Godot 项目设置中找到 `python/debugger/enabled` 并启用，然后重新启动游戏运行会话。对应 `project.godot` 内容为：

```ini
[python]

debugger/enabled=true
```

调试器需要同时满足：该设置为 true，而且游戏进程启动时连接了 Godot 远程调试器。通常从编辑器的运行按钮启动即可满足后者；普通独立运行不自动具有调试连接。

成功启用时输出包含 `=> Python debugger enabled`。仓库 demo 已打开这个项目设置，新项目默认没有打开。

## 设置第一个断点

先创建脚本 `res://scripts/DebugExample.py` 并挂到 Node：

```python
from godot import Extends
from godot.classes import Node


class DebugExample(Extends(Node)):
    def _ready(self):
        base = 10
        result = self.calculate(base)  # 在这一行设置断点
        print(result)

    def calculate(self, value):
        doubled = value * 2
        return doubled + 1
```

保存后，在 Godot Script 编辑器点击可执行行的行号区域设置断点。运行场景，预期暂停在调用 `calculate` 前。使用调试面板的继续、逐过程、逐语句和跳出操作观察执行位置。

不要把断点仅放在注释、空行或未被执行的函数中。Python 面板中的普通模块也可以预先设置断点，模块尚未 import 不妨碍先保存断点。

## 调试普通模块

```python
# res://site-packages/calc_rules.py
def reward(level):
    bonus = level * 10  # 在 Python 面板打开文件后设置断点
    return bonus
```

在节点脚本 `_ready` 中 `import calc_rules`，再调用 `calc_rules.reward(3)`。调试器会把模块路径归一化为 `res://site-packages/...`，从调用栈可返回相应源码。

面板用于打开源码，不会为了设置断点提前执行模块。

## 能观察到什么

| 信息 | 当前表现 |
| --- | --- |
| 调用栈 | Python 函数名、文件路径和行号；最内层帧排在前面 |
| 局部变量 | 当前选中帧的变量 |
| 脚本成员 | Python 实例成员；宿主通过 Godot 对象展示 |
| 全局变量 | 模块全局状态，过滤函数、类型、模块等导入噪声 |
| Godot 值 | 能转换为 Variant 的值按引擎类型展示 |
| Python 列表、字典、自定义对象 | 通常以 `repr()` 字符串展示，不保证递归展开 |

变量显示会调用对象的 `repr`；自定义 `__repr__` 应保持简单，避免改变游戏状态或依赖仍在运行的场景。

## 异常

未处理异常在宿主报告它时可触发中断，并展示异常传播时记录的栈与变量。被正常捕获的异常不会都自动作为未处理错误暂停。

```python
# 放在一个会被调用的方法中，用于读者自行理解异常位置
def divide(self, count):
    total = 12
    return total / count
```

以 `count=0` 调用时，预期报告除零错误。由于异常栈是记录下来的快照，不能把它当作仍可逐行恢复的实时调用栈。

“跳过断点”用于普通断点；真实错误的中断与普通断点不同。若源码在编辑器加载阶段就失败，先看输出中的语法或类声明错误，因为编辑器进程自身没有启用这套游戏进程跟踪。

## 监视表达式的语言

监视表达式经过 Godot 的 `Expression` 处理，**不是 Python REPL**。简单数值表达式例如 `base + 1` 可以用于观察，但 Python 列表推导、import 语句、任意 Python 函数调用不属于这里的保证范围。没有 Variant 对应物的值可能已是显示字符串。

外部 DAP 客户端可通过 Godot 编辑器的调试适配层使用同一个调试桥。它不是 CPython 的 `debugpy`，也不是 pocketpy 独立解释器文档中的另一套调试服务。外部客户端配置取决于所用 Godot/编辑器插件版本，本手册不预置未经核对的端口或启动配置。

## 性能分析与调试的区别

逐行跟踪有运行开销。测量游戏性能前关闭 `python/debugger/enabled` 并重新开始运行会话。当前 Godot 的 Python 函数 Profiler 接口返回空数据，不代表 Python 没有耗时。

pocketpy 的 `pkpy.profiler_*` 使用跟踪钩子；不要与本调试器同时启用，见[运行时模块](12-runtime-modules.md)。

依据：[调试器设置与行为](../src/lang/PythonDebugger.cpp)、[语言调试接口](../src/lang/PythonScriptLanguage.cpp)、[断点与单步场景](../demo/tests/DebuggerFixture.py)、[异常场景](../demo/tests/DebuggerExcFixture.py)。
