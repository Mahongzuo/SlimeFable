// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SideScrollingJumpPad.generated.h"

class UBoxComponent;

/**
 *  A simple jump pad that launches characters into the air
 */
UCLASS(abstract)
class ASideScrollingJumpPad : public AActor
{
	GENERATED_BODY()
	
	/** Jump pad bounding box */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Box;

protected:

	/** Vertical velocity to set the character to when they use the jump pad */
	UPROPERTY(EditAnywhere, Category="Jump Pad", meta = (ClampMin=0, ClampMax=10000, Units="cm/s"))
	float ZStrength = 1000.0f;

public:	

	/** Constructor */
	ASideScrollingJumpPad();

protected:

	UFUNCTION()
	void BeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

};
