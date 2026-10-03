# 示例项目与辅助工具

[返回目录](README.md) · [源码安装](17-build-and-stubs.md)

## 如何阅读 demo

`demo/` 是独立的 Godot 项目，`res://` 指向该目录。使用预编译包时解压到 `demo/`；自行编译的输出也默认放到它的 addons 中。不是每个演示场景都能在默认扩展中使用：SBX 场景需要另行启用 SBX。

demo 的项目功能标记包含 Godot 4.6 和 Mobile 渲染器，不应把它等同于扩展的最低 4.4 要求。在较旧引擎或不同渲染环境中使用时，先检查项目设置。

| 示例位置 | 展示内容 | 适合参考什么 |
| --- | --- | --- |
| [main.tscn](../demo/main.tscn) / [MyScript.py](../demo/MyScript.py) | 属性、信号、原生方法、普通模块、协程 | 核心 API 的组合用法 |
| [new_script.gd](../demo/new_script.gd) | GDScript 属性与信号接收 | 两种语言如何协作 |
| [site-packages/test.py](../demo/site-packages/test.py) | 普通函数模块 | 最小 import 布局 |
| [test_op_in.tscn](../demo/bugs/test_op_in.tscn) | Godot Array 的 `in` 操作 | 容器绑定 |
| [test_yield_signal_with_args.tscn](../demo/bugs/test_yield_signal_with_args.tscn) | 等待带参数信号 | 信号与协程恢复 |
| [test_leak_node.tscn](../demo/bugs/test_leak_node.tscn) | 节点创建销毁、Python 对象回收 | 区分节点生命周期与 GC |
| [ConstantsFixture.py](../demo/tests/ConstantsFixture.py) | 全局常量命名空间 | `godot.constants` 迁移 |
| [DebuggerFixture.py](../demo/tests/DebuggerFixture.py) / [dbgfixture.py](../demo/site-packages/dbgfixture.py) | 断点、函数调用、普通模块 | 跨文件调试 |
| [DebuggerExcFixture.py](../demo/tests/DebuggerExcFixture.py) | 异常链与局部变量 | 异常调试 |
| [space_test.tscn](../demo/sbx_extension/space_test.tscn) | 可选 SBX 世界、移动、砖块、台阶与软碰撞 | 完整物理可视化 |
| [sbx/playground.py](../demo/site-packages/sbx/playground.py) | SBX 场景背后的 Python 业务 | 瓦片模板、角色、查询和模拟组织 |

回归示例的注释和断言描述的是设计目标，不代表本次文档工作已执行这些测试。初学者优先使用[快速开始](02-quick-start.md)中的小场景。

## SBX Playground 操作导航

源码中的场景控制包括方向键 / WASD 移动、空格跳跃、F 切换软/硬碰撞、G 切换追踪状态；鼠标左键使用当前工具，右键移除，数字键和滚轮选择工具。

场景通过 GDScript UI 与 `PythonScript.eval` 连接 `sbx.playground`，并使用 SpaceDebugDraw 及 MultiMesh 显示模拟状态。`sbx` 是 **demo 普通模块**，原生 API 的包名是 `sbxcpp`；两者不是同一个模块。

可复用的空间规则见[SBX 空间手册](15-sbx-space.md)，不要将整个 playground 的全局实例结构当作每个项目必需的架构。

## `SlidingWindow`：固定样本窗口

以下辅助脚本位于 `addons/godot-pocketpy/sbx_extension/`，属于 GDScript 类。默认无 SBX 的 CI 包会移除该资源目录；如果只需要这些纯脚本工具，也可以有选择地携带它们及其依赖。

```gdscript
var window = SlidingWindow.new(60, 2)
window.append(16.4)
window.append(17.0)
print(window.avg())
print(window.stats())
print(window.text_stats())
```

| API | 说明 |
| --- | --- |
| `SlidingWindow.new(size, decimals=2)` | 保存最近 size 个样本；size 应为正数 |
| `append(value)` | 添加有限浮点数，超过容量移除最旧值 |
| `avg()` | 平均值，空窗口返回 0 |
| `stats()` | 返回 `avg`、`min`、`max` 的 Dictionary |
| `text_stats()` | 按 decimals 生成统计字符串 |

空窗口的最小/最大值仍是正/负无穷大，至少加入一个样本后再把 `stats()` 用于界面显示。

源码：[SlidingWindow.gd](../demo/addons/godot-pocketpy/sbx_extension/sliding_windows/SlidingWindow.gd)。

