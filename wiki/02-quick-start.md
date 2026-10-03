# 第一个 Python 场景

[返回目录](README.md) · [脚本模型](03-script-model.md)

本例创建一个按钮计数器。它只需要默认版扩展，涵盖脚本挂载、导出属性、节点访问、信号连接和普通模块导入。以下是供你手动操作的步骤，本手册编写时没有执行示例。

## 1. 创建场景

在 Godot 中创建如下场景并保存为 `res://scenes/main.tscn`。将 Button 和 Label 摆放到可见位置，给 Button 设置文字“加分”。

```text
Main (Control)       ← 挂载 CounterDemo.py
├── Button (Button)
└── Label (Label)
```

节点名称区分大小写。后面使用 `get_node("Button")`、`get_node("Label")`，所以名称必须对应。

## 2. 创建普通模块

确认 `res://site-packages/.gdignore` 已存在，新建 `res://site-packages/score_rules.py`：

```python
def next_score(current, amount):
    return current + amount
```

这个文件是普通模块，不挂到节点上，也不需要 `Extends`。

## 3. 创建节点脚本

选择 Main，打开“附加脚本”，语言选择 `Python`，路径填 `res://scripts/CounterDemo.py`。将内容改为：

```python
from godot import Extends, export, signal, Callable
from godot.classes import Control
from score_rules import next_score


class CounterDemo(Extends(Control)):
    increment = export(int, default=1)
    score_changed = signal("value")

    def __init__(self):
        self.score = 0

    def _ready(self):
        self.label = self.owner.get_node("Label")
        button = self.owner.get_node("Button")
        button.pressed.connect(Callable(self.owner, "on_pressed"))
        self.refresh_label()

    def refresh_label(self):
        self.label.text = "Score: " + str(self.score)

    def on_pressed(self):
        self.score = next_score(self.score, self.increment)
        self.refresh_label()
        self.score_changed.emit(self.score)
        print("score =", self.score)
```

文件名 `CounterDemo.py` 与类名 `CounterDemo` 必须一致。类通过 `Extends(Control)` 指定宿主类型；场景根节点必须是 Control 或其子类。

## 4. 设置 Inspector

选择 Main，在脚本属性区把 `increment` 改成 `5`。`self.score` 是普通 Python 状态，不会自动出现在 Inspector 中；`increment` 使用 `export`，因此可由场景保存。

脚本的 `__init__` 只初始化自己的计数状态。读取 Inspector 设置和获取子节点放在 `_ready` 中。

## 5. 预期表现

由读者启动当前场景后，Label 应显示 `Score: 0`。每点击一次 Button，分数加 5，Label 更新，输出面板出现新的分数。`score_changed` 会携带当前分数发出一次。

如果你从 Godot 的信号面板手动连接了 `pressed`，请去掉代码中的连接语句，避免重复连接。

## 6. 扩展：键盘输入

在文件顶部增加导入，在类中增加回调：

```python
from godot.constants import KEY_SPACE

# 放在 CounterDemo 类内
def _unhandled_key_input(self, event):
    if event.pressed and not event.echo and event.keycode == KEY_SPACE:
        self.on_pressed()
```

这里传入的是 Godot 的键盘事件。若界面控件已经处理了按键，`_unhandled_key_input` 可能不会收到它；可以先用鼠标按钮确认计数逻辑，再调整界面焦点和输入设计。

## 出错时先看这几项

| 现象 | 检查 |
| --- | --- |
| 无法选择 Python | 扩展安装目录、平台库、Godot 版本和编辑器是否已重启 |
| 找不到 `CounterDemo` | 文件名、类名和大小写是否一致 |
| 找不到 `score_rules` | 文件是否放在 `res://site-packages/score_rules.py` |
| 找不到 Button 或 Label | 节点是否为 Main 的直接子节点，名称是否相同 |
| `Callable` 参数错误 | 第一项是 `self.owner`，第二项是方法名字符串 |
| 属性没有显示 | 是否使用 `export`，脚本是否成功加载 |

可继续阅读[属性与信号](05-exports-signals.md)，或查看仓库现有的 [MyScript 示例](../demo/MyScript.py)。
