# 常见问题、限制与排错

[返回目录](README.md)

先记录实际报错原文、Godot 版本、平台/CPU、扩展安装包的 `GIT_COMMIT_HASH.txt`，再按下面对应问题定位。本文档基于静态阅读，标注的源码问题不代表已运行复现。

## 安装和编辑器

| 现象 | 检查或处理 |
| --- | --- |
| 附加脚本中没有 Python | 检查 addons 层级、`.gdextension`、匹配架构的库和 Godot 4.4 最低要求；重启编辑器 |
| Windows 有库但加载失败 | 检查描述文件实际引用的是 `template_debug` 还是 `template_release` |
| Intel Mac / Linux / Web 无可用库 | 当前描述文件没有这些即用配置；不要只改平台名称就发布 |
| 插件列表里找不到 godot-pocketpy | 它由 GDExtension 自动加载，不通过同名 `plugin.cfg` 启用 |
| FileSystem 看不到 site-packages | `.gdignore` 的预期效果，切换到 Python 面板 |
| Python 面板缺少新文件 | 点击 Refresh；检查隐藏项、链接和 `.py` 扩展名 |
| IDE 提示缺少 godot 模块 | 检查项目根目录的 `pyrightconfig.json`、stubPath 与 typings 是否存在 |
| `godot.scripts` 补全缺类 | 类文件放到 `res://scripts/`，执行重建索引；其他目录不会被当前工具扫描 |
| 没有语法错误红线、跳转或自动回调生成 | 这些语言服务未完整实现；看 Godot 输出并配置外部静态工具 |

## 脚本声明和实例

| 报错或现象 | 原因与正确写法 |
| --- | --- |
| `Failed to find class 'X' in ...` | `X.py` 必须包含 `class X(...)`，注意大小写 |
| `Class 'X' ... must derive from Extends(...)` | 使用 `class X(Extends(Node))`，不能直接继承绑定 Node |
| `Duplicate class name` | 不同目录也不能有同名挂载脚本；同时改类名、文件名和引用 |
| `Failed to find base class` | 检查 Extends 参数和父脚本加载错误 |
| `no reloading context available` | 把 Extends/export/signal 当普通运行时函数调用了；应在挂载脚本声明时使用 |
| `Duplicate member` | 继承链重复声明同名导出项或信号 |
| `self.get_tree()` 不存在 | 引擎对象在 `self.owner`，使用 `self.owner.get_tree()` |
| `__init__` 中读不到 Inspector 设置 | 场景覆盖值尚未应用；在 `_ready` 读取 |
| Python 普通字段无法从 GDScript `get` | 属性桥仅公开导出项与信号；给普通字段提供访问方法 |
| `Actor()` 无法创建 | 先加载脚本；该构造入口要求可实例化的 Node 基类，不传构造参数 |
| `_notification` 不按预期调用 | 当前 notification 桥为空；不要把它作为已实现能力依赖 |

## API 和数据转换

| 报错或现象 | 处理 |
| --- | --- |
| 找不到 `godot.KEY_SPACE` | 改用 `from godot.constants import KEY_SPACE` |
| `py_tovariant: no Variant counterpart for type 'list'` | 使用 Godot Array 并逐项追加；dict/bytes 同样需要显式转换 |
| `Variant ... does not support attribute assignment` | 对向量/颜色重建值后赋回宿主属性，不直接修改分量 |
| `len(godot_array)` 或迭代失败 | 用 `.size()` 和索引循环 |
| `Callable` 无法接收 Python 方法 | 用 `Callable(self.owner, "方法名")` |
| `GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS` / `TOO_MANY_ARGUMENTS` | 核对所安装版本的 `.pyi` 签名与位置参数数目 |
| `GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT` | 检查实际引擎类型，尤其是 bytes/list/dict 与 Godot 容器的区别 |
| `as_` 类型不匹配 | 它会抛出 TypeError，需提前 isinstance 检查或显式处理 |
| 原生 API 无法使用关键字参数 | 按 `.pyi` 所示顺序传位置参数 |
| `load` 失败 | 检查 `res://` 根、大小写、导出包含规则和资源类型 |

## 属性、信号与协程

| 报错或现象 | 处理 |
| --- | --- |
| `cannot export type` | 使用支持的标量类型、Godot Variant 类型或 Resource/Node 类 |
| `expected a Resource or Node subclass` | 不能任意 export 一个 Object 子类 |
| `node exports require a Node-derived script` | 导出节点引用的脚本自身需挂在 Node 系列对象上 |
| `export_range() extra hints must be 'str'` | 额外提示写为字符串；min/max/step 则是数值 |
| `coroutine yielded value must be 'godot.Signal'` | 等待 timer.timeout；嵌套生成器使用 yield from |
| 协程没有收到信号参数 | 当前恢复机制不传参；用普通信号回调保存数据 |
| 协程停止后 Timer 仍在 | 取消只移除协程；Timer 是独立的 Godot 对象 |
| 主线程无响应 | 检查长循环、阻塞 sleep、大量同步工作；协程不会自动并行 |

