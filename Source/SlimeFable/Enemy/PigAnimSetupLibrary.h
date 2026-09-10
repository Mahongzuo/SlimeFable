// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PigAnimSetupLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;
class UBlendSpace;

/** Editor helpers to wire ABP_Pig: Locomotion SM → DefaultSlot → Root. */
UCLASS()
class SLIMEFABLE_API UPigAnimSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Pig AnimGraph: Locomotion StateMachine (Idle / Move / Rest / Hit / Death) → DefaultSlot → Root.
	 * Idle uses IdleSequence (random variants). Move uses GroundBlendSpace X=Speed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pig|Locomotion")
	static bool WirePigLocomotionGraph(
		UAnimBlueprint* AnimBP,
		UBlendSpace* GroundBlendSpace,
		UAnimSequence* IdleBreathe,
		UAnimSequence* IdleLook,
		UAnimSequence* IdleSniff,
		UAnimSequence* IdleChew,
		UAnimSequence* RestSequence,
		UAnimSequence* HitSequence,
		UAnimSequence* DeathSequence);
};
