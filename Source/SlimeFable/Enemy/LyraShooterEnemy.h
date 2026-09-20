// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/LyraAbilitySet.h"
#include "Character/LyraCharacterWithAbilities.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "CombatDamageable.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeLockTarget.h"
#include "Enemy/EnemyCombatTypes.h"
#include "UIExtensionSystem.h"
#include "LyraShooterEnemy.generated.h"

class UAIPerceptionStimuliSourceComponent;
class UWidgetComponent;
class UEnemyCombatComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class ULyraAbilitySet;
class ULyraCameraMode;
class ULyraGameplayAbility;
class USlimeDodgeComponent;
class USlimeLyraQuickBarComponent;
class UUserWidget;
class ULyraEquipmentInstance;
class ULyraEquipmentManagerComponent;
class ULyraHealthComponent;
class ULyraInputConfig;
class ULyraInventoryItemDefinition;
class ULyraInventoryItemInstance;
class ULyraInventoryManagerComponent;
class ULyraPawnData;
class USlimeHealthComponent;
class USlimeStatusComponent;

/** One Lyra HUD widget to drop into a UIExtensionPoint slot of the HUD layout while the player drives this body. */
USTRUCT(BlueprintType)
struct FSlimeLyraHUDWidgetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	TSoftClassPtr<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD", meta = (Categories = "HUD.Slot"))
	FGameplayTag SlotTag;
};

