# 可摆放 Actor（日关卡 BP）

做其它日期的交互物 / 敌人 / 纪念品前先读本页。0815 已踩过这些坑。

## 编辑器里能选中、不能移动/缩放

根是 `UStaticMeshComponent` 时默认 **Static**。视口常出现橙框、**没有**移动/缩放 gizmo。

- 构造里对根（以及会点到的子网格）调用 `SetMobility(EComponentMobility::Movable)`。
- `ACharacter` 胶囊已是 Movable；若另挂占位 `StaticMesh`，占位也要 Movable，否则组件选择模式点立方体会锁变换。
- 传送门先例：`DayChapterPortal.cpp`。

## 禁止 `SetWorldScale3D`（构造 / Construction）

`SetWorldScale3D` 在 CDO 阶段写世界缩放，会和视口 gizmo 抢变换，看起来像锁死缩放。

用 `SetRelativeScale3D`。已摆实例若仍锁，Details 把 Mobility 改成 Movable，或删了重拖。

## 敌人默认是占位立方体

`AEnemyCharacter` 构造里**隐藏**角色 Mesh，另挂引擎 Cube。`OnConstruction` 的 `RebuildMeshParts` 只有 `PrimarySkeletalMesh` 能加载时才显示骨骼。

建敌人 BP **必须**写到该日 CDO：

- `PrimarySkeletalMesh`
- `PrimaryAnimClass`（否则 T-pose）
- `DeathMontage`、`Moves[].Skill.AttackMontage`（或 Tower 的 `MissileSkill.AttackMontage`）

只填任务 Objective 不够。C++ 招式槽有伤害，Montage 空则打人不播动画。网格**不要**写死在 C++ 默认值，按日用 Python 绑。

## 建 BP 一次写完 CDO

用 `Content/Python/` 脚本创建并 `set_editor_property`：任务 ID、网格、AnimBP、Montage。不要只建空壳再指望人手填。资产不存在就 `log_warning`，不要假装绑好了。

0815 参考：`create_0815_blueprints.py` 的 `bind_1945_enemy_meshes()`。

## 摆进哪一层

门和年份摆件必须属于**日关卡**（Outliner World = `MMDD` 或 `SL_MMDD_YYYY`），Current 不能是 `OperaHouse/Maps/Demonstration`。点过剧院 Actor 后，Levels 把日关卡设回 Current 再摆。

## 出生点用 PlayerStart，不要写死坐标

子图 / 日关卡出生点 = 该图**持久关**里的 `PlayerStart`，自己拖位置。禁止在 C++ 里按年份写 `FVector`。

流送布景（`Venice Showcase`、`OperaHouse/Demonstration`）里的 PlayerStart **不算**。`ASlimeFableGameMode` 优先用持久关那一枚。建 `SL_*` 用脚本补一枚可拖的，不要只建空 World。
