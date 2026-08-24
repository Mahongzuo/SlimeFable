# 项目地图

## 顶层

| 路径 | 说明 |
|------|------|
| `SlimeFable.uproject` | UE 5.8 工程；模块 `SlimeFable`；引擎根 `D:\Program Files\Epic Games\UE_5.8` |
| `Source/SlimeFable/` | 游戏 C++ |
| `Content/` | 资产（地图、蓝图、模板变体） |
| `Config/` | 默认 ini（含 GameplayTags、AssetManager） |
| `Content/Python/` | 编辑器批处理脚本 |
| `.mcp.json` | Cursor → Unreal MCP（8010） |
| `.cursor/skills/` | 本仓库 Agent Skills（工具链路径：`slimefable-spec/references/toolchain.md`） |
| `.cursor/rules/` | alwaysApply 等规则 |
| `AGENTS.md` | 任意 agent 的入口说明 |

## Source 布局

| 路径 | 说明 |
|------|------|
| `Source/SlimeFable/DayLevel/` | 日关卡类型与 `UDayLevelSubsystem` |
| `Source/SlimeFable/Quest/` | 任务书、`UQuestSubsystem`、任务 UI / 交互 Actor |
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
| `Content/_Slime/Days/MM/MMDD` | 366 天故事资产目录（Quests / Actors / NPCs / Enemies / Audio / FX） |
| `Content/_Slime/Quest` | 共用任务 UI / 通用任务 Actor |
| `Content/Python/create_day_content_folders.py` | 批建 / 补齐 `_Slime/Days` 366 目录 |
| `Content/Data/DayLevels/DA_DayLevelRegistry` | 日关卡 Primary Data Asset |
| `Content/ThirdPerson/` | 第三人称模板内容 |
| `Content/Variant_*` | 官方模板变体关 |
| `Content/Python/create_day_levels.py` | 创建/刷新日关卡 + Registry |
| `Content/Python/start_mcp_8010.py` | 手动启动 MCP 8010 |
| `Content/Audio/` | 全局 BGM / SFX / Theme（Comfy 生成落盘） |
| `.cursor/skills/slimefable-audio/` | ComfyUI 音乐/音效生成 Skill + CLI |

## 配置要点

- `Config/DefaultEngine.ini`：`GameDefaultMap` / `EditorStartupMap` → `/Game/Maps/Main.Main`；`r.XGEController.Enabled=0`（solo 机失效 Incredibuild/XGE 会导致打包着色器 CPU 极低）
- `Config/DefaultGame.ini`：扫描 `/Game/Maps` 与 `DayLevelRegistry`（`/Game/Data/DayLevels`）；C++ soft-path Niagara（`Mixed_Magic_VFX_Pack` / `BlinkAndDashVFX` / `NiagaraExamples/FX_*` 等运行时子目录）须 `DirectoriesToAlwaysCook`；全局音频 `/Game/Audio` AlwaysCook；勿 AlwaysCook 整包 `NiagaraExamples`（`Utilities` 依赖缺失的 NiagaraFluids，会刷 Error）
- `Config/DefaultGameplayTags.ini`：`Exploration.*` 预留标签
- `Config/DefaultEditor.ini`：MCP `ServerPortNumber=8010`、`bAutoStartServer=False`（打包 Cook 勿自动绑端口；手动 `ModelContextProtocol.StartServer 8010`）
