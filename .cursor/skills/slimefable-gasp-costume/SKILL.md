---
name: slimefable-gasp-costume
description: >-
  Add GASP costume enemies (UEFN walk + official Mixamo retarget + unique attack
  montages) following the proven Meituan/Xigua/Nailong/Niulai pipeline. Use when
  adding a new Content/Enemy costume character, clothing enemy, Mixamo mesh on
  GASP, RTG_UEFN_to_*, CostumeLab, or when the user mentions 服装敌人、套皮敌人、
  新增角色、美团、西瓜、奶龙、牛来.
---

# SlimeFable GASP 服装敌人

四个已跑通角色：美团 / 西瓜 / 奶龙 / 牛来。新增角色按本流程，不要另起炉灶。

## 已落地角色

| 角色 | 网格 | 幻形技能 |
|------|------|----------|
| meituan | `/Game/Enemy/meituan/UnrealEdObject` | E 召奶龙、R 跳砸（落地命中+索敌）；Q 隐 |
| xigua | `/Game/Enemy/xigua/UnrealEdObject` | Q 突袭、E 召猪、R 风光波、T 砸瓜雨 |
| nailong | `/Game/Enemy/nailong/naiwa` | 无专属招，QER 全隐 |
| niulai | `/Game/Enemy/niulai/niulai` | 无专属招，QER 全隐 |

LMB 仍走 `FillDefaultGaspMoves()`（GASP 近战）。西瓜 Q `Weight = 0`，AI 不捡。美团 E（召奶龙）/ R（跳砸）、西瓜 E（召猪）/ R（风光波）/ T（砸瓜雨）`Weight = 1`，AI 在射程内会放；远程 CD 中追到近战距离普攻。美团 R 落地才结算、出招期间追目标。幻形召唤物为玩家队，打敌人不打自己。西瓜 T：附近 5 米砸 30 个可切程序网格西瓜，落地或砸到角色后切开，5 秒销毁；砸中敌对（含史莱姆）造成伤害。

沙盒图：`/Game/Maps/Sandbox/FeatureLabs/CostumeLab`  
服装根：`/Game/_Slime/Enemies/Costume/`（`BP_*Enemy`、`Visual/`、`Rigs/`、`Montages/`）

## 硬规则

1. **不要**把 `BP_GaspRagdollEnemy` 副本 reparent 到 `AGaspCostumeEnemy`，否则丢掉 `BP_SlimeSandboxMoverBase`。服装 BP 父类保持 `BP_GaspRagdollEnemy`，只设 `CostumeKind`。
2. **不要**重跑 `create_gasp_ragdoll_enemy.py`。
3. **不要**改父类 `BP_GaspRagdollEnemy` 的 VisualOverride（会污染 Echo）。
4. 不要再复制 Echo IK 再 `set_skeletal_mesh`（异族骨架返回 `ok=False`）。新建空 `IK_{id}_Official`，先绑服装网格，再官方 Auto Characterizer。
5. Mixamo/FBX 空 `root` 被拷 UEFN 根旋转会整只躺倒：**关掉 Root Motion**，**关掉 Run IK Rig**。
6. `ABP_GenericRetarget` 用 `SkeletalMesh.ComponentTags[0]`（仅资产名，如 `RTG_UEFN_to_Xigua`）查 `IKRetargeter_Map`。
7. 服装招的 Mixamo Montage 打在**外观网格**；GASP 近战打在 **UEFN 源网格**。外观 ABP 仍是 Echo 骨架时，`Montage_Play` 会失败，要播槽里的 `AnimSequence`（见 `PlayOwnerAttackMontage`）。运行时只关 Root Motion，**不要** `ForceRootLock`（会按第一帧悬空）。会水平滑的招（目前只有西瓜 Q）才 `LockMixamoTravelBoneToFirstFrame`，**只钉 Hips XY、保留 Z**。跳击 / 站立法术不要钉 Hips。烘焙：`Content/Python/fix_costume_combat_root_lock.py`（从桌面 Mixamo FBX 重导，只 XY-lock `Surprise_Uppercut`）。西瓜 Q 播到 1.5 秒截断。
8. 有 `SkillR` 的服装，R 不再触发 Ragdoll BP 倒地。死亡 / 受伤倒地仍走官方 ragdoll。
9. 幻形后满血；被打过的血量写进 `FSlimeDevourCapture.SavedMorphHP`；回血药 / 加攻药打在幻形体上。
10. 测新角色用 FeatureLab，禁止往原版 `SlimeLab` 堆。视觉验证归用户。

## 新增角色清单

