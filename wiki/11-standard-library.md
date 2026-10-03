# Python 标准库模块与兼容性

[返回目录](README.md) · [游戏与运行时模块](12-runtime-modules.md)

这里介绍当前 pocketpy 提供的 Python 常用模块。它们是精简实现，不代表 CPython 同名模块的完整接口。依据为本仓库的 pocketpy 源码与文档；主项目的编译开关会进一步影响可用性。

## 导入和使用环境

以下片段放在 `site-packages` 的普通模块或节点脚本的方法中使用。它们不需要 pip 安装。游戏中的文件读写使用 Godot FileAccess；默认构建关闭了 pocketpy 的 `os`、`io` 支持。

### 模块速查

表格里的示例是表达式或紧凑片段；先导入对应模块，再将片段放到合适的函数中。

| 模块 | 适用场景与主要接口 | 示例 / 关键区别 |
| --- | --- | --- |
| `builtins` | 基础类型、`print`、`range`、`len`、`sum`、`sorted`、`enumerate` 等 | `sum([1, 2, 3])`；`print` 输出到 Godot |
| `math` | 实数数学、三角函数、取整、角度换算 | `math.sqrt(25)`、`math.radians(90)`；不要假定取整返回类型与所有 CPython 版本一致 |
| `cmath` | 复数及其数学函数 | `cmath.sqrt(complex(-1, 0))` |
| `random` | 随机选择、区间采样、独立生成器、状态保存 | `random.Random(7).randint(1, 6)` |
| `bisect` | 有序列表查找与插入 | `bisect.insort_left(values, 10)`；列表需预先有序 |
| `heapq` | 最小堆、任务优先级 | `heapq.heappush(queue, (3, "task"))` |
| `collections` | `Counter`、`deque`、`defaultdict` | `collections.Counter(["a", "a", "b"])`；Counter 返回普通 dict |
| `functools` | `cache`、`lru_cache`、`reduce`、`partial` | `functools.reduce(lambda a, b: a + b, [1, 2, 3])` |
| `operator` | 运算函数、`itemgetter`、`attrgetter` | `operator.itemgetter("score")({"score": 10})` |
| `enum` | 命名枚举 `Enum` | 见下方状态枚举例子；不是全套 CPython enum API |
| `dataclasses` | 简单数据类、`asdict` | 见下方配置数据例子；不要假定支持 `field`、`frozen` 等完整参数集 |
| `typing` | 类型注解占位符、`TYPE_CHECKING`、`cast` 等 | `typing.cast(int, value)` 不做运行时验证 |
| `json` | 文本数据交换、配置、简单存档 | `json.dumps({"score": 10})`、`json.loads(text)` |
| `pickle` | pocketpy 对象图序列化 | `pickle.loads(pickle.dumps([1, 2]))`；不要认作 CPython pickle 格式 |
| `base64` | 二进制到文本编码 | `base64.b64encode(b"hello")`；编码返回 bytes |
| `time` | 时间戳、单调时钟、性能计时、当地时间 | `time.monotonic()`；不要在游戏主线程调用 `sleep` 等待动画 |
| `datetime` | 简单日期、日期时间和时间差 | `from datetime import date` 后 `date.today()` |
| `unicodedata` | 东亚字符宽度类别 | `unicodedata.east_asian_width("中")`；不是完整 Unicode 数据库接口 |
| `sys` | 解释器版本、平台、递归深度 | `sys.version`、`sys.platform`、`sys.getrecursionlimit()` |
| `traceback` | 格式化当前处理的异常 | 在 `except` 中使用 `traceback.print_exc()` |
| `inspect` | 生成器函数判断、用户类型判断、函数签名 | `inspect.isgeneratorfunction(task)`、`inspect.signature(func)` |
| `dis` | 查看 pocketpy 字节码 | `dis.dis(func)`，用于深入排查脚本行为 |
| `importlib` | 已导入模块的重载入口 | 当前集成的 `reload` 路径有源码级风险，见[模块页](07-modules.md)；重启游戏替代 |
| `long_v1` | Python 源码实现的大整数辅助类型 | `from long_v1 import long` 后 `long("123456789012345678901")` |

另外的 `vmath`、`array2d`、`easing`、`lz4`、`msgpack`、`gc`、`pkpy`、`stdc`、`picoterm` 在[运行时模块页](12-runtime-modules.md)逐项介绍。

## 数学和可复现的随机序列

```python
import math
import random

direction_radians = math.radians(45)
print(math.cos(direction_radians))

rng = random.Random(123)
saved_state = rng.getstate()
first_roll = rng.randint(1, 6)
rng.setstate(saved_state)
same_roll = rng.randint(1, 6)
print(first_roll, same_roll)
```