## 导入、重载和发布

| 现象 | 处理 |
| --- | --- |
| 普通模块导入失败 | 放在 `res://site-packages/`，包提供 `__init__.py` |
| 普通模块被当成挂载脚本 | 保留 `site-packages/.gdignore`，并使用 Python 面板编辑 |
| 新内容保存后没有生效 | 普通模块有缓存；停止游戏再运行，Refresh 只更新文件列表 |
| 想手动 `importlib.reload` | 当前 Godot 导入回调与 reload 调用参数存在空指针风险；本版本使用重启运行会话替代 |
| 工作线程加载 Python 脚本失败 | 运行时新脚本编译/重载要求主线程；不要假定所有 threaded load 场景可用 |
| `os` / `io` / `conio` 导入失败 | 默认构建关闭 OS 功能；用 Godot 的文件、输入和系统接口 |
| stubs 中有模块，运行时却没有 | typings 复制了上游可选模块声明，检查实际编译开关 |
| 编辑器正常、导出包 import 失败 | 检查模块是否被 exclude_filter 排除，以及扩展导出插件是否加载 |
| 导出后找不到 JSON 或图片 | 普通模块插件只添加 `.py`；单独安排非 Python 数据的导出 |
| pip 安装了包但游戏找不到 | 游戏运行的是内嵌 pocketpy，不读取 CPython 环境 |

## 调试

| 现象 | 处理 |
| --- | --- |
| Python 断点不停 | 打开 `python/debugger/enabled`，从有远程调试连接的会话启动，重启游戏 |
| 无 `Python debugger enabled` 输出 | 检查设置是否保存、进程是否连接 Godot 调试器 |
| 普通模块断点位置不对 | 确认编辑器源码已保存，重新运行以加载新版本 |
| 字典显示成字符串 | 没有 Variant 对应物的 Python 值通过 repr 展示 |
| 监视表达式不接受 Python 语法 | 使用 Godot Expression 范围内的表达式；它不是 Python REPL |
| Profiler 里没有 Python 方法数据 | Godot 语言 Profiler 接口尚未实现 |
| 开启调试后变慢 | 逐行跟踪有开销，测性能时关闭调试并重新启动会话 |
| 同时打开 pkpy profiler 与调试器出问题 | 两者使用跟踪钩子，分开运行 |

## 可选 SBX

| 现象 | 处理 |
| --- | --- |
| `sbxcpp` 不存在 | 使用真正编入 SBX 的库；只有 `.pyi` 不够 |
| LevelDB 打开失败 | 使用可写目录，按需 create_if_missing，检查是否被另一个实例占用 |
| LevelDB 迭代接口异常 | 当前 `_DBIter` 注册/查找模块名不一致，见[存储页](14-sbx-storage-network.md) |
| KCP 已 connect 却 kcp_opened 为 false | 该状态要等收到应用消息，不是本地 socket 创建状态 |
| KCP 消息延迟一帧 | 把 send_kcp 放在同帧 poll 前 |
| 重连仍有旧 WebSocket 状态 | 一次 Network 只能 connect_ws 一次；重建对象 |
| KCP 收不到超大消息 | 接收缓冲上限为 32 KiB，按协议分片 |
| Space 拒绝尺寸 | 地图维度需能被 chunk_size 整除，且每轴至少 3 个 chunk |
| Space 拒绝 delta | 使用不小于 1/60 秒的固定步长 |
| 物体没有重力 | 设置 body 的 gravity scale，默认 0 |
| 碰到物体却无回调 | 主动搜索者需为带 MONITORING 的 KINEMATIC |
| flags 无法修改 | 创建后固定；改碰撞层或重建物体 |
| TILE 无法 teleport | 位置在 create_tile_body 时固定 |
| 查询结果不像 BodyID | 瓦片命中用 vec3i(x,z,layer)，普通物体才是 BodyID |

## 报告问题时提供什么

提供复现所需的最小场景、Python 脚本、普通模块、错误原文、期望结果、实际结果，以及构建版本和平台。如果涉及 SBX，额外说明其源码/构建版本。可以先从[快速开始](02-quick-start.md)的场景删减成最小例子，避免发送无关资源或真实存档。
