// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "Combat/SlimeDevourTarget.h"
#include "EnemyCombatTypes.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "Slime/FoliageInteractVolume.h"
#include "SlimeLockTarget.h"
#include "GaspSandboxPawn.generated.h"

class AGaspSandboxPawn;
class UAbilitySystemComponent;
class UAnimMontage;
class UCapsuleComponent;
class UCharacterMoverComponent;
class UChildActorComponent;
class UEnemyAttributeSet;
class UEnemyCombatComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class UMotionWarpingComponent;
class UNavMoverComponent;
class USlimeFoliageInteractComponent;
class USkeletalMeshComponent;
class USlimeHealthComponent;
class USlimeLockOnComponent;
class USlimeStatusComponent;
class UWidgetComponent;
struct FCharacterDefaultInputs;

/**
 * InputProducer bridge: call Blueprint ProduceInput first, then apply AI face / chase gait / freeze.
 * Keeps SandboxCharacter_Mover's BP graph as the authoritative producer.
 */
UCLASS()
class SLIMEFABLE_API UGaspMoverInputBridge : public UObject, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	void Init(AGaspSandboxPawn* InOwner);

	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

protected:
	UPROPERTY()
	TWeakObjectPtr<AGaspSandboxPawn> OwnerPawn;
};

/**
 * Thin C++ parent for a duplicated SandboxCharacter_Mover Blueprint.
 * Official locomotion / camera / Traversal / VisualOverride stay in BP.
 * This class only adds slime devour / combat / health / AI bridge hooks.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AGaspSandboxPawn : public APawn,
	public ISlimeDevourTarget,
	public ISlimeLockTarget,
	public ICombatDamageable,
	public IFoliageInteractVolume
{
	GENERATED_BODY()

public:
	AGaspSandboxPawn();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetFoliageInteractVolume(FVector& OutLocation, float& OutRadius) const override;
	virtual bool ShouldSuppressFoliageInteract() const override;
	virtual bool CanBeLockedOn() const override;
	virtual FVector GetLockOnLocation() const override;
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	virtual bool IsDevourableNow() const override;
	virtual float GetDevourHealthThreshold() const override { return DevourHealthThreshold; }
	virtual float GetHealthPercent() const override;
	virtual FText GetResolvedDisplayName() const override;
	virtual FLinearColor ResolveDevourWheelTint() const override;
	virtual USkeletalMeshComponent* GetPrimarySkeletalMesh() const override { return CachedSkeletalMesh; }
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const override;
	virtual USkeletalMeshComponent* GetMorphVisualMesh() const override { return GetDevourPreviewMesh(); }
	virtual UCapsuleComponent* GetDevourCapsule() const override { return CachedCapsule; }
	virtual USlimeHealthComponent* GetEnemyHealth() const override { return Health; }
	virtual USlimeStatusComponent* GetEnemyStatus() const override { return Status; }
	virtual UEnemyCombatComponent* GetEnemyCombat() const override { return Combat; }
	virtual const TArray<FEnemyMoveDef>& GetEnemyMoves() const override { return Moves; }
	virtual void ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const override;
	virtual void InitAsMorphTarget(AActor* Master) override;
	virtual void InitAsPhantom(float LifeSeconds, AActor* Master) override;
	virtual void BeginDevouredDeath(AActor* Devourer) override;
	virtual void SetDevourLocked(bool bLocked) override { bDevourLocked = bLocked; }
	virtual bool IsDevourLocked() const override { return bDevourLocked; }
	virtual bool IsMorphTarget() const override { return bMorphTarget; }
	virtual bool IsInDeathSequence() const override { return bDeathSequence; }
	virtual bool IsDevouredDeath() const override { return bDevouredDeath; }
	bool IsCombatKnockdown() const { return bCombatKnockdown; }
	virtual bool UsesSingleNodeAnims() const override { return false; }
	virtual bool UsesMoverMovement() const override { return true; }
	virtual void FreezeForDevour() override;
	virtual void RestoreFromDevour() override;
	virtual void StopMeshAnimation() override;
	virtual void ClearElementAuraFlash() override;
	virtual void PlayElementAuraFlash(ESlimeElement Element, float Duration) override;
	virtual void PlayElementAuraFlashByColor(FLinearColor Color, float Duration) override;
	virtual FVector GetVisualBoundsCenter() const override;
	virtual FVector GetHudAnchorLocation() const override;
	virtual float GetHealthBarZOffset() const override { return HealthBarZOffset; }
	virtual bool GetStableMeshBounds(FBox& OutBox) const override;
	virtual float GetMorphCameraArmLengthMin() const override { return MorphCameraArmLengthMin; }
	virtual void SetMorphGameplayEnabled(bool bEnabled) override;
	virtual void RefreshHealthBarAnchor() override;
	virtual TSubclassOf<APawn> GetDevourSpawnClass() const override { return GetClass(); }

	UFUNCTION(BlueprintPure, Category = "GASP")
	UCharacterMoverComponent* GetMoverComponent() const { return CachedMover; }

	UFUNCTION(BlueprintPure, Category = "GASP")
	UNavMoverComponent* GetNavMoverComponent() const { return CachedNavMover; }

	/** AI / direct-drive movement intent in world XY (unit-ish). */
	void SetAiMoveIntent(const FVector& WorldIntent);
	void ClearAiMoveIntent();
	/** AI facing intent in world XY — feed OrientationIntent via input bridge. */
	void SetAiFaceIntent(const FVector& WorldIntent) { AiFaceIntent = WorldIntent; }
	void ClearAiFaceIntent() { AiFaceIntent = FVector::ZeroVector; }
	bool WantsChaseGait() const { return bWantChaseGait; }
	void SetWantChaseGait(bool bIn) { bWantChaseGait = bIn; }
	const FVector& GetAiMoveIntent() const { return AiMoveIntent; }
	const FVector& GetAiFaceIntent() const { return AiFaceIntent; }
	bool IsMoverFrozen() const { return bMoverFrozen; }

	const TArray<FEnemyMoveDef>& GetMoves() const { return Moves; }
	void EnsureCombatReady() { EnsureMoveKit(); }

	UFUNCTION(BlueprintCallable, Category = "GASP")
	virtual void TriggerSandboxRagdoll();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ToolTip = "能否被史莱姆吞噬。默认开。"))
	bool bDevourable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "血量低于这个比例才能被吞噬。默认 0.2。"))
	float DevourHealthThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "锁定顶栏名字。空则显示「动作试样」。"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm",
			ToolTip = "血条在胶囊顶上方的额外厘米。默认 12。"))
	float HealthBarZOffset = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "0.0", Units = "cm",
			ToolTip = "幻形后最近 SpringArm 臂长。默认 90。"))
	float MorphCameraArmLengthMin = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "招式表。空则 BeginPlay 填默认近战（无 Dash）。"))
	TArray<FEnemyMoveDef> Moves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "探测玩家并进入追击的距离。默认 1200。"))
	float DetectRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "期望与玩家保持的水平距离。默认 180。"))
	float PreferredDistance = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "脱战牵制距离。默认 1800。"))
	float LeashRange = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0",
			ToolTip = "追击时步态用 Run。默认 600。"))
	float ChaseSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Patrol",
		meta = (ToolTip = "空闲时是否在出生点附近巡逻。默认开。"))
	bool bWanderWhenIdle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Patrol",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "巡逻半径。默认 500。"))
	float WanderRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Patrol",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "巡逻停顿最短秒数。默认 1.5。"))
	float WanderPauseMin = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Patrol",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "巡逻停顿最长秒数。默认 4。"))
	float WanderPauseMax = 4.f;

	FVector GetSpawnOrigin() const { return SpawnOrigin; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击闪红 / 水火暗物附着 Overlay。默认 /Game/_Slime/FX/M_EnemyHitFlash。空则不闪。"))
	TSoftObjectPtr<UMaterialInterface> HitFlashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "雷属性附着 Overlay。默认 MI_Mesh_Overlay_TeslaCoil_Player。空则退回 HitFlashMaterial。"))
	TSoftObjectPtr<UMaterialInterface> LightningHitOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "风属性附着 Overlay。默认 MI_EnemyHitOverlay_Wind。空则退回 HitFlashMaterial。"))
	TSoftObjectPtr<UMaterialInterface> WindHitOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Audio",
		meta = (ToolTip = "GASP 出招挥砍音。空则用默认 /Game/Audio/SFX/Combat/sfx_attack_01。"))
	TSoftObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat|Audio",
		meta = (ToolTip = "GASP 受击音。空则用默认 /Game/Audio/SFX/Combat/sfx_hit_01。"))
	TSoftObjectPtr<USoundBase> HitTakenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击前/后/左/右蒙太奇（UEFN shove _V）。空则用默认四向。"))
	TSoftObjectPtr<UAnimMontage> HitReactFront;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击后方蒙太奇。"))
	TSoftObjectPtr<UAnimMontage> HitReactBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击左方蒙太奇。"))
	TSoftObjectPtr<UAnimMontage> HitReactLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "受击右方蒙太奇。"))
	TSoftObjectPtr<UAnimMontage> HitReactRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "0.05", ClampMax = "2.0", Units = "s",
			ToolTip = "受击蒙太奇截断秒数（_V 后半是倒地，默认只播 0.6）。"))
	float HitReactSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "出招中是否被受击打断。默认关（只闪白）。"))
	bool bHitReactInterruptsAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Ragdoll",
		meta = (ToolTip = "打开后受击走官方 Ragdoll_OnHit，倒地走 TriggerRagdoll。默认开。"))
	bool bEnableRagdollKit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Ragdoll",
		meta = (ClampMin = "0.2", Units = "s",
			ToolTip = "战斗倒地的滑动伤害窗口秒数。默认 5。窗口内累计掉血达阈值才 ragdoll。"))
	float StunWindowSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Ragdoll",
		meta = (ClampMin = "0.05", ClampMax = "1.0",
			ToolTip = "窗口内累计掉血占 MaxHP 的比例，达到则倒地。默认 0.3（30%）。"))
	float StunDamagePercent = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Ragdoll",
		meta = (ClampMin = "0.1", Units = "s",
			ToolTip = "战斗倒地后调用官方爬起的延迟秒数。默认 1。死亡路径不爬起。"))
	float GetUpDelaySeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Death",
		meta = (ClampMin = "0.2", Units = "s",
			ToolTip = "备用：死亡动画加载失败时再等这么久才溶解。正常按倒地动画时长走。"))
	float DeathRagdollLingerSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Death",
		meta = (ClampMin = "0.2", Units = "s",
			ToolTip = "骨骼溶解时长。默认 1.2。只改 overlay，不关物理。"))
	float DeathDissolveSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|Death",
		meta = (ToolTip = "死亡溶解 Overlay。默认 /Game/_Slime/FX/M_EnemyDeathDissolve。"))
	TSoftObjectPtr<UMaterialInterface> DeathDissolveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "出生时 HP 比例。0.2 可直接测吞噬。0 关闭。Lab 默认 0.2。"))
	float DebugStartHealthPercent = 0.2f;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "解除幻形时要移除的 IMC。默认 /Game/Input/IMC_Sandbox。"))
	TSoftObjectPtr<UInputMappingContext> SandboxMapping;

