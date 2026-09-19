---
name: slimefable-lyra-shooter
description: >-
  Lyra third-person shooter enemy (devour / morph / shoot) and session listen
  server hooks. Use when editing ALyraShooterEnemy, import_lyra_shooter.py,
  LyraShooterLab, SlimeHostListen, or integrating LyraGame into SlimeFable.
---

# SlimeFable × Lyra 射击敌人

官方工程 `E:\UE\LyraStarterGame` **只读**。拷贝脚本：`Content/Python/import_lyra_shooter.py`。

## 硬规则

1. 不要改 `/Game/Characters/Heroes/B_Hero_Default`。复制再 reparent 到 `ALyraShooterEnemy`。
2. 不要把 `GameDefaultMap` 换成 `L_LyraFrontEnd`，不要覆盖 Channel1–4（Traversable / FluidTrace / Obstacle / Slicable）。Lyra Trace 在 Channel5–9。
3. 不要换 `GameDefaultMap` / `LyraGameEngine`。`AssetManagerClassName` 必须是 `LyraAssetManager`；`SignificanceManagerClassName` 必须是 `LyraSignificanceManager`；`AbilitySystemGlobals` 用 Lyra 的 GCM。
4. 测图只用 `/Game/Maps/Sandbox/FeatureLabs/LyraShooterLab`。
5. `LyraGame` 是第二模块（`IMPLEMENT_GAME_MODULE`），主模块仍是 `SlimeFable`。
6. `LocalPlayerClassName=/Script/CommonGame.CommonLocalPlayer`、`GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`（GameInstance 已是 `ULyraGameInstance`，缺这两项会 ensure）。**不要**换成 `LyraLocalPlayer` / `LyraSettingsLocal`：会把 PixelStreaming2Servers 初始化炸掉。
7. Lyra 资产引用的 Tag 必须在 `Config/DefaultGameplayTags.ini` 里（对照 `Config/LyraReference/DefaultGameplayTags.ini`）。少一个 Tag = 每次授能力时一次 GameplayTag ensure（约 6 秒卡顿）。
8. `ALyraCharacter` 关了 `bStartWithTickEnabled`；`ALyraShooterEnemy` 子类要自己开。`ABP_Mannequin_Base` 必须 `LinkAnimClassLayers(ABP_UnarmedAnimLayers)`，否则骨骼错位；Mesh 相对位置要 `(0,0,-90)`（B_Hero_Default 的 Mesh 在胶囊中心，身体本来是 cosmetics 子 Actor）。
9. **游戏模块的构造函数里禁止 `ConstructorHelpers` 硬加载任何 Lyra ABP**（`ABP_Mannequin_Base`、`ABP_*AnimLayers`、`ABP_Weap_*`）。本工程开了 `SequencerAnimMixerToolset`，编译后的 ABP 带 `FAnimSubsystem_SequencerMixer`（`/Script/MovieSceneAnimMixer`）。`SlimeFable` 模块的 CDO 在 Default 阶段插件之前构建，此时该脚本包还没注册 → 日志出 `VerifyImport: Failed to find script package for import object 'Package /Script/MovieSceneAnimMixer'` + `struct type mismatch ... != prop FallbackStruct`，该属性退化成 FallbackStruct，`FAnimNode_SequencerMixerTarget::Update_AnyThread` 的 `GetSubsystem<>()` 第一帧就 `Assertion failed: Subsystem [AnimInstance.h:937]`。正确做法：`TSoftObjectPtr / TSoftClassPtr`，在 `OnConstruction` / `PostInitializeComponents` 里 `LoadSynchronous`（见 `ALyraShooterEnemy::EnsureBodyVisuals`）。看到上面两行日志 = 又有人在构造函数里硬加载了。
10. 从 Lyra 拷完 ABP 可以跑 `Content/Python/recompile_lyra_animbps.py`（无头）让它们在本工程重新编译；这不是 9 的替代品，9 才是崩溃根因。
11. `Source/LyraGame/Physics/LyraCollisionChannels.h` 的宏必须和 `DefaultEngine.ini` 一致：Interaction=5、Weapon=6、Weapon_Capsule=7、Weapon_Multi=8、AimAssist=9（`DefaultGame.ini [/Script/ShooterCoreRuntime.ShooterCoreRuntimeSettings] AimAssistCollisionChannel=ECC_GameTraceChannel9`）。官方头文件是 1–4，直接拷过来会让 Lyra 武器射线打到 GASP 的 FluidTrace/Obstacle。
12. `/ShooterCore/Weapons/B_WeaponInstance_Base` 的 OnUnequipped 原版是 `GetTypedPawn(B_Hero_ShooterMannequin_C) → HealthComponent → IsDeadOrDying`，我们的敌人不是那个 BP，卸枪必报 `Accessed None ... CallFunc_GetTypedPawn_ReturnValue`。已用 `Content/Python/fix_lyra_weapon_instance_pawn_type.py` 把类字面量改成 `LyraCharacter`（`USlimeBlueprintFixupLibrary::RetargetBlueprintClassRefs`，Python 摸不到 pin）。**`import_lyra_shooter.py` 会用官方 ShooterCore 覆盖 `B_WeaponInstance_Base` 和 `WeaponAudioFunctions`，重跑导入后必须再跑这个脚本。**
13. 幻形后的相机：`ULyraCameraComponent` 只评估相机模式栈，往栈里推模式的是 `DetermineCameraModeDelegate`，官方由 `ULyraHeroComponent` 在 PlayerState/Experience 初始化链走完后绑定，幻形体永远走不到那步 → 栈空 → 视角卡在世界原点/地下、不跟人。`ALyraShooterEnemy::BindStandaloneCamera`（`ActivateStandaloneForPlayer` 里）自己绑，返回 `PawnData.DefaultCameraMode` 或 `StandaloneCameraMode`（默认 `CM_ThirdPerson`）。为此给 `ULyraCameraComponent` 加了 `MinimalAPI`。
14. 玩家开枪时屏幕左上刷 `LyraGameState does not have a MusicManager` 来自 `/ShooterCore/System/Audio/WeaponAudioFunctions::SendWeaponFire` 的 PrintString（官方 Experience 会往 GameState 挂 `B_MusicManagerComponent`，我们不跑它，也不要 Lyra 的 BGM）。`fix_lyra_weapon_instance_pawn_type.py` 第二步用 `RemoveFunctionCallNodes` 把那个叶子 PrintString 删掉。
15. 无头 `-nullrhi` 跑图时 `B_WeaponFire / B_WeaponDecals` 的 `Accessed None ... NS_MuzzleFlash / Decal` 是 Niagara 在 NullRHI 下 `SpawnSystemAttached` 返回空造成的，PIE 里没有，不用修。

