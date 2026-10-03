# Godot API、命名空间与数据类型

[返回目录](README.md) · [属性与信号](05-exports-signals.md)

## 四个运行时模块

| 模块 | 内容 | 典型导入 |
| --- | --- | --- |
| `godot` | 脚本辅助函数、Variant 类型和引擎单例 | `from godot import Vector2, Input, load` |
| `godot.classes` | 引擎类，以及内建 Variant 类的入口 | `from godot.classes import Node2D, PackedScene` |
| `godot.constants` | 全局常量和全局枚举值 | `from godot.constants import KEY_SPACE, OK` |
| `godot.scripts` | 已加载的项目 Python 脚本类 | `from godot.scripts import Actor` |

`from godot import *` 适合脚本入门；不会自动导入全局常量。引擎类常量仍放在所属类型上，例如 `Node.PROCESS_MODE_ALWAYS`、`Vector2.ZERO`。

`godot.constants` 不附加到根模块命名空间；使用上表的显式导入形式。不要依赖先 `import godot` 再访问 `godot.constants` 或 `godot.KEY_SPACE`。

随包 `typings/godot/alias.pyi` 和以 `_` 开头的类型文件服务于静态类型检查，不是额外的运行时 Python 模块。不要照着每一个 `.pyi` 文件名直接 import。

## 类、对象与单例

```python
from godot import Vector2, Color, Engine, OS
from godot.classes import Node2D

node = Node2D()
node.position = Vector2(100.0, 50.0)
print(node.position.x)
print(Color.from_hsv(0.3, 0.5, 0.8, 1.0))
print(OS.get_name(), Engine.get_frames_per_second())
# 将 node 加入你的场景；若最终不需要它，负责释放。
```

可实例化的引擎类用 `Node2D()`，不是 GDScript 风格的 `Node2D.new()`。单例使用根模块提供的现有对象，例如 `Input`、`Time`、`ProjectSettings`、`ResourceLoader`。抽象类、服务器接口等不一定允许构造。

Godot 原生方法主要按位置参数转发。示例中使用 `create_timer(1.0)` 这样的调用；不要因为 `.pyi` 展示了参数名，就假定所有原生方法支持 Python 关键字参数。

## 辅助函数参考

| 函数 | 作用 | 失败或限制 |
| --- | --- | --- |
| `Extends(base)` | 声明脚本基类；接受引擎类或脚本路径 | 在脚本加载上下文中使用 |
| `export(cls, default=None)` | 声明 Inspector 属性 | 见[属性页](05-exports-signals.md) |
| `export_range(min, max, step, *extra_hints, default=None)` | 声明范围属性 | 三个范围参数均需提供 |
| `signal(*argument_names)` | 声明信号 | 参数为名称字符串 |
| `load(path)` | 用 Godot ResourceLoader 加载资源 | 加载失败抛出 `RuntimeError` |
| `as_(value, cls)` | 检查 Godot/Python 类型并返回原对象 | 不匹配抛出 `TypeError`，不是返回 `None` |
| `var_to_bytes(value)` | Godot Variant 二进制序列化 | 返回 Godot `PackedByteArray` |
| `bytes_to_var(buffer)` | 反序列化 Godot Variant | 输入使用 Godot `PackedByteArray` |

扩展也让内置 `isinstance` 理解 Godot 类型：

```python
from godot import as_, Vector2
from godot.classes import Node2D

print(isinstance(Vector2(1, 2), Vector2))
node = self.owner.get_node("Player")
if isinstance(node, Node2D):
    player = as_(node, Node2D)
    player.position = Vector2(20, 30)
```

这个片段应放在已有节点脚本的方法中，场景里需有对应的 Player 节点。

## 数据如何跨越 Python 与 Godot

| Python 侧值 | Godot 侧表示 | 说明 |
| --- | --- | --- |
| `None` | Nil / 空对象 | Godot 空对象也转换为 `None` |
| `bool`、`int`、`float`、`str` | Bool、Int、Float、String | 直接转换；整数为 64 位 |
| `godot.Vector2` 等包装值 | 对应 Variant | 从 Godot 返回后仍是绑定包装值 |
| `vmath.vec2/vec2i/vec3/vec3i/vec4i` | 对应 Godot 向量 | 支持向 Godot 传递；返回时不保证转回 vmath 类型 |
| `vmath.color32` | `Color` | 通道按 0–255 转成 0–1 |
| Python `list`、`tuple`、`dict`、`bytes` | 没有默认转换 | 显式建立 Godot 容器 |
| 普通 Python 对象、函数、绑定方法 | 没有默认转换 | 保留在 Python 中，或使用明确桥接 |

无法转换时常见日志为 `py_tovariant: no Variant counterpart for type '...'`，转换结果可能变成 Nil。它未必抛出可供你的 `try/except` 捕获的 Python 异常，因此应在调用前就选对类型。

### Array 和 Dictionary

```python
from godot import Array, Dictionary

items = Array()
items.append("sword")
items.append("potion")

data = Dictionary()
data["level"] = 3
data["items"] = items

print(data["level"], "items" in data, "potion" in items)
for i in range(items.size()):
    print(items[i])
```

绑定实现了按键读写及 `in`，但没有统一提供 Python 的 `__len__`、`__iter__` 协议。使用 `.size()` 和索引循环，不要把 `len(items)` 或 `for item in items` 当成所有 Godot 容器都支持的写法。

不能用 `Array([1, 2])` 代替转换；其中的 Python 列表仍需先跨越绑定。可以逐项追加：

```python
def to_godot_array(values):
    result = Array()
    for value in values:
        result.append(value)
    return result
```

此函数只处理元素本身可转换的一层列表。嵌套 Python 容器也要逐层转换。

### bytes 与 PackedByteArray

```python
from godot import PackedByteArray

def to_packed_bytes(data):
    result = PackedByteArray()
    for value in data:
        result.append(value)
    return result

def to_python_bytes(data):
    return bytes([data[i] for i in range(data.size())])
```

`FileAccess.get_buffer()` 和 `var_to_bytes()` 返回的是 `PackedByteArray`；`msgpack`、`lz4` 和 SBX LevelDB 使用的是 Python `bytes`。两者相接时需要上述转换。

## 向量等值类型的赋值

读取 `position.x` 没有问题，但不要假设能修改临时向量的分量：

```python
from godot import Vector2

position = self.owner.position
self.owner.position = Vector2(position.x + 10.0, position.y)
```

`Vector2`、`Vector3`、`Color` 等小型值在当前绑定中不支持直接属性赋值，所以 `self.owner.position.x = 10` 和 `position.x = 10` 都不是推荐用法。重建值并赋回宿主属性最明确。二元运算还应保持支持的方向，例如 `velocity * delta`；不要假设 `delta * velocity` 的反向协议已经实现。

## 资源与序列化

```python
from godot import load, Dictionary, var_to_bytes, bytes_to_var

scene = load("res://scenes/Enemy.tscn")
enemy = scene.instantiate()
# 在节点脚本里：self.owner.add_child(enemy)

state = Dictionary()
state["score"] = 42
encoded = var_to_bytes(state)
decoded = bytes_to_var(encoded)
print(decoded["score"])
```

场景资源必须实际存在。对于 Python 原生容器，使用 `json` 或 `msgpack` 更直接；不要把二者的数据格式与 Godot Variant 的序列化格式混用。

依据：[类型转换](../src/lang/Common.cpp)、[对象和类访问](../src/lang/BindingsHooks.cpp)、[辅助函数与运算符](../src/lang/Bindings.cpp)、[类型生成规则](../stubgen/map.py)。
