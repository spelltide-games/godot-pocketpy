# 游戏数学、网格、压缩与运行时模块

[返回目录](README.md) · [标准库](11-standard-library.md)

## `vmath`：轻量数学值

`vmath` 提供 `vec2`、`vec3`、`vec2i`、`vec3i`、`vec4i`、`mat3x3`、`color32` 等。适合 Python 侧批量计算，也是 SBX 空间 API 要求的向量类型。

```python
from vmath import vec2, vec3, vec2i, rgb

position = vec3(1, 2, 3)
velocity = vec3(2, 0, 0)
position = position + velocity * 0.5
print(position.x, velocity.length())

point = vec2(2, 3).with_x(5)
cell = vec2i(2, 4)
tint = rgb(255, 128, 0)
```

浮点向量提供 `dot`、`length`、`length_squared`、`normalize` 等；二维向量还包括 `rotate`、`angle` 和 `smooth_damp`。分量采用只读值接口，用 `with_x` 等方法或重新构造值进行修改。

`vmath.vec3` 和 `godot.Vector3` 并不是同一种对象。Godot 绑定可接收若干 vmath 值，但 SBX 接口直接检查 `vec3` 等 pocketpy 类型，不能把 Godot Vector3 随意传进去。需要时明确转换：

```python
from vmath import vec3

godot_position = self.owner.position  # 适用于 Node3D 脚本
physics_position = vec3(godot_position.x, godot_position.y, godot_position.z)
```

完整签名：[vmath.pyi](../pocketpy/include/typings/vmath.pyi)。

## `array2d`：二维数组与视图

```python
from array2d import array2d

grid = array2d(8, 6, 0)  # 列数、行数、默认值
grid[2, 3] = 1          # x/列 在前，y/行 在后
print(grid.width, grid.height, grid.get(99, 99, -1))

view = grid[1:4, 2:5]
copy = view.copy()
print(grid.count(1), grid.tolist())

for position, value in grid:
    if value == 1:
        print(position, value)
```

`array2d_view` 是共享底层数组的视图；需要独立数据时用 `copy()`。`tolist()` 转成 Python 二维列表。`get` 支持越界默认值，直接索引不要依赖这种默认行为。

| 功能 | API |
| --- | --- |
| 尺寸与位置 | `n_cols`、`n_rows`、`width`、`height`、`shape`、`numel`、`is_valid` |
| 内容处理 | `map`、`apply`、`zip_with`、`copy`、`tolist` |
| 检查与范围 | `count`、`all`、`any`、`index`、`get_bounding_rect` |
| 网格算法 | `count_neighbors`、`get_connected_components`、`convolve` |
| 显示 | `render`、`render_with_color` |

邻域使用精确字符串 `"Moore"` 或 `"von Neumann"`。例如：

```python
labels, component_count = grid.get_connected_components(1, "von Neumann")
```

### 分块数组 `chunked_array2d`

```python
from array2d import chunked_array2d
from vmath import vec2i

world = chunked_array2d(16, 0, True)
world[vec2i(20, 3)] = 7
chunk_pos, local_pos = world.world_to_chunk(vec2i(20, 3))
print(chunk_pos, local_pos)
```

第三个参数控制访问时是否自动添加 chunk。支持 `add_chunk`、`remove_chunk`、`move_chunk`、`get_context` 和 `view_chunk` 等视图接口。它是通用存储结构；SBX 的 `Tilemap` 另有八层瓦片和环绕规则。

完整签名：[array2d.pyi](../pocketpy/include/typings/array2d.pyi)。

## `easing`：缓动曲线

```python
import easing

t = 0.5
weight = easing.InOutSine(t)
value = 10.0 + (100.0 - 10.0) * weight
```

函数接受进度并返回缓动后的权重。常用族有 `Linear`，以及 `In` / `Out` / `InOut` 前缀配合 `Sine`、`Quad`、`Cubic`、`Quart`、`Quint`、`Expo`、`Circ`、`Back`、`Elastic`、`Bounce`。通常由调用方把普通进度控制在 0–1；Back/Elastic 等曲线可能产生超出端点的权重。

它只计算曲线，不负责启动动画、累计时间或更新节点。每帧进度由 `_process(delta)` 或 Godot Tween 等工作流管理。

## `lz4`：二进制压缩

当前主项目默认启用。

```python
import lz4

payload = b"tile data tile data tile data"
compressed = lz4.compress(payload)
restored = lz4.decompress(compressed)
```

接口为 `compress(bytes) -> bytes`、`decompress(bytes) -> bytes`，使用 LZ4 block 形式，不是通用的 `.lz4` frame 文件读写器。跨语言交换时需约定格式，不要只按扩展名判断兼容性。

## `msgpack`：紧凑数据编码

当前主项目默认启用。

