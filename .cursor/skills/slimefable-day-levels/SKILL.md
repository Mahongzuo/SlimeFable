---
name: slimefable-day-levels
description: >-
  SlimeFable 366 calendar day-level framework (MMDD maps, registry, DayId,
  exploration tags, per-day save keys). Use when editing day levels, DA_DayLevelRegistry,
  DayLevelSubsystem, Exploration tags, or 「历史上的今天」 entry flow.
---

# SlimeFable 日关卡框架

API 与路径细节见 [references/day-level-api.md](references/day-level-api.md)。

## 命名与路径

```
/Game/Maps/Days/
  01/0101 … 0131
  02/0201 … 0229    ← 含闰日
  …
  12/1201 … 1231
```

- DayId = `MMDD` 字符串 / `FName`（如 `0812`、`0229`）
- Soft path：`/Game/Maps/Days/08/0812.0812`
- 合法月日共 **366**；禁止创建非法日（如 `0230`）

## 核心资产与代码

| 项 | 位置 |
|----|------|
| 注册表 | `/Game/Data/DayLevels/DA_DayLevelRegistry` |
| 类型 | `Source/SlimeFable/DayLevel/DayLevelTypes.h`（`FDayId`、`FDayLevelEntry`、`UDayLevelRegistry`） |
| 子系统 | `Source/SlimeFable/DayLevel/DayLevelSubsystem.h` |
| Tags | `Config/DefaultGameplayTags.ini` → `Exploration.Objective/Collectible/Boss` |
| 批处理 | `Content/Python/create_day_levels.py` |

## 工作流

**重建/补齐空关卡 + 刷新 Registry：**

```text
py E:/UE/SlimeFable/Content/Python/create_day_levels.py
```

脚本行为：已存在的 `.umap` 跳过；始终按 366 天重写 Registry 条目。

**运行时查询（设计意图）：**

1. GameInstance 取得 `UDayLevelSubsystem`
2. `SetRegistry` 指向 `DA_DayLevelRegistry`（或后续由 AssetManager 加载）
3. `GetTodayDayId()` / `GetLevelForDayId()` / `GetTodayLevel()`
4. 存档 slot：`GetSaveSlotKeyForDayId`（等于 DayId 字符串）

## 探索度（约定，未实现统计）

在 Actor 上挂 GameplayTag，根命名空间：

- `Exploration.Objective.*` — 小任务
- `Exploration.Collectible.*` — 收集物
- `Exploration.Boss.*` — Boss

计入探索度的逻辑尚未实现；扩展时沿用上述根，并按 DayId 分存档。

## 不要做

- 用 MCP 循环创建 366 个关卡
- 把日关卡改回单目录平铺或改名脱离 `MMDD`
- 在未设计 UI 前塞完整选关界面/存档系统（除非用户明确要求）
- 往空日关卡里塞大量占位美术（用户自行制作场景）
