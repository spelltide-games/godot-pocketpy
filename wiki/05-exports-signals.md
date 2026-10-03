# Inspector 属性与信号

[返回目录](README.md) · [协程](06-coroutines.md)

## `export(cls, default=None)`

`export` 在脚本类体中声明可由 Inspector 编辑、可随场景保存的字段。普通类型注解或普通类变量不会自动成为导出属性。

```python
# res://scripts/Unit.py，挂到 Node2D 或其子类
from godot import Extends, export, export_range, Color, Vector2
from godot.classes import Node, Node2D, Texture2D, PackedScene


class Unit(Extends(Node2D)):
    display_name = export(str, default="新单位")
    active = export(bool, default=True)
    health = export(int, default=100)
    speed = export(float, default=180.0)
    offset = export(Vector2, default=Vector2(0, 0))
    tint = export(Color, default=Color(1, 1, 1, 1))
    target = export(Node)
    portrait = export(Texture2D)
    projectile_scene = export(PackedScene)
    volume = export_range(0.0, 1.0, 0.05, default=0.8)

    def _ready(self):
        print(self.display_name, self.health, self.speed)
        if self.target is not None:
            print(self.target.get_path())
```

| 类型种类 | 可用声明 | 注意事项 |
| --- | --- | --- |
| Python 标量 | `int`、`float`、`bool`、`str` | 默认值使用相匹配的类型 |
| Godot 内建值 | `Vector2`、`Color`、`Rect2`、`Array`、`Dictionary` 等 | 默认值也应是能转换的 Godot 值 |
| Resource 类 | `Texture2D`、`PackedScene` 等 | 在 Inspector 中选择兼容的资源 |
| Node 类 | `Node`、`Node2D` 等 | 挂载脚本本身必须继承 Node 类 |
| 其他 Object 类 / 普通 Python 类 | 不接受 | 会报告导出类型错误 |
| Python `list` / `dict` | 不接受 | 使用 Godot `Array` / `Dictionary` 或在运行时管理 |

没有设置引用时，应按 `None` 处理。建议对标量和向量明确写 `default`，避免依赖编辑器占位对象自动构造出来的默认显示值。

### 默认值与实例字段

每个实例会收到声明默认值的副本；Godot 容器的复制不应被理解为自动深拷贝所有嵌套资源。需要每实例独有的 Python 容器时，在 `__init__` 中创建：

```python
def __init__(self):
    self.inventory = []
    self.cooldowns = {}
```

读取用户在 Inspector 保存的最终值应放在 `_ready`。详细初始化顺序见[脚本模型](03-script-model.md)。

## `export_range(min, max, step, *extra_hints, default=None)`

范围属性必须提供 `min`、`max`、`step`。三者中任意一个是浮点数，属性就成为 float；三个都是 int 时属性为 int。

```python
# 放在脚本类体中
charges = export_range(0, 9, 1, default=3)
distance = export_range(0.0, 100.0, 0.5, "or_greater", "suffix:m", default=5.0)
```

`extra_hints` 是交给 Godot Inspector 的提示字符串，例如 `or_greater`、`or_less`、`suffix:m`。非字符串提示会抛出 `TypeError`。范围提示控制编辑方式，不代替游戏逻辑中的输入校验。

当前辅助函数没有提供完整的 GDScript 注解集合，例如 `export_group`、`export_category` 或自定义 RPC 注解。不要把 GDScript 的所有注解机械翻译成 Python 同名函数。

## `signal(*argument_names)`

信号也在类体声明。参数是名称，不是类型声明；引擎端将它们视为未指定类型的 Variant 参数。

```python
# res://scripts/Health.py，挂到 Node
from godot import Extends, export, signal
from godot.classes import Node


class Health(Extends(Node)):
    max_hp = export(int, default=100)
    health_changed = signal("old_value", "new_value")
    died = signal()

    def _ready(self):
        self.hp = self.max_hp

    def take_damage(self, amount):
        old = self.hp
        self.hp = max(0, self.hp - amount)
        self.health_changed.emit(old, self.hp)
        if old > 0 and self.hp == 0:
            self.died.emit()
```

信号出现在 Godot 的“节点 → 信号”面板。可以连接到另一个 Python 或 GDScript 节点。传入信号的值同样受[类型转换规则](04-godot-api.md)限制，Python 原生字典不会因为是信号参数就自动成为 Godot Dictionary。

## 连接已有 Godot 信号

Godot 需要的是 `Callable`。Python 绑定方法和 lambda 不能直接替代它。

```python
from godot import Callable

# 放在包含 Button 子节点的脚本类中
def _ready(self):
    self.button = self.owner.get_node("Button")
    self.click_callback = Callable(self.owner, "on_pressed")
    if not self.button.pressed.is_connected(self.click_callback):
        self.button.pressed.connect(self.click_callback)

def on_pressed(self):
    print("clicked")

def _exit_tree(self):
    if self.button.pressed.is_connected(self.click_callback):
        self.button.pressed.disconnect(self.click_callback)
```

此片段假设 `_ready` 已成功完成，且 Button 在清理时仍然存在。需要适应动态节点寿命的业务代码应先检查对象是否有效。

带参数信号的目标方法要接收相应数量的参数。`Callable(self.owner, "on_health_changed")` 最终按方法名进入 Python 脚本；不要写 `Callable(self, ...)`。

```python
def on_health_changed(self, old_value, new_value):
    print(old_value, "->", new_value)
```

也可通过编辑器连接，但当前 Python 语言插件不提供自动生成回调函数的完整能力。先手动写好目标方法，再在连接界面指定现有方法名。

## GDScript 接收 Python 信号

以下 GDScript 需要与挂载 `Health.py` 的 `HealthNode` 同处一个父节点下：

```gdscript
extends Node

func _ready():
    var health_node = get_parent().get_node("HealthNode")
    health_node.health_changed.connect(_on_health_changed)

func _on_health_changed(old_value, new_value):
    print(old_value, " -> ", new_value)
```

不要在继承层级中重复声明同名导出项或信号；当前实现将其报告为 `Duplicate member`。

依据：[声明 API](../src/lang/Bindings.cpp)、[导出元数据](../src/lang/PythonScript.cpp)、[跨语言属性访问](../src/lang/PythonScriptInstance.cpp)。
