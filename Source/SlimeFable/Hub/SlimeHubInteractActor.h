// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeHubInteractActor.generated.h"

/** Shared F-interact target for the museum hub (farm plots, seed crate, day gate). */
UCLASS(Abstract, Blueprintable)
class SLIMEFABLE_API ASlimeHubInteractActor : public AActor
{
	GENERATED_BODY()

public:
	virtual bool TryInteract(APawn* Interactor);
	virtual FText GetInteractPromptVerb() const;
	virtual bool CanBeFocused() const { return true; }
	virtual FVector GetPromptWorldLocation() const;
};
