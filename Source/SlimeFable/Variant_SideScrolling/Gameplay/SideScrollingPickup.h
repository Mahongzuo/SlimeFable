// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SideScrollingPickup.generated.h"

class USphereComponent;

/**
 *  A simple side scrolling game pickup
 *  Increments a counter on the GameMode
 */
UCLASS(abstract)
class ASideScrollingPickup : public AActor
{
	GENERATED_BODY()
	
	/** Pickup bounding sphere */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* Sphere;

public:

	/** Constructor */
	ASideScrollingPickup();

protected:

	/** Handles pickup collision */
	UFUNCTION()
	void BeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/** Passes control to BP to play effects on pickup */
	UFUNCTION(BlueprintImplementableEvent, Category="Pickup", meta = (DisplayName = "On Picked Up"))
	void BP_OnPickedUp();
};
