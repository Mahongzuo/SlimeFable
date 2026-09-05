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
#include "GaspMoverEnemy.generated.h"

class UAbilitySystemComponent;
class APlayerController;
class UAnimMontage;
class UCameraComponent;
class UCapsuleComponent;
class UCharacterMoverComponent;
class UEnemyAttributeSet;
class UEnhancedInputComponent;
class UEnemyCombatComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class UNavMoverComponent;
class USlimeFoliageInteractComponent;
class USkeletalMeshComponent;
class USlimeHealthComponent;
class USlimeLockOnComponent;
class USlimeStatusComponent;
class USpringArmComponent;
class UWidgetComponent;
struct FInputActionValue;
struct FCharacterDefaultInputs;

UENUM(BlueprintType)
enum class EGaspVisualOverride : uint8
{
	UEFN UMETA(DisplayName = "UEFN Mannequin"),
	Manny,
	Quinn,
	Echo,
	TwinBlast,
	UE4 UMETA(DisplayName = "UE4 Mannequin")
};

/** Devourable GASP pawn: Mover locomotion + runtime visual retarget. Not ACharacter. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AGaspMoverEnemy : public APawn,
	public ISlimeDevourTarget,
	public ISlimeLockTarget,
	public ICombatDamageable,
	public IFoliageInteractVolume,
	public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	AGaspMoverEnemy();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

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
	virtual USkeletalMeshComponent* GetPrimarySkeletalMesh() const override { return SourceMesh; }
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const override;
	virtual USkeletalMeshComponent* GetMorphVisualMesh() const override { return GetDevourPreviewMesh(); }
	virtual UCapsuleComponent* GetDevourCapsule() const override { return Capsule; }
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
	UCharacterMoverComponent* GetMoverComponent() const { return Mover; }

	UFUNCTION(BlueprintCallable, Category = "0_Config|GASP")
	void SetVisualOverride(EGaspVisualOverride Override);

	/** Fill a BP struct (S_CharacterPropertiesForAnimation) for SandboxCharacter_Mover_ABP. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GASP",
		meta = (CustomStructureParam = "OutProperties",
			ToolTip = "把当前速度/步态/站姿写入任意结构体（给 Get_PropertiesForAnimation 用）。"))
	void FillGaspAnimProperties(UPARAM(ref) int32& OutProperties);
	DECLARE_FUNCTION(execFillGaspAnimProperties);

	/**
	 * Native body for BPI_SandboxCharacter_Pawn::Get_PropertiesForAnimation.
	 * Wired at runtime onto the BP function so SandboxCharacter_Mover_ABP gets real velocity/gait
	 * without requiring a hand-authored Blueprint interface graph.
	 */
	DECLARE_FUNCTION(execGet_PropertiesForAnimation);

	/** Native body for BPI_SandboxCharacter_Pawn::Get_PropertiesForTraversal (AC_TraversalLogic). */
	DECLARE_FUNCTION(execGet_PropertiesForTraversal);

	/** AI / direct-drive movement intent in world XY (unit-ish). */
	void SetAiMoveIntent(const FVector& WorldIntent) { AiMoveIntent = WorldIntent; }
	void ClearAiMoveIntent() { AiMoveIntent = FVector::ZeroVector; }
	/** AI facing intent in world XY — feed OrientationIntent (do not SetActorRotation; Mover overwrites). */
	void SetAiFaceIntent(const FVector& WorldIntent) { AiFaceIntent = WorldIntent; }
	void ClearAiFaceIntent() { AiFaceIntent = FVector::ZeroVector; }
	bool WantsChaseGait() const { return bWantChaseGait; }
	void SetWantChaseGait(bool bIn) { bWantChaseGait = bIn; }

	const TArray<FEnemyMoveDef>& GetMoves() const { return Moves; }
	/** Ensure Moves kit is filled (safe to call from AI OnPossess before BeginPlay). */
	void EnsureCombatReady() { EnsureMoveKit(); }

	/** BP / Traversal component implements this. Return true if a vault/mantle started. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GASP")
	bool TryTraversalAction();

	UFUNCTION(BlueprintCallable, Category = "GASP")
	void TriggerSandboxRagdoll();

	/** BP / Smart Object component implements this. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GASP")
	bool TrySmartObjectInteract();

	/** BP / interaction chooser implements this. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GASP")
	bool TryTakedown();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP",
		meta = (ToolTip = "可见覆盖网格。MM 永远跑隐藏的 UEFN 源骨架。"))
	EGaspVisualOverride VisualOverride = EGaspVisualOverride::Echo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP",
		meta = (ToolTip = "调试：同时显示橙色 UEFN 源网格。默认关。"))
	bool bDebugShowSourceMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|LookAt",
		meta = (ToolTip = "LookAt POI Actor。空则幻形时看锁定目标。"))
	TObjectPtr<AActor> LookAtPOI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|GASP|LookAt",
		meta = (ToolTip = "是否叠加 LookAt。默认开。"))
	bool bEnableLookAt = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "出生时 HP 比例。0.2 可直接测吞噬。0 关闭。"))
	float DebugStartHealthPercent = 0.f;

protected:
	void ApplyVisualOverride();
	void EnsureMoverModes();
	void EnsureMoveKit();
	void SnapSourceMeshToCapsule();
	void SnapVisualMeshToSource(USkeletalMeshComponent* Mesh);
	void BindWorldHealthBar();
	void BindMorphInput(UEnhancedInputComponent* EnhancedInput);
	void MorphMove(const FInputActionValue& Value);
	void MorphMoveStopped(const FInputActionValue& Value);
	void MorphLook(const FInputActionValue& Value);
	void MorphJumpStarted();
	void MorphJumpReleased();
	void MorphCrouchStarted();
	void MorphSprintStarted();
	void MorphSprintStopped();
	void MorphInteractStarted();
	void MorphTakedownStarted();
	void MorphTriggerRagdollStarted();
	void AddSandboxMapping(APlayerController* PC);
	void RemoveSandboxMapping(APlayerController* PC);
	USkeletalMeshComponent* FindVisualMesh(EGaspVisualOverride Override) const;
	USkeletalMeshComponent* ResolveActiveVisualMesh() const;
	bool DispatchBoolEventOnComponents(FName FunctionName);
	UInputAction* LoadSoftAction(const TSoftObjectPtr<UInputAction>& SoftAction) const;
	void FillGaspAnimPropertiesInternal(UScriptStruct* Struct, void* StructPtr);
	void FillGaspTraversalPropertiesInternal(UScriptStruct* Struct, void* StructPtr);
	void WriteMoverCustomInputs(FMoverInputCmdContext& InputCmdResult) const;
	FString ResolveDesiredGaitName() const;
	void UpdateAnimMotionState(float DeltaSeconds);
	/** Register BPI_SandboxCharacter_Pawn + hijack Get_PropertiesForAnimation/Traversal → native fill. */
	void WireSandboxAnimInterface();
	void PlayHitFlash();
	void TickHitFlash();
	void ClearHitFlash();
	float PlayDeathKnockdownMontage();
	void PlayHitReact(const FVector& DamageLocation);
	void StopHitReact();
	UAnimMontage* ResolveHitReactMontage(const FVector& DamageLocation) const;
	void SuspendMoverSim();
	void ResumeMoverSim();
	void SetMorphLocomotionTicksEnabled(bool bEnabled);
	bool WantsHeldRagdollMode() const;
	void ForcePhysicalRagdollBodies(bool bDisableCapsule);
	bool ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs);
	void KeepDeathRagdollPhysics();
	void BeginKnockdownDeath();
	void QueueDeathRagdoll();
	void AccrueCombatStun(float Damage);
	void BeginCombatKnockdown();
	void TickCombatKnockdown();
	void EndCombatKnockdown();
	void ConfirmDeathRagdollThenStopAI();
	void StopDeathController();

	UFUNCTION()
	void PlayCombatGetUp();

	UFUNCTION()
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void StartDeathDissolve();

	UFUNCTION()
	void TickDeathDissolve();

	UFUNCTION()
	void FinishDeathSequence();

	bool bSandboxAnimInterfaceWired = false;
	bool bSandboxTraversalInterfaceWired = false;

	UFUNCTION()
	void HandleDied();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USkeletalMeshComponent> SourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UCharacterMoverComponent> Mover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UNavMoverComponent> NavMover;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> MorphCameraBoom;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> MorphFollowCamera;

	UPROPERTY(Transient)
	TObjectPtr<USlimeLockOnComponent> MorphLockOn;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphMoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphMouseLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MorphJumpAction;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形时加上的薄 IMC（仅 Crouch/Sprint）。默认 /Game/Input/IMC_GaspMorphLocomotion。勿用完整 IMC_Sandbox（会吞 WASD）。"))
	TSoftObjectPtr<UInputMappingContext> SandboxMapping;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形 Interact。默认 /Game/Input/IA_Interact。"))
	TSoftObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形 Takedown。默认 /Game/Input/IA_Takedown。"))
	TSoftObjectPtr<UInputAction> TakedownAction;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形 TriggerRagdoll。默认 /Game/Input/IA_TriggerRagdoll。"))
	TSoftObjectPtr<UInputAction> TriggerRagdollAction;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形蹲下（toggle）。默认 /Game/Input/IA_Crouch。"))
	TSoftObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "0_Config|GASP",
		meta = (ToolTip = "幻形冲刺（hold）。默认 /Game/Input/IA_Sprint。"))
	TSoftObjectPtr<UInputAction> SprintAction;

	bool bDevourLocked = false;
	bool bMorphTarget = false;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bPhantomInstance = false;
	bool bMoverFrozen = false;
	bool bJumpHeld = false;
	bool bJumpJustPressed = false;
	bool bPendingRagdoll = false;
	bool bWantChaseGait = false;
	bool bWantsCrouch = false;
	bool bWantsSprint = false;
	bool bSandboxMappingAdded = false;
	bool bRequestedGroundMode = false;
	bool bMoverSimSuspended = false;
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
	FVector CachedMoveIntent = FVector::ZeroVector;
	FVector AiMoveIntent = FVector::ZeroVector;
	FVector AiFaceIntent = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	float MoveDebugAccum = 0.f;
	FVector CachedAcceleration = FVector::ZeroVector;
	FName CachedMovementMode = TEXT("Falling");
	FString CachedGaitName = TEXT("Walk");
	FString CachedStanceName = TEXT("Stand");
	FString CachedRotationModeName = TEXT("OrientToMovement");
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
