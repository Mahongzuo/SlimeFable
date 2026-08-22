# 本机工具链（勿每次再搜）

本机路径。引擎搬家后只改这一份，并同步 `AGENTS.md` 快表。

## 路径

| 项 | 值 |
|----|-----|
| 工程 | `E:\UE\SlimeFable\SlimeFable.uproject` |
| 引擎根 | `D:\Program Files\Epic Games\UE_5.8` |
| UBT | `D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` |
| 编辑器 GUI | `D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe` |
| 无头编辑器 | `D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| MCP | `http://127.0.0.1:8010/mcp`（仓库 `.mcp.json`；本机 8000 常被占用） |

不要在 `C:\Program Files\Epic Games`、`E:\UE` 下递归找 `Build.bat` / `UnrealEditor.exe`。

## 默认工作假设

用户开 Agent **改 C++ / 配置** 时，**Unreal Editor 默认是关的**（Live Coding 会挡完整 UBT）。

1. **只改 C++**：直接 UBT，不要为了编译去开编辑器。
2. **要刷资产 / 跑编辑器 Python**：优先无头 `UnrealEditor-Cmd.exe`。
3. **要 MCP 查场景、改关卡 Actor**：主动拉起 GUI 编辑器，等加载完再 `StartServer 8010`。
4. 编辑器已开时：完整 UBT 可能失败；无头脚本必须加 `-nullrhi`，避免抢 GPU。

## 编译（UBT）

PowerShell：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlimeFableEditor Win64 Development -Project="E:\UE\SlimeFable\SlimeFable.uproject" -WaitMutex
```

- 目标：`SlimeFableEditor`；配置：`Win64 Development`。
- 新增 `UCLASS` / `UFUNCTION` / `UPROPERTY` 必须完整 UBT，不要指望 Live Coding。
- XGE 未激活时 UBT 会警告并走本机；项目已设 `r.XGEController.Enabled=0`，属预期。

## 无头跑 Python

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/SCRIPT.py" -unattended -nop4 -nullrhi -nosound
```

把 `SCRIPT.py` 换成实际脚本（例如 `create_enemy_death_dissolve.py`）。有 GUI 编辑器在跑时也加 `-nullrhi`。

## 拉起 GUI + MCP

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "E:\UE\SlimeFable\SlimeFable.uproject"
```

编辑器起来后（Output Log 不再刷加载）：

- 控制台：`ModelContextProtocol.StartServer 8010`
- 或：`py E:/UE/SlimeFable/Content/Python/start_mcp_8010.py`
- Cursor MCP 连 `http://127.0.0.1:8010/mcp`。discovery 失败就刷新连接，不要改回 8000。

MCP 细则见 `slimefable-unreal-mcp`。
