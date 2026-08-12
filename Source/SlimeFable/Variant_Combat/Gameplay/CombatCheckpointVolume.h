// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "CombatCheckpointVolume.generated.h"

UCLASS(abstract)
class ACombatCheckpointVolume : public AActor
{
	GENERATED_BODY()
	
	/** Collision box volume */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Components, meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Box;

public:	
	
	/** Constructor */
	ACombatCheckpointVolume();

protected:

	/** Handles overlaps with the box volume */
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
