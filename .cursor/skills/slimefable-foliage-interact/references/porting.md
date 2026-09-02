# 把拨草迁到其他 Unreal 工程

目标：对方工程的草 Master WPO 调用同一套 MF，角色挂同一组件。不需要抽插件。

## 复制文件

C++（只依赖 Engine，不要带上 `SlimeCharacter` / `EnemyCharacter`）：

- `Source/SlimeFable/Slime/FoliageInteractVolume.h`
- `Source/SlimeFable/Slime/SlimeFoliageInteractComponent.h` / `.cpp`
- `Source/SlimeFable/Slime/SlimeFoliageInteractSubsystem.h` / `.cpp`

把 `SLIMEFABLE_API` 换成目标模块 API 宏；`LogSlimeFable` 换成目标日志。`#include "SlimeFable.h"` 改成目标 PCH / 日志头。

Python：

- `Content/Python/wire_slime_foliage_interact.py`
- `Content/Python/verify_slime_foliage_interact.py`

模块 `Build.cs` 已有 `Engine` 即可。Pawn（玩家、敌人）CDO 上 `CreateDefaultSubobject<USlimeFoliageInteractComponent>`。

## 接线别人的草

1. 关编辑器，UBT 编过。
2. 无头跑 wire，**必须**指定对方材质：

```text
--material=/Game/.../M_TheirFoliage
--out-dir=/Game/FoliageInteract
--ground-dirs=/Game/.../Grass,/Game/.../Flower
--disable-mis=/Game/.../MI_TreeLeaves
```

脚本会：创建/复用 MPC + MF；把 MF 接到该 Master 的 World Position Offset（若找不到 `WPO_Fin` 会报错——对方图若没有这个 Named Reroute，先改 `wire_foliage_master` 去接现有 WPO 输入，或让 MF 的 `ExistingWPO` 接 0）。

3. 地被 MI 打开 `Enable Slime Interact?`；树/灌木走 `--disable-mis` 强制 False。
4. `verify_slime_foliage_interact.py` 带同样的 `--material` / `--out-dir`。
5. 默认 MPC 软路径是 `/Game/_Slime/Environment/FoliageInteract/MPC_SlimeFoliage`。对方若用了 `--out-dir`，运行时设  
   `slime.FoliageInteract.Mpc /Game/FoliageInteract/MPC_SlimeFoliage.MPC_SlimeFoliage`

## 体积（可选）

不实现 `IFoliageInteractVolume`：组件用骨骼 `Bounds`，没有网格则用胶囊脚底。

需要史莱姆膜 / 死亡不压草：Owner 实现接口。

- `GetFoliageInteractVolume`：世界坐标中心 + 半径（cm）
- `ShouldSuppressFoliageInteract`：死亡、隐藏、停放时返回 true

菜单关：没有 Pawn 或 Pawn 是 `ASpectatorPawn` 时子系统清槽，不写交互。

## 不要做

- 不要给每根草加 Collision / Overlap
- 不要按实例写 `PerInstanceCustomData`（密草上很贵）
- 不要把 MPC 写成绝对世界坐标
- 不要用 `ObjectPosition` 当草高