1. 把网格 / Skeleton / 攻击序列放进 `Content/Enemy/<id>/`（本机列目录确认，Content 被 gitignore）。
2. 在 `Content/Python/create_gasp_costume_enemies.py` 的 `COSTUMES` 加一项：`id`、`mesh`、`skel`、攻击 `montages`（可空）。
3. 在 `EnemyCombatTypes.h` 的 `EGaspCostumeKind` 加枚举。
4. 在 `EnemyCombat::CostumeDisplayName / CostumeRetargeterPath / CostumeVisualClassPath / CostumeRetargeterTag / AppendCostumeSkills` 补路径和 QER。
5. 无头跑创建脚本（编辑器关，加 `-nullrhi`）：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/create_gasp_costume_enemies.py" -unattended -nop4 -nullrhi -nosound
```

6. 若只修运行时 RTG（Root Motion / ABP ops），跑 `Content/Python/fix_costume_retarget_runtime.py`，不要重配整条 IK。
6b. 服装招贴胶囊：跑 `Content/Python/fix_costume_combat_root_lock.py`（重导 5 段源动画；只对西瓜 Q / `Surprise_Uppercut` 钉 Hips XY，跳击不要钉 Z）。
6c. 简模换菲比 Cloth Toon：跑 `Content/Python/create_costume_toon_materials.py`（MI 在 `/Game/_Slime/Enemies/Costume/Materials/`，parent `M_PhoebeToon_Cloth`，不改菲比 Master）。必须同时写网格 slot 0 **和** `BP_Visual_*` 的 `OverrideMaterials`；Visual 是 Echo 副本，只改网格槽会被旧 override 盖掉。
7. 需要测图时用 `create_feature_lab.py --name Costume --spawn /Game/_Slime/Enemies/Costume/BP_<Id>Enemy`。
8. C++ 用 UBT；改完让用户在 CostumeLab 看走路 / 近战 / 专属招 / 幻形满血。

## 官方重定向（已验证）

源 IK 复用 `RTG_UEFN_to_Echo` 上的官方 `IK_UEFN_Mannequin`。目标：

1. 空 `IK_{id}_Official` → 绑服装网格 → `apply_auto_generated_retarget_definition()` + `apply_auto_fbik()`。四个现有角色都认出 **Mixamo**，根骨 `Hips`。
2. 就地重配 `RTG_UEFN_to_*`：`remove_all_ops` + `add_default_ops` + Exact 链映射。
3. 脚链别名：`LeftFoot←LeftToe`、`RightFoot←RightToe`。
4. 关掉 Root Motion、关掉 Run IK Rig。
5. 补 Echo ABP 需要的 op，**名字必须对上**：`Blend to Source`、`Offset Goals`。缺了 `UpdateRetargetProfile` 会每帧刷 `BTSettingsController`。

可见性：`ApplyActiveVisualOnly` 只藏源网格本身，不 `SetVisibility(false, true)` 往子级传。`CostumeKind != None` 时清掉 LeaderPose（`EnsureEchoFollowsSource`）。源网格 `AlwaysTickPoseAndRefreshBones`。

## 出招

| 路径 | 网格 |
|------|------|
| GASP 近战（`AM_ComboAttack`，Variant_Combat 骨架） | 只打 UEFN 源网格 → 外观重定向。禁止按「骨架指针不同」误判成服装招，否则会拆掉 ABP 变成 T-pose |
| 服装 Mixamo 招（路径含 `/_Slime/Enemies/Costume/Montages/`） | 外观网格单节点播槽内 Sequence，招完恢复重定向 ABP |

Montage 软路径：`/Game/_Slime/Enemies/Costume/Montages/AM_<Id>_<Move>`。

## 禁止

- 用 MCP 逐个手建 IK / RTG / 366 张图
- 凭观感改线宽 / 颜色；站姿 / 出招手感归用户看视口
- 奶龙 / 牛来走路已 OK 时，不要为了别的角色重跑他们的 IK
- 把服装 BP 接到 `AGaspCostumeEnemy` 上当「更干净的父类」

## 关键文件

| 文件 | 作用 |
|------|------|
| `Content/Python/create_gasp_costume_enemies.py` | 官方 IK / RTG / Visual BP / 敌人 BP / Montage |
| `Content/Python/fix_costume_retarget_runtime.py` | Root Motion / ABP ops 运行时修补 |
| `Content/Python/fix_costume_combat_root_lock.py` | 服装招 in-place root lock + Hips 钉第一帧 |
| `Content/Python/create_costume_toon_materials.py` | 四只简模 MI parent 到 `M_PhoebeToon_Cloth` |
| `Source/SlimeFable/Enemy/EnemyCombatTypes.cpp` | `AppendCostumeSkills`、路径表 |
| `Source/SlimeFable/Enemy/EnemyCombatComponent.cpp` | `PlayOwnerAttackMontage` |
| `Source/SlimeFable/Enemy/GaspSandboxPawn.cpp` | 可见性、Visual/RTG、幻形满血 |
| `Source/SlimeFable/Enemy/GaspRagdollEnemy.cpp` | 服装禁 LeaderPose；R 不与 SkillR 抢倒地 |
