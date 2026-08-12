// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatActivatable.generated.h"

/**
 *  Interactable Interface
 *  Provides a context-agnostic way of activating, deactivating or toggling actors
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UCombatActivatable : public UInterface
{
	GENERATED_BODY()
};

class ICombatActivatable
{
	GENERATED_BODY()

public:

	/** Toggles the Interactable Actor */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ToggleInteraction(AActor* ActivationInstigator) = 0;

	/** Activates the Interactable Actor */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ActivateInteraction(AActor* ActivationInstigator) = 0;

	/** Deactivates the Interactable Actor */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void DeactivateInteraction(AActor* ActivationInstigator) = 0;
};
