---
name: slimefable-week-cycle
description: >-
  SlimeFable reusable day-hub portals and per-chapter week cycles (1/2/3).
  Use when placing lobby portals, configuring year/chapter sub-levels, week
  unlock, OpenLevel travel, or when the user mentions 传送门、周目、大厅门、
  BP_DayChapterPortal, AdvancedPortals, OperaHouse, or a shared hub for all dates.
---

# SlimeFable 周目与时光博物馆

所有日期共用 `/Game/_Slime/Models/MapModel/TimeMuseum/Maps/L_TimeMuseum_Environment`。日期与地图解耦；农田、家园和建筑只有这一份。旧日地图、OperaHouse 资产保留作兼容，不再批量套剧院或复制大厅。

## 新日期内容

1. Registry `/Game/Data/DayLevels/DA_DayLevelRegistry`：`SubLevels` 登记故事键到地图；`ChapterOrder` 显式填写故事顺序。按数组分配门位，禁止遍历 Map 或按年份排序。
2. 制作 `SL_{DayId}_{Chapter}` 年份地图和对应任务书。不要为新日期复制博物馆或摆一套日专属门。
3. 博物馆的 `MuseumYearPortal` 有六个室内门位，`SlotNumber` 为 1～6。当日第 N 项绑定 N 号门；未使用的门隐藏并禁用碰撞/交互。
4. 超容量、重复编号、遗漏顺序、缺少地图会明确报错。要扩容，在博物馆补放新的唯一编号门，并补放标签 `MuseumYear_N` 的 PlayerStart，落点在触发区域外。

`Content/Python/setup_museum_travel.py` 是幂等迁移脚本：备份、重新读盘、校验磁盘指纹后才保存；保留现有 Actor 的位置。该脚本不重建农田，不得以 `setup_timemuseum_hub.py` 代替。

已有顺序优先来自任务书；0815 无资产书时来自 `UDayQuestBook::Make0815Book`。有地图但无书序的剩余项只在迁移时固定排序，结果记录 `Saved/MuseumTravel/setup.json`。新内容必须显式配置。

## 门与到达点

`/Game/_Slime/Quest/Actors/BP_DayChapterPortal` 保留原外观，`DestinationMode`：

- `Story`：旧门默认，沿用 `TargetChapterId`、故事解锁、周目选择；`bUseHostDayId` 使用当前选择日期。
- `Map`：`DestinationMap` 指定独立探索地图；`DestinationArrivalTag` 指定目标 PlayerStart；`MuseumReturnTag` 记录返回点。无需完成任务。
- `Museum`：返回时光博物馆，无任务限制。无来源记录时使用博物馆默认出生点。

博物馆原有门固定通往 Ruin，不占年份门位。Ruin 北侧有返回门。走入与 F 均支持；只受理一次切图。到达点必须落在触发盒外并通过导航、胶囊净空检查。

## 日期、切图和存档

- `TravelToToday` 选择现实当天再进入博物馆；`TravelToDayId` 选择指定日期。博物馆内切日期只刷新任务和门位，不 OpenLevel，不重建农田/家园。
- GI 的 `UDayLevelSubsystem` 保留所选日期、目的地类型、返回门位。直接博物馆 PIE 默认今天；午夜不自动切故事日期。
- 年份地图仍使用 OpenLevel，独立灯光及原有资产。Ruin 不加载所选日期任务。
- 返回统一用 `TravelToMuseumHub`；旧 `QuestSubsystem::TravelToHub` 也转到该入口。Esc 在博物馆外显示「返回时光博物馆」，合并旧「回到大厅」，保留其他暂停功能。
- 返回年份门旁：标签 `MuseumYear_N`；Ruin 返回：`MuseumRuin`；无有效标签退回默认 PlayerStart。
- 先保存进度、取消待进入门与周目界面、解除暂停、恢复输入，再切图。加载页沿用 `USlimeFableGameInstance` + `USlimeLoadingGateWidget`。
- 任务存档仍按 MMDD；Ruin 强化使用现实日期，和所选故事日期互不影响。
- 0812 从博物馆 `Legacy0812` 兼容入口进入原地图，保留原场景/玩法/存档。0815 等原年份资产不复制。

## 周目（按年分开）

`HighestWeekByChapter`：打通该年最高 N 周目后，该年开 N+1，封顶 3。

- 最高仍为 1：直接进入一周目；已通一周目则打开 `UWeekSelectWidget`。
- 按任务书顺序解锁；任一章达到二周目后，该日所有年份解锁。
- 未解锁显示原因；不通过切日期/返回清空周目。
- 难度 `WeekIndex` 的敌人倍率 0.85 / 1.0 / 1.40，仅应用于故事地图。

## 并行工作和脚本

TimeMuseum 是共享二进制：保存串行，先备份最新磁盘版，保存前校验未被外部改动；不保存旧加载副本覆盖别人。完整 UBT 与编辑器占用错开。

`create_day_levels.py` 必须保留已有 Level、SubLevels 和 ChapterOrder；`create_0815_sublevels.py` 保留已有映射与顺序。`apply_operahouse_lobby.py` 默认退出，仅显式历史维护才可启用。不要再为 366 天套大厅，不改原农田/家园生成逻辑。

位置、外观与走动观感交用户在 PIE 确认；自动验证负责碰撞、导航、流程和存档。
