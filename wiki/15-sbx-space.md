# 可选 SBX：环形地图、碰撞与查询

[返回目录](README.md) · [SBX 安装前提](14-sbx-storage-network.md) · [演示导航](16-demo-tools.md)

`sbxcpp.space` 提供面向瓦片世界的三维碰撞模拟。它使用轴对齐盒子的组合形状，游戏代码设定运动速度；不会替你提供通用刚体的力矩、旋转、质量和反弹模拟。它与 Godot 自带物理世界相互独立，节点显示位置需要由业务同步。

接口使用 `vmath.vec3`、`vec2i` 等类型，不能直接用 `godot.Vector3` 替代。以下内容依据本地可选 SBX 源码，未执行物理示例。

## 1. 核心对象

| 对象 | 用途 |
| --- | --- |
| `Tile` | 一格地图中的 4 个砖层或全部 8 个层位 |
| `Tilemap` | 地图存储、环绕坐标和共享切片 |
| `Space` | 地图、普通物体、碰撞层矩阵、模拟与查询 |
| `BodyID` | 物体句柄；包含 `type`、`id`、`is_valid` |
| `BodyFlags` | 创建时固定的物体行为标志 |
| `BroadPhaseFlags` | 查询时是否包含瓦片、普通物体 |
| `SpaceDebugDraw` | Godot 场景中的调试可视化节点 |

`BodyType` 在类型提示里是整数 Literal，不是可导入的运行时枚举类。物体类型值为：0 = TILE、1 = STATIC、2 = KINEMATIC。

## 2. 最小运动世界

以下普通模块可保存为 `res://site-packages/simple_world.py`：

```python
from sbxcpp.space import Space, Tilemap, BodyFlags
from vmath import vec3

class SimpleWorld:
    def __init__(self):
        self.tilemap = Tilemap(30, 30)
        self.space = Space(self.tilemap, 10, self)
        self.space.gravity = vec3(0, -9.81, 0)

        self.floor = self.space.create_body(1, vec3(4, 0.5, 4), 0)
        self.space.teleport_body(self.floor, vec3(15, -0.5, 15))

        flags = BodyFlags.MONITORING
        self.actor = self.space.create_body(2, vec3(0.3, 0.5, 0.3), flags)
        self.space.teleport_body(self.actor, vec3(15, 2, 15))
        self.space.body_set_gravity_scale(self.actor, 1.0)

    def step(self):
        self.space.step(1.0 / 30.0)
        return self.space.body_get_position(self.actor)

    def on_pair_add(self, a, b, xzl, normal, max_sep):
        print("开始接触：", a, b, xzl)

    def on_pair_remove(self, a, b, xzl):
        print("结束接触：", a, b, xzl)
```

这是物理数据，不会自行创建可见网格。你可以按固定 30Hz 调用 `step`，把返回位置赋给 Godot Node3D 的 position，或使用本页的调试绘制。

### 尺寸与步长约束

- `chunk_size` 必须为正，且整除地图宽和高。
- 两个水平轴都至少有 3 个 chunk，例子中的 30/10 正好为 3。
- 普通物体整体包围盒的 X、Z **全尺寸**不能超过一个 chunk；Y 高度不受这条限制。
- `step(delta)` 要求正的步长，且不得小于 `1/60` 秒。可采用 20–60Hz 的固定模拟步长。
- 不要把每个渲染帧未经处理的 delta 直接传进去；高刷新率会造成步长小于限制。

例如，在 Godot `_process` 内用时间累加器调度，并由业务安排卡顿时的补帧上限：

```python
# __init__ 中：self.accumulated = 0.0
# _ready 中：self.world = SimpleWorld()
def _process(self, delta):
    self.accumulated += delta
    fixed_delta = 1.0 / 30.0
    while self.accumulated >= fixed_delta:
        self.world.step()
        self.accumulated -= fixed_delta
```

## 3. 形状、运动和标志

`create_body(type, aabbs, flags)` 的形状参数可以是一个盒子的 **半尺寸**，或 1–4 个 `(offset, extent)` 元组的列表。

