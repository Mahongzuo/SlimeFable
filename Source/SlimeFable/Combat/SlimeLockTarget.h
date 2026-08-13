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
};
