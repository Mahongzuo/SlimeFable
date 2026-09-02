---
name: slimefable-foliage-interact
description: >-
  SlimeFable interactive grass WPO: character volume drives MPC_SlimeFoliage,
  MF_SlimeFoliageInteract bends/flattens foliage. Use when editing 拨草, grass WPO,
  foliage interact, wiring MF_SlimeFoliageInteract into another grass master, or
  porting the system to another Unreal project.
---

# SlimeFable 拨草（WPO 交互）

做拨草、把交互接到别的草材质、或移植到其他工程之前：读完本文件；复制文件与接线命令见 [references/porting.md](references/porting.md)。

这是**纯材质 WPO**，不是 PhysX 碰草。C++ 只把角色球体写进 MPC；草顶点自己位移。

## 本仓库零件

| 件 | 路径 |
|----|------|
| 体积接口 | `Source/SlimeFable/Slime/FoliageInteractVolume.h` |
| 组件 | `SlimeFoliageInteractComponent`（史莱姆 / 敌人 CDO 已挂） |
| 子系统 | `SlimeFoliageInteractSubsystem`（每帧写最近 4 槽） |
| MPC | `/Game/_Slime/Environment/FoliageInteract/MPC_SlimeFoliage` |
| MF | `/Game/_Slime/Environment/FoliageInteract/MF_SlimeFoliageInteract` |
| 接线 | `Content/Python/wire_slime_foliage_interact.py` |
| 校验 | `Content/Python/verify_slime_foliage_interact.py` |

默认接到 Fishermans `M_Foliage` 的 WPO（加在 `WPO_Fin` 风摇之后）。`Enable Slime Interact?` 默认 True；树叶 / 灌木 / 广告牌 MI 强制 False。

## 调参（组件 `0_Config|Foliage`）

| 字段 | 默认 | 作用 |
|------|------|------|
| `RadiusScale` | 1.15 | 球体半径 = max(胶囊, 接口/网格半径) × 本值 |
| `MaxBend` | 36 | 梢端水平拨开（cm），写 MPC |
| `Flatten` | 20 | 梢端下压（cm） |
| `GrassHeight` | 80 | HeightMask 分母 |
| `IdlePartStrength` | 0.45 | 站住时强度 |

改完即时生效（除接线脚本改过的 HLSL）。调试：`slime.FoliageInteract.Debug 1`（绿/红球 + `abs=` / `camRel=`）。别的工程改 MPC 路径：`slime.FoliageInteract.Mpc /Game/Foo/MPC_SlimeFoliage.MPC_SlimeFoliage`。

## 硬坑（必须遵守）

1. **不是碰撞**。给草开 Overlap 不会让 WPO 生效，而且很贵。
2. **MPC 写摄像机相对 XYZ**。材质：`To = (WorldPos - CameraPos) - InteractPos`。写绝对世界坐标时 `Influence` 恒为 0。
3. **高度用 `LocalPosition`**。Nanite / ISM 上 `ObjectPositionWS` 经常是 0，`HeightMask` 会整株死掉。
4. **距离用三维球体**（`length(To)`）。只用 XY 会变成无限高圆柱，跳起来脚下草不回弹。
5. **`bAutoActivate = true`**。否则 `IsActive()` 恒假，Strength 一直 0。
6. **StaticSwitch 默认 True**。没 override 的草 Master / 子 MI 否则只有风摇。树叶 MI 再强制 False。
7. **`SetMaterialInstanceStaticSwitchParameterValue` 成功也返回 False**。写完必须回读 override。
8. 体积优先 `IFoliageInteractVolume`（史莱姆膜、敌人死亡抑制）；否则骨骼 Bounds / 胶囊。组件**不要**再 include 项目角色类。

## 接到本仓库另一张草 Master

无头（有 GUI 时加 `-nullrhi`）：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/wire_slime_foliage_interact.py" -- --material=/Game/Your/M_Grass --ground-dirs=/Game/Your/Grass --disable-mis=/Game/Your/MI_Leaves -unattended -nop4 -nullrhi -nosound
```

再跑 `verify_slime_foliage_interact.py`，同样带 `--material` / `--out-dir`。

移植到其他 Unreal 工程：见 [references/porting.md](references/porting.md)。
