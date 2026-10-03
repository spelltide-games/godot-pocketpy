# 脚本、实例与生命周期

[返回目录](README.md) · [Godot API](04-godot-api.md)

## 一个脚本文件对应一个挂载入口

```python
# res://scripts/Actor.py
from godot import Extends, export
from godot.classes import Node2D


class Actor(Extends(Node2D)):
    speed = export(float, default=120.0)

    def __init__(self):
        self.hp = 100

    def _ready(self):
        print(self.owner.get_path(), self.speed)
```

每个可挂载 `.py` 文件需要有与文件名一致的入口类，并通过 `Extends` 继承脚本实例基类。跨目录放置同名脚本也会冲突，例如 `actors/Actor.py` 和 `ui/Actor.py` 不应同时存在。

推荐把这些文件统一放在 `res://scripts/`。其他 Godot 可扫描目录也能承载脚本，但当前脚本索引生成器只扫描 `scripts/`。普通工具函数和辅助类放到 `site-packages/`，不要把普通模块当成挂载脚本。

Python 脚本不能内嵌在 `.tscn` 中，必须保存为独立 `.py` 文件。

## `self`、`self.owner` 与 `node.script`

| 写法 | 含义 | 例子 |
| --- | --- | --- |
| `self` | Python 脚本实例 | `self.hp`、`self.take_damage(10)` |
| `self.owner` | 挂载脚本的 Godot 对象 | `self.owner.position`、`self.owner.get_tree()` |
| `node.script` | Python 绑定提供的脚本实例入口 | `node.script.take_damage(10)` |
| `node.get_script()` | Godot Script 资源 | 检查、持有或加载脚本资源 |

`self.owner` 不是 GDScript 中用于场景归属的 `Node.owner` 属性。如果确实需要后者，使用 `self.owner.owner`。

在 Python 中，若对象没有挂载 PythonScript，特殊属性 `node.script` 返回 `None`。这个特殊含义不适用于 GDScript；GDScript 应通过 `call`、导出属性或信号访问 Python 脚本。

```python
target = self.owner.get_node("../Enemy")
if target.script is not None:
    target.script.take_damage(10)
```

## 初始化顺序

当前实现先创建 Python 实例，设置 `owner`、脚本声明的默认值和信号，然后调用无参 `__init__()`。Godot 场景保存的属性覆盖值在实例构造之后应用。

因此，“声明默认值”和“Inspector 保存值”是两回事。`export(int, default=10)` 的默认值会在构造阶段写入，但你不应在 `__init__` 依赖场景中配置的最终值；没有提供默认值的导出项可能仍是 `None`。

| 阶段 | 推荐用途 |
| --- | --- |
| 模块顶层 / 类声明 | 导入、常量和声明；避免依赖运行中的场景 |
| `__init__(self)` | 初始化每个实例自己的 Python 字段、列表和字典 |
| `_enter_tree(self)` | 节点进入场景树时的工作 |
| `_ready(self)` | 获取子节点、读取导出值、连接信号 |
| `_process(self, delta)` | 每个渲染帧的逻辑，`delta` 单位为秒 |
| `_physics_process(self, delta)` | 物理帧的移动或模拟 |
| `_input(self, event)` / `_unhandled_input(self, event)` | 输入事件 |
| `_exit_tree(self)` | 断开外部依赖、取消任务和关闭会话 |

引擎调用的这些脚本方法使用 Godot 对应回调名。不要把内部通知桥接视为完整实现：当前 `notification_func` 为空，不能依赖自定义 `_notification` 获得与 GDScript 相同的通知分发。

## Python 脚本继承

```python
# res://scripts/FastActor.py
from godot import Extends


class FastActor(Extends("res://scripts/Actor.py")):
    def _ready(self):
        super()._ready()
        print("基础速度：", self.speed)
```

参数是已有 Python 脚本的资源路径。父脚本必须可加载且有效。继承链中的导出项和信号会被收集；不要在子类重复声明同名导出属性或信号，否则会出现 `Duplicate member`。

该机制不等于完整的 Godot 脚本继承反射：当前 `_get_base_script`、`_inherits_script` 等接口没有完整提供继承信息，编辑器也不提供对应的“从文件继承”能力。使用上面的显式路径方式，不依赖继承向导。

## 创建另一个脚本节点

`godot.scripts` 按类名暴露已经成功加载的运行时脚本类。创建实例前先保证脚本资源已经加载。

```python
# 某个节点脚本的方法内
def spawn_actor(self):
    from godot import load

    actor_script = load("res://scripts/Actor.py")
    from godot.scripts import Actor

    actor_node = Actor()
    self.owner.add_child(actor_node)
    actor_node.script.hp = 50
```

`Actor()` 返回挂载好脚本的 **Godot 节点**，并非一个脱离节点的 Python 业务对象。该构造入口当前要求可实例化的 Node 类型，不应拿它创建 Resource 脚本。没有构造参数传递协议；需要参数时，在实例创建后调用初始化方法或设置属性。`add_child` 可能立即触发 `_ready`，需要在 `_ready` 前设置的状态应提前赋值。

创建已保存的场景通常更适合游戏对象，见[实用配方](13-recipes.md)。只为获得类型补全而生成 `scripts.pyi` 不会自动预加载所有运行时脚本。

## 对象寿命

Godot 节点遵循场景树的生命周期。移出树不等于释放，需要销毁时调用 `node.queue_free()`。持有 Python 变量不会让已被 Godot 释放的节点重新有效。

脚本解绑或宿主释放时，扩展会移除脚本实例的 GC 根并清理协程。业务代码仍应在 `_exit_tree` 中停止与场景相关的任务，不要在节点销毁后继续调用旧包装对象。

普通 Python 对象由 pocketpy 的 GC 管理；不要用 `__del__` 代替明确的网络 `dispose()`、数据库 `close()` 或 Godot 节点释放。

## 编辑器执行边界

当前没有 Python 版 `@tool`。但为提取脚本类、导出值和信号，编辑器加载可挂载脚本时会执行其顶层代码；它导入的普通模块也可能因此执行。把游戏启动操作放进 `_ready`，避免在文件顶层建立连接、修改存档或创建运行时场景。

单纯从 Python 面板打开普通模块只读写源码，不会因为“打开文档”而执行它。详见[模块管理](07-modules.md)。

依据：[脚本资源实现](../src/lang/PythonScript.cpp)、[实例桥接](../src/lang/PythonScriptInstance.cpp)、[脚本构造与辅助函数](../src/lang/Bindings.cpp)。
