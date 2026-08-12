// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatDamageable.generated.h"

/**
 *  CombatDamageable interface
 *  Provides functionality to handle damage, healing, knockback and death
 *  Also provides functionality to warn characters of incoming sources of damage
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UCombatDamageable : public UInterface
{
	GENERATED_BODY()
};

class ICombatDamageable
{
	GENERATED_BODY()

public:

	/** Handles damage and knockback events */
	UFUNCTION(BlueprintCallable, Category="Damageable")
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) = 0;

	/** Handles death events */
	UFUNCTION(BlueprintCallable, Category="Damageable")
	virtual void HandleDeath() = 0;

	/** Handles healing events */
	UFUNCTION(BlueprintCallable, Category="Damageable")
	virtual void ApplyHealing(float Healing, AActor* Healer) = 0;

	/** Notifies the actor of impending danger such as an incoming hit, allowing it to react. */
	UFUNCTION(BlueprintCallable, Category="Damageable")
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) = 0;
};
