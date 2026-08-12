# 项目地图

## 顶层

| 路径 | 说明 |
|------|------|
| `SlimeFable.uproject` | UE 5.8 工程；模块 `SlimeFable` |
| `Source/SlimeFable/` | 游戏 C++ |
| `Content/` | 资产（地图、蓝图、模板变体） |
| `Config/` | 默认 ini（含 GameplayTags、AssetManager） |
| `Content/Python/` | 编辑器批处理脚本 |
| `.mcp.json` | Cursor → Unreal MCP（8010） |
| `.cursor/skills/` | 本仓库 Agent Skills |
| `.cursor/rules/` | alwaysApply 等规则 |
| `AGENTS.md` | 任意 agent 的入口说明 |

## Source 布局

| 路径 | 说明 |
|------|------|
| `Source/SlimeFable/DayLevel/` | 日关卡类型与 `UDayLevelSubsystem` |
| `Source/SlimeFable/Variant_Combat/` | 第三人称战斗模板变体 |
| `Source/SlimeFable/Variant_Platforming/` | 平台跳跃变体 |
| `Source/SlimeFable/Variant_SideScrolling/` | 横版变体 |
| `SlimeFableCharacter` / `GameMode` / `PlayerController` | 第三人称基础类 |

`SlimeFable.Build.cs` 已依赖 `GameplayTags`，并包含 `SlimeFable/DayLevel` include 路径。

## Content 布局

| 路径 | 说明 |
|------|------|
| `Content/Maps/Main.umap` | 默认入口 / 枢纽 |
| `Content/Maps/Days/01` … `12` | 366 空日关卡（`MMDD.umap`） |
| `Content/Data/DayLevels/DA_DayLevelRegistry` | 日关卡 Primary Data Asset |
| `Content/ThirdPerson/` | 第三人称模板内容 |
| `Content/Variant_*` | 官方模板变体关 |
| `Content/Python/create_day_levels.py` | 创建/刷新日关卡 + Registry |
| `Content/Python/start_mcp_8010.py` | 手动启动 MCP 8010 |

## 配置要点

- `Config/DefaultEngine.ini`：`GameDefaultMap` / `EditorStartupMap` → `/Game/Maps/Main.Main`
- `Config/DefaultGame.ini`：扫描 `/Game/Maps` 与 `DayLevelRegistry`（`/Game/Data/DayLevels`）
- `Config/DefaultGameplayTags.ini`：`Exploration.*` 预留标签
- `Config/DefaultEditor.ini`：MCP `ServerPortNumber=8010`、`bAutoStartServer=True`
