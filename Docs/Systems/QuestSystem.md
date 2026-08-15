# 史莱姆任务系统

全游戏通用、尽量少学的任务框架。「历史上的今天」每天可以有若干章节（年份），每章 1 条主任务、若干顺序分支，外加**不挡过关的支线**。

玩家只记：看左上角 → 跟一个 ◆ → 按 **F**。按 **J** 打开史书，可把追踪切到支线。

## 三层结构

| 层 | 含义 | 玩家看到的 |
|----|------|------------|
| 日 `DayId` | 一张主关卡，如 `0815` | 选关日历上的一天 |
| 章 Chapter | 一个年份 / 一段故事，常对应子图 | 「1920 油墨与呐喊」 |
| 主任务 | 这一章的一句话目标，通常每年 1 条 | 左上角标题（默认追踪） |
| 分支 | 主任务下的顺序步骤，2～4 条 | 标题下那一行进度 |
| 支线 | 同结构，纪念品 / 可选战 | 史书「支线」栏；不挡过关 |

规则：

- 主线同时只有 1 条进行中的分支；做完才 `AdvanceAfterBranch` / `CompleteChapter`。
- 支线碰见就计数，不参与过章；完成或切章后追踪回到主线。
- 「收集 3 份」是一条分支 + 计数，不是 3 条并列任务。
- 章内主线全部分支完成 → 先播完成横幅与配音，再 `TravelToSubLevel`；无子关则回主图或通关。
- 未写任务书的日子：不显示任务 UI。

## 四种目标

| 类型 | 动作 | 实现 |
|------|------|------|
| Collect | 按 F 捡 | `AQuestInteractActor` 或 `ASlimeWorldPickup` |
| Talk | 对 NPC / 石碑按 F | `AQuestInteractActor` / `AQuestChapterGate` |
| Defeat | 打倒带目标组件的敌人 | `UQuestObjectiveComponent`（死亡时 `TryContribute`） |
| Reach | 走进体积 | `AQuestReachVolume` |

共用机关（Details 用 `0_Config`，蓝图 `/Game/_Slime/Quest/Actors/`）：

| 类 | 作用 |
|----|------|
| `ASlimeElementLock` | 当前元素对上才开门；错了提示「换水」等 |
| `ASlimeSplitPad` | 本体或子团压上；`bLatchFragment` 咬住 G 发射的子团 |
| `ASlimeReactionHearth` | 两种 Aura 对上已有反应表才开（如水+雷） |

## 资产目录

地图只放关卡：`/Game/Maps/Days/MM/MMDD`。

每日故事资产（蓝图、NPC、敌人、音频、特效、任务书）放在：

```text
/Game/_Slime/
  Quest/                 共用框架（UI、通用 Actor、完成配音）
  Days/MM/MMDD/
    Quests/ Actors/ NPCs/ Enemies/ Audio/ FX/
```

- 路径公式：`/Game/_Slime/Days/{MM}/{MMDD}/`
- 366 天目录由 `Content/Python/create_day_content_folders.py` 批建
- 多章日子（如 0815）另有 `Hub/` + `Y{Year}/`；不要给普通天建年份夹
- 任务书 `DA_Quest_MMDD` 有内容再放，不批量空资产
- 0815 子图：`Content/Python/create_0815_sublevels.py`

## UI

- 左上角追踪条：半透岩褐 + 弱暖金描边 + 书签竖线；跟**当前追踪**（默认主线，可切支线）。点击打开史书。
- 完成横幅：屏幕正中略偏上，「已完成」+ 任务名，暖金大号字；分支约 5 秒，章/通关约 6.5 秒。无进行中任务时横幅仍显示。
- 配音：`/Game/_Slime/Quest/Audio/VO_QuestStepDone`（一步与一章共用）。缺资产则只出字。
- 史书 `UQuestLogWidget`：热键 **J**（`ESlimeInputAction::QuestLog`），再按 J / Esc 关。左栏主线/支线，右栏说明，底栏 `J 史书 · 点击切换追踪 · 主线仍挡过关`。不暂停世界。
- 寻路点：同时最多 1 个，跟当前追踪；视野外夹到屏幕边缘
- 风格走 `FMenuUIStyle`，禁止 Halftone / 蓝胶囊

## 代码入口

| 项 | 位置 |
|----|------|
| 类型 / 任务书 | `Source/SlimeFable/Quest/` |
| 运行时 | `UQuestSubsystem`（GameInstance） |
| 史书 | `UQuestLogWidget`，入口 `ToggleQuestLog` |
| 子关 Travel | `UDayLevelSubsystem::TravelToSubLevel` |
| 日条目子图 | `FDayLevelEntry::SubLevels` |
| 存档 slot | `Quest_{DayId}`（主线进度 + `CompletedSideQuestIds` + 追踪） |

GameplayTag 仍挂在 `Exploration.Objective / Collectible / Boss` 下，不另起 `Quest.*`。

## SlimeLab 试玩

进入 `SlimeLab` 后，`UQuestSubsystem` 会自动挂上「任务系统试炼」并在玩家前方刷出：

1. 两颗金色试炼石（Collect 0/2，按 F 拾取）
2. 一块棕色石碑（Talk，按 F 交谈）
3. 一个暖金终点圈（Reach，走进即可）

左上角应出现追踪条，屏幕上有一个 ◆ 指向当前目标。该任务书不存档，重进关卡可重测。

## 不做

- 小地图、多寻路点、NavMesh 自动寻路
- 完整探索度统计、366 天空 `DA_Quest_*`
- 用 MCP 逐张建子图（用 Python）
