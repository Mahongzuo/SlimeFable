// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatDamageable.h"
#include "CombatDummy.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;

/**
 *  A simple invincible combat training dummy
 */
UCLASS(abstract)
class ACombatDummy : public AActor, public ICombatDamageable
{
	GENERATED_BODY()
	
	/** Root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** Static base plate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BasePlate;

	/** Physics enabled dummy mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Dummy;

	/** Physics constraint holding the dummy and base plate together */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UPhysicsConstraintComponent* PhysicsConstraint;

public:	
	
	/** Constructor */
	ACombatDummy();

	// ~Begin CombatDamageable interface

		/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows reaction to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~End CombatDamageable interface

protected:

	/** Blueprint handle to apply damage effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta = (DisplayName = "On Dummy Damaged"))
	void BP_OnDummyDamaged(const FVector& Location, const FVector& Direction);
};
