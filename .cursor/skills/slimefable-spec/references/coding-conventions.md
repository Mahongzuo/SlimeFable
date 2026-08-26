# 编码与资产约定

## C++

- 目标引擎 **5.8**；优先反射友好类型（`UCLASS` / `USTRUCT` / `UFUNCTION` / `UPROPERTY`）。
- 新日关卡相关代码放在 `Source/SlimeFable/DayLevel/`，命名与现有 `DayLevel*` 一致。
- 日志使用 `LogSlimeFable`（见 `SlimeFable.h`）。
- Blueprint 可调用的查询放在 `UDayLevelSubsystem`（GameInstanceSubsystem），避免在 Actor 上散落重复逻辑。
- Soft 引用日关卡：Registry 用 `FSoftObjectPath`；运行时查询用 `TSoftObjectPtr<UWorld>`。
- 新加模块依赖时同步改 `SlimeFable.Build.cs`。
- Live Coding 开启时完整 UBT 可能被拒；新增 UCLASS/UFUNCTION 用下面的 `Build.bat` 关编辑器编译。本机路径见 [toolchain.md](toolchain.md)，**不要满盘搜引擎**。

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlimeFableEditor Win64 Development -Project="E:\UE\SlimeFable\SlimeFable.uproject" -WaitMutex
```

- 关卡摆件根 `StaticMesh`（以及会点到的占位子网格）必须 `SetMobility(Movable)`，否则视口能选中、无移动/缩放 gizmo。
- 构造 / Construction 用 `SetRelativeScale3D`，禁止 `SetWorldScale3D`（会和编辑器变换抢、缩放像锁死）。
- 敌人网格与 AnimBP **不要**写死在 C++ 默认值；建该日 BP 时用 Python 绑到 CDO（`PrimarySkeletalMesh` / `PrimaryAnimClass` / 攻击与死亡 Montage）。只建空壳会永远显示占位立方体。细则见 `slimefable-day-levels` 的 `placeable-actors.md`。

## 相机与导航（镜头 / AI）

- **Camera 通道**：墙体/地板 Block Camera；装饰物、可交互道具、Pawn 对 Camera 设 **Ignore**（玩家仍 Block 道具）。SpringArm `ProbeChannel=Camera`，墙过滤另用 WorldStatic 竖直面。
- **日关卡 / 大厅**：需有 **Nav Mesh Bounds Volume** 覆盖可走区域；改家具/台阶后 **Rebuild** Recast。敌人 `MaxStepHeight≈60`，NavMesh `AgentMaxStepHeight` 宜 ≥ 60。
- **跳跃跨断层**：一期不做；二期用 Smart `NavLinkProxy`，勿靠永久 `AddMovementInput` 直推。

## Details 分类

关卡里会摆的玩法 Actor / 组件，设计师可改字段统一进 **`0_Config`**，避免埋在 Transform / Mesh / Camera 下面翻找。

```cpp
UCLASS(meta = (PrioritizeCategories = "0_Config"))
// ...
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD")
FText DisplayName;
```

- 根名固定 `0_Config`。`0_` 字母序靠前；`UCLASS` 上加 `PrioritizeCategories = "0_Config"`，把它抬到 Transform 等引擎类目之前。
- 子项用管道：`0_Config|HUD`、`0_Config|Stats`、`0_Config|Mesh`、`0_Config|Combat`、`0_Config|Enemy`…
- 组件指针用 `VisibleAnywhere` + `Category = "Z_Components"` + `AdvancedDisplay`，不要放进 `0_Config`。
- 不要用 `Slime|Enemy`、`LockOn`、`Quest` 当关卡摆件的主类目。
- `DisplayName` 等 Actor 字段在 **Actor 根** 上改（World Outliner 点角色本身），不要点 Mesh / Camera 组件。
- 敌人锁定顶栏名字：`0_Config|HUD` → `DisplayName`；空则回退「敌人」，不显示 `BP_` 内部名。头顶小条只有血量，没有名字文字。

## 悬停说明（必做）

新的 `EditAnywhere` / `0_Config` 字段必须加中文 `ToolTip`：怎么填、默认值、和谁联动。不要只靠英文字段名。案例：传送门。

```cpp
UPROPERTY(EditAnywhere, Category = "0_Config|Gate",
	meta = (GetOptions = "GetTargetChapterIdOptions",
		ToolTip = "大厅：下拉选当天 Registry 里的年份/故事。关末回大厅：选 Hub。"))
FName TargetChapterId;

UPROPERTY(EditAnywhere, Category = "0_Config|Portal",
	meta = (ClampMin = "1", ClampMax = "10",
		ToolTip = "1–10 对应 BP_Portal_1…10 的外观。只换皮，不换进哪一年。"))
int32 PortalStyle = 1;
```

- `ToolTip` 用中文，写给摆关的人看。
- 有固定选项时优先 `GetOptions` 下拉，不要让人猜该填 `1920` 还是 `Hub`。
- 旧字段改到时顺手补；新字段不能缺。

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
- 需要编辑器子系统时：改代码场景下编辑器默认关着，用无头批处理；要 MCP 再主动开 GUI（路径见 [toolchain.md](toolchain.md)）。

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_day_levels.py" -unattended -nop4 -nullrhi -nosound
```

与已开编辑器抢 GPU 时务必加 `-nullrhi`。
