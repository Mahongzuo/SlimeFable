# Unreal MCP 运维（SlimeFable）

## 端口与配置

| 位置 | 值 |
|------|-----|
| 推荐 URL | `http://127.0.0.1:8010/mcp` |
| 项目 `.mcp.json` | `unreal-mcp` → 上述 URL |
| `Config/DefaultEditor.ini` | `ServerPortNumber=8010`, `bAutoStartServer=True`, `bEnableToolSearch=True` |
| 用户 Saved 偏好 | `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` 中同名节 |

失败特征（日志）：

```text
LogHttpListener: Error: HttpListener unable to bind to 127.0.0.1:8000
```

处理：改用 8010，或释放 8000 后 `ModelContextProtocol.StartServer`。

## 插件

工程已启用（见 `SlimeFable.uproject`）：

- `ModelContextProtocol`（Unreal MCP）
- `AllToolsets` 及各类 `*Toolset`
- `Terminal`（可选，编辑器内终端）

Toolsets 由 Toolset Registry 发现；MCP 本身不实现具体编辑工具。

## 协议要点

- 传输：HTTP + SSE（Streamable HTTP）；无 stdio/WebSocket。
- 默认仅本机 loopback；无鉴权，勿暴露到公网。
- `serverInfo.name` 一般为 `unreal-mcp`。
- 调试可用：`npx @modelcontextprotocol/inspector`，指向 `http://127.0.0.1:8010/mcp`。

## 常用控制台命令

| 命令 | 作用 |
|------|------|
| `ModelContextProtocol.StartServer [port]` | 启动/指定端口 |
| `ModelContextProtocol.StopServer` | 停止 |
| `ModelContextProtocol.RefreshTools` | 重扫 toolset |
| `ModelContextProtocol.GenerateClientConfig Cursor` | 写项目客户端配置 |
| `Log LogModelContextProtocol Verbose` | 提高日志等级 |

## 调用模式示例

伪流程（串行）：

1. `list_toolsets`
2. `describe_toolset`（`toolset_name` = 目标 toolset）
3. `call_tool`（`tool_name` = `exists`，`arguments.path` = `/Game/Maps/Days/08/0812`）

校验 Registry：`path` = `/Game/Data/DayLevels/DA_DayLevelRegistry`。

## Python 远程执行

若启用了 Python Remote Execution，可作为 MCP 之外的编辑器内脚本通道；本仓库主路径仍是 MCP + `Content/Python/` 脚本。
