// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SideScrollingInteractable.h"
#include "SideScrollingMovingPlatform.generated.h"

/**
 *  Simple moving platform that can be triggered through interactions by other actors.
 *  The actual movement is performed by Blueprint code through latent execution nodes.
 */
UCLASS(abstract)
class ASideScrollingMovingPlatform : public AActor, public ISideScrollingInteractable
{
	GENERATED_BODY()
	
public:	
	
	/** Constructor */
	ASideScrollingMovingPlatform();

protected:

	/** If this is true, the platform is mid-movement and will ignore further interactions */
	bool bMoving = false;

	/** Destination of the platform in world space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moving Platform")
	FVector PlatformTarget;

	/** Time for the platform to move to the destination */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moving Platform", meta = (ClampMin = 0, ClampMax = 10, Units="s"))
	float MoveDuration = 5.0f;

	/** If this is true, the platform will only move once. */
	UPROPERTY(EditAnywhere, Category="Moving Platform")
	bool bOneShot = false;

public:

// ~begin IInteractable interface 

	/** Performs an interaction triggered by another actor */
	virtual void Interaction(AActor* Interactor) override;

// ~end IInteractable interface

	/** Resets the interaction state. Must be called from BP code to reset the platform */
	UFUNCTION(BlueprintCallable, Category="Moving Platform")
	virtual void ResetInteraction();

protected:

	/** Allows Blueprint code to do the actual platform movement */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Moving Platform", meta = (DisplayName="Move to Target"))
	void BP_MoveToTarget();

};