```python
# 一个完整宽度为 0.6、高度为 1.8、深度为 0.6 的物体
body = space.create_body(2, vec3(0.3, 0.9, 0.3), BodyFlags.CAN_STEP)

# 两个盒子组成的形状；space 是已有的 Space
shape = [
    (vec3(0, 0, 0), vec3(0.3, 0.4, 0.3)),
    (vec3(0, 0.5, 0), vec3(0.2, 0.1, 0.2)),
]
compound = space.create_body(2, shape, 0)
```

`offset` 是盒子中心相对物体位置的偏移，`extent` 是盒子沿三个轴的半尺寸。体素盒的并集参与碰撞，没有旋转形状接口。

| 标志 | 值 | 行为 |
| --- | --- | --- |
| `TRIGGER` | 1 | 不阻挡也不被阻挡，仍可被查询并参与接触事件 |
| `CAN_STEP` | 2 | KINEMATIC 自动上下台阶，单级高度关联砖高的四分之一 |
| `MONITORING` | 4 | KINEMATIC 在步末主动搜索接触对并报告事件 |
| `SOFT` | 8 | 两个 SOFT 物体不硬性阻挡彼此，重叠时在水平面推开 |

按位或组合，例如 `BodyFlags.CAN_STEP | BodyFlags.SOFT`。flags 创建时决定，之后没有 setter；要改变可碰撞关系，使用 layer 和 mask，或按业务流程重建物体。

所有物体初始 gravity scale 为 0；仅设置 `space.gravity` 并不会使物体下落。要启用重力调用 `body_set_gravity_scale(body, 1.0)`。

KINEMATIC 依次沿轴扫掠并沿墙滑动，被阻挡轴的速度归零。`instant_velocity` 仅加在下一步，与长期 velocity 相加。STATIC 可以手动传送，但不由速度作为一般运动体推进。

### 物体方法速查

| 方法 | 用途 |
| --- | --- |
| `create_body(type, aabbs, flags)` | 创建 STATIC / KINEMATIC；TILE 使用另一入口 |
| `destroy_body(bid)` | 销毁物体；之后不要继续使用旧句柄 |
| `teleport_body(bid, position)` | 设置世界位置并环绕；TILE 不允许移动 |
| `body_get_layer` / `body_set_layer` | 物体所属碰撞层，0–31 |
| `body_get_flags` | 查询固定标志 |
| `body_get_velocity` / `body_set_velocity` | 长期速度 |
| `body_get_instant_velocity` / `body_set_instant_velocity` | 下一步的一次性速度 |
| `body_get_gravity_scale` / `body_set_gravity_scale` | 重力缩放 |
| `body_get_position` | 当前世界位置 |
| `body_get_aabb_extent` | 整体包围盒半尺寸 |
| `body_get_chunk_index` / `body_get_chunk_pos` | 所属 chunk |

`BodyID.is_valid` 是句柄自身的有效标记，不能替代所有物体寿命管理；持有一个旧 ID 不代表其物体永远存在。

## 4. 碰撞层和回调

Space 有 32 个层，默认相互碰撞。`set_layer_mask(layer, mask)` 会对称更新关系：修改层 1 对层 2 的关系，也改变层 2 对层 1 的关系。

```python
space.body_set_layer(body, 1)
space.set_layer_mask(1, (1 << 0) | (1 << 2))
print(space.get_layer_mask(1))
```

创建 Space 的 callbacks 对象应实现：

```python
def on_pair_add(self, a, b, xzl, normal, max_sep):
    pass

def on_pair_remove(self, a, b, xzl):
    pass
```

每个 step 先完成所有移动，再搜索接触并回调。只有带 MONITORING 的 KINEMATIC 主动搜索；两边都监视时，同一对仍只报告一次。回调中的 `a` 是报告该接触的监视者。

如果 `b` 是瓦片，`xzl` 为 `vec3i(x, z, layer)`；如果是普通物体，则为 `vec3i(-1, -1, -1)`。事件基于带有容差的接触距离，不能把所有 ADDED 都理解为几何体已深度穿插。

不要在碰撞回调中再次 step，也不要直接在受保护的回调期间销毁物体或瓦片。可以把待处理操作加入 Python 列表，等当前 step 返回后统一执行。

## 5. Tile 和八层地图

`Tilemap(width, height)` 的平面轴是 X、Z；API 参数名称里的 `y` 对应地图第二个轴，不是空间竖直 Y。