## 资产

| 项 | 路径 |
|----|------|
| 敌人 BP | `/Game/_Slime/Enemies/Lyra/BP_LyraShooterEnemy` |
| 官方源 BP | `/Game/Characters/Heroes/B_Hero_Default` |
| PawnData | `/Game/Characters/Heroes/SimplePawnData/SimplePawnData` |
| 步枪 AbilitySet | `/ShooterCore/Weapons/Rifle/AbilitySet_ShooterRifle` |
| 参考 ini | `Config/LyraReference/` |

## 脚本

```powershell
py E:/UE/SlimeFable/Content/Python/import_lyra_shooter.py
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/create_lyra_shooter_enemy.py" -unattended -nop4 -nullrhi -nosound
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/create_feature_lab.py --name LyraShooter --spawn /Game/_Slime/Enemies/Lyra/BP_LyraShooterEnemy --count 2" -unattended -nop4 -nullrhi -nosound
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/populate_lyra_shooter_lab.py" -unattended -nop4 -nullrhi -nosound
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" "-ExecutePythonScript=E:/UE/SlimeFable/Content/Python/fix_lyra_weapon_instance_pawn_type.py" -unattended -nop4 -nullrhi -nosound
```

无头验证 AI 行为（75 秒后手动杀进程，看 `equipped / AI fire start / Lyra bullet` 与有没有 `Accessed None`）：

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" /Game/Maps/Sandbox/FeatureLabs/LyraShooterLab -game -nullrhi -nosound -unattended -nop4 -abslog="$env:TEMP\lyralab_run.log" -LogCmds="LogSlimeFable Verbose"
```

## 玩法

吞噬 / 幻形走 `ISlimeDevourTarget`。Lyra 体 `UsesExternalPossessInput/Camera` + `UsesSelfContainedPlayerCombat`，Possess 卸史莱姆 IMC，LMB 走 Lyra 武器 GAS 而不是 `PollPlayerCombatKeys`。

### 血量 / 伤害契约

- `USlimeHealthComponent` 是 Lyra 敌人的**唯一真血**。`ULyraHealthComponent::OnHealthChanged` 只按比例镜像成 SlimeHealth 的伤害/治疗（`HandleLyraHealthChanged`），**不要**再每帧 `SyncSlimeHealthFromLyra` 反写 CurrentHP（那会把史莱姆近战全吃掉）。
- Lyra 子弹打到**没有 ASC** 的 Actor（史莱姆、GASP 服装敌人、幻形体）走 `FLyraPlainWeaponDamage::OnHit`（`LyraGameplayAbility_RangedWeapon::ApplyPlainDamageToNonAbilityTargets`），由 `SlimeFable.cpp` 绑到 `ICombatDamageable::ApplyDamage` / `USlimeHealthComponent`。数值来自 `ULyraRangedWeaponInstance::PlainHitDamage`，由 `ALyraShooterEnemy::ApplyWeaponDamageOverrides` 按 `AIGunDamagePerHit`（AI 打玩家，默认低）/ `PlayerGunDamagePerHit`（幻形打敌人）覆盖；玩家幻形被打再乘 `MorphIncomingGunDamageScale`。
- 子弹要命中史莱姆/GASP，靶子的 `Pawn` / `CharacterMesh` / `CharacterCapsule` 碰撞预设必须 Block `Lyra_TraceChannel_Weapon(_Capsule)`（见 `DefaultEngine.ini` 的 `+EditProfiles`）。
- 死亡：`HandleDeath` 卸枪 → Ragdoll → `DeathRagdollSeconds` 后走 SlimeHealth 溶解；`bMorphTarget` 时只 `ForceUnmorph`。
- `IsDevourableNow()` **不能**看 `bDevourLocked`：`FreezeDevourTarget` 会先把它设 true，`TickPhase` 每帧再查 `CanDevourTarget`，一查就 Abort，表现为「按了 F 不缩小」。和 `AGaspSandboxPawn` 一致，只看 bDevourable / 幻形 / 死亡 / 玩家控制。

### 配枪 / AI 开火

- 背包和装备组件（`ULyraInventoryManagerComponent` / `ULyraEquipmentManagerComponent`）直接挂在 `ALyraShooterEnemy` 上，不在 PlayerController（官方在 PC 上，AI 没有 Lyra PC）。`BeginPlay → EnsureDefaultWeapon` 授 `DefaultWeaponItem`（默认 `ID_Rifle`）。
- AI 开火 = `TickAIWeapon` 一轮 = `[AIAimTelegraphSeconds 举枪预警][AIBurstSeconds 连射][AIBurstPauseSeconds 停]`，连射期模拟 `InputTag.Weapon.Fire(Auto)` Pressed/Released；`bAIInfiniteAmmo` 时补 `Lyra.ShooterGame.Weapon.SpareAmmo`。每轮开头调 `USlimeDodgeComponent::NotifyPlayerIncomingAttack`，史莱姆在预警 + 连射内按 RMB 就是完美闪避（`PerfectInvulnDuration` 无敌，子弹走 `SlimeHealth->ApplyDamage` 会被 `IsInvulnerable` 挡掉）。`USlimeDodgeComponent::IsInEnemyThreatRange` 把 `AIFireRange` 内的活体射击兵算作威胁，否则 RMB 是闪现不是翻滚。
- 朝向：B_Hero_Default 是 orient-to-movement，站桩时不转身。`AIFireRange` 内 `SetAIFacingMode(true)` 关掉 orient/controller-yaw，`FaceTarget` 按 `AIFaceTargetYawRate` 转 yaw；`AIController->SetFocalPoint` 保证控制旋转（枪的射线方向）每帧跟着玩家。被玩家 Possess / 死亡 / 被吞噬锁定时恢复原 BP 设置。
- 头顶血条：`HealthBar`（`USlimeWorldHealthBar`，挂胶囊顶 + `HealthBarZOffset`），`HealthBarVisibleRange` 1200 或被打过后常显；实现了 `ISlimeLockTarget`，中键锁定后走 HUD 顶栏血条。
- 队伍：AI=`EnemyTeamId`(2)，幻形体=`PlayerTeamId`(1)，在 `PostInitializeComponents` / `InitAsMorphTarget` 设，不要在构造函数设。
- 编辑器里摆的 BP 不是 A-pose：`OnConstruction` 在非 GameWorld 下 `SetUpdateAnimationInEditor(true)` + `InitAnim` + `EnsureAnimLayersLinked`。
- 武器 / 弹药台：`ASlimeLyraWeaponPickup`（不是官方 `B_WeaponSpawner`，那个给 PC Quickbar），给 `ALyraShooterEnemy` 的背包加 `PickupDefinition` 对应武器，已有则补 `AmmoMagazines` 个弹匣。摆台脚本 `Content/Python/populate_lyra_shooter_lab.py`（清掉 Tag `LyraShooterLabContent` 后重摆步枪 / 手枪 / 霰弹枪 + 掩体）。

### 玩家幻形层（`ActivateStandaloneForPlayer` / `DeactivateStandaloneForPlayer`）

`PossessedBy(PC)` 和 `USlimeMorphComponent::PossessMorphTarget` 都会调 Activate，每一步幂等；`UnPossessed` / `EndPlay` 调 Deactivate。

- **能力**：`SimplePawnData` 一个能力都不给，跳不起来就是这原因。Activate 里按 `StandaloneAbilitySets`（默认 `AbilitySet_ShooterHero`）用 `ULyraAbilitySet::GiveToAbilitySystemFiltered` 授予，`StandaloneExcludedAbilities` 默认剔掉 `GA_Hero_Death`（Lyra 死亡链会销毁幻形体，死亡归 SlimeHealth）、`GA_SpawnEffect`、`GA_ADS`（内部 `GetLyraPlayerControllerFromActorInfo`，我们的 PC 不是 Lyra PC，会报 Accessed None）。
- **队伍**：`EnsurePlayerTeam` 把 PlayerState（`ALyraPlayerState`，NoTeam 时）和幻形体都设成 `PlayerTeamId`。`ULyraDamageExecution → CanCauseDamage` 任一边 NoTeam 就 0 伤害。
- **换枪**：`USlimeLyraQuickBarComponent`（`ULyraQuickBarComponent` 子类，6 槽）由 Activate 挂到 PC 上，背包里所有可装备物品塞进槽位，之后**装备归快捷栏**（`EquipInventoryItem` / `GiveWeaponItem` 检测到 `PlayerQuickBar` 就改走 `SetActiveSlotIndex`，`SyncEquippedWeaponFromManager` 再把 `EquippedWeapon(+Item)` 从 EquipmentManager 镜像回来）。数字键 1..`WeaponQuickSlots` 与鼠标滚轮在 `TickQuickSlotKeys` 轮询 `ESlimeInputAction::Element1-6`（幻形期间 `USlimeAbilityComponent::PollAbilityKeys` 已锁，不冲突）。注意 `ULyraQuickBarComponent::FindEquipmentManager` 走 `Controller->GetPawn()`，`APlayerController::OnPossess` 在 `PossessedBy` 返回之后才 `SetPawn`，所以 `SetupPlayerQuickBar` 里 `PC->GetPawn()!=this` 直接返回，`PossessedBy` 用 `SetTimerForNextTick` 补一遍。玩家离开时 `TeardownPlayerQuickBar` 清槽、销毁组件并把最后那把枪重新装回身体。
- **HUD**：`ShowStandaloneHUD` 直接 `CreateWidget(PC, W_ShooterHUDLayout)->AddToPlayerScreen`（本工程没有 `PrimaryGameLayout`，不进 CommonUI 层栈，也**不要** `ActivateWidget`，否则 `ULyraHUDLayout` 的 `UI.Action.Escape` 会抢掉史莱姆的 Esc）。再用 `UUIExtensionSubsystem::RegisterExtensionAsWidgetForContext(SlotTag, LocalPlayer, Class)` 挂 `StandaloneHUDWidgets`：`W_WeaponReticleHost → HUD.Slot.Reticle`（准星 + 弹药弧）、`W_QuickBar → HUD.Slot.Equipment`（枪名 / 子弹 / 换弹）。控件建好后下一帧 `RebroadcastState()` 重发 QuickBar 的 SlotsChanged / ActiveIndexChanged 消息。`SlimeFable.Build.cs` 因此依赖 `UIExtension`。布局里 `StandaloneHUDRemovedWidgets`（默认 `W_Healthbar`）建好即 `RemoveFromParent`：史莱姆 HUD 左下角已是真血条，Lyra 那条是下限 1 的镜像。史莱姆 HUD 自己在 `USlimeCombatHUDWidget::Refresh` 里遇到 `UsesSelfContainedPlayerCombat()` 的幻形体就把底部 1-6 元素栏 `HotbarRoot` 折叠（1-6 已归 QuickBar）。`W_ShooterHUDLayout` 还引用 `W_ControllerDisconnected → W_LyraMenuButton`（在被跳过的 `Content/UI/Menu`），`import_lyra_shooter.py` 的 `copy_menu_button_closure` 只拉这颗按钮和它引用的 Menu/Art 材质闭包，缺它整个 HUD 布局编译失败（`Button_ChangeUser not found`）。
- **准星淘汰动画**：`W_Reticle_*` 监听 `Lyra.Elimination.Message` 后 `Cast Instigator → LyraPlayerState`。我们的 ASC 挂在 Pawn 上，Instigator 是 Pawn，原版必报 `Accessed None ... AsLyra_Player_State`；`ULyraHealthComponent::HandleOutOfHealth` 现在用 `GetPlayerStateFromObject` 能解出 PS 就换成 PS（AI 无 PS 仍留 Pawn）。
- **视角 / 蹲**：`InputLook` 里若 `bEnableLegacyInputScales`（史莱姆要保留）且 PC 的旧 `InputPitchScale` 为负，把 Y 再翻一次——Lyra `IMC_Default` 已按无 legacy 的工程把鼠标 Y 取反，叠上 -2.5 会二次翻转（鼠标上 → 镜头下）。蹲：`BindStandaloneInput` 按 `InputTag.Crouch` 找 Native，没有再搜 Ability，再没有硬加载 `/Game/Input/Actions/IA_Crouch`（不要用 GASP 的 `/Game/Input/IA_Crouch`），`Started → InputCrouch → ToggleCrouch`。`TickCrouch` 再轮询史莱姆 `Flatten`（默认 C）和左 Ctrl 的按下边沿，Enhanced Input 没挂上也能蹲。`ActivateStandaloneForPlayer` 调 `EnsurePlayerCrouch`：强制 `NavAgent.bCanCrouch`，站立胶囊半高 ≤ 蹲半高时把蹲半高收到 `Standing * 0.7`。幻形体没有 HeroComponent，不自带蹲。
- **右键**：`TickRightMouse` 轮询 `ESlimeInputAction::Dodge`（不是 Enhanced Input；`BindStandaloneInput` 跳过 `InputTag.Weapon.ADS`），并把幻形体上的 `MorphSlimeDodge.bPollRightMouse` 关掉。按住 ≥ `ADSHoldSeconds`(0.22) = ADS：`SetADSActive` 推 `ADSCameraMode`（`CM_ThirdPersonADS`，带 `Lyra.Weapon.SteadyAimingCamera`，武器散布靠这个 Tag 收紧）+ 叠 `IMC_ADS_Speed`；松开退出。点按 = `USlimeDodgeComponent::TryHandleRightClick`（威胁圈内翻滚 / 完美闪避 0.5 s 无敌，圈外闪现）。
- **血量双向**：玩家控制时 SlimeHealth 仍是真血；Lyra 子弹打到 → `HandleLyraHealthChanged` 按比例扣 SlimeHealth（×`MorphIncomingGunDamageScale`），随后 `SyncLyraHealthFromSlime` 把 SlimeHealth 百分比写回 `LyraHealthSet.Health`，**下限 1**（Lyra 血到 0 会走死亡事件）。`bSyncingLyraHealth` 防重入，`bLyraHealthSyncPending` 延到下一帧再写（ASC 正在执行 GE 时不能改属性）。

## 联机（Listen / Null）

玩法 PC 继承 `ALyraPlayerController`，GI 继承 `ULyraGameInstance`，PS 是 `ASlimeFablePlayerState`。控制台：

- `SlimeHostListen /Game/Maps/Sandbox/FeatureLabs/LyraShooterLab`
- `SlimeJoin 127.0.0.1`

Steam 默认关。主菜单开房按钮后接同一对 Exec，不要换 Lyra FrontEnd。

## 禁止

- 往日关卡堆射击敌人（FeatureLab 玩通之前）
- 整包覆盖 `Content/UI` 土色菜单
- 把官方 Lyra 工程改脏
