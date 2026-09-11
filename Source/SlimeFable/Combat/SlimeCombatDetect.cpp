// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/SlimeCombatDetect.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GaspEnemyAIController.h"
#include "Enemy/GaspSandboxPawn.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SlimeHealthComponent.h"

namespace SlimeCombatDetect
{
	bool IsEngagedCombatant(AActor* Actor)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || Pawn->IsPlayerControlled())
		{
			return false;
		}

		if (const AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(Actor))
		{
			if (Gasp->IsMorphTarget() || Gasp->IsInDeathSequence())
			{
				return false;
			}
			if (const AGaspEnemyAIController* AI = Cast<AGaspEnemyAIController>(Gasp->GetController()))
			{
				return AI->IsEngaged();
			}
			return false;
		}

		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
		{
			if (Enemy->IsMorphTarget())
			{
				return false;
			}
			return Enemy->IsInCombat();
		}

		return false;
	}

	bool IsLocalCombatActive(APlayerController* PC)
	{
		if (!PC)
		{
			return false;
		}
		UWorld* World = PC->GetWorld();
		if (!World)
		{
			return false;
		}

		for (TActorIterator<AGaspSandboxPawn> It(World); It; ++It)
		{
			if (IsEngagedCombatant(*It))
			{
				return true;
			}
		}
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
		{
			if (IsEngagedCombatant(*It))
			{
				return true;
			}
		}
		return false;
	}
}
