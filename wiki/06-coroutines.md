# 协程：等待信号与管理任务

[返回目录](README.md) · [普通模块](07-modules.md)

本项目用 Python 生成器实现协作式任务。入口为脚本实例的 `start_coroutine`，等待点使用 `yield`；不要使用 `async def` / `await` 来套用 CPython 的 asyncio 工作流。

## 最小例子

```python
# res://scripts/Countdown.py，挂到 Node
from godot import Extends
from godot.classes import Node


class Countdown(Extends(Node)):
    def _ready(self):
        self.task_id = self.start_coroutine(self.countdown())

    def countdown(self):
        for number in (3, 2, 1):
            print(number)
            yield self.owner.get_tree().create_timer(1.0).timeout
        print("开始")

    def _exit_tree(self):
        self.stop_all_coroutines()
```

`start_coroutine` 会立即推进生成器到第一个 `yield`。预期先输出 3，再在每次定时器超时后继续；它不会自动把整段函数放到另一个线程。

## API 参考

| 调用 | 参数 | 返回值 / 行为 |
| --- | --- | --- |
| `self.start_coroutine(generator)` | 已调用生成器函数得到的生成器对象 | 返回本脚本实例内的任务 ID |
| `self.stop_coroutine(id)` | 任务 ID | 找到并移除任务时返回 `True`，否则 `False` |
| `self.stop_all_coroutines()` | 无 | 移除当前脚本实例的所有任务，返回 `None` |

ID 只应由创建该任务的脚本实例使用，不代表全局任务句柄。即使生成器立即结束，启动调用仍可返回 ID，因此 ID 存在不等于任务仍在执行。

## 能等待什么

```python
# 等待一个 Godot 定时器
yield self.owner.get_tree().create_timer(0.5).timeout

# 等待下一次 process_frame 信号
yield self.owner.get_tree().process_frame

# 等待按钮按下
yield self.owner.get_node("Button").pressed
```

每次 `yield` 的值必须是 `godot.Signal`。`yield 1.0` 不表示等待一秒，`yield None` 不表示等待一帧。错误值会报告 `coroutine yielded value must be 'godot.Signal'`。

### 嵌套生成器

```python
def wait_seconds(self, seconds):
    yield self.owner.get_tree().create_timer(seconds).timeout

def sequence(self):
    print("准备")
    yield from self.wait_seconds(0.5)
    print("执行")
    yield from self.wait_seconds(1.0)
    print("结束")
```

使用 `yield from` 委托给另一个生成器；直接 `yield self.wait_seconds(...)` 会把生成器对象交给等待器而导致类型错误。

## 带参数信号与多个等待者

可以等待带参数信号，也可以让多个协程同时等待同一个信号。当前恢复逻辑只调用生成器的下一步，**不会把信号参数送回生成器**。

因此不要依赖 `value = yield some_signal` 取得发射参数。要保留参数，可以在发射前保存状态：

```python
# 这些成员放在同一个脚本类中，顶部需导入 signal
received = signal("value")

def publish_value(self, value):
    self.last_value = value
    self.received.emit(value)

def wait_value(self):
    yield self.received
    print("收到：", self.last_value)
```

如果信号由外部对象发出，先用普通回调保存参数，再从自己的逻辑继续处理，避免依赖多个连接之间未经约定的执行顺序。

## 取消、异常与清理

取消任务会从脚本的任务表中移除生成器。等待中的一次性信号连接不会因此成为新的任务；之后触发时找不到 ID，就不会推进生成器。

停止协程不等于停止对应 Timer，也不等于执行生成器的清理代码。必须释放的资源放到明确的清理方法中，由 `_exit_tree` 等调用。生成器完成或抛出异常后会被移除；错误会写入 Godot 输出。

协程恢复要求主线程。不要从工作线程触发用来恢复这些协程的信号，也不要用 `time.sleep()` 模拟等待；后者会阻塞当前执行线程。

依据：[协程实现](../src/lang/Bindings.cpp)、[带参数信号示例](../demo/bugs/TestYieldSignalWithArgs.py)。