```python
import msgpack
import lz4

record = {"tick": 12, "inputs": [1, 0], "blob": b"abc"}
encoded = msgpack.dumps(record)
decoded = msgpack.loads(encoded)
packed_snapshot = lz4.compress(encoded)
```

核心接口是 `dumps(object) -> bytes` 与 `loads(bytes)`。支持 `None`、bool、int、float、str、bytes、list、dict；字典键按当前实现使用 str 或 int。不要直接假定 tuple、set 或自定义类有编码协议。

只有启用 SBX 后才会安装 Godot Variant 的 ext 类型回调。默认版中出现 `Vector3` 等 Godot 对象时应先转换成普通数据；详见[SBX 序列化](14-sbx-storage-network.md)。

## `gc`：Python 垃圾回收

```python
import gc

print(gc.isenabled())
unreachable = gc.collect()
print("本次回收计数：", unreachable)
```

`enable` / `disable` 控制自动回收，`collect` 请求完整回收，`collect_hint` 提供适合帧末调度的提示入口；`setup_debug_callback` 可观测回收过程。

`track` / `untrack` / `is_tracked` 面向理解对象引用图的高级用途。通常保留自动 GC 即可。GC 不能代替节点 `queue_free`，也不能代替外部资源的显式关闭。

## `pkpy`：解释器状态与性能工具

```python
import pkpy

print(pkpy.currentvm())
print(pkpy.memory_usage())
print(pkpy.memory_usage_info())
print(pkpy.configmacros)
```

| 入口 | 用途 | 当前项目条件 |
| --- | --- | --- |
| `memory_usage`、`memory_usage_info` | pocketpy 内存信息 | 不代表 Godot 的全部内存 |
| `currentvm` | 当前 VM 索引 | 普通用户不需要手动切换 VM |
| `configmacros` | 查看当前编译功能信息 | 比仅观察 stubs 更能说明构建能力 |
| `profiler_begin/end/reset/report` | pocketpy 行级性能数据 | 不与 Python 调试器同时启用 |
| `TValue` | 供高级桥接使用的值包装 | 不是 Godot Variant 自动转换的通用替代 |
| `watchdog_begin/end` | 超时看门狗 | 主项目默认关闭 `PK_ENABLE_WATCHDOG` |
| `ComputeThread` | pocketpy 计算线程 | 主项目默认关闭 `PK_ENABLE_THREADS` |

性能采样形式如下，需读者在关闭 Python 调试器的独立运行会话中按需使用：

```python
import pkpy

pkpy.profiler_reset()
pkpy.profiler_begin()
total = sum(range(1000))
pkpy.profiler_end()
report = pkpy.profiler_report()
print(report)
```

## `stdc` 与 `picoterm`

`stdc` 提供 C 风格内存类型与底层内存操作，用于已有原生接口的对接。普通玩法通常不需要它。

```python
import stdc
print(stdc.sizeof(stdc.Int))
```

类型包括整数、浮点、Bool、Pointer 等包装，另有 `addressof`、`malloc/free`、`memcpy` 和读写缓冲区的函数。裸指针与缓冲区大小由调用方负责，不具备 Python 容器的边界保护。接口详见 [stdc.pyi](../pocketpy/include/typings/stdc.pyi)。

`picoterm` 提供文本列宽、ANSI 字符串拆分和格式扫描辅助：

```python
import picoterm
print(picoterm.wcswidth("生命值"))
```

`enable_full_buffering_mode` 面向终端缓冲；Godot 的 `print` 已接管输出，不应把它当成游戏 UI 绘制工具。

## 有源码或 stubs，但默认不可用的模块

| 模块 / 能力 | 当前状态 | 用户替代方案 |
| --- | --- | --- |
| `os`、`io` 及其文件接口 | `PK_ENABLE_OS=OFF` | Godot FileAccess、DirAccess、OS |
| `conio` | 依赖桌面平台和 OS 开关，当前默认不注册 | Godot Input 和界面控件 |
| `colorcvt` | VM 中注册调用被注释 | 使用可用的 Godot Color 方法，或按需求实现转换 |
| `cute_png` | `PK_BUILD_MODULE_CUTE_PNG=OFF` | Godot Image / 纹理资源 |
| `periphery` | 默认构建未启用，当前 VM 初始化也未调用注册入口 | 自定义原生集成后再确认能力 |
| 动态库 Python 模块加载 | `PK_ENABLE_DLL=OFF` | 将模块整合进扩展后重新编译 |

安装包复制了上游的整个 typings 目录，因此其中可能出现这些名字。**有补全不等于可导入**。主项目配置依据：[CMakeLists.txt](../CMakeLists.txt)，实际模块注册依据：[vm.c](../pocketpy/src/interpreter/vm.c)。
