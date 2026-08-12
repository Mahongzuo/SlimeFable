// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SideScrollingSoftPlatform.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;

/**
 *  A side scrolling game platform that the character can jump or drop through.
 */
UCLASS(abstract)
class ASideScrollingSoftPlatform : public AActor
{
	GENERATED_BODY()
	
	/** Root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** Platform mesh. The part we collide against and see */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

	/** Collision volume that toggles soft collision on the character when they're below the platform. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* CollisionCheckBox;

public:	
	
	/** Constructor */
	ASideScrollingSoftPlatform();

protected:

	/** Handles soft collision check box overlaps */
	UFUNCTION()
	void OnSoftCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Restores soft collision state when overlap ends */
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
};
