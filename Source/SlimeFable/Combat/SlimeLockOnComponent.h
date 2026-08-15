// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeLockOnComponent.generated.h"

class APlayerController;
class UInputAction;
class UEnhancedInputComponent;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "100.0", Units = "cm"))
	float AcquireRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "100.0", Units = "cm"))
	float BreakRange = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bPollLockOnKey = true;

private:
	AActor* FindBestTarget() const;
	void ApplyLockCamera(float DeltaTime);
	void RestoreMovement();
	void RestoreCameraBoom();
	APlayerController* GetPlayerController() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockedTarget;

	bool bSavedOrientToMovement = true;
	bool bHaveSavedMovement = false;
	bool bHaveSavedBoom = false;
	FVector SavedBoomSocketOffset = FVector(0.f, 0.f, 40.f);
	bool bPollLockDown = false;
};
