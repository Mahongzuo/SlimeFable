// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/SlimeCombatDetect.h"
#include "Combat/SlimeLockOnComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace SlimeCombatDetect
{
	bool IsLocalCombatActive(APlayerController* PC)
	{
		if (!PC)
		{
			return false;
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			if (const USlimeLockOnComponent* Lock = Pawn->FindComponentByClass<USlimeLockOnComponent>())
			{
				if (Lock->IsLockedOn())
				{
					return true;
				}
			}
		}
		if (UWorld* World = PC->GetWorld())
		{
			for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
			{
				if (It->IsInCombat())
				{
					return true;
				}
			}
		}
		return false;
	}
}
