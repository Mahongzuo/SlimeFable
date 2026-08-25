// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "EnemyCombatTypes.h"
#include "EnemyEncounterSubsystem.h"
#include "Slime/SlimeElementTypes.h"
#include "SlimeLockTarget.h"
#include "EnemyCharacter.generated.h"

class UEnemyCombatComponent;
class UAbilitySystemComponent;
class UEnemyAttributeSet;
class UGameplayEffect;
enum class EEnemyCombatRole : uint8;
class USlimeHealthComponent;
class USlimeStatusComponent;
class UWidgetComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMeshComponent;
class UAnimMontage;
class UAnimInstance;
class UAnimationAsset;
class USkeletalMesh;
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USlimeSouvenirDefinition;
class UQuestObjectiveComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AEnemyCharacter : public ACharacter, public ISlimeLockTarget, public ICombatDamageable
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	USlimeHealthComponent* GetEnemyHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	USlimeStatusComponent* GetEnemyStatus() const { return Status; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UEnemyCombatComponent* GetEnemyCombat() const { return Combat; }

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	UAbilitySystemComponent* GetEnemyAbilitySystem() const { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	EEnemyCombatRole GetCombatRole() const { return CombatRole; }

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	bool RequestAttackSlot(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void ReleaseAttackSlot();

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	int32 GetEncounterPhase() const;

	/** Applies a timed state GameplayEffect (stagger / guard / invulnerable / super armor / empower). */
	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	bool ApplyTimedState(TSubclassOf<UGameplayEffect> EffectClass, float Duration, float Power = 1.f);

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	bool HasCombatStateTag(FGameplayTag Tag) const;

	/** Poise break / scripted stagger. Interrupts the current attack and opens a counter window. */
	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void EnterStagger(float Duration, AActor* StaggerInstigator);

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	bool IsStaggered() const;

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	float GetPoisePercent() const;

	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	float GetHealthPercent() const;

	/** Called from UEnemyAttributeSet once GAS has resolved an attribute change. */
	void OnGasDamageApplied(float Damage, AActor* DamageInstigator);
	void OnGasHealingApplied(float Healing, AActor* HealingInstigator);
	void OnPoiseBroken(AActor* PoiseInstigator);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual bool CanBeLockedOn() const override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual FVector GetLockOnLocation() const override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UStaticMeshComponent* GetPlaceholderMesh() const { return PlaceholderMesh; }

	const TArray<TObjectPtr<USceneComponent>>& GetGeneratedParts() const { return GeneratedParts; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsInDeathSequence() const { return bDeathSequence; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsPhantomInstance() const { return bPhantomInstance; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevouredDeath() const { return bDevouredDeath; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevourLocked() const { return bDevourLocked; }

	void SetDevourLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevourableNow() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetPhantomMaster() const { return PhantomMaster.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitAsPhantom(float LifeSeconds, AActor* Master);

	/** True while this enemy is the player's morph body (possessed slime disguise). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsMorphTarget() const { return bMorphTarget; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetMorphMaster() const { return MorphMaster.Get(); }

	/**
	 *  Turns this enemy into a player-controllable morph body: player team, no AI, a
	 *  temporary third-person camera, and movement input bindings copied from the slime.
	 *  Call after SpawnActorDeferred and before FinishSpawning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitAsMorphTarget(AActor* Master);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void BeginDevouredDeath(AActor* Devourer);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void BeginPhantomExpire();

	FLinearColor ResolveDevourWheelTint() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PlayHitFlash();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PlayElementHitFlash(FLinearColor FlashColor);

	/** Sustained elemental aura using the per-element overlay table (Lightning/Wind special MIs). */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PlayElementAuraFlash(ESlimeElement Element, float Duration);

	/** Sustained tint via M_EnemyHitFlash + HitColor (reaction residue / color-only callers). */
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (DisplayName = "Play Element Aura Flash (Color)"))
	void PlayElementAuraFlashByColor(FLinearColor FlashColor, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearElementAuraFlash();


	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void HandleDeath() override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	/** Soft / skeletal mesh kit assembled in Construction Script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh")
	TArray<FEnemyMeshPart> MeshParts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh",
		meta = (ToolTip = "主骨骼网格。空则编辑器/游戏显示占位立方体。按日 BP 用脚本绑，不要写死在 C++。"))
	TSoftObjectPtr<USkeletalMesh> PrimarySkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh",
		meta = (ToolTip = "主骨骼用的 AnimBP。填了才会站姿/Idle；空则 T-pose。和 PrimarySkeletalMesh 一起绑。"))
	TSoftClassPtr<UAnimInstance> PrimaryAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh",
		meta = (ToolTip = "勾选后按主骨骼参考姿势包围盒重算胶囊半径/半高，并把 Mesh 的 Z 偏移设为 -半高。网格和默认 192cm 胶囊对不上时用来消悬空/陷地。默认开。"))
	bool bAutoFitCapsuleToMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "敌人在遭遇中的职责：看门狗追击、武士决斗、机枪手压制、天皇指挥。"))
	EEnemyCombatRole CombatRole = EEnemyCombatRole::Duelist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ToolTip = "硬直资源，受击会消耗；归零时进入失衡。默认 100。"))
	float MaxPoise = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ToolTip = "格挡资源，供后续 Boss/武士 Ability 使用。"))
	float Guard = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ClampMax = "3.0",
			ToolTip = "每点伤害转成多少硬直伤害。0 = 打不出失衡。默认 0.6，配合 MaxPoise 100 约 3~5 下破防。"))
	float PoiseDamageRatio = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ToolTip = "脱战/未受击时每秒回复的硬直。默认 12。"))
	float PoiseRegenPerSecond = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "硬直归零后的失衡时长，也是玩家的反击窗口。默认 1.2 秒。"))
	float StaggerDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "格挡状态下的伤害减免比例。0.65 = 只吃 35% 伤害。仅武士等有 Guard 的单位生效。"))
	float GuardDamageReduction = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "击退抗性。0 完全吃击退，1 完全免疫。空中时竖直击飞不再叠加。默认 0。"))
	float KnockbackResistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ToolTip = "能否被史莱姆吞噬。塔默认关。幻形实例运行时会关掉。"))
	bool bDevourable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "血量低于这个比例才能被吞噬。0.2 = 剩 20% 血。0 则改用史莱姆吞噬组件上的全局阈值。默认 0.2，避免残血窗口太窄被普攻打死。"))
	float DevourHealthThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ToolTip = "勾选后攻击伤害归零（含近战/投射）。SlimeLab 测试桩勾上。"))
	bool bHarmless = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "出生时把 HP 设为 MaxHP 的这个比例。0 表示关闭。设 0.2 可直接测吞噬。脱战复位也会再套一次。"))
	float DebugStartHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ToolTip = "勾选后关掉脱战/卡住回出生点（掉虚空仍会拉回）。SlimeLab 测试桩运行时会自动勾。"))
	bool bSuppressOutOfCombatReset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "锁定顶栏显示的名字。每个拖进关卡的实例都可以改。空则显示「敌人」，不会用 BP_ 内部名。"))
	FText DisplayName;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetResolvedDisplayName() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm",
		ToolTip = "血条在网格顶上方的额外厘米。默认 12。网格用参考姿势包围盒，不跟动画抖。没网格时退回胶囊顶。"))
	float HealthBarZOffset = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "100.0", Units = "cm"))
	float HealthBarVisibleRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence")
	bool bAllowDespawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float ActiveRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float SleepRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float DespawnRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "0.0", Units = "s"))
	float OutOfCombatResetSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float VoidResetDepth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "50.0", Units = "cm"))
	float StuckResetDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats",
		meta = (ToolTip = "死亡时播放的 Montage。播完后冻结最后一帧再溶解，不会混回 Idle。空则直接溶解。"))
	TSoftObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death", meta = (ClampMin = "0.2", Units = "s",
		ToolTip = "溶解持续时间（秒）。默认 1.2。到点后销毁 Actor。"))
	float DeathDissolveSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ToolTip = "死亡时在身体中心生成的 Niagara。骨架没有 head socket 时用网格包围盒中心，避免特效埋在脚底。"))
	TSoftObjectPtr<UNiagaraSystem> DeathDissolveNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ToolTip = "叠在骨骼上的溶解 Overlay 材质，驱动 DissolveAmount。默认 /Game/_Slime/FX/M_EnemyDeathDissolve。空则只靠淡出网格。"))
	TSoftObjectPtr<UMaterialInterface> DeathDissolveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击闪红 / 水火暗物附着 Overlay。默认 /Game/_Slime/FX/M_EnemyHitFlash。空则不闪。驱动 HitFlash+HitColor。"))
	TSoftObjectPtr<UMaterialInterface> HitFlashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "雷属性附着 Overlay。默认 MI_Mesh_Overlay_TeslaCoil_Player。空则退回 HitFlashMaterial。"))
	TSoftObjectPtr<UMaterialInterface> LightningHitOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "风属性附着 Overlay。默认 MI_Overlay_Player_DeBuff。空则退回 HitFlashMaterial。"))
	TSoftObjectPtr<UMaterialInterface> WindHitOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "受击闪红持续时间（秒）。默认 0.35，与史莱姆一致。"))
	float HitFlashDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "1.0",
		ToolTip = "受击闪红脉冲频率。默认 18。"))
	float HitFlashFrequency = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "元素附着闪光：亮起时长（秒）。默认 0.5。"))
	float AuraFlashOnSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "元素附着闪光：恢复本色时长（秒）。默认 1.0。"))
	float AuraFlashOffSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Souvenir")
	TSoftObjectPtr<USlimeSouvenirDefinition> SouvenirReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Souvenir")
	TSoftClassPtr<AActor> SouvenirDropClass;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void ApplyWeekDifficulty(int32 InWeekIndex);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Presence")
	virtual void SetEnemyPresence(EEnemyPresence NewPresence);

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	EEnemyPresence GetEnemyPresence() const { return Presence; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	bool WantsCombatBudget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	FTransform GetSpawnTransform() const { return SpawnTransform; }

	float GetSavedHP() const { return SavedHP; }
	void SetSavedHP(float InHP) { SavedHP = InHP; }

	/** World-space center of visible mesh parts (primary / MeshParts / placeholder). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetVisualBoundsCenter() const;

	/** Reference-pose mesh box in world space (no animation jitter). */
	bool GetStableMeshBounds(FBox& OutBox) const;

	/** Actor XY + cached mesh top Z. Ignores animation jitter; follows the capsule. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHudAnchorLocation() const;

	void RefreshWorldHealthBarVisibility(const APawn* Player, const AActor* LockedTarget);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Presence")
	void RestoreToSpawn();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	virtual bool UsesSingleNodeAnims() const { return false; }

	void PlayMeshAnimation(UAnimationAsset* Asset, bool bLoop);
	void StopMeshAnimation();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	virtual bool IsInCombat() const;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Enhanced Input handlers for morph body movement / look. */
	void MorphMove(const FInputActionValue& Value);
	void MorphLook(const FInputActionValue& Value);
	void UpdateMorphSafeTransform();

	virtual void OnRestoredToSpawn();
	void TickOutOfCombatReset(float DeltaSeconds);
	void ApplyHealthBarOffset();
	void RefreshWorldHealthBarVisibility();
	void UpdateHudAnchorCache() const;
	void RebuildMeshParts();
	void FitCapsuleToMesh();
	void ApplySingleNodeAnimModeIfNeeded();
	void ClearGeneratedParts();
	USceneComponent* ResolveAttachParent(const FEnemyMeshPart& Part) const;
	void ApplyPlaceholderVisual();
	void EnsureDefaultQuestIds();
	void EnsureDefaultDisplayName();

	virtual void ApplyDifficultyToCombat(float DamageMul, float IntervalMul);
	void CaptureDifficultyBases();
	void DropSouvenirReward();
	void PlayDeathMontageThenDissolve();
	void FreezeDeathPoseAndDissolve();
	void StartDeathDissolve();
	void TickDeathDissolve();
	void FinishDeathSequence();
	void ApplyDeathDissolveVisual(float Alpha);
	void TickHitFlash();
	void ClearHitFlashOverlay();
	void ApplyHitFlashToMesh(UMeshComponent* MeshComp);
	void ApplyHitFlashToAllMeshes();
	void ClearHitFlashFromMeshes();
	UMaterialInterface* ResolveElementHitOverlay(ESlimeElement Element) const;
	void BeginAuraOverlay(UMaterialInterface* FlashMat, bool bUsesHitFlashParams, float Duration);
	void DriveAuraOverlayIntensity(float Pulse);
	void ApplyPhantomVisuals();
	void ApplyLabDummyFlags();
	void ApplyHealthOverridesAfterBeginPlay();

	UFUNCTION()
	void HandleDied();

	/** Mirrors legacy health-component changes back into the GAS Health attribute. */
	UFUNCTION()
	void HandleHealthFacadeChanged(float CurrentHP, float FacadeMaxHP);

	void InitAbilitySystem();
	void TickPoiseRegen(float DeltaSeconds);
	void OnGuardBroken(AActor* GuardInstigator);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USlimeStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> EnemyAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UEnemyCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UQuestObjectiveComponent> Objective;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> GeneratedParts;

	UPROPERTY(Transient)
	EEnemyPresence Presence = EEnemyPresence::Active;

	FTransform SpawnTransform = FTransform::Identity;
	/** Re-entrancy guard: true while GAS is forwarding damage into the legacy health facade. */
	bool bRoutingGasDamage = false;
	bool bSyncingHealthFacade = false;
	FVector PendingDamageLocation = FVector::ZeroVector;
	FVector PendingDamageImpulse = FVector::ZeroVector;
	float PoiseRegenBlockedUntil = 0.f;
	float SavedHP = -1.f;
	bool bPresenceRegistered = false;
	float OutOfCombatSeconds = 0.f;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bDevourLocked = false;
	mutable bool bHudAnchorCached = false;
	mutable float HudAnchorRelZ = 0.f;
	bool bPhantomInstance = false;
	float PhantomLifeSeconds = 5.f;
	TWeakObjectPtr<AActor> PhantomMaster;
	FTimerHandle PhantomLifeTimer;

	// ---- Morph target (player disguise) -----------------------------------------
	bool bMorphTarget = false;
	TWeakObjectPtr<AActor> MorphMaster;
	FTransform MorphSafeTransform = FTransform::Identity;
	bool bHasMorphSafeTransform = false;
	/** Temporary third-person camera created in InitAsMorphTarget. */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> MorphCameraBoom;
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> MorphFollowCamera;
	/** Input action assets copied from the slime so Enhanced Input works on this pawn. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphMoveAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphLookAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphMouseLookAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphJumpAction;
	bool bDifficultyBasesCaptured = false;
	float DifficultyBaseMaxHP = 200.f;
	float DeathDissolveElapsed = 0.f;
	FTimerHandle DeathDissolveTimer;
	FTimerHandle DeathMontageFallbackTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DeathDissolveMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitFlashMID;

	float HitFlashRemaining = 0.f;
	float HitFlashStartTime = 0.f;
	float ActiveFlashDuration = 0.35f;
	float ActiveFlashFrequency = 18.f;
	bool bAuraFlashActive = false;
	/** When false, aura uses specialty overlays (TeslaCoil / DeBuff) via Opacity Multiplier + show/hide. */
	bool bAuraOverlayUsesHitFlashParams = true;
	float AuraOverlayOpacityMul = 2.f;
	FTimerHandle HitFlashTimer;
};
