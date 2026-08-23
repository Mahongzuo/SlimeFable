// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeEnemyGameplayTags.h"

#include "EnemyEncounterSubsystem.h"

namespace SlimeEnemyTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Alert, "Combat.State.Alert");
	UE_DEFINE_GAMEPLAY_TAG(State_Telegraph, "Combat.State.Telegraph");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "Combat.State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Recover, "Combat.State.Recover");
	UE_DEFINE_GAMEPLAY_TAG(State_Staggered, "Combat.State.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Guarding, "Combat.State.Guarding");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "Combat.State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_SuperArmor, "Combat.State.SuperArmor");
	UE_DEFINE_GAMEPLAY_TAG(State_Whiffed, "Combat.State.Whiffed");

	UE_DEFINE_GAMEPLAY_TAG(Event_Hit, "Combat.Event.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_GuardBreak, "Combat.Event.GuardBreak");
	UE_DEFINE_GAMEPLAY_TAG(Event_PhaseChanged, "Combat.Event.PhaseChanged");
	UE_DEFINE_GAMEPLAY_TAG(Event_SupportCalled, "Combat.Event.SupportCalled");
	UE_DEFINE_GAMEPLAY_TAG(Event_BossExposed, "Combat.Event.BossExposed");

	UE_DEFINE_GAMEPLAY_TAG(Role_Chaser, "Combat.Role.Chaser");
	UE_DEFINE_GAMEPLAY_TAG(Role_Duelist, "Combat.Role.Duelist");
	UE_DEFINE_GAMEPLAY_TAG(Role_Suppressor, "Combat.Role.Suppressor");
	UE_DEFINE_GAMEPLAY_TAG(Role_Commander, "Combat.Role.Commander");

	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Combat.Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_PoiseDamage, "Combat.Data.PoiseDamage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Healing, "Combat.Data.Healing");
	UE_DEFINE_GAMEPLAY_TAG(Data_Duration, "Combat.Data.Duration");
	UE_DEFINE_GAMEPLAY_TAG(Data_Power, "Combat.Data.Power");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Skill, "Combat.Ability.Skill");

	FGameplayTag RoleTag(uint8 Role)
	{
		switch (static_cast<EEnemyCombatRole>(Role))
		{
		case EEnemyCombatRole::Chaser:
			return Role_Chaser;
		case EEnemyCombatRole::Suppressor:
			return Role_Suppressor;
		case EEnemyCombatRole::Commander:
			return Role_Commander;
		case EEnemyCombatRole::Duelist:
		default:
			return Role_Duelist;
		}
	}
}
