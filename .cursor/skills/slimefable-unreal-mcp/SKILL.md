---
name: slimefable-unreal-mcp
description: >-
  Operate Unreal Editor for SlimeFable via Unreal MCP (UE 5.8 ModelContextProtocol).
  Use when spawning actors, inspecting the scene, creating assets, running editor
  Python, or when the user mentions Unreal MCP, toolsets, or editor automation.
---

# SlimeFable Unreal MCP

在编辑器内通过 MCP 驱动 UE。官方文档：[Unreal MCP in Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)。

运维细节见 [references/mcp-ops.md](references/mcp-ops.md)。本机 UBT / 编辑器绝对路径见 spec 的 [toolchain.md](../slimefable-spec/references/toolchain.md)（**不要再搜引擎安装目录**）。

## 何时开编辑器

用户开 Agent **改代码时，编辑器默认是关的**。按任务选通道：

| 需求 | 做法 |
|------|------|
| 只编 C++ | UBT `Build.bat`（见 toolchain.md），不要为编译开编辑器 |
| 刷资产 / 跑 `Content/Python/*.py` | 无头 `UnrealEditor-Cmd.exe` + `-ExecutePythonScript` + `-nullrhi` |
| 查场景、摆 Actor、MCP 改关卡 | **主动拉起** GUI `UnrealEditor.exe`，加载完再 `ModelContextProtocol.StartServer 8010` |

## 连接前提

1. GUI 编辑器已打开本工程；控制台执行 `ModelContextProtocol.StartServer 8010`（默认不自动启动，以免 Package/Cook 抢端口误报失败）。未开则按上一节主动启动。
2. 客户端 URL：**`http://127.0.0.1:8010/mcp`**（仓库 `.mcp.json`）。
3. 本机 **8000 常被占用**；项目已默认 8010。勿改回 8000 除非确认端口空闲。
4. Cursor 侧若 discovery 失败：确认编辑器 Output Log 有 bind，并刷新 MCP 连接。
5. **打包前**请关掉其它占用 8010 的编辑器实例；Cook 日志若出现 `HttpListener unable to bind` 会导致 UAT 误判失败。

## Tool Search 工作流（默认开启）

`tools/list` 只返回三个元工具，按顺序使用：

1. `list_toolsets` — 发现 toolset
2. `describe_toolset` — 取某个 toolset 的 schema
3. `call_tool` — 传入 `toolset_name`、`tool_name`、`arguments`

常用资产 toolset 名示例：`editor_toolset.toolsets.asset.AssetTools`。

## 硬性规则

- **先列本机 Content 再 MCP**：`Content/` gitignore 会使 Grep/Glob 报 0。找 WBP/贴图先 `Get-ChildItem -Recurse`，不要先下结论「没有这个资产」。
- **串行调用**：MCP Tool 在游戏线程串行执行；禁止并行 / 重叠 Tool 调用。
- **批量关卡**：366 日关卡用 `Content/Python/create_day_levels.py`，禁止 MCP 逐关创建。
- **先描述后调用**：不确定参数时先 `describe_toolset`，再 `call_tool`。
- **改完刷新**：新写的自定义 toolset 在编辑器控制台执行 `ModelContextProtocol.RefreshTools`。

## 退路（MCP 不可用时）

编辑器控制台：

```text
py E:/UE/SlimeFable/Content/Python/create_day_levels.py
py E:/UE/SlimeFable/Content/Python/start_mcp_8010.py
```

无头（编辑器关着或已开抢 GPU 时都加 `-nullrhi`）：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_day_levels.py" -unattended -nop4 -nullrhi -nosound
```

主动开 GUI：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "E:\UE\SlimeFable\SlimeFable.uproject"
```

起来后：`ModelContextProtocol.StartServer 8010`，或 `py E:/UE/SlimeFable/Content/Python/start_mcp_8010.py`。也可 `ModelContextProtocol.GenerateClientConfig Cursor`。
