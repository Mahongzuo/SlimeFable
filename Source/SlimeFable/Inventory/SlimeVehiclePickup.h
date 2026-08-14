// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeWorldPickup.h"
#include "SlimeVehiclePickup.generated.h"

class APawn;

/**
 * World vehicle prop: F mounts the slime flyer instead of adding an inventory item.
 * After eject, physics-drops to the ground and can be used again once settled.
 */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimeVehiclePickup : public ASlimeWorldPickup
{
	GENERATED_BODY()

public:
	ASlimeVehiclePickup();

	virtual void Tick(float DeltaSeconds) override;

	virtual bool TryPickup(APawn* Picker) override;
	virtual FText GetInteractPromptVerb() const override;

	/** Spawned mid-air after eject: simulate physics until settled on ground. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void BeginDropped(APawn* IgnoredPicker, float IgnoreSeconds = 0.4f);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsSettled() const { return !bDropping; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool CanBeUsedBy(const APawn* Picker) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle", meta = (ClampMin = "10.0"))
	float SettleSpeedThreshold = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle", meta = (ClampMin = "0.05", Units = "s"))
	float SettleHoldSeconds = 0.2f;

protected:
	void FinishSettle();

	bool bDropping = false;
	float SettleTimer = 0.f;
	float IgnorePickerUntil = 0.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> IgnoredPicker;
};