protected:
	friend class UGaspMoverInputBridge;

	void ResolveBlueprintComponents();
	void EnsureInputBridge();
	void EnsureMoveKit();
	void EnsureMoverModes();
	void EnsureCapsuleIsRoot();
	void EnsureMeshTickAfterMover();
	void SuspendMoverSim();
	void ResumeMoverSim();
	void ApplyActiveVisualOnly();
	void BindWorldHealthBar();
	void AttachHealthBarToCapsule();
	void RefreshWorldHealthBarVisibility();
	void RemoveSandboxMappingFromController(AController* OldController);
	bool DispatchBoolEventOnComponents(FName FunctionName);
	bool CallBoolFunctionByName(FName FunctionName);
	void BindMorphLocomotionActions(APlayerController* PC);
	void UnbindMorphLocomotionActions();
	virtual void RestoreUnexpectedRagdoll();
	virtual bool WantsHeldRagdollMode() const;
	void ForcePhysicalRagdollBodies(bool bDisableCapsule);
	virtual void KeepDeathRagdollPhysics();
	void SetMorphLocomotionTicksEnabled(bool bEnabled);
	virtual bool ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs);
	virtual void BeginKnockdownDeath();
	void QueueDeathRagdoll();
	virtual void AccrueCombatStun(float Damage);
	virtual void BeginCombatKnockdown();
	virtual void TickCombatKnockdown();
	virtual void EndCombatKnockdown();
	virtual void ConfirmDeathRagdollThenStopAI();
	void StopDeathController();
	void SetSmartObjectPlayerLogic(bool bEnable);

	UFUNCTION()
	virtual void PlayCombatGetUp();

	UFUNCTION()
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	virtual void StartDeathDissolve();

	UFUNCTION()
	void TickDeathDissolve();

	UFUNCTION()
	void FinishDeathSequence();

	UFUNCTION()
	void MorphJumpStarted();

	UFUNCTION()
	void MorphJumpReleased();
	void PlayHitFlash();
	void TickHitFlash();
	void ClearHitFlash();
	float PlayDeathKnockdownMontage();
	void PlayHitReact(const FVector& DamageLocation);
	void StopHitReact();
	void TickMorphTrace(float DeltaSeconds);
	UAnimMontage* ResolveHitReactMontage(const FVector& DamageLocation) const;
	USkeletalMeshComponent* FindChildActorVisualMesh() const;
	static bool IsCameraAttachedMesh(const UMeshComponent* Mesh);

	UFUNCTION()
	virtual void HandleDied();

	UFUNCTION()
	void HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);

	/** World location at BeginPlay — AI wander origin. */
	FVector SpawnOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USlimeStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> EnemyAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UEnemyCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USlimeFoliageInteractComponent> FoliageInteract;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UGaspMoverInputBridge> InputBridge;

	/** Resolved from Blueprint SCS (SandboxCharacter_Mover kit). Names must NOT match BP SCS vars. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<UCapsuleComponent> CachedCapsule;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<USkeletalMeshComponent> CachedSkeletalMesh;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<UCharacterMoverComponent> CachedMover;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<UNavMoverComponent> CachedNavMover;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<UMotionWarpingComponent> CachedMotionWarping;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Z_Resolved")
	TObjectPtr<UChildActorComponent> CachedVisualOverride;

	UPROPERTY(Transient)
	TObjectPtr<USlimeLockOnComponent> MorphLockOn;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphJumpAction;

	UPROPERTY(Transient)
	TArray<uint32> MorphJumpBindingHandles;

	bool bDevourLocked = false;
	bool bMorphTarget = false;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bPhantomInstance = false;
	bool bMoverFrozen = false;
	bool bMoverSimSuspended = false;
	bool bWantChaseGait = false;
	bool bPendingRagdoll = false;
	bool bMeshTickAfterMoverWired = false;
	bool bMorphTraceHasPrev = false;
	/** Traversal started this frame — clear Jump from ProduceInput so Mover Jump 不会打断翻越。 */
	bool bSuppressJumpForTraversal = false;
	/** Tick restored mesh/capsule after a stray ragdoll notify — next ProduceInput forces Walking. */
	bool bForceWalkingAfterRagdollRestore = false;
	bool bDeathRagdollArmed = false;
	bool bCombatKnockdown = false;
	bool bCombatGetUpRequested = false;
	bool bDeathAwaitingRagdoll = false;
	FVector LastDamageLocation = FVector::ZeroVector;
	FVector LastDamageImpulse = FVector::ZeroVector;
	TArray<TPair<float, float>> StunHits;
	float CombatGetUpRequestedTime = 0.f;
	float CombatKnockdownStartTime = 0.f;
	float DeathAwaitingRagdollTime = 0.f;
	FTimerHandle CombatGetUpTimer;
	float DeathDissolveElapsed = 0.f;
	FTimerHandle DeathRagdollTimer;
	FTimerHandle DeathDissolveTimer;
	TObjectPtr<UMaterialInstanceDynamic> DeathDissolveMID;
	int32 MorphTraceFinalizeCount = 0;
	FVector MorphTracePrevCapsule = FVector::ZeroVector;
	FVector MorphTracePrevMesh = FVector::ZeroVector;
	FVector MorphTracePrevCamera = FVector::ZeroVector;
	FVector AiMoveIntent = FVector::ZeroVector;
	FVector AiFaceIntent = FVector::ZeroVector;
	TWeakObjectPtr<AActor> MorphMaster;
	TObjectPtr<UMaterialInstanceDynamic> HitFlashMID;
	float HitFlashRemaining = 0.f;
	float HitFlashStartTime = 0.f;
	bool bAuraFlashActive = false;
	bool bAuraOverlayUsesHitFlashParams = true;
	float AuraOverlayOpacityMul = 2.f;
	FTimerHandle HitFlashTimer;
	TWeakObjectPtr<UAnimMontage> ActiveHitReactMontage;
	FTimerHandle HitReactTimer;
};