同一实现、相同状态和相同调用顺序有助于复现随机逻辑。联网同步仍需要统一模拟步长、输入顺序和版本；设置种子本身不是跨所有平台完全一致的证明。`random.getstate()` 返回 Python bytes。

## 业务数据与容器

```python
from dataclasses import dataclass, asdict
from enum import Enum
from collections import Counter, deque, defaultdict

class Phase(Enum):
    WAITING = 0
    PLAYING = 1

@dataclass
class PlayerInfo:
    name: str
    score: int = 0

player = PlayerInfo("小蓝", 10)
print(asdict(player), Phase.PLAYING.name)

counts = Counter(["wood", "wood", "stone"])
pending = deque()
pending.append("start")
print(pending.popleft(), counts["wood"])

groups = defaultdict(list)
groups["allies"].append(player)
```

这些数据留在 Python 侧。要作为 Godot 方法参数、信号参数或返回值传出去，先转换为 Godot 容器或使用序列化文本。`dataclasses.asdict` 也不会自动变成 Godot Dictionary。

## 排序、优先队列与缓存

```python
import bisect
import heapq
from functools import lru_cache
from operator import itemgetter

levels = [1, 4, 8]
bisect.insort_left(levels, 6)
print(levels)

queue = []
heapq.heappush(queue, (2, "save"))
heapq.heappush(queue, (1, "spawn"))
print(heapq.heappop(queue))

rows = [{"score": 2}, {"score": 1}]
print(sorted(rows, key=itemgetter("score")))

@lru_cache(maxsize=32)
def square(value):
    return value * value
```

缓存适合参数可作键、结果由参数决定的函数。不要给依赖场景当前状态的查询套缓存，然后期望节点变化自动使缓存失效。

## JSON、Base64 与 pickle

```python
import json
import base64
import pickle

text = json.dumps({"name": "hero", "score": 25}, indent=2)
state = json.loads(text)

encoded = base64.b64encode(b"payload")
raw = base64.b64decode(encoded)

snapshot = pickle.dumps([state["score"], "checkpoint"])
restored = pickle.loads(snapshot)
```

JSON 适合可读数据，不原生表示 Godot 节点、向量或字节串。pickle 可以保存更多 pocketpy 对象，但模块顶层类和函数需要在恢复环境中仍可找到；更换解释器或业务类定义时，应自行安排存档版本和迁移。不要对未经信任的数据使用对象恢复型格式。

`json` 的公开简化接口为 `loads(s)` 与 `dumps(obj, indent=0)`；不要直接套用 CPython 的所有可选参数或 `load(file)` / `dump(file)` 工作流。

## 时间与异常

```python
import time
import traceback
from datetime import datetime

start = time.perf_counter()
print(datetime.now())
print("耗时：", time.perf_counter() - start)

try:
    value = int("not a number")
except ValueError:
    traceback.print_exc()
    message = traceback.format_exc()
```

`traceback.format_exc()` 在没有正在处理的异常时返回 `None`。游戏延时请用 Timer 信号与[协程](06-coroutines.md)，而不是阻塞式 `time.sleep`。

## 反射与类型注解

```python
import inspect
import sys
import dis
from typing import TYPE_CHECKING

def add(a, b=1):
    return a + b

print(sys.version, sys.platform)
print(inspect.signature(add))
print(inspect.isgeneratorfunction(add))
# 需要研究字节码时，读者可自行调用 dis.dis(add)。

if TYPE_CHECKING:
    from godot.classes import Node2D
```

`TYPE_CHECKING` 在运行时为 False。类型注解主要帮助静态工具；`typing.cast` 返回原值，Godot 对象需要运行时类型检查时使用 `isinstance` 或 `godot.as_`。

## 与 CPython 的重要区别

依据随仓库的[差异说明](../pocketpy/docs/features/differences.md)，需特别留意：

- 内置 `int` 是 64 位；`bool` 不继承 `int`。
- 不支持 CPython 的二进制扩展生态。
- 不提供通用描述符协议、`__slots__`、多重继承和 `__del__`；`property` 有专门支持。
- 不应依赖完整的 `try ... else/finally` 语义；本手册例子用 `try/except` 和显式资源清理。
- 不应依赖所有原地运算特殊方法或 CPython 的每一个字符串、标准库边角行为。
- 带星号解包赋值的星号项应放在最后，例如 `head, *tail = values`。

编辑器为某些 Python 关键字着色，不表示解释器实现了对应全部语义。移植现成 Python 代码时，先从实际使用的语法、模块和调用签名逐项确认。

进一步参考：[模块文档目录](../pocketpy/docs/modules/)、[Python 实现的模块](../pocketpy/python/)、[原生模块](../pocketpy/src/modules/)、[解释器模块注册](../pocketpy/src/interpreter/vm.c)。
