// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeSliceableActor.h"
#include "SlimeWatermelon.generated.h"

/** Watermelon prop with default mesh / cap / slice SFX wired in. */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimeWatermelon : public ASlimeSliceableActor
{
	GENERATED_BODY()

public:
	ASlimeWatermelon();
};
