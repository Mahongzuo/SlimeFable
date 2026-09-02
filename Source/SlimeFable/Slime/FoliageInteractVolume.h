// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FoliageInteractVolume.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFoliageInteractVolume : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional volume source for USlimeFoliageInteractComponent.
 * Other projects can skip this: the component falls back to mesh Bounds / capsule.
 */
class SLIMEFABLE_API IFoliageInteractVolume
{
	GENERATED_BODY()

public:
	/** Custom interact sphere (world cm). Return false to use mesh/capsule fallback. */
	virtual bool GetFoliageInteractVolume(FVector& OutLocation, float& OutRadius) const
	{
		return false;
	}

	/** True = report strength 0 this frame (dead, hidden, parked, …). */
	virtual bool ShouldSuppressFoliageInteract() const
	{
		return false;
	}
};
