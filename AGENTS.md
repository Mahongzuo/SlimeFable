# SlimeFable — Agent 入口

在改代码或资产之前，**先读项目 spec**，再执行任务。

## 必读顺序

1. [`.cursor/skills/slimefable-spec/SKILL.md`](.cursor/skills/slimefable-spec/SKILL.md) — 项目宪法与路由  
2. 按任务继续：
   - 编辑器 / MCP → [`.cursor/skills/slimefable-unreal-mcp/SKILL.md`](.cursor/skills/slimefable-unreal-mcp/SKILL.md)
   - 日关卡 / 探索 Tag / 按日存档 → [`.cursor/skills/slimefable-day-levels/SKILL.md`](.cursor/skills/slimefable-day-levels/SKILL.md)

Cursor 规则 [`.cursor/rules/slimefable-agent-spec.mdc`](.cursor/rules/slimefable-agent-spec.mdc)（`alwaysApply`）会要求遵守上述流程。

## 快速事实

| 项 | 值 |
|----|-----|
| 引擎 | UE 5.8 |
| 模块 | `SlimeFable` |
| 默认地图 | `/Game/Maps/Main.Main` |
| 日关卡 | `/Game/Maps/Days/MM/MMDD`（366，含 0229） |
| Registry | `/Game/Data/DayLevels/DA_DayLevelRegistry` |
| MCP | `http://127.0.0.1:8010/mcp`（见 `.mcp.json`） |
| 批量脚本 | `Content/Python/create_day_levels.py` |

## Cursor 用法

- 打开本仓库后，Customize → Skills 应可见三个 `slimefable-*` skill。
- 聊天中可 `/slimefable-spec` 等手动调用。
- 细节文档在各 skill 的 `references/` 下，按需再读。
