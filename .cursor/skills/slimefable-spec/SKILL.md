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
3. 编译 / 开编辑器 / MCP 路径读 [references/toolchain.md](references/toolchain.md)（**不要再搜 UBT**）。
4. 写 C++/改资产约定时读 [references/coding-conventions.md](references/coding-conventions.md)。
5. 按任务再加载专项 skill（见下方路由）。
6. 用中文与用户沟通；代码标识符与引擎 API 保持英文。

## 任务路由

| 任务 | 下一步 |
|------|--------|
| 编辑器操作、刷资产、查 Actor、跑 Python、MCP | 读并遵循 `slimefable-unreal-mcp` |
| `/Game/Maps/Days`、`/Game/_Slime/Days`、DayId、Registry、探索 Tag、按日存档 | 读并遵循 `slimefable-day-levels` |
| 大厅传送门、OperaHouse 共用大厅、年份子图、1/2/3 周目解锁 | 读并遵循 `slimefable-week-cycle` |
| 主菜单 / 选关 / HUD / 字体与 UI 视觉 | 读并遵循 `slimefable-ui` |
| 音乐 / 音效 / BGM / SFX / ComfyUI 音频生成 | 读并遵循 `slimefable-audio` |
| 幻形 / 吞噬变身 / 多槽材质丢失 / WorldGrid Face·Up·头发 | 读并遵循 `slimefable-morph-materials` |
| 拨草 / 草 WPO 交互 / 接别的草 Master / 移植到其他工程 | 读并遵循 `slimefable-foliage-interact` |
| 通用玩法 / 模板变体 | 以本 spec + coding-conventions 为准；若含 UI 仍先读 `slimefable-ui` |

手动调用：`/slimefable-spec`、`/slimefable-unreal-mcp`、`/slimefable-day-levels`、`/slimefable-week-cycle`、`/slimefable-ui`、`/slimefable-audio`、`/slimefable-morph-materials`、`/slimefable-foliage-interact`。

## 项目硬事实

- 引擎：**Unreal Engine 5.8**（`SlimeFable.uproject`）
- 引擎根：`D:\Program Files\Epic Games\UE_5.8`（UBT / 编辑器绝对路径见 [toolchain.md](references/toolchain.md)）
- 运行时模块：`SlimeFable`
- 默认地图：`/Game/Maps/Main.Main`
- 日关卡：`/Game/Maps/Days/MM/MMDD`（含 `0229`，共 366）
- 注册表：`/Game/Data/DayLevels/DA_DayLevelRegistry`
- MCP：`http://127.0.0.1:8010/mcp`（见仓库根 `.mcp.json`；本机 8000 常被占用）
- **改代码时编辑器默认关着**：C++ 用 UBT；刷资产用无头 `UnrealEditor-Cmd.exe`；要 MCP 再主动开 GUI。

## 强制约束

- **搜资产以本机目录为准**：`Content/` 在 `.gitignore` 中，Grep/Glob 会跳过并报 0，**不等于盘上没有**。找 WBP / uasset / 贴图 / Python 脚本时，对本机路径列目录（PowerShell `Get-ChildItem -Path E:\UE\SlimeFable\Content -Recurse -Filter *Inventory*`，或 Python `os.walk`）。禁止用 Grep 无结果或 git status 未列出判断资产不存在；git 只约束是否提交。
- **先读再改**：未读本 spec（及任务相关 skill）不得批量改 Content/Source。
- **MCP 串行**：Tool 在游戏线程串行执行；禁止并行叠 MCP 调用。
- **批量建关**：用 `Content/Python/create_day_levels.py`，禁止用 MCP 逐个创建 366 关。
- **Live Coding**：编辑器开着时 UBT 可能失败；新 UCLASS/UFUNCTION 用 [toolchain.md](references/toolchain.md) 的 `Build.bat` 完整编译，不要满盘搜引擎。
- **范围克制**：只改任务需要的文件；不主动写用户未要的 Markdown/计划文件；不改 Cursor 内置 `~/.cursor/skills-cursor/`。
- **视觉验证归用户**：描边、材质、光照、阴影、UI 等观感由用户在视口 / PIE 亲自确认。Agent 做完实现就停，把要看什么说清楚。禁止凭 MCP 截图、无头日志或自己「看起来不对」去改线宽 / 裁剪 / 颜色 / 曝光。用户已说效果不错的视觉点，禁止继续「优化」。
- **Details 分类**：关卡摆件的 `EditAnywhere` 用根类目 `0_Config`（`PrioritizeCategories` 置顶）；可调字段必须有中文 `ToolTip`（怎么填、默认、联动）；细则见 [references/coding-conventions.md](references/coding-conventions.md)。

## 当前阶段边界

已做：空日关卡骨架、Registry、DayLevelSubsystem、Exploration Tag 命名预留、Main 主菜单与选关日历 UI（见 `slimefable-ui`）。

未做（勿擅自铺开）：关卡美术内容、完整探索度统计、Boss/收集物玩法实现。任务框架见 `Docs/Systems/QuestSystem.md`；每日内容资产在 `/Game/_Slime/Days`。
