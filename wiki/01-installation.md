# 安装、升级与平台选择

[返回目录](README.md) · [下一步：快速开始](02-quick-start.md)

## 环境要求

扩展描述文件声明 `compatibility_minimum = "4.4"`。这表示最低 Godot 版本为 4.4，并不保证较新 Godot 中新增的 API 都已进入当前安装包的类型提示。仓库中的 `godot-cpp` 跟踪 4.4 分支，而 `demo/project.godot` 的功能标记为 4.6；使用示例项目时还要考虑它自身的版本与渲染设置。

下面是当前 `.gdextension` 声明的架构，不要只按操作系统名称选择二进制文件。

| 平台 | 当前声明 | 安装方式与边界 |
| --- | --- | --- |
| Windows | `x86_64` | CI 提供对应库；目录名为 `bin/windows` |
| macOS | `arm64` | 当前描述文件只声明 ARM64，不能据此认定支持 Intel Mac |
| Android | `arm64` | 对应 arm64-v8a；需要匹配的 Godot 导出模板 |
| iOS | `arm64` | 构建配置面向真机，不包含模拟器声明 |
| Linux | 没有库条目 | 需自行构建并补充描述文件，见[源码安装](17-build-and-stubs.md) |
| Web、其他 CPU 架构 | 没有库条目 | 当前仓库未给出即用发行配置 |

使用预编译扩展不要求安装 CPython、pip 或 C++ 编译器。只有生成绑定或编译扩展的用户才需要构建工具。

## 安装预编译包

仓库 README 指向 [GitHub Actions 构建页](https://github.com/pocketpy/godot-pocketpy/actions)。在成功的 `build` 任务中下载合并后的 **`godot-pocketpy`** artifact；下载入口和可用构建以该页面实际状态为准。本手册没有检查在线构建状态。

1. 关闭目标项目的 Godot 编辑器。
2. 解压安装包到 `project.godot` 所在目录。
3. 检查是否形成下面的结构，避免多套一层 `addons` 或压缩包名称目录。
4. 重新打开项目，查看输出中的扩展加载错误。
5. 选择一个节点，在“附加脚本 / Attach Script”的语言列表中确认出现 `Python`。

```text
你的项目/
├── project.godot
└── addons/
    └── godot-pocketpy/
        ├── godot-pocketpy.gdextension
        ├── PythonScript_icon.png
        ├── GIT_COMMIT_HASH.txt
        ├── LICENSE.txt
        ├── typings/
        └── bin/
            ├── windows/
            ├── macos/
            ├── android/
            └── ios/
```

这是 GDExtension，由 Godot 加载；不需要在“项目设置 → 插件”中启用一个同名插件。扩展会自动注册 Python 语言及其编辑器功能。Python 面板通常出现在 FileSystem 面板附近。

描述文件设置了 `reloadable = false`。首次安装、替换动态库和升级后都应重启编辑器。

## 准备普通模块目录

在项目根目录手动创建 `site-packages/`，并在里面创建空文件 `.gdignore`。

```text
site-packages/
├── .gdignore
└── helpers.py
```

`.gdignore` 会隐藏 FileSystem 中的目录，但 Python 专用面板仍可显示和编辑它。它不是禁止 Python 导入的标记。完整导入规则见[普通模块与包](07-modules.md)。

## 升级已有项目

保留项目自己的 `scripts/`、`site-packages/` 和资源。在编辑器关闭时更换扩展目录，确保动态库、`.gdextension`、`typings/` 来自同一个版本；不要把新类型提示与旧动态库混用。

升级后可按以下顺序人工检查：

1. `Python` 仍出现在附加脚本列表。
2. 打开原有脚本，确认没有类名或导入错误。
3. 使用“项目 → 工具 → Python: Rebuild Scripts Index File”更新脚本索引。
4. 检查全局常量导入，旧式 `from godot import KEY_SPACE` 应改为 `from godot.constants import KEY_SPACE`。
5. 对发布平台重新导出游戏。

若原来使用 SBX，升级包也必须包含对应 SBX 原生模块，仅保留旧 `.pyi` 不会恢复运行时功能。

## 源码检出的库路径为何不同

仓库中的 Windows `.gdextension` 指向 `template_debug`，而 CI 打包阶段会替换成 `template_release`。这与 `windows.x86_64.debug` / `.release` 左侧的选择条件是两回事：必须检查右侧实际文件名是否存在。

不要直接把源码仓库中的描述文件覆盖到预编译安装包上。若自行构建，按[构建说明](17-build-and-stubs.md)让描述文件与输出库匹配。

## 卸载

先移除或替换场景中的 Python 脚本引用，再关闭编辑器，移走 `addons/godot-pocketpy/`。纯 `.py` 源文件可以保留，但 Godot 将无法执行它们。使用 SBX 的场景还需要移除相应的扩展节点和资源依赖。

依据：[扩展描述文件](../demo/addons/godot-pocketpy/godot-pocketpy.gdextension)、[打包配置](../.github/workflows/main.yml)、[项目 README](../README.md)。
