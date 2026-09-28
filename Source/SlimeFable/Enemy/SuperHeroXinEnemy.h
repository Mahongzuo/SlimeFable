// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "Combat/SlimeDevourTarget.h"
#include "EnemyCombatTypes.h"
#include "GameFramework/Character.h"
#include "SlimeLockTarget.h"
#include "TimerManager.h"
#include "SuperHeroXinEnemy.generated.h"

class UAnimInstance;
class UAnimMontage;
class UEnemyCombatComponent;
class UInputMappingContext;
class UMaterialInterface;
class USkeletalMesh;
class USlimeHealthComponent;
class USlimeLockOnComponent;
class USlimeStatusComponent;
class UWidgetComponent;

/**
 * Thin Character parent for a duplicated SuperheroFlight player Blueprint.
 * Official flight / SpringArm / ABP stay in the BP. This class only adds
 * slime devour, health, close-range punch AI hooks, and Xin Form2 visuals.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASuperHeroXinEnemy : public ACharacter,
	public ISlimeDevourTarget,
	public ISlimeLockTarget,
	public ICombatDamageable
{
	GENERATED_BODY()

public:
	ASuperHeroXinEnemy(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void Jump() override;

	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	virtual bool IsDevourableNow() const override;
	virtual float GetDevourHealthThreshold() const override { return DevourHealthThreshold; }
	virtual float GetHealthPercent() const override;
	virtual FText GetResolvedDisplayName() const override;
	virtual FLinearColor ResolveDevourWheelTint() const override;
	virtual USkeletalMeshComponent* GetPrimarySkeletalMesh() const override { return GetMesh(); }
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const override;
	virtual USkeletalMeshComponent* GetMorphVisualMesh() const override { return GetDevourPreviewMesh(); }
	virtual UCapsuleComponent* GetDevourCapsule() const override { return GetCapsuleComponent(); }
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
	virtual bool UsesSingleNodeAnims() const override { return false; }
	virtual bool UsesMoverMovement() const override { return false; }
	virtual bool UsesExternalPossessInput() const override { return true; }
	virtual bool UsesExternalPossessCamera() const override { return true; }
	/** false: the morphed player polls LMB through UEnemyCombatComponent::PollPlayerCombatKeys (same Punch move as the AI). */
	virtual bool UsesSelfContainedPlayerCombat() const override { return false; }
	virtual void FreezeForDevour() override;
	virtual void RestoreFromDevour() override;
	virtual void StopMeshAnimation() override;
	virtual void ClearElementAuraFlash() override {}
	virtual FVector GetVisualBoundsCenter() const override;
	virtual FVector GetHudAnchorLocation() const override;
	virtual float GetHealthBarZOffset() const override { return HealthBarZOffset; }
	virtual bool GetStableMeshBounds(FBox& OutBox) const override;
	virtual float GetMorphCameraArmLengthMin() const override { return MorphCameraArmLengthMin; }
	virtual void SetMorphGameplayEnabled(bool bEnabled) override;
	virtual void RefreshHealthBarAnchor() override;
	virtual TSubclassOf<APawn> GetDevourSpawnClass() const override { return GetClass(); }

	virtual bool CanBeLockedOn() const override;
	virtual FVector GetLockOnLocation() const override;

	void EnsureCombatReady() { EnsurePunchMove(); }
	const TArray<FEnemyMoveDef>& GetMoves() const { return Moves; }
	FVector GetSpawnOrigin() const { return SpawnOrigin; }

	void SetAiMoveIntent(const FVector& WorldIntent);
	void ClearAiMoveIntent() { AiMoveIntent = FVector::ZeroVector; }
	const FVector& GetAiMoveIntent() const { return AiMoveIntent; }
	bool WantsCombatDodge() const;
	bool PlaySourceAttackMontage(UAnimMontage* Montage);

	/** Zoom the morphed camera by whole wheel notches (negative = closer). */
	void AdjustMorphCameraZoom(int32 WheelSteps);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ToolTip = "能否被史莱姆吞噬。默认开。"))
	bool bDevourable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "血量低于这个比例才能被吞噬。默认 0.2。"))
	float DevourHealthThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "锁定顶栏名字。空则显示「心月狐（飞行）」。"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm",
			ToolTip = "血条在胶囊顶上方的额外厘米。默认 12。"))
	float HealthBarZOffset = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "超过这个距离不显示头顶血条。默认 1200。"))
	float HealthBarVisibleRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "0.0", Units = "cm",
			ToolTip = "幻形后最近 SpringArm 臂长。默认 180。"))
	float MorphCameraArmLengthMin = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "幻形后最远 SpringArm 臂长（滚轮拉远上限）。默认 900。"))
	float MorphCameraArmLengthMax = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "5.0", Units = "cm",
			ToolTip = "每格滚轮改变的臂长。默认 60。"))
	float MorphCameraZoomStep = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "1.0", ClampMax = "30.0",
			ToolTip = "臂长插值速度（地面时由本类驱动，飞行时飞行组件自己插值）。默认 8。"))
	float MorphCameraZoomInterpSpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats",
		meta = (ClampMin = "1.0",
			ToolTip = "血量上限。默认 160。"))
	float MaxHP = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "招式表。空则 BeginPlay 填一记近战拳。"))
	TArray<FEnemyMoveDef> Moves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "探测玩家并进入追击的距离。默认 1200。"))
	float DetectRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "期望与玩家保持的水平距离。默认 160。"))
	float PreferredDistance = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0", Units = "cm",
			ToolTip = "脱战牵制距离。默认 1800。"))
	float LeashRange = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ClampMin = "100.0",
			ToolTip = "追击走路速度。默认 520。"))
	float ChaseSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观网格。默认 Xin 二阶段 /Game/_Slime/Models/Xin2/Xin_Form2_UE5。"))
	TSoftObjectPtr<USkeletalMesh> VisualBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观重定向 ABP。默认 ABP_XinForm2_Retarget。软引用，禁止构造函数硬加载。"))
	TSoftClassPtr<UAnimInstance> VisualAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "ABP_GenericRetarget 查表用的 ComponentTag，必须等于 RTG 资产名。默认 RTG_Manny_to_XinForm2。"))
	FName VisualRetargetTag = FName(TEXT("RTG_Manny_to_XinForm2"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观描边 Overlay。默认菲比 MI_PhoebeOutline_Overlay。"))
	TSoftObjectPtr<UMaterialInterface> VisualOverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Flight",
		meta = (ToolTip = "幻形后挂上的飞行 IMC。默认 IMC_SuperheroFlight。只在玩家 Possess 时加。"))
	TSoftObjectPtr<UInputMappingContext> FlightMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Flight",
		meta = (ToolTip = "隐藏 Manny 的飞行 ABP。默认 ABP_Player_UE5。软引用，禁止构造函数硬加载。"))
	TSoftClassPtr<UAnimInstance> SourceFlightAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat",
		meta = (ToolTip = "近战拳 Montage，必须是 Manny 骨架。空则只结算球体积伤害。"))
	TSoftObjectPtr<UAnimMontage> PunchMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "出生时 HP 比例。0.2 可直接测吞噬。0 关闭。Lab 默认 0.2。"))
	float DebugStartHealthPercent = 0.2f;

