# SlimeFable — Agent 入口

在改代码或资产之前，**先读项目 spec**，再执行任务。

## 必读顺序

1. [`.cursor/skills/slimefable-spec/SKILL.md`](.cursor/skills/slimefable-spec/SKILL.md) — 项目宪法与路由  
2. 按任务继续：
   - 编辑器 / MCP → [`.cursor/skills/slimefable-unreal-mcp/SKILL.md`](.cursor/skills/slimefable-unreal-mcp/SKILL.md)
   - 日关卡 / `_Slime/Days` 内容目录 / 探索 Tag / 按日存档 → [`.cursor/skills/slimefable-day-levels/SKILL.md`](.cursor/skills/slimefable-day-levels/SKILL.md)
   - 大厅传送门 / 年份子图 / 周目 → [`.cursor/skills/slimefable-week-cycle/SKILL.md`](.cursor/skills/slimefable-week-cycle/SKILL.md)
   - 菜单 / HUD / UI 视觉 → [`.cursor/skills/slimefable-ui/SKILL.md`](.cursor/skills/slimefable-ui/SKILL.md)
   - 音乐 / 音效 / ComfyUI 音频 → [`.cursor/skills/slimefable-audio/SKILL.md`](.cursor/skills/slimefable-audio/SKILL.md)
   - 幻形 / 多槽材质 / 吞噬变身外观 → [`.cursor/skills/slimefable-morph-materials/SKILL.md`](.cursor/skills/slimefable-morph-materials/SKILL.md)

Cursor 规则 [`.cursor/rules/slimefable-agent-spec.mdc`](.cursor/rules/slimefable-agent-spec.mdc)（`alwaysApply`）会要求遵守上述流程。

**搜 Content**：目录被 gitignore，Grep/Glob 常为 0。找资产用本机列目录（`Get-ChildItem -Recurse`），不要拿 git 当清单。细则见 spec skill「强制约束」。

## 快速事实

| 项 | 值 |
|----|-----|
| 引擎 | UE 5.8（`D:\Program Files\Epic Games\UE_5.8`） |
| 模块 | `SlimeFable` |
| 默认地图 | `/Game/Maps/Main.Main` |
| 日关卡 | `/Game/Maps/Days/MM/MMDD`（366，含 0229） |
| 每日内容 | `/Game/_Slime/Days/MM/MMDD`（Quests / Actors / NPCs / Enemies / Audio / FX） |
| Registry | `/Game/Data/DayLevels/DA_DayLevelRegistry` |
| UBT | `D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat`（完整命令见 spec `references/toolchain.md`） |
| MCP | `http://127.0.0.1:8010/mcp`（见 `.mcp.json`；改代码时编辑器默认关，要 MCP 再开 GUI） |
| 批量脚本 | `create_day_levels.py`（地图）/ `create_day_content_folders.py`（内容目录）/ `apply_operahouse_lobby.py`（共用剧院大厅；已套范围见 week-cycle） |

## Cursor 用法

- 打开本仓库后，Customize → Skills 应可见 `slimefable-*` skill。
- 聊天中可 `/slimefable-spec`、`/slimefable-week-cycle`、`/slimefable-ui`、`/slimefable-audio`、`/slimefable-morph-materials` 等手动调用。
- 细节文档在各 skill 的 `references/` 下，按需再读。
