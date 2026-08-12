# 编码与资产约定

## C++

- 目标引擎 **5.8**；优先反射友好类型（`UCLASS` / `USTRUCT` / `UFUNCTION` / `UPROPERTY`）。
- 新日关卡相关代码放在 `Source/SlimeFable/DayLevel/`，命名与现有 `DayLevel*` 一致。
- 日志使用 `LogSlimeFable`（见 `SlimeFable.h`）。
- Blueprint 可调用的查询放在 `UDayLevelSubsystem`（GameInstanceSubsystem），避免在 Actor 上散落重复逻辑。
- Soft 引用日关卡：Registry 用 `FSoftObjectPath`；运行时查询用 `TSoftObjectPtr<UWorld>`。
- 新加模块依赖时同步改 `SlimeFable.Build.cs`。
- Live Coding 开启时完整 UBT 可能被拒；新增 UCLASS/UFUNCTION 通常需要关编辑器编译或重启编辑器。

## 资产

- 日关卡包路径：`/Game/Maps/Days/{MM}/{MMDD}`，资产名即 DayId（如 `0812`）。
- 不要把 366 个关卡平铺在单一文件夹；按月分子目录。
- 批量创建/修复用 `Content/Python/create_day_levels.py`（已存在则跳过关卡，刷新 Registry）。
- 空关卡默认无占位网格，方便后续导入场景。
- Primary Asset：`DayLevelRegistry` 扫描目录 `/Game/Data/DayLevels`。

## GameplayTag

预留根（见 `Config/DefaultGameplayTags.ini`）：

- `Exploration.Objective`
- `Exploration.Collectible`
- `Exploration.Boss`

后续挂到 Actor 计入探索度时，在这些根下扩展子 Tag，不要另起平行根名。

## 存档约定（骨架）

- SaveGame slot key = DayId 字符串（如 `"0812"`）。
- 使用 `UDayLevelSubsystem::GetSaveSlotKeyForDayId`；完整读写尚未实现，扩展时保持该 key。

## 编辑器 Python

- 脚本放 `Content/Python/`。
- 需要编辑器子系统时，优先在已打开的编辑器内执行；无头批处理可用：

```text
UnrealEditor-Cmd.exe SlimeFable.uproject -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_day_levels.py" -unattended -nop4 -nullrhi -nosound
```

与已开编辑器抢 GPU 时务必加 `-nullrhi`。
