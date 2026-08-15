# Day Level API 参考

## `FDayId`

- 字段：`FName Id`（`MMDD`）
- 构造：`FDayId(FName)` / `FDayId(FString)`
- `IsValid()` / `ToString()`

## `FDayLevelEntry`

| 属性 | 类型 | 说明 |
|------|------|------|
| `DayId` | `FName` | 如 `0812` |
| `Month` | `int32` | 1–12 |
| `Day` | `int32` | 月内日 |
| `Level` | `FSoftObjectPath` | 指向日关卡 World |
| `SubLevels` | `TMap<FName, TSoftObjectPtr<UWorld>>` | 可选年份/章节子图，键如 `1920` |

辅助：

- `GetSaveSlotKey()` → DayId 字符串
- `GetLevelSoftPtr()` → `TSoftObjectPtr<UWorld>`

## `UDayLevelRegistry`（PrimaryDataAsset）

- `TArray<FDayLevelEntry> Entries`
- `FindEntry(FName DayId, FDayLevelEntry& OutEntry)`
- `FindEntryForMonthDay(int32 Month, int32 Day, FDayLevelEntry& OutEntry)`
- PrimaryAssetType：`DayLevelRegistry`

资产路径：`/Game/Data/DayLevels/DA_DayLevelRegistry`

## `UDayLevelSubsystem`（GameInstanceSubsystem）

| 方法 | 作用 |
|------|------|
| `SetRegistry` / `GetRegistry` | 绑定注册表 |
| `GetTodayDayId` | 本地系统日期 → DayId（含闰日 0229） |
| `MakeDayId(Month, Day)` | 静态生成 DayId |
| `MakeDayLevelPackagePath(Month, Day)` | `/Game/Maps/Days/MM/MMDD` |
| `GetLevelForDayId` | 查 Soft World |
| `GetTodayLevel` | 今天的关卡 |
| `GetSaveSlotKeyForDayId` | 日关卡约定 slot 名 = DayId |
| `GetSubLevelForDayId` | 查 `SubLevels` 里某章的 Soft World |
| `TravelToSubLevel` | 从当前日关卡 Travel 到某章子图 |

## 路径公式

```text
PackagePath = /Game/Maps/Days/{MM:02d}/{MMDD}
ObjectPath  = {PackagePath}.{MMDD}
ContentRoot = /Game/_Slime/Days/{MM:02d}/{MMDD}
QuestBook   = {ContentRoot}/Quests/DA_Quest_{MMDD}
```

示例：8 月 12 日 → `/Game/Maps/Days/08/0812.0812`，内容 → `/Game/_Slime/Days/08/0812/`

## 批处理脚本字段

`create_day_levels.py`：

- `ROOT = /Game/Maps/Days`
- `REGISTRY_FOLDER = /Game/Data/DayLevels`
- `REGISTRY_NAME = DA_DayLevelRegistry`
- `DAYS_IN_MONTH` 含二月 29 天
