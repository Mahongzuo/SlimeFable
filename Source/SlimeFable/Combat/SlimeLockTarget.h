// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SlimeLockTarget.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class USlimeLockTarget : public UInterface
{
	GENERATED_BODY()
};

class ISlimeLockTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual bool CanBeLockedOn() const = 0;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual FVector GetLockOnLocation() const = 0;

	/** Two stacked HP tubes (phase 1 on top, phase 2 below). Default is a single bar. */
	virtual bool UsesDualHealthBars() const { return false; }
	virtual void GetDualHealthPercents(float& OutPhase1, float& OutPhase2) const
	{
		OutPhase1 = 1.f;
		OutPhase2 = 1.f;
	}
};
