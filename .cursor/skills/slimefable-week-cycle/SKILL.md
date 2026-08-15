---
name: slimefable-week-cycle
description: >-
  SlimeFable reusable day-hub portals and per-chapter week cycles (1/2/3).
  Use when placing lobby portals, configuring year/chapter sub-levels, week
  unlock, OpenLevel travel, or when the user mentions 传送门、周目、大厅门、
  BP_DayChapterPortal, AdvancedPortals, OperaHouse, or a shared hub for all dates.
---

# SlimeFable 周目与大厅传送门

任意日期共用同一份剧院大厅，只在该日持久关摆 N 扇门。不要为 366 天各做一套大厅美术，也不要复制 `Demonstration`。0815 摆法见 [`0815_PlaceableGuide.md`](../../../Content/_Slime/Days/08/0815/0815_PlaceableGuide.md)。

## 新日期怎么做（大厅 + N 门 + Registry 子图）

以后每个日期只做三件事：

1. **Registry**：该日 `SubLevels` 登记故事键 → 子图。0815 有 6 个年份；0816 可以 8 个。子图名 `SL_{DayId}_{Chapter}`，例如 `/Game/_Slime/Days/08/0816/SL_0816_xxxx`。
2. **大厅摆门**：编辑器打开 `/Game/Maps/Days/MM/MMDD`（剧院会作为 AlwaysLoaded 子关卡叠进来）。在视口里对着舞台选位置，把 `BP_DayChapterPortal` 拖进**当前日关卡**（Outliner 属于 `0816`，不是 `Demonstration`）。`TargetChapterId` 各填一个 Registry 故事键。
3. **做子图内容**：对应 `SL_{DayId}_{Chapter}`。关末再放一扇 `TargetChapterId=Hub` 的门。

不想玩某年：ESC →「回到大厅」→ 走另一扇门。

## 共用大厅（禁止再做一套）

365 张空日关卡（**跳过已有内容的 `0812`**）流送同一份 [`/Game/OperaHouse/Maps/Demonstration`](../../../Content/OperaHouse/Maps)。不整图复制。

批处理（禁止 MCP 逐关）：

```text
UnrealEditor-Cmd.exe "E:/UE/SlimeFable/SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/apply_operahouse_lobby.py" -unattended -nop4 -nullrhi -nosound
```

脚本：清空图自带地板/灯/天空雾，保留 PlayerStart；若尚未挂 `Demonstration` 则 `LevelStreamingAlwaysLoaded`。已挂过的关跳过，可重跑。不改 `create_day_levels.py` 的「已存在则跳过」。

### 已套大厅（下次批处理先看这里）

| 日关卡 | 状态 | 何时 |
|--------|------|------|
| `0101`–`0209` | 已挂 `Demonstration` | 2026-08-15 全量跑到一半后停下 |
| `0210` | **未完成**（停下时正在 flush） | — |
| `0211`–`0811` | **未做** | — |
| `0812` | **永不套**（已有内容） | — |
| `0813`–`0814` | **未做** | — |
| `0815`–`0831` | 已挂并核对（`0815`/`0831` lobby=True，`0812` 仍无剧院） | 2026-08-15 指定切片 |
| `0901`–`1231` | **未做** | — |

补其余关卡：把 `apply_operahouse_lobby.py` 的 `ONLY_DAY_IDS` 留空后重跑（已挂的会 `skip`）。只跑一段时写成 `{"0901", "0902", ...}`。禁止 MCP 逐关。

**不要**打开/保存 `OperaHouse/Maps/Demonstration` 去摆门。门必须属于日关卡持久层，否则每天都会看到 0815 的门。

## 摆件

路径：`/Game/_Slime/Quest/Actors/BP_DayChapterPortal`

Details `0_Config`：

| 字段 | 含义 |
|------|------|
| `TargetChapterId` | **下拉**选当天 Registry 年份/故事，或 `Hub`（关末回大厅）。悬停看说明。不要手打 |
| `bUseHostDayId` | 默认 true：进门用当前日关卡。勾选时不显示 `DayId` |
| `DayId` | 仅取消宿主日时手填 MMDD |
| `PortalStyle` | 1–10，套用 `BP_Portal_1`…`10` 外观（只换皮） |
| `bEnterOnOverlap` | 走近也进 |

Quest 栏的 Chapter/Quest/Branch **不用填**（那是拾取/到达用的）。新 `0_Config` 字段必须有中文 `ToolTip`，见 `slimefable-spec` coding-conventions。

走近或 F 进入。未解锁：中央横幅角标「未解锁」，正文「先完成 XXXX」或关末「先完成主线」。不要静默失败。解锁：书序；任意一章到二周目后该日全部门可进。

## 切图

有子图就 `UDayLevelSubsystem::TravelToSubLevel`（`OpenLevel`）。大厅 World 卸掉，灯光不叠。回大厅用 `TravelToHub` / `TravelToDayId`。有子图的日子通关**不自动切图**；关末再放一扇 `TargetChapterId=Hub` 的门。Lab 等无子图日子仍可走 `NextChapterId`。

加载页复用选关：`USlimeFableGameInstance` MoviePlayer + `USlimeLoadingGateWidget`（着色器 + 贴图流送就绪再放行）。

ESC 暂停：仅在年份子图（`SL_*`）显示墨迹按钮「回到大厅」，走 `UQuestSubsystem::TravelToHub(ActiveDayId)`。已在大厅则隐藏。

## 周目（按年分开）

存档 `HighestWeekByChapter`：打通 **该年** 第 N 周目且 N 已是该年最高 → 该年开 N+1（封顶 3）。

- 只打 1945 二周目 → 只开 1945 三周目。其它年可以一直停在一周目。
- 该年最高仍为 1：直接进一周目。
- 该年已通一周目：弹出 `UWeekSelectWidget`，按该年解锁亮/灰。
- 任意一章最高 ≥ 2：该日所有年份门可进（书序锁解除）。
- 本局难度 `WeekIndex` 由面板写入；敌人 `ApplyWeekDifficulty`。周 1/2/3 倍率 0.85 / 1.0 / 1.40。

入口：`UQuestSubsystem::TravelToChapter` / `GetHighestWeek` / `IsChapterUnlocked` / `ShowLockedChapterBanner`。

## 不要做

- 不要再做一套大厅，不要复制 `Demonstration`
- 不要打开剧院图去摆门（门属于日关卡）
- 不要用 MCP 逐关处理 366 张图或逐关摆 366 扇门
- 不要把周目做成全局一条，绑死非战斗年
- 不要用关卡流送叠大厅灯光进年份子图
- 不要另做一套霓虹加载页
