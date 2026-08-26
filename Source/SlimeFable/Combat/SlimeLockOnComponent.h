// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeLockOnComponent.generated.h"

class APlayerController;
class UInputAction;
class UEnhancedInputComponent;
class USlimeHealthComponent;
class UCameraComponent;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeLockOnComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BindInput(UEnhancedInputComponent* EnhancedInput);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearLockOn();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsLockedOn() const { return LockedTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetLockedTarget() const { return LockedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat", meta = (WorldContext = "WorldContextObject"))
	static bool IsLockedByLocalPlayer(const UObject* WorldContextObject, const AActor* Target);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "100.0", Units = "cm",
		ToolTip = "中键索敌搜索半径（厘米）。默认 2000。"))
	float AcquireRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "100.0", Units = "cm",
		ToolTip = "锁定后超过这个距离自动解锁。默认 2400。"))
	float BreakRange = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", Units = "s",
		ToolTip = "手动解锁后短时禁止立刻重锁，避免中键同一次按下触发两次 Toggle。默认 0.15。"))
	float RelockCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "取景框上下额外边距（厘米）。默认 30。"))
	float FramingPadding = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "1.0", ClampMax = "2.0",
		ToolTip = "FOV 取景余量，略大于 1 避免贴边。默认 1.05。"))
	float FramingSlack = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "200.0", Units = "cm",
		ToolTip = "锁定时允许的最大臂长。默认 420（避免室内穿墙）。"))
	float LockOnArmLengthMax = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "-89.0", ClampMax = "0.0",
		ToolTip = "锁定 Pitch 下限（俯视）。默认 -28。"))
	float LockPitchMin = -28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", ClampMax = "30.0",
		ToolTip = "锁定 Pitch 上限（几乎不仰视，防钻地）。默认 3。"))
	float LockPitchMax = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "锁定时 SpringArm SocketOffset.Z 抬高。默认 50。"))
	float LockSocketLiftZ = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "Focus XY 偏向敌人的权重（0=全史莱姆，1=全敌人）。默认 0.55。"))
	float FocusEnemyWeight = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|LockOn", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "取景所需臂长变化超过此值才上抬 Desired，减少抖动。默认 30。"))
	float FramingArmHysteresis = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input",
		meta = (ToolTip = "Enhanced Input 的锁定动作。若已绑定则不再轮询中键，避免双触发。"))
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input",
		meta = (ToolTip = "无 LockOnAction 时用中键轮询。有 Enhanced Input 绑定时会自动关掉。默认开。"))
	bool bPollLockOnKey = true;

private:
	AActor* FindBestTarget() const;
	void ApplyLockCamera(float DeltaTime);
	void BeginLockCameraFraming();
	void RestoreMovement();
	void RestoreCameraBoom();
	APlayerController* GetPlayerController() const;
	bool CanAcquireLock() const;
	void GetActorVerticalSpan(const AActor* Actor, float& OutMinZ, float& OutMaxZ) const;
	float ComputeFramingArmLength(float FrameHeight, float HorizontalSep, const UCameraComponent* Cam) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockedTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<USlimeHealthComponent> LockedHealth;

	bool bSavedOrientToMovement = true;
	bool bHaveSavedMovement = false;
	bool bHaveSavedBoom = false;
	FVector SavedBoomSocketOffset = FVector(0.f, 0.f, 12.f);
	float SavedDesiredArmLength = 260.f;
	bool bHaveSavedArmLength = false;
	float FramingFloorArm = 120.f;
	float AppliedFramingFloorArm = 0.f;
	bool bPollLockDown = false;
	bool bLockOnActionBound = false;
	float RelockBlockUntil = 0.f;
};
