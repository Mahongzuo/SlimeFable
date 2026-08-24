// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeLockOnComponent.generated.h"

class APlayerController;
class UInputAction;
class UEnhancedInputComponent;
class USlimeHealthComponent;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "100.0", Units = "cm",
		ToolTip = "中键索敌搜索半径（厘米）。默认 2000。"))
	float AcquireRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "100.0", Units = "cm",
		ToolTip = "锁定后超过这个距离自动解锁。默认 2400。"))
	float BreakRange = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "0.0", Units = "s",
		ToolTip = "手动解锁后短时禁止立刻重锁，避免中键同一次按下触发两次 Toggle。默认 0.15。"))
	float RelockCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input",
		meta = (ToolTip = "Enhanced Input 的锁定动作。若已绑定则不再轮询中键，避免双触发。"))
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input",
		meta = (ToolTip = "无 LockOnAction 时用中键轮询。有 Enhanced Input 绑定时会自动关掉。默认开。"))
	bool bPollLockOnKey = true;

private:
	AActor* FindBestTarget() const;
	void ApplyLockCamera(float DeltaTime);
	void RestoreMovement();
	void RestoreCameraBoom();
	APlayerController* GetPlayerController() const;
	bool CanAcquireLock() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockedTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<USlimeHealthComponent> LockedHealth;

	bool bSavedOrientToMovement = true;
	bool bHaveSavedMovement = false;
	bool bHaveSavedBoom = false;
	FVector SavedBoomSocketOffset = FVector(0.f, 0.f, 40.f);
	bool bPollLockDown = false;
	bool bLockOnActionBound = false;
	float RelockBlockUntil = 0.f;
};
