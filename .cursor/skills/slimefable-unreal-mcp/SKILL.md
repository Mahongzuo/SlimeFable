---
name: slimefable-unreal-mcp
description: >-
  Operate Unreal Editor for SlimeFable via Unreal MCP (UE 5.8 ModelContextProtocol).
  Use when spawning actors, inspecting the scene, creating assets, running editor
  Python, or when the user mentions Unreal MCP, toolsets, or editor automation.
---

# SlimeFable Unreal MCP

在编辑器内通过 MCP 驱动 UE。官方文档：[Unreal MCP in Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)。

运维细节见 [references/mcp-ops.md](references/mcp-ops.md)。

## 连接前提

1. Unreal Editor 已打开本工程，且 MCP 已监听。
2. 客户端 URL：**`http://127.0.0.1:8010/mcp`**（仓库 `.mcp.json`）。
3. 本机 **8000 常被占用**；项目已默认 8010。勿改回 8000 除非确认端口空闲。
4. Cursor 侧若 discovery 失败：确认编辑器 Output Log 有 bind，并刷新 MCP 连接。

## Tool Search 工作流（默认开启）

`tools/list` 只返回三个元工具，按顺序使用：

1. `list_toolsets` — 发现 toolset
2. `describe_toolset` — 取某个 toolset 的 schema
3. `call_tool` — 传入 `toolset_name`、`tool_name`、`arguments`

常用资产 toolset 名示例：`editor_toolset.toolsets.asset.AssetTools`。

## 硬性规则

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

或无头命令行（编辑器已开时建议加 `-nullrhi`）：

```text
UnrealEditor-Cmd.exe "E:/UE/SlimeFable/SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_day_levels.py" -unattended -nop4 -nullrhi -nosound
```

控制台也可：`ModelContextProtocol.StartServer 8010`、`ModelContextProtocol.GenerateClientConfig Cursor`。
