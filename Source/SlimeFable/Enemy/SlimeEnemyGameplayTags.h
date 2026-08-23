// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 *  Native gameplay tags for the enemy combat stack. Declared here instead of
 *  DefaultGameplayTags.ini so C++ and Blueprint share one source of truth.
 */
namespace SlimeEnemyTags
{
	// Combat states owned by abilities / the AI decision layer.
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Alert);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Telegraph);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Recover);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Guarding);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperArmor);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Whiffed);

	// Combat events broadcast through the ability system.
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardBreak);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PhaseChanged);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_SupportCalled);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_BossExposed);

	// Encounter roles.
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Chaser);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Duelist);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Suppressor);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Commander);

	// SetByCaller data channels.
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_PoiseDamage);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Healing);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Power);

	// Ability activation channels used by the StateTree decision layer.
	SLIMEFABLE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill);

	/** Tag for a combat role, used when tagging enemies for encounter queries. */
	SLIMEFABLE_API FGameplayTag RoleTag(uint8 Role);
}