## `FrameRateWindow` 与 `TimeDeltaWindow`

二者继承 SlidingWindow。`tick()` 记录本次与上次调用的毫秒时间差：FrameRateWindow 转成 `1000 / delta_ms`，TimeDeltaWindow 直接保存毫秒差。

```gdscript
extends Node

var frame_rates = FrameRateWindow.new(60)
var frame_times = TimeDeltaWindow.new(60)

func _process(_delta):
    frame_rates.tick()
    frame_times.tick()
```

首次 tick 建立时间基准，不产生正常的差值样本。避免在同一毫秒内多次调用 FrameRateWindow.tick，否则 delta 为 0 会形成非法采样值。它计算的是瞬时帧率样本的平均值，与“总帧数 / 总时间”定义不完全相同。

源码：[FrameRateWindow.gd](../demo/addons/godot-pocketpy/sbx_extension/sliding_windows/FrameRateWindow.gd)、[TimeDeltaWindow.gd](../demo/addons/godot-pocketpy/sbx_extension/sliding_windows/TimeDeltaWindow.gd)。

## `FrameStabilizer`：输入帧缓冲消费节奏

适用于已从网络得到按顺序排列的输入帧，由缓冲长度调节消费速度。

```gdscript
extends Node

var stabilizer = FrameStabilizer.new(30)

func receive_frame(inputs: Dictionary):
    stabilizer.add_frame(inputs)

func _process(delta):
    var frame = stabilizer.get_frame(delta)
    if frame != null:
        print("消费输入帧：", frame)
```

`get_frame(delta)` 在还没到消费时机或没有数据时返回 null，否则返回一个 Dictionary。`delta` 为秒。控制器将当前速率限制在理想值约一半到一点五倍之间，并提供 `buf_size_wnd`、`err_wnd`、`fps_wnd` 统计窗口。

它每次最多返回一帧，不会自动补齐丢帧、排序帧编号、合并玩家输入或实现回滚。上游协议需要保证顺序与完整性，也需要自行管理缓冲上限。

源码：[FrameStabilizer.gd](../demo/addons/godot-pocketpy/sbx_extension/lockstep_go/FrameStabilizer.gd)。

## Immediate Gizmos：独立的绘制插件

位置：`demo/addons/immediate_gizmos/`。这是独立的 Godot 编辑器插件，有自己的 `plugin.cfg`，与无需手动启用的 Python GDExtension 不同。demo 已在项目设置中启用它。

使用其 GDScript 类 `ImmediateGizmos2D`、`ImmediateGizmos3D` 绘制即时辅助图形：

```gdscript
extends Node2D

func _process(_delta):
    ImmediateGizmos2D.reset()
    ImmediateGizmos2D.set_color(Color(0.2, 0.8, 1.0))
    ImmediateGizmos2D.line(Vector2.ZERO, Vector2(100, 0))
    ImmediateGizmos2D.line_circle(Vector2(100, 100), 20)
```

| 模块 | 主要接口 |
| --- | --- |
| 共享绘制状态 | `set_color`、`set_transform`、`set_required_selection`、`set_font`、`set_font_size`、`reset` |
| 2D 线形 | `line`、`line_strip`、`line_polygon`、`line_arc`、`line_circle`、`line_capsule`、`line_rect`、`line_square` |
| 3D 线形 | `line`、`line_strip`、`line_polygon`、`line_arc`、`line_circle`、`line_sphere`、`line_capsule`、`line_cuboid`、`line_cube` |
| 文字 | 两个维度均有 `draw_text`，支持对齐与尺寸相关参数 |
| 编辑器预览 | GDScript `@tool` 配合 `set_required_selection` 控制仅选中时绘制 |

2D 和 3D 的 transform 参数分别为 Transform2D 和 Transform3D。矩形/立方体尺寸参数应按实际函数实现理解，例如当前 2D `line_rect` 将传入 size 用作中心向两侧的偏移。

Python 根模块不会自动导出这些 GDScript 类；从 Python 使用时，可以在 GDScript 节点上包装成普通方法再调用。可参考插件的 [README](../demo/addons/immediate_gizmos/README.md)、[2D 源码](../demo/addons/immediate_gizmos/scripts/immediate_gizmos_2d.gd)、[3D 源码](../demo/addons/immediate_gizmos/scripts/immediate_gizmos_3d.gd)及[许可文件](../demo/addons/immediate_gizmos/LICENSE)。
