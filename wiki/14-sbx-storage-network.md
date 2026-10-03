# 可选 SBX：存储、网络与序列化

[返回目录](README.md) · [SBX 空间与瓦片](15-sbx-space.md)

## 使用前提

本页适用于另行构建了 SBX 的扩展。主项目默认 `GODOT_POCKETPY_WITH_SBX=OFF`，CI 的 `WITH_SBX` 也为 OFF。默认安装包中的 `msgpack` 可用，不代表 `sbxcpp` 可用。

SBX 提供三个 Python 子模块：`sbxcpp.leveldb`、`sbxcpp.lockstep`、`sbxcpp.space`，以及 Godot 类 `SpaceDebugDraw`。独立源码需要由使用者另行获取并放到 `sbx_extension/`；它不是主仓库的 Git 子模块，主仓库递归克隆不会自动得到它。构建方式见[源码安装](17-build-and-stubs.md)。

以下文档依据本工作区提供的 SBX 源码，不能当成未安装 SBX 环境中的可用性保证。相关源码链接需要同样具有该可选目录才可打开。

## `sbxcpp.leveldb`：键值存储

LevelDB 适合保存大量按字符串键访问的二进制记录，例如玩家数据、地图块和缓存。值必须是 Python bytes，通常配合 `msgpack` 或 `lz4`。

```python
from sbxcpp.leveldb import LevelDB
import msgpack

db = LevelDB("user://world_db", create_if_missing=True)
db.put("player:1", msgpack.dumps({"name": "hero", "level": 3}), sync=True)

data = db.get("player:1")
if data is not None:
    player = msgpack.loads(data)
    print(player["name"])

db.write({
    "player:2": msgpack.dumps({"name": "mage", "level": 2}),
    "obsolete:key": None,
}, sync=True)
db.delete("temporary:key")
db.close()
```

该例适合放在普通模块函数中，并由运行时脚本调用。不要在编辑器可能执行的模块顶层打开真实存档库。长期使用时让服务对象持有 DB，并在退出阶段明确 `close()`。

### 构造参数

```text
LevelDB(path, create_if_missing=False, error_if_exists=False,
        paranoid_checks=False, bloom_filter_policy_bits=0)
```

| 参数 | 含义 |
| --- | --- |
| `path` | 数据库目录；会经 Godot 路径转换，存档推荐 `user://...` |
| `create_if_missing` | 为 true 时允许创建新库；默认 false |
| `error_if_exists` | 为 true 时把已存在数据库视为错误 |
| `paranoid_checks` | 启用更严格的数据库检查 |
| `bloom_filter_policy_bits` | 大于 0 时配置 Bloom filter；0 不启用 |

目录父级需要存在且可写。打开失败抛出 `RuntimeError`；关闭后操作会报告 `LevelDB is closed`。不要用 `res://` 作为发布版可写存档位置。

### 方法参考

| 方法 | 返回值与含义 |
| --- | --- |
| `get(key, verify_checksums=False)` | 找到返回 bytes，缺失返回 `None` |
| `put(key, value, sync=False)` | 写入一个字符串键与 bytes 值 |
| `delete(key, sync=False)` | 删除键 |
| `write(ops, sync=False)` | 原子批量操作；Python dict 的值为 bytes 表示写入，为 `None` 表示删除 |
| `close()` | 关闭库与其共享迭代器 |
| `iter(start=None, end=None, verify_checksums=False)` | 声明为有序的 `(str, bytes)` 迭代器；当前限制见下文 |

`sync=True` 请求同步写入，适合关键保存点；它可能增加调用耗时。原子批次针对一组写入操作，不代表任意多次读取与写入之间具备通用事务语义。

### 迭代器的当前限制

接口设计的区间为 `start <= key < end`，省略 start/end 表示不设该侧边界。同一个 DB 内部只保留一个共享迭代器，不能依赖嵌套或并行遍历保持独立游标。

当前源码把 `_DBIter` 注册到 `sbxcpp.leveldb`，创建迭代器时却按 `leveldb` 查找类型。这是静态阅读发现的名称不一致，本次未运行验证。**不要将 `db.iter(...)` 作为当前版本已确认可用的遍历方案**；需要此功能时先修正或确认所用发行版已经修复。上面的存取示例不依赖迭代器。

参考：[类型声明](../sbx_extension/typings/sbxcpp/leveldb.pyi)、[实现及迭代器限制](../sbx_extension/src/LevelDB.cpp)。

## `sbxcpp.lockstep`：WebSocket 与 KCP 通道

`Network` 是传输层，不自带登录、房间管理、权威帧编号、输入聚合或状态回滚协议。它使用 WebSocket 处理会话类消息，使用 UDP/KCP 处理游戏消息，由业务每帧主动 `poll()`。

### 接口与返回值

