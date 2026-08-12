---
name: slimefable-spec
description: >-
  SlimeFable project constitution and agent routing for UE 5.8. Use when working
  in this repo, before editing C++/Blueprints/assets, or when the user mentions
  SlimeFable, day levels, 历史上的今天, exploration tags, save slots, or Unreal MCP.
---

# SlimeFable Spec（必读）

史莱姆冒险穿越 / 「历史上的今天」：366 个按日期的关卡骨架已就位。Agent 在本仓库做任何实质性改动前，必须先完成本 skill。

## 开工清单

1. 读完本文件。
2. 需要目录/模块细节时读 [references/project-map.md](references/project-map.md)。
3. 写 C++/改资产约定时读 [references/coding-conventions.md](references/coding-conventions.md)。
4. 按任务再加载专项 skill（见下方路由）。
5. 用中文与用户沟通；代码标识符与引擎 API 保持英文。

## 任务路由

| 任务 | 下一步 |
|------|--------|
| 编辑器操作、刷资产、查 Actor、跑 Python、MCP | 读并遵循 `slimefable-unreal-mcp` |
| `/Game/Maps/Days`、DayId、Registry、探索 Tag、按日存档 | 读并遵循 `slimefable-day-levels` |
| 通用玩法 / UI / 模板变体 | 以本 spec + coding-conventions 为准 |

手动调用：`/slimefable-spec`、`/slimefable-unreal-mcp`、`/slimefable-day-levels`。

## 项目硬事实

- 引擎：**Unreal Engine 5.8**（`SlimeFable.uproject`）
- 运行时模块：`SlimeFable`
- 默认地图：`/Game/Maps/Main.Main`
- 日关卡：`/Game/Maps/Days/MM/MMDD`（含 `0229`，共 366）
- 注册表：`/Game/Data/DayLevels/DA_DayLevelRegistry`
- MCP：`http://127.0.0.1:8010/mcp`（见仓库根 `.mcp.json`；本机 8000 常被占用）

## 强制约束

- **先读再改**：未读本 spec（及任务相关 skill）不得批量改 Content/Source。
- **MCP 串行**：Tool 在游戏线程串行执行；禁止并行叠 MCP 调用。
- **批量建关**：用 `Content/Python/create_day_levels.py`，禁止用 MCP 逐个创建 366 关。
- **Live Coding**：编辑器开着时 UBT 可能失败；新 UCLASS/UFUNCTION 通常需关编辑器完整编译或重启。
- **范围克制**：只改任务需要的文件；不主动写用户未要的 Markdown/计划文件；不改 Cursor 内置 `~/.cursor/skills-cursor/`。

## 当前阶段边界

已做：空日关卡骨架、Registry、DayLevelSubsystem、Exploration Tag 命名预留。

未做（勿擅自铺开）：关卡美术内容、完整 SaveGame/探索度统计、选关 UI、Boss/收集物玩法实现。