protected:
	void EnsurePunchMove();
	void EnsureVisuals();
	void HideSourceMeshKeepPose();
	void ApplyVisualMesh();
	void StopLivingVisualPhysics();
	void DisableVisualHandIK();
	void EnsureSourceFlightAnim();
	void CancelFlightDodgeIfAny();
	void CallFlightFunctions(const TArray<FName>& Names);
	void BindWorldHealthBar();
	void RefreshWorldHealthBarVisibility();
	void AddFlightMapping(APlayerController* PC);
	void RemoveFlightMapping(AController* OldController);
	void StripFlightMappingIfNotPlayer();
	void StripTutorialHud();
	void ForEachFlightComponent(TFunctionRef<void(UActorComponent*)> Fn) const;
	void BindFlightComponentCaches();
	void SetFlightLogicEnabled(bool bEnabled);
	void RestoreSourceAttackAnim();
	UAnimMontage* ResolveSourceSlotMontage(UAnimMontage* Montage);
	void TickMorphCameraZoom(float DeltaSeconds);
	void CaptureFlightArmBase();
	void ApplyFlightArmLengths(bool bRestoreBase);
	bool IsOwnerWheelOpen() const;
	void StopFlightIfAny();
	void ApplyGroundMovement();
	bool IsPlayerMorphBody() const;
	bool IsInCombatThreat() const;

	UFUNCTION()
	void HandleDied();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USlimeStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UEnemyCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<USlimeLockOnComponent> MorphLockOn;

	/** Source montage -> transient copy whose track plays on the flight ABP's DefaultSlot. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimMontage>, TObjectPtr<UAnimMontage>> DefaultSlotMontageCache;

	/** Flight BP DefaultArmLength / AimingArmLength as authored; wheel zoom offsets from these. */
	float FlightBaseDefaultArm = 0.f;
	float FlightBaseAimingArm = 0.f;
	bool bFlightArmBaseCaptured = false;
	float MorphZoomOffset = 0.f;

	FVector SpawnOrigin = FVector::ZeroVector;
	FVector AiMoveIntent = FVector::ZeroVector;
	TWeakObjectPtr<AActor> MorphMaster;
	bool bDevourLocked = false;
	bool bMorphTarget = false;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bPhantomInstance = false;
	bool bFlightMappingAdded = false;
	bool bSourceAttackPlaying = false;
	FTimerHandle SourceAttackRestoreTimer;
};