```python
from sbxcpp.space import Tile, Tilemap

tilemap = Tilemap(30, 30)
tilemap.set(2, 3, 0, 1)
cell = tilemap.get_tile(2, 3)
print(cell[0], cell.top_layer(), cell.layer_height(0))

tilemap.set_tile(2, 3, Tile(1, 1, 0, 0))
new_cell = tilemap.get_tile(2, 3).with_l(4, 2)
tilemap.set_tile(2, 3, new_cell)
```

`Tile` 构造接受 4 个砖层值，或全部 8 个值。`with_l` 返回改变指定层后的 Tile。`top_layer` 只看砖层，全部为空时返回 -1。

| 常量 | 值 / 用途 |
| --- | --- |
| `TILE_BRICK_LAYERS` | 4，层 0–3 为砖 |
| `TILE_ALL_LAYERS` | 8，层 4–7 为表面 |
| `TILE_INDEX_BITS` | 10 |
| `TILE_INDEX_MASK` | `0x3FF`，提取瓦片编号 |
| `TILE_ORIENTATION_MASK` | `0x7`，提取方向 |
| `TILE_BODY_INDEX_MASK` | `0x1FFF`，提取编号加方向的碰撞模板索引 |
| `PIXEL_SIZE_XZ`、`PIXEL_SIZE_Y` | 原生模拟采用的像素尺度 |

一个槽位是 uint16：低 10 位为瓦片编号，高 6 位为状态；状态低 3 位是方向，其余 3 位供业务使用。编号 0 表示空。

```python
from sbxcpp.space import TILE_INDEX_BITS, TILE_INDEX_MASK, TILE_ORIENTATION_MASK

tile_id = 12 | (3 << TILE_INDEX_BITS)
index = tile_id & TILE_INDEX_MASK
orientation = (tile_id >> TILE_INDEX_BITS) & TILE_ORIENTATION_MASK
```

### 地图方法

| 接口 | 含义 |
| --- | --- |
| `width` / `height` | 地图宽高 |
| `set(x, y, layer, value)` / `get(...)` | 单槽位读写 |
| `set_tile(x, y, tile)` / `get_tile(...)` | 整格读写 |
| `sliced(x, y, w, h)` | 创建共享数据的切片 |
| `slice_x/y/w/h` | 当前切片范围信息 |

切片不是快照。若 Space 已经开始维护接触状态，业务增删瓦片应优先使用 `space.create_tile(...)` / `space.destroy_tile(...)`，不要把原始地图数组读写当作完整的事件维护接口。

## 6. 瓦片碰撞模板与砖高度

`create_tile_body(tile_body_index, aabbs, position, flags)` 同时创建、放置并注册一套碰撞形状。`position` 相对槽位原点；X/Z 原点位于格子中心，竖直原点位于层高。每个盒子必须满足 X/Z 上 `abs(position + offset) + extent <= 0.5`。

砖层的方向由邻居规则决定：西侧为位 1、北侧为位 2、西北对角为位 4。每个砖编号要为实际可能出现的规则键注册碰撞形状。表面层使用槽位自身的方向位。

下面为编号 1 的八种方向注册相同的完整砖形状，适合理解模板与数据的关系：

```python
from sbxcpp.space import TILE_INDEX_BITS, get_tile_brick_height
from vmath import vec3

h = get_tile_brick_height()
for orientation in range(8):
    template_index = 1 | (orientation << TILE_INDEX_BITS)
    space.create_tile_body(
        template_index,
        vec3(0.5, h * 0.5, 0.5),
        vec3(0, h * 0.5, 0),
        0,
    )
space.create_tile(2, 3, 1, 1)
```

默认砖高约为 `4 / sqrt(14)`，所有 Space 共用一个进程级值。砖层 l 的基准高度为 `(l - 2) * brick_height`，因此层 1 和层 2 的边界位于世界 Y=0。表面位于最高砖顶面；无砖时位于 `-2 * brick_height`。

`set_tile_brick_height(height)` 修改全局高度，快照不保存它。应在建立形状和世界前统一配置，并在恢复快照前使用同样配置。

## 7. 环绕坐标