/**
 * Possessable Lyra shooter for slime devour / morph.
 * Duplicated B_Hero_Default should reparent to this class (do not edit the official Heroes BP).
 *
 * Health: USlimeHealthComponent is the source of truth for devour / HUD. Lyra weapon damage lands on
 * LyraHealthComponent and is mirrored into it as a delta; slime melee goes straight into it.
 * Weapon: the pawn carries its own LyraInventoryManager + LyraEquipmentManager (Lyra puts them on the
 * controller via the ShooterCore experience, which we do not run). AI fires with bursts; a morphed
 * player fires through the same GAS abilities via Enhanced Input.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ALyraShooterEnemy : public ALyraCharacterWithAbilities,
	public ISlimeDevourTarget,
	public ISlimeLockTarget,
	public ICombatDamageable
{
	GENERATED_BODY()

public:
	ALyraShooterEnemy(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	/** Keep the team across possession changes (Lyra resets to NoTeam by default). */
	virtual FGenericTeamId DetermineNewTeamAfterPossessionEnds(FGenericTeamId OldTeamID) const override { return OldTeamID; }

	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ISlimeLockTarget: middle-mouse lock-on + HUD health bar, same as the GASP bodies.
	virtual bool CanBeLockedOn() const override;
	virtual FVector GetLockOnLocation() const override { return GetHudAnchorLocation(); }

	virtual bool IsDevourableNow() const override;
	virtual float GetDevourHealthThreshold() const override { return DevourHealthThreshold; }
	virtual float GetHealthPercent() const override;
	virtual FText GetResolvedDisplayName() const override;
	virtual FLinearColor ResolveDevourWheelTint() const override;
	virtual USkeletalMeshComponent* GetPrimarySkeletalMesh() const override;
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const override;
	virtual UCapsuleComponent* GetDevourCapsule() const override;
	virtual USlimeHealthComponent* GetEnemyHealth() const override { return Health; }
	virtual USlimeStatusComponent* GetEnemyStatus() const override { return Status; }
	virtual UEnemyCombatComponent* GetEnemyCombat() const override { return Combat; }
	virtual void ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const override;
	virtual void InitAsMorphTarget(AActor* Master) override;
	virtual void InitAsPhantom(float LifeSeconds, AActor* Master) override;
	virtual void BeginDevouredDeath(AActor* Devourer) override;
	virtual void SetDevourLocked(bool bLocked) override { bDevourLocked = bLocked; }
	virtual bool IsDevourLocked() const override { return bDevourLocked; }
	virtual bool IsMorphTarget() const override { return bMorphTarget; }
	virtual bool IsInDeathSequence() const override { return bDeathSequence; }
	virtual bool IsDevouredDeath() const override { return bDevouredDeath; }
	virtual bool UsesSingleNodeAnims() const override { return false; }
	virtual bool UsesMoverMovement() const override { return false; }
	virtual bool UsesExternalPossessInput() const override { return true; }
	virtual bool UsesExternalPossessCamera() const override { return true; }
	virtual bool UsesSelfContainedPlayerCombat() const override { return true; }
	virtual void FreezeForDevour() override;
	virtual void RestoreFromDevour() override;
	virtual void StopMeshAnimation() override;
	virtual void ClearElementAuraFlash() override {}
	virtual FVector GetVisualBoundsCenter() const override;
	virtual FVector GetHudAnchorLocation() const override;
	virtual float GetHealthBarZOffset() const override { return HealthBarZOffset; }
	virtual bool GetStableMeshBounds(FBox& OutBox) const override;
	virtual float GetMorphCameraArmLengthMin() const override { return 280.f; }
	virtual void SetMorphGameplayEnabled(bool bEnabled) override;
	virtual void RefreshHealthBarAnchor() override;
	virtual TSubclassOf<APawn> GetDevourSpawnClass() const override { return GetClass(); }

	/**
	 * Player took the body: PawnData + hero abilities (jump / ADS / dash), Enhanced Input, Lyra camera,
	 * quick bar (1..N keys), Lyra HUD (reticle / ammo), player team. Idempotent; runs from PossessedBy and the morph component.
	 */
	void ActivateStandaloneForPlayer(APlayerController* PC);

	/** Player left the body (unmorph / phantom expiry): tear down quick bar, HUD, mapping context, ADS. */
	void DeactivateStandaloneForPlayer(AController* OldController);

	bool IsADSActive() const { return bADSActive; }

	/**
	 * Add a Lyra inventory item (ID_Rifle_C ...) to this pawn. Already owned → refills spare ammo instead.
	 * bEquip swaps the currently equipped weapon for it. Authority only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Weapon")
	ULyraInventoryItemInstance* GiveWeaponItem(TSubclassOf<ULyraInventoryItemDefinition> ItemDef, bool bEquip = true);

	/** Equip an item already in the inventory (unequips the current weapon). */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Weapon")
	bool EquipInventoryItem(ULyraInventoryItemInstance* Item);

	UFUNCTION(BlueprintPure, Category = "Lyra|Weapon")
	ULyraInventoryItemInstance* FindInventoryItemOfDef(TSubclassOf<ULyraInventoryItemDefinition> ItemDef) const;

	UFUNCTION(BlueprintPure, Category = "Lyra|Weapon")
	ULyraInventoryItemInstance* GetEquippedWeaponItem() const { return EquippedWeaponItem; }

	/** Add Magazines * MagazineSize to Lyra.ShooterGame.Weapon.SpareAmmo. Returns rounds added. */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Weapon")
	int32 RefillSpareAmmo(ULyraInventoryItemInstance* Item, int32 Magazines);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ToolTip = "能否被史莱姆吞噬。默认开。"))
	bool bDevourable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "血量低于这个比例才能被吞噬。默认 0.2。"))
	float DevourHealthThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "锁定顶栏名字。空则显示「射击兵」。"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm",
			ToolTip = "血条在胶囊顶上方的额外厘米。默认 20。"))
	float HealthBarZOffset = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ClampMin = "0.0", ClampMax = "10000.0", Units = "cm",
			ToolTip = "玩家在这个距离内才显示头顶血条（被打过后一直显示）。远程兵默认 1200，比近战的 500 大。"))
	float HealthBarVisibleRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ToolTip = "出生自带的 Lyra 物品定义。默认 ShooterCore ID_Rifle。空 = 没枪。"))
	TSoftClassPtr<ULyraInventoryItemDefinition> DefaultWeaponItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ClampMin = "0.0", ClampMax = "200.0",
			ToolTip = "AI 开枪打到史莱姆等非 GAS 目标时每发伤害（PlainHitDamage）。默认 3。"))
	float AIGunDamagePerHit = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ClampMin = "0.0", ClampMax = "500.0",
			ToolTip = "玩家幻形后开枪打到非 GAS 目标（GASP 服装敌人等）每发伤害。默认 12。"))
	float PlayerGunDamagePerHit = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ToolTip = "AI 备弹自动补满。玩家幻形后不补。默认开。"))
	bool bAIInfiniteAmmo = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ClampMin = "0.0", ClampMax = "2.0",
			ToolTip = "玩家幻形成这个身体后，被 Lyra 枪（GAS 伤害）打到时的伤害倍率。默认 0.35。AI 身体不受影响。"))
	float MorphIncomingGunDamageScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ClampMin = "0.0", ClampMax = "20.0", Units = "s",
			ToolTip = "非吞噬死亡：布娃娃多少秒后开始溶解消失。默认 2.5。"))
	float DeathRagdollSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "100.0", ClampMax = "10000.0", Units = "cm",
			ToolTip = "看见玩家且距离小于此值才开枪。默认 2200。"))
	float AIFireRange = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "0.0", ClampMax = "5000.0", Units = "cm",
			ToolTip = "距离大于此值才继续逼近。默认 700。"))
	float AIPreferredRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "0.05", ClampMax = "10.0", Units = "s",
			ToolTip = "一次连射按住开火的秒数。默认 0.5。"))
	float AIBurstSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "0.0", ClampMax = "10.0", Units = "s",
			ToolTip = "两次连射之间松开的秒数。默认 1.4。"))
	float AIBurstPauseSeconds = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s",
			ToolTip = "每轮连射前的举枪预警秒数：此时已通知史莱姆 DodgeComponent（完美闪避窗口），但还没出子弹。默认 0.35。"))
	float AIAimTelegraphSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "0.0", ClampMax = "3600.0",
			ToolTip = "交战时身体转向玩家的角速度（度/秒）。默认 540。"))
	float AIFaceTargetYawRate = 540.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|AI",
		meta = (ClampMin = "-100.0", ClampMax = "200.0", Units = "cm",
			ToolTip = "瞄准点相对玩家 Actor 位置的 Z 偏移。默认 0。"))
	float AIAimHeightOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Team",
		meta = (ClampMin = "0", ClampMax = "254",
			ToolTip = "AI 敌人的 Lyra TeamId。Lyra 伤害执行只允许不同队互伤。默认 2。"))
	uint8 EnemyTeamId = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Team",
		meta = (ClampMin = "0", ClampMax = "254",
			ToolTip = "幻形 / 幻影身体的 Lyra TeamId，和 SlimePlayGameMode 给玩家 PlayerState 的一致。默认 1。"))
	uint8 PlayerTeamId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "幻形 / AI 用的 PawnData。默认 SimplePawnData。"))
	TSoftObjectPtr<ULyraPawnData> StandalonePawnData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "幻形时绑定的 Lyra InputConfig。默认 InputData_Hero（带 Weapon.Fire / FireAuto / Reload）。空则用 PawnData 的。"))
	TSoftObjectPtr<ULyraInputConfig> StandaloneInputConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "幻形时额外挂上的 IMC，默认 Lyra IMC_Default。"))
	TSoftObjectPtr<UInputMappingContext> StandaloneMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "幻形时 LyraCameraComponent 用的相机模式。PawnData.DefaultCameraMode 为空时兜底。默认 CM_ThirdPerson；没有它相机栈是空的，视角会卡在世界原点/地下。"))
	TSoftClassPtr<ULyraCameraMode> StandaloneCameraMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "右键长按瞄准（ADS）时的相机模式。默认 ShooterCore CM_ThirdPersonADS。GA_ADS 自己的 SetCameraMode 需要 LyraHeroComponent，我们没有，所以在这里切。"))
	TSoftClassPtr<ULyraCameraMode> ADSCameraMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "瞄准期间叠加的输入映射（压低鼠标灵敏度）。默认 ShooterCore IMC_ADS_Speed，即 GA_ADS 自己会加的那份。"))
	TSoftObjectPtr<UInputMappingContext> ADSMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "玩家幻形后额外授予的 Lyra 能力集（跳跃 / ADS / 冲刺 / 近战 / 换枪…）。默认 ShooterCore AbilitySet_ShooterHero。AI 身体不授予。"))
	TArray<TSoftObjectPtr<ULyraAbilitySet>> StandaloneAbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "上面能力集里要跳过的能力。默认 GA_Hero_Death（Lyra 死亡流程会销毁幻形身体，死亡由 SlimeHealth 接管）和 GA_SpawnEffect。"))
	TArray<TSoftClassPtr<ULyraGameplayAbility>> StandaloneExcludedAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ClampMin = "0.05", ClampMax = "1.0", Units = "s",
			ToolTip = "右键按住超过这么久算瞄准（ADS，松开退出）；更短的点按算完美闪避 / 翻滚。默认 0.22。"))
	float ADSHoldSeconds = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "玩家幻形后 AddToViewport 的 Lyra HUD 布局（准星 / 血条 / 弹药槽的挂点）。默认 ShooterCore W_ShooterHUDLayout。空 = 不显示 Lyra HUD。"))
	TSoftClassPtr<UUserWidget> StandaloneHUDLayoutClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "注册到 HUD 布局挂点的 Lyra 控件。默认 W_WeaponReticleHost→HUD.Slot.Reticle（准星 + 弹药弧）、W_QuickBar→HUD.Slot.Equipment（枪名 / 子弹数 / 换弹提示）。"))
	TArray<FSlimeLyraHUDWidgetEntry> StandaloneHUDWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "HUD 布局里要移除的内建子控件。默认 W_Healthbar：史莱姆 HUD 左下角已经有真血条，Lyra 那条只是镜像（下限 1），两条会打架。"))
	TArray<TSoftClassPtr<UUserWidget>> StandaloneHUDRemovedWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "Lyra HUD 布局的 ZOrder。史莱姆 HUD 之上一点即可。默认 5。"))
	int32 StandaloneHUDZOrder = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weapon",
		meta = (ClampMin = "1", ClampMax = "9",
			ToolTip = "玩家幻形后的武器快捷槽数，数字键 1..N 切枪（W_QuickBar 只画前 3 格）。默认 6。"))
	int32 WeaponQuickSlots = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "没装备武器时链接到 ABP_Mannequin_Base 的动画层。默认 ABP_UnarmedAnimLayers；没有它骨骼是错的。"))
	TSoftClassPtr<UAnimInstance> DefaultAnimLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "Mesh 为空时兜底的身体。默认 SKM_Manny。软引用，PostInitializeComponents 才加载。"))
	TSoftObjectPtr<USkeletalMesh> DefaultBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lyra",
		meta = (ToolTip = "AnimClass 为空时兜底的动画蓝图。默认 ABP_Mannequin_Base。软引用，禁止在构造函数硬加载。"))
	TSoftClassPtr<UAnimInstance> DefaultAnimClass;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<USlimeStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UEnemyCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> AIPerceptionStimuliSource;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<ULyraInventoryManagerComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<ULyraEquipmentManagerComponent> EquipmentManager;

	/** Screen-space USlimeWorldHealthBar above the capsule, same widget as the GASP / slime enemies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<ULyraEquipmentInstance> EquippedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<ULyraInventoryItemInstance> EquippedWeaponItem;

	UFUNCTION()
	void HandleLyraHealthChanged(ULyraHealthComponent* LyraHealthComp, float OldValue, float NewValue, AActor* DamageInstigator);

	UFUNCTION()
	void HandleSlimeDied();

	UFUNCTION()
	void HandleSlimeHealthChanged(float CurrentHP, float MaxHP);

	/** Player-driven body: SlimeHealth is the truth; push its fraction into LyraHealthSet (never below 1). */
	void SyncLyraHealthFromSlime();

	// --- morph player layer (only while a PlayerController owns this body) -------------------------
	bool IsPlayerMorphBody() const;
	void EnsurePlayerTeam(APlayerController* PC);
	void GrantStandaloneHeroAbilities();
	void AddStandaloneMappingContext(APlayerController* PC);
	void RemoveStandaloneMappingContext(AController* OldController);
	void SetupPlayerQuickBar(APlayerController* PC);
	void TeardownPlayerQuickBar();
	void RebroadcastQuickBar();
	/** EquipmentManager is the truth once the quick bar drives equips; mirror it into EquippedWeapon(+Item). */
	void SyncEquippedWeaponFromManager();
	void ShowStandaloneHUD(APlayerController* PC);
	void HideStandaloneHUD();
	void EnsurePlayerCrouch();
	void TryToggleCrouch();
	void InputCrouch(const FInputActionValue& Value);
	void InputQuickSlot(int32 SlotIndex);
	void InputCycleSlot(bool bForward);
	void TickQuickSlotKeys();
	void TickRightMouse();
	void TickCrouch();
	void SetADSActive(bool bActive);
	USlimeDodgeComponent* FindMorphDodge();

	void EnsureBodyVisuals();
	/**
	 * Kick an async load of every Standalone* / HUD / weapon soft ref the morph layer LoadSynchronous()es.
	 * Runs once per process from BeginPlay (first Lyra body in a level); the handle is kept alive so
	 * PossessEnemy no longer pays a FlushAsyncLoading hitch for ShooterCore GA / HUD / fonts.
	 */
	void PreloadStandaloneAssets();
	void EnsureAnimLayersLinked();
	void RelinkWeaponAnimLayers();
	void EnsureDefaultWeapon();
	void ApplyWeaponDamageOverrides();
	void BindLyraHealthMirror();
	void TrySetTeam(uint8 TeamId);
	void GrantStandaloneAbilities();
	void BindStandaloneInput(APlayerController* PC);
	/** ULyraHeroComponent normally feeds the camera mode; we never reach its init state, so feed it here. */
	void BindStandaloneCamera();
	TSubclassOf<ULyraCameraMode> DetermineStandaloneCameraMode() const;
	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputAbilityPressed(FGameplayTag InputTag);
	void InputAbilityReleased(FGameplayTag InputTag);
	void ProcessLyraAbilityInput(float DeltaSeconds);
	void TickSimpleChase(float DeltaSeconds);
	void TickAIWeapon(float DeltaSeconds, APawn* Target, float Distance);
	void SetAIFiring(bool bFire);
	/** Rotate the body toward the target (AI only; Lyra hero BPs orient to movement, which never faces a strafing target). */
	void FaceTarget(const FVector& TargetLocation, float DeltaSeconds);
	void SetAIFacingMode(bool bFaceTarget);
	void BindWorldHealthBar();
	void RefreshWorldHealthBarVisibility();

	UPROPERTY()
	TArray<FLyraAbilitySet_GrantedHandles> GrantedHandles;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDLayoutWidget;

	TArray<FUIExtensionHandle> HUDExtensionHandles;
	TWeakObjectPtr<USlimeLyraQuickBarComponent> PlayerQuickBar;
	TWeakObjectPtr<USlimeDodgeComponent> CachedMorphDodge;
	TWeakObjectPtr<UEnhancedInputComponent> BoundStandaloneInput;
	FTimerHandle QuickBarRebroadcastHandle;

	bool bHeroAbilitiesGranted = false;
	bool bStandaloneMappingAdded = false;
	bool bRightMouseHeld = false;
	bool bADSActive = false;
	bool bSyncingLyraHealth = false;
	bool bLyraHealthSyncPending = false;
	float RightMousePressTime = 0.f;
	float LastCrouchToggleTime = -1.f;

	bool bDevourLocked = false;
	bool bMorphTarget = false;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bStandaloneReady = false;
	bool bLyraHealthBound = false;
	bool bAIFiring = false;
	bool bAIFacingTarget = false;
	bool bSavedOrientToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedUseControllerRotationYaw = false;
	int32 AILastTelegraphCycle = -1;
	float AIBurstClock = 0.f;
	float AIDebugLogClock = 0.f;
	float TickDebugLogClock = 0.f;
	FTimerHandle DeathDissolveHandle;
	TWeakObjectPtr<AActor> MorphMaster;
};
