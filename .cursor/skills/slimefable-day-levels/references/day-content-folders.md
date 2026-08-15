# 每日内容目录（`/Game/_Slime/Days`）

地图只放关卡：`/Game/Maps/Days/MM/MMDD`。  
故事、任务书、NPC、敌人、音频、特效放在 `_Slime`，按月日分类，与地图对齐。

## 路径公式

```text
/Game/_Slime/Days/{MM}/{MMDD}/
  Quests/     DA_Quest_{MMDD}（有内容再放，禁止批量空资产）
  Actors/     任务交互物、机关、收集物
  NPCs/
  Enemies/
  Audio/
  FX/
```

- DayId = `MMDD`（含 `0229`，共 366）
- 禁止非法日（如 `0230`）
- 共用框架：`/Game/_Slime/Quest/UI`、`/Game/_Slime/Quest/Actors`
- 根下已有 `Enemy/`、`PickUp/` 是通用角色/拾取，不要把按日内容塞进去

## 多章日子（例外）

只有像 0815 这种「一天多个年份子关」才加章目录：

```text
/Game/_Slime/Days/08/0815/
  Quests/ Hub/
  Y1920/Actors NPCs Enemies Audio FX
  …
  Y2026/…
```

年份列表写在脚本 `MULTI_CHAPTER_DAYS`。其他天不要建 `Y*`。

0815 子图与共用蓝图：

```text
py Content/Python/create_0815_sublevels.py   # SL_0815_YYYY + Registry.SubLevels
py Content/Python/create_0815_blueprints.py  # 机关 BP + 1945 三战 BP
```

## 生成逻辑

脚本：[`Content/Python/create_day_content_folders.py`](../../../Content/Python/create_day_content_folders.py)

| 符号 | 作用 |
|------|------|
| `DAYS_IN_MONTH` | 与 `create_day_levels.py` 相同，二月 29 天 |
| `DAY_DIRS` | 每天 6 类：Quests / Actors / NPCs / Enemies / Audio / FX |
| `MULTI_CHAPTER_DAYS` | `0815` → 1920…2026；自动加 `Hub/` + `Y{Year}/` |
| `create_day_kit(day_id)` | 建一天 |
| `create_all_day_kits()` | 扫 366，已存在则跳过 |
| `try_create_quest_book(day_id)` | 仅编辑器、仅指定日；不要对 366 天调用 |

编辑器控制台或无头：

```text
py E:/UE/SlimeFable/Content/Python/create_day_content_folders.py
```

无 `unreal` 模块时对 `Content/_Slime/Days` 做 mkdir（Content 在 gitignore，目录只在本机；脚本是仓库真源）。

运行时任务书软路径（见 `UQuestSubsystem::ResolveBookForWorld`）：

```text
/Game/_Slime/Days/{MM}/{MMDD}/Quests/DA_Quest_{MMDD}
```

## 不要做

- 用 MCP / 手工循环创建 366 个内容目录
- 批量创建 366 个空 `DA_Quest_*`
- 给没有多章的日子建 `Y1920` 这类年份夹
- 把每日资产平铺进 `/Game/_Slime` 根或 `Maps/Days`
