// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatLavaFloor.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 *  A basic actor that applies damage on contact through the ICombatDamageable interface. 
 */
UCLASS(abstract)
class ACombatLavaFloor : public AActor
{
	GENERATED_BODY()
	
	/** Floor mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

protected:

	/** Amount of damage to deal on contact */
	UPROPERTY(EditAnywhere, Category="Damage")
	float Damage = 10000.0f;

public:	

	/** Constructor */
	ACombatLavaFloor();

protected:

	/** Blocking hit handler */
	UFUNCTION()
	void OnFloorHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
