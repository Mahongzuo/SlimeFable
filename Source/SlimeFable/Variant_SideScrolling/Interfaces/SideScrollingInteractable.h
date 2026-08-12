// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SideScrollingInteractable.generated.h"

/**
 *  
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class USideScrollingInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Simple interface to allow Actors to interact without having knowledge of their internal implementation.
 */
class ISideScrollingInteractable
{
	GENERATED_BODY()

public:

	/** Triggers an interaction by the provided Actor */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual void Interaction(AActor* Interactor) = 0;

};
