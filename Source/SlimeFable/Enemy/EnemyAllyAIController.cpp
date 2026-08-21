// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyAllyAIController.h"

#include "EnemyCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeHealthComponent.h"

AEnemyAllyAIController::AEnemyAllyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAllyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(InPawn))
	{
		Master = Enemy->GetPhantomMaster();
	}
}

void AEnemyAllyAIController::SetMaster(AActor* InMaster)
{
	Master = InMaster;
}

APawn* AEnemyAllyAIController::FindCombatFocus() const
{
	APawn* Self = GetPawn();
	UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return nullptr;
	}

	APawn* Best = nullptr;
	float BestDistSq = FMath::Square(AllySeekRange);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Other = *It;
		if (!Other || Other == Self)
		{
			continue;
		}
		if (!USlimeHitProbe::IsHostile(Self, Other))
		{
			continue;
		}
		if (const USlimeHealthComponent* Health = Other->FindComponentByClass<USlimeHealthComponent>())
		{
			if (!Health->IsAlive())
			{
				continue;
			}
		}
		const float DistSq = FVector::DistSquared(Self->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

void AEnemyAllyAIController::TickIdle(float DeltaSeconds, float Dist)
{
	if (FindCombatFocus())
	{
		Super::TickIdle(DeltaSeconds, Dist);
		return;
	}

	APawn* Self = GetPawn();
	APawn* Follow = Cast<APawn>(Master.Get());
	if (!Self || !Follow)
	{
		return;
	}

	const float FollowDist = FVector::Dist(Self->GetActorLocation(), Follow->GetActorLocation());
	if (FollowDist > 180.f)
	{
		MoveToActor(Follow, 120.f);
	}
	else if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}
}
