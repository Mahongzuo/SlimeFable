// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatAttacker.generated.h"

/**
 *  CombatAttacker Interface
 *  Provides common functionality to trigger attack animation events.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UCombatAttacker : public UInterface
{
	GENERATED_BODY()
};

class ICombatAttacker
{
	GENERATED_BODY()

public:

	/** Performs an attack's collision check. Usually called from a montage's AnimNotify */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void DoAttackTrace(FName DamageSourceBone) = 0;

	/** Performs a combo attack's check to continue the string. Usually called from a montage's AnimNotify */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckCombo() = 0;

	/** Performs a charged attack's check to loop the charge animation. Usually called from a montage's AnimNotify */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckChargedAttack() = 0;
};
