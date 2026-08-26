// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyCharacter.h"
#include "SlimeDevourComponent.generated.h"

class APlayerController;
class ACharacter;
class UPoseableMeshComponent;
class UStaticMeshComponent;
class UMeshComponent;
class USlimeBodyComponent;
class USlimeCombatComponent;
class USlimePhantomWheelWidget;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class USkeletalMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ESlimeDevourPhase : uint8
{
	Idle,
	Charging,
	Latch,
	Shrink,
	Retract,
	Digest,
	CloseRangeShrink,
	CloseRangeDash
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeDevourCapture
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	TSubclassOf<AEnemyCharacter> EnemyClass;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	FLinearColor WheelTint = FLinearColor(0.42f, 0.32f, 0.2f);

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	float CaptureRadius = 40.f;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	FVector CaptureBoundsOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	FTransform CaptureWorldXform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	TArray<TObjectPtr<UMaterialInterface>> SkeletalMaterials;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Devour")
	TArray<TObjectPtr<UMaterialInterface>> StaticMaterials;

	bool IsValidCapture() const { return EnemyClass.Get() != nullptr; }
};

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeDevourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeDevourComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	bool IsDevouring() const { return Phase != ESlimeDevourPhase::Idle; }

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	bool IsCombatLocked() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	bool CanStartDevour() const { return Phase == ESlimeDevourPhase::Idle; }

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	ESlimeDevourPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	float GetDigestAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	int32 GetPhantomSlotCount() const { return PhantomSlots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	int32 GetPhantomSlotCapacity() const { return PhantomSlotCapacity; }

	const TArray<FSlimeDevourCapture>& GetPhantomSlots() const { return PhantomSlots; }

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	int32 GetSelectedPhantomSlot() const { return SelectedPhantomSlot; }

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void CyclePhantomSelection(int32 Step);

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	AEnemyCharacter* FindBestDevourTarget() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	bool CanDevourTarget(const AEnemyCharacter* Enemy) const;

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool TryDevourFocused();

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool BeginHold(AEnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void CancelHold();

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool ReleaseHold();

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool TryStartDevour(AEnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void AbortDevour(bool bRestoreBody);

	UFUNCTION(BlueprintPure, Category = "Slime|Devour")
	bool IsPhantomWheelOpen() const { return bPhantomWheelOpen; }

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool TryOpenPhantomWheel();

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void ClosePhantomWheel(bool bCommit);

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void TickPhantomWheelInput();

	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	bool TryPhantom(int32 Slot);

	/**
	 *  Removes a phantom slot without spawning a phantom. Used by the morph system when a
	 *  morphed enemy is killed in action — the capture is consumed so it cannot be morphed
	 *  again this day. Safe to call on an empty/invalid slot (no-op).
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Devour")
	void ConsumePhantomSlot(int32 Slot);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "可吞噬扫描半径。默认 800cm，长按 F 读条期间超出此距离会取消。"))
	float DevourRadius = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.01", ClampMax = "1.0",
		ToolTip = "全局吞噬血量阈值。敌人自己填了大于 0 的值时以敌人为准。默认 0.2 即残血 20%。"))
	float DevourHealthThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "短按 F 触发快速吞噬的最大距离。严格小于此值才进入近距离流程，默认 300cm。"))
	float CloseRangeRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "近距离短按吞噬时敌人缩小到可吞尺寸的时长，默认 0.75 秒。"))
	float CloseRangeShrinkSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "近距离缩小后史莱姆冲刺到敌人处的时长，默认 0.5 秒。"))
	float CloseRangeDashSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour", meta = (ClampMin = "1.0", ClampMax = "40.0",
		ToolTip = "近距冲刺时 CameraLagSpeed，默认 28；结束后恢复。"))
	float CloseRangeDashCameraLag = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", Units = "s",
		ToolTip = "长按 F 蓄力时长。默认 1.2s，读满才发射子球。"))
	float HoldSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "1", ClampMax = "3",
		ToolTip = "远距包裹子球数量。默认 1。"))
	int32 LatchShotCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "子球抛物线飞向包裹点的时长。默认 0.65s。"))
	float LatchSeconds = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "抛物线额外抬高。默认 80cm，与 G 键弧高同量级。"))
	float LatchArcHeight = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", Units = "s",
		ToolTip = "远距先把敌人缩小到可吞尺寸的时长。默认 1.5s。"))
	float ShrinkSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", ClampMax = "1.0",
		ToolTip = "缩小后敌人包围球半径相对史莱姆 RestRadius 的比例。默认 0.6。"))
	float ShrinkFitFraction = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", ClampMax = "1.0",
		ToolTip = "消化阶段体内网格相对视觉球半径的适配比例。默认 0.55，避免穿出表面。"))
	float InnerFitFraction = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", ClampMax = "0.6",
		ToolTip = "每个贴附子球占主体粒子的比例。默认 0.5 即一半体积。3 球合计约 1.5 倍。"))
	float LatchShotFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "子球从本体依次弹出的间隔。默认 0.12s。"))
	float LatchStaggerSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "1.0", Units = "s",
		ToolTip = "贴附子球寿命。默认 8s，牵引时会自动续命。"))
	float LatchShotLife = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "200.0", Units = "cm/s",
		ToolTip = "钉住后微收贴附点的牵引速度。默认 900，发射阶段走抛物线不拉拽。"))
	float LatchPullSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", ClampMax = "1.0",
		ToolTip = "贴附点沿网格法线外推，相对子球半径的比例。默认 0.2，贴紧网格。"))
	float LatchStickOffsetFraction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", Units = "s",
		ToolTip = "缩小后把子球和敌人网格缓收回体内的时长。默认 0.9s。不改 G 键召回速度。"))
	float RetractSeconds = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "体内消化时长。默认 10s，结束时体内网格消失。"))
	float DigestSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.2", Units = "s",
		ToolTip = "消化最后这段推 DissolveLine / Opacity。默认 1.5s。"))
	float DigestDissolveSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "体内网格相对视觉球心的微调上抬。默认 0.05，只防贴地穿底。"))
	float InnerMeshLiftFraction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", Units = "s",
		ToolTip = "按住 Q 这么久才弹出幻形轮盘。短于此时松开仍放技能1。"))
	float PhantomWheelHoldSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.5", Units = "s"))
	float PhantomLifeSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "1", ClampMax = "8"))
	int32 PhantomSlotCapacity = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CycleCooldown = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour")
	bool bSlowTimeWhileWheelOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WheelTimeDilation = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "敌人被吞入体内瞬间。空则 /Game/Audio/SFX/Combat/sfx_swallow_01。"))
	TSoftObjectPtr<USoundBase> SwallowSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Devour")
	TObjectPtr<UMaterialInterface> DigestDissolveMaterial;

protected:
	void TickPhase(float DeltaTime);
	void EnterPhase(ESlimeDevourPhase NewPhase);
	void FaceTarget(AEnemyCharacter* Enemy);
	bool IsTargetStillValid(const AEnemyCharacter* Enemy) const;
	bool PrepareDevourTarget(AEnemyCharacter* Enemy);
	void BeginCloseRange(AEnemyCharacter* Enemy);
	void BeginCloseRangeDash(AEnemyCharacter* Enemy);
	void TickCloseRangeDash(float DeltaTime);
	void SetCloseRangeCameraLag(bool bBoost);
	void SetOwnerMovementFrozen(bool bFrozen);
	bool LaunchCloseRangeWrapper(AEnemyCharacter* Enemy);
	FVector GetWrapCenter(const AEnemyCharacter* Enemy) const;
	bool IsCloseRangeWrapped(const AEnemyCharacter* Enemy) const;
	void FreezeDevourTarget(AEnemyCharacter* Enemy);
	void RestoreDevourTarget(AEnemyCharacter* Enemy);
	void HideEnemyWidgets(AEnemyCharacter* Enemy, bool bHide) const;
	void BeginLatch(AEnemyCharacter* Enemy);
	void TryLaunchNextLatchShot(AEnemyCharacter* Enemy);
	void TickLatch(AEnemyCharacter* Enemy, float DeltaTime);
	void UpdateLatchTargets(AEnemyCharacter* Enemy);
	void GetLatchAttachPoints(const AEnemyCharacter* Enemy, TArray<FVector>& OutPoints) const;
	bool GetEnemyMeshBox(const AEnemyCharacter* Enemy, FBox& OutBox) const;
	bool TryGetLatchBoneLocation(const USkeletalMeshComponent* Skel, const TArray<FName>& Names, FVector& OutLocation) const;
	FVector MakeLatchPointFromAnchor(const AEnemyCharacter* Enemy, const FVector& Anchor, const FVector& Axis, float MiniRadius) const;
	FVector TraceMeshAttachPoint(const AEnemyCharacter* Enemy, const FVector& Center, const FVector& Axis, float ExtentAlong, float MiniRadius) const;
	void ClearOwnerLockOn() const;
	float GetLatchMiniRadius() const;
	void ApplyEnemyShrink(AEnemyCharacter* Enemy, float Alpha) const;
	void BeginRetract(AEnemyCharacter* Enemy);
	void TickRetract(float DeltaTime);
	/** Place enemy so GetWrapCenter equals DesiredWrapCenter. */
	void SetEnemyWrapCenter(AEnemyCharacter* Enemy, const FVector& DesiredWrapCenter) const;
	int32 GetActiveLatchShotCount() const;
	void SwallowTarget();
	void CaptureEnemy(AEnemyCharacter* Enemy, FSlimeDevourCapture& OutCapture) const;
	void PushPhantomSlot(const FSlimeDevourCapture& Capture);
	void SpawnInnerMesh(const FSlimeDevourCapture& Capture);
	void ConfigureInnerMesh(UMeshComponent* Comp) const;
	void UpdateInnerMesh(float DeltaTime);
	FTransform MakeFittedInnerTransform(float ExtraZ = 0.f) const;
	void ApplyInnerDissolve(float Alpha);
	void DestroyInnerMesh();
	void RestoreBody();
	void FinishDevour();
	void CleanupLatchShots();
	bool BuildCaptureFromMesh(USkeletalMeshComponent* Skel, UStaticMeshComponent* StaticMeshComp, FSlimeDevourCapture& OutCapture) const;
	APlayerController* GetPlayerController() const;
	FVector GetBlobCenter() const;
	float GetBlobRadius() const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatComponent> Combat;

	UPROPERTY(Transient)
	TObjectPtr<USlimePhantomWheelWidget> PhantomWheelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> InnerPoseable;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InnerStatic;

	UPROPERTY(Transient)
	TArray<FSlimeDevourCapture> PhantomSlots;

	TWeakObjectPtr<AEnemyCharacter> DevourTarget;
	TWeakObjectPtr<AActor> PendingDestroyEnemy;
	FSlimeDevourCapture ActiveCapture;
	FTransform EnemyStartXform = FTransform::Identity;
	FVector EnemyStartScale = FVector::OneVector;
	FVector EnemyTargetScale = FVector::OneVector;
	TArray<uint8> LatchShotIds;
	TArray<uint8> LatchPinned;
	FVector RetractStartLocation = FVector::ZeroVector;
	FVector RetractStartWrapCenter = FVector::ZeroVector;
	FVector CloseRangeHoverLocation = FVector::ZeroVector;
	FVector CloseRangeDashStart = FVector::ZeroVector;
	FVector CloseRangeDashEnd = FVector::ZeroVector;
	uint8 CloseRangeShotId = 0;
	int32 LatchLaunchIndex = 0;
	float SavedCameraLagSpeed = 12.f;
	float SavedEnemyGravityScale = 1.f;
	bool bCameraLagBoosted = false;
	bool bSavedEnemyGravity = false;
	bool bOwnerMovementFrozen = false;

	ESlimeDevourPhase Phase = ESlimeDevourPhase::Idle;
	float PhaseElapsed = 0.f;
	int32 SelectedPhantomSlot = 0;
	float CycleCooldownRemaining = 0.f;
	float SavedTimeDilation = 1.f;
	float InnerWobble = 0.f;
	bool bPhantomWheelOpen = false;
};
