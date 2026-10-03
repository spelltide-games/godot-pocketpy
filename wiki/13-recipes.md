# 常用玩法与互操作配方

[返回目录](README.md) · [数据类型](04-godot-api.md)

以下例子均为使用说明，未在本次文档工作中运行。文件名、节点类型和所需资源都在各节中列出。

## CharacterBody2D 移动

准备一个 CharacterBody2D，添加 CollisionShape2D 并配置形状；在 Input Map 中添加 `move_left`、`move_right`、`move_up`、`move_down` 四个动作及按键。保存为 `res://scripts/TopDownPlayer.py`：

```python
from godot import Extends, export, Input
from godot.classes import CharacterBody2D


class TopDownPlayer(Extends(CharacterBody2D)):
    speed = export(float, default=220.0)

    def _physics_process(self, delta):
        direction = Input.get_vector(
            "move_left", "move_right", "move_up", "move_down"
        )
        self.owner.velocity = direction * self.speed
        self.owner.move_and_slide()
```

`velocity` 是每秒速度，`move_and_slide()` 使用物理步长处理运动；这里不再乘一次 `delta`。若只是手动移动 Node2D 的位置，则应使用 `position + velocity * delta`，两种 API 的计时方式不同。

## 生成 PackedScene

先创建可实例化的 `Enemy.tscn`，将其拖入生成器节点的 `enemy_scene` 属性。生成器脚本保存在 `res://scripts/EnemySpawner.py` 并挂到 Node2D：

```python
from godot import Extends, export, Vector2
from godot.classes import Node2D, PackedScene


class EnemySpawner(Extends(Node2D)):
    enemy_scene = export(PackedScene)

    def spawn(self):
        if self.enemy_scene is None:
            print("请先在 Inspector 设置 enemy_scene")
            return None
        enemy = self.enemy_scene.instantiate()
        enemy.position = Vector2(100, 80)
        self.owner.add_child(enemy)
        return enemy
```

这里假定 Enemy 场景的根节点有 `position` 属性，例如 Node2D。返回的是 Godot 节点，可作为跨语言方法返回值。释放时对该节点调用 `queue_free()`。

## 用 JSON 保存 Python 数据

将以下普通模块保存为 `res://site-packages/save_data.py`。它使用 `user://` 写入简单配置，不需要 SBX。

```python
import json
from godot.classes import FileAccess
from godot.constants import OK

SAVE_PATH = "user://player_save.json"

def save_score(score):
    file = FileAccess.open(SAVE_PATH, FileAccess.WRITE)
    if file is None:
        print("保存失败，错误码：", FileAccess.get_open_error())
        return False
    file.store_string(json.dumps({"version": 1, "score": score}))
    file.flush()
    write_error = file.get_error()
    file.close()
    if write_error != OK:
        print("写入失败，错误码：", write_error)
        return False
    return True

def load_score():
    if not FileAccess.file_exists(SAVE_PATH):
        return 0
    file = FileAccess.open(SAVE_PATH, FileAccess.READ)
    if file is None:
        print("读取失败，错误码：", FileAccess.get_open_error())
        return 0
    text = file.get_as_text()
    file.close()
    try:
        state = json.loads(text)
        if state.get("version") != 1:
            print("未知存档版本")
            return 0
        return int(state.get("score", 0))
    except Exception as exc:
        print("存档格式无效：", exc)
        return 0
```

调用例子放在节点脚本的方法中：

```python
from save_data import save_score, load_score

self.score = load_score()
save_score(self.score + 10)
```

这是小型示例：需要处理断电恢复、备份或多槽位时，在此基础上增加临时文件替换、备份和版本迁移策略。不要每个渲染帧都写磁盘。

## Python 调用 GDScript

给一个 Node 挂载以下 GDScript，并把它命名为 `Calculator`：

```gdscript
extends Node

func add_values(a: int, b: int) -> int:
    return a + b
```

在它的父节点的 Python 脚本中：

```python
def _ready(self):
    calculator = self.owner.get_node("Calculator")
    result = calculator.call("add_values", 2, 3)
    print(result)
```

GDScript 返回的基础类型会转成 Python 基础类型；返回 Godot Array 或 Dictionary 时，使用对应的 Godot 容器操作规则。

## GDScript 调用 Python

准备 `res://scripts/ScoreService.py`，挂到名为 `ScoreService` 的 Node：

```python
from godot import Extends, export
from godot.classes import Node


class ScoreService(Extends(Node)):
    score = export(int, default=0)

    def add_score(self, amount):
        self.score += amount
        return self.score
```

在父节点的 GDScript 中：

```gdscript
extends Node

func _ready():
    var service = get_node("ScoreService")
    var score = service.call("add_score", 5)
    print(score, service.get("score"))
```

Godot 属性桥接公开导出项和信号，不会自动公开所有普通 Python 字段。需要跨语言访问普通状态时提供方法，或声明明确的导出属性。

## 从 GDScript 求值 Python 表达式

扩展注册了 `PythonScript.eval(code)`：

```gdscript
var result = PythonScript.eval("1 + 2")
print(result)
```

这里只接受表达式，结果还需能转换为 Godot Variant。Python 原生列表、自定义对象等不会自动作为 Godot 对象返回；失败会记录 Python 错误并返回空 Variant。不要把用户输入直接拼接成待执行表达式。复杂业务优先通过明确的方法接口互调。

## 二进制数据在两个世界之间传递

```python
import msgpack
from godot import PackedByteArray
from godot.classes import FileAccess

payload = msgpack.dumps({"tick": 10})
buffer = PackedByteArray()
for value in payload:
    buffer.append(value)

file = FileAccess.open("user://snapshot.bin", FileAccess.WRITE)
if file is not None:
    file.store_buffer(buffer)
    file.close()
```

读取后的 `PackedByteArray` 可用 `bytes([buffer[i] for i in range(buffer.size())])` 转回 Python bytes，再交给 `msgpack.loads`。

依据：[现有示例](../demo/MyScript.py)、[方法调用桥](../src/lang/PythonScriptInstance.cpp)、[表达式求值入口](../src/lang/PythonScript.hpp)。