| 方法 | 用途 |
| --- | --- |
| `wrap_point(vec3 或 vec2i)` | 把水平坐标归入地图范围 |
| `wrap_chunk(vec2i)` | 环绕 chunk 坐标 |
| `diff_point(lhs, rhs)` | 最近镜像下的差；支持 vec3、vec2、vec2i |
| `diff_chunk(lhs, rhs)` | 环形 chunk 差 |
| `closest_mirror_point(pos, ref_pos)` | 找到最靠近参考点的镜像位置 |

```python
target_delta = space.diff_point(target_position, actor_position)
visible_target = space.closest_mirror_point(target_position, camera_position)
```

这可以避免追踪目标或绘制时绕地图走远路。环绕区间是 `[0, size)`；无效或失去可用浮点精度的极大坐标不能作为正常世界位置输入。

## 8. 空间查询

`BroadPhaseFlags.INCLUDE_TILES = 1`、`INCLUDE_BODIES = 2`、`ALL = 0xffffffff`。查询结果中普通物体表示为 BodyID，瓦片表示为 `vec3i(x, z, layer)`。

| API | 参数和结果 |
| --- | --- |
| `broad_phase(vmin, vmax, layer_mask, flags, out)` | AABB 范围候选；返回 list，不等于最终精确接触 |
| `ray_cast(origin, direction, max_distance, layer_mask, flags)` | 最近射线命中 BodyID / 瓦片坐标，未命中为 `None` |
| `filter_chunk_bodies(base_chunk_pos, radius, layer_mask, out)` | chunk 范围内的普通物体列表 |
| `cylinder_cast(center, radius, height, start_angle, sweep_angle, layer_mask, flags, out)` | 圆柱或扇形区域查询；角度为弧度 |

`out` 必须传入，可为 `None` 或 Python list；传入列表时它会被清空并复用，不是追加结果。

```python
from sbxcpp.space import BroadPhaseFlags, BodyID
from vmath import vec3

results = []
space.broad_phase(vec3(0, 0, 0), vec3(5, 3, 5), 0xffffffff,
                  BroadPhaseFlags.ALL, results)
hit = space.ray_cast(vec3(15, 10, 15), vec3(0, -1, 0), 20.0,
                     0xffffffff, BroadPhaseFlags.ALL)
if hit is None:
    print("未命中")
elif isinstance(hit, BodyID):
    print("普通物体", hit.id)
else:
    print("瓦片槽位", hit)
```

射线内部会归一化方向；接近零的方向不能形成有效射线。不要把 broad_phase 的候选结果直接当成攻击伤害的最终判定。

## 9. 快照与 Godot 调试绘制

```python
snapshot = space.to_var()
restored_space = Space.from_var(snapshot, callbacks)

saved_id = body.to_var()
restored_id = BodyID.from_var(saved_id)
```

`to_var` 返回可序列化的 Godot Variant 数据，恢复 Space 需要重新提供回调对象。BodyID 的恢复只是恢复标识，需与对应世界快照配套。快照是当前实现的数据布局，不是跨版本格式承诺；不要手工拼装、截断或将不同版本的原始快照直接互换。

启用 SBX 后，可以在 Godot 场景添加 `SpaceDebugDraw` 节点。从 Python 获取该节点并调用：

```python
debug_draw = self.owner.get_node("SpaceDebugDraw")
debug_draw.rebuild(space.get_ptr(), True, 1, 1, 1)
```

参数依次是有效 Space 的地址、是否包含瓦片、中心 chunk 的 x/y、chunk 半径。半径 0 只显示一个 chunk，1 为周围 3×3。`tile_color` 和 `body_color` 控制颜色，`get_tile_boxes()` 返回上次瓦片重建所用的盒子分组数据。

Space 必须在绘制调用期间仍存活。`get_ptr()` 是供此类原生桥接使用的地址，不要保存到持久存档或跨进程传递。可选扩展类不一定出现在根据 Godot 原始 API 生成的 `godot.classes` 中；通过已放置的场景节点访问最明确。

完整依据：[space.pyi](../sbx_extension/typings/sbxcpp/space.pyi)、[空间绑定](../sbx_extension/src/space/SpaceBindings.cpp)、[空间模拟](../sbx_extension/src/space/Space.cpp)、[快照实现](../sbx_extension/src/space/ToVariant.cpp)、[调试绘制](../sbx_extension/include/space/SpaceDebugDraw.hpp)。