| API | 作用 |
| --- | --- |
| `Network(callbacks)` | callbacks 实现 `on_ws_data(bytes)`、`on_kcp_data(bytes)` |
| `connect_ws(host, port)` | 发起 WebSocket 连接，返回 Godot 错误码 |
| `connect_kcp(host, port, conv)` | 初始化 KCP 通道；conv 必须与服务端约定一致，返回 Godot 错误码 |
| `send_ws(data)` | 发送 Python bytes，返回 Godot 错误码 |
| `send_kcp(data)` | 把 Python bytes 入队；返回底层 ikcp 结果，负数表示失败 |
| `poll()` | 收包、分发回调，并在末尾更新 KCP 发送 |
| `set_kcp_timeout(ms)` | 活跃检查超时；默认 5000ms，0 关闭时间阈值检查 |
| `reset_kcp()` | 关闭并重置 KCP/UDP，保留 WebSocket |
| `dispose()` | 释放整个会话；重连时新建 Network |

`connect_ws` 内部组装 `ws://host:port`，不接收完整 URL、路径或 TLS 选项。不要把它当成接受任意 `wss://...` 的高层客户端。

### 状态不是同一种含义

| 属性 | 解释 |
| --- | --- |
| `ws_opened` | 曾观察到 WebSocket 打开；是锁存状态 |
| `ws_closed` | 曾观察到 WebSocket 关闭；也是锁存状态 |
| `kcp_opened` | 收到过有效 KCP 应用消息后为 true；本地通道刚建立时不一定为 true |
| `kcp_unresponsive` | 活跃通道触发 KCP dead-link 或超时条件；不自动执行重连 |

不能只以 `ws_opened` 判断当前仍可发消息，还应考虑 `ws_closed`。也不要等待 `kcp_opened` 才发业务约定的首包，否则可能与服务端互相等待。

### 可组合的会话例子

将以下代码保存到 `res://site-packages/game_session.py`。host、port 和 conv 由你自己的服务端协议提供，例子没有预置公共服务器。

```python
from sbxcpp.lockstep import Network
from godot.constants import OK
import msgpack

class GameSession:
    def __init__(self):
        self.network = Network(self)
        self.has_game_channel = False
        self.pending = []

    def connect_control(self, host, port):
        return self.network.connect_ws(host, port)

    def open_game_channel(self, host, port, conv):
        result = self.network.connect_kcp(host, port, conv)
        self.has_game_channel = result == OK
        return result

    def queue_input(self, frame, buttons):
        self.pending.append(msgpack.dumps({"frame": frame, "buttons": buttons}))

    def tick(self):
        if self.has_game_channel:
            for payload in self.pending:
                result = self.network.send_kcp(payload)
                if result < 0:
                    print("发送入队失败：", result)
            self.pending.clear()
        self.network.poll()

    def on_ws_data(self, data):
        print("控制消息字节数：", len(data))
        # 按服务端约定解析，并在获得参数后建立游戏通道。

    def on_kcp_data(self, data):
        print("游戏消息字节数：", len(data))
        # 只有双方约定 MessagePack 时才在此使用 msgpack.loads(data)。

    def close(self):
        self.pending.clear()
        self.has_game_channel = False
        self.network.dispose()
```

在节点脚本 `_ready` 创建 `GameSession`，在 `_process` 调用 `tick`，在 `_exit_tree` 调用 `close`。`send_kcp` 必须先于同帧的 `poll`；该接口只入队，末尾的 KCP 更新才处理实际发送时机。

这个示例只展示调用组织，不实现离线队列上限、认证、重试和房间协议。应用应限制积压并明确连接失败时如何处理输入。

### 重连与大小限制

- 每个 Network 只能调用一次 `connect_ws`，包括失败的尝试；再次尝试需新对象。
- 重新建立 KCP 前调用 `reset_kcp`，再用服务端给出的参数调用 `connect_kcp`。
- 接收缓冲区为 32 KiB，超大 KCP 消息会被丢弃并记录错误；大快照应由业务分片。
- `poll` 才能推进网络和分发回调；主线程阻塞时传输也会受影响。
- 当前公开类型声明没有设置“UDP 冗余份数”的方法，不应依赖旧说明中未暴露的配置。

参考：[网络类型声明](../sbx_extension/typings/sbxcpp/lockstep.pyi)、[网络实现](../sbx_extension/src/LockstepGoNetwork.cpp)。

## SBX 对 `msgpack` 的扩展

启用 SBX 会为 pocketpy `msgpack` 安装扩展类型回调：Godot Variant 使用 ext type 0，payload 为 Godot `var_to_bytes` 的数据。

```python
import msgpack
from godot import Vector3

data = msgpack.dumps({"position": Vector3(1, 2, 3)})
state = msgpack.loads(data)
print(state["position"])
```

这不是独立的 `sbxcpp.msgpack` 模块，也没有额外的 GDScript `MessagePack` 类。外部 MessagePack 客户端需自行支持该 ext 格式；不要期望其他语言自动理解 Godot payload。节点和资源引用也不应被当成可携带完整对象寿命的普通存档值。

参考：[MessagePack 扩展](../sbx_extension/src/MessagePack.cpp)、[模块注册](../sbx_extension/src/sbx.cpp)。
