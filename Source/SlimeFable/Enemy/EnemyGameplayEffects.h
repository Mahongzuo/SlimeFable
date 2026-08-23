// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EnemyGameplayEffects.generated.h"

/** Instant damage. Magnitude comes from SetByCaller Combat.Data.Damage / Combat.Data.PoiseDamage. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnemyDamage();
};

/** Instant healing. Magnitude comes from SetByCaller Combat.Data.Healing. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyHealing : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnemyHealing();
};

/** Base for tag-only state effects whose duration is a SetByCaller Combat.Data.Duration. */
UCLASS(Abstract)
class SLIMEFABLE_API UGE_EnemyTimedState : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnemyTimedState();

protected:
	void GrantStateTag(const FGameplayTag& Tag);
};

/** Poise break: staggered, cannot act, counter window for the player. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyStagger : public UGE_EnemyTimedState
{
	GENERATED_BODY()

public:
	UGE_EnemyStagger();
};

/** Boss phase shield / scripted immunity. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyInvulnerable : public UGE_EnemyTimedState
{
	GENERATED_BODY()

public:
	UGE_EnemyInvulnerable();
};

/** Samurai guard stance. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyGuard : public UGE_EnemyTimedState
{
	GENERATED_BODY()

public:
	UGE_EnemyGuard();
};

/** Super armor: poise damage is ignored while active. */
UCLASS()
class SLIMEFABLE_API UGE_EnemySuperArmor : public UGE_EnemyTimedState
{
	GENERATED_BODY()

public:
	UGE_EnemySuperArmor();
};

/** Commander buff: multiplies DamagePower for a limited time. */
UCLASS()
class SLIMEFABLE_API UGE_EnemyEmpower : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_EnemyEmpower();
};
