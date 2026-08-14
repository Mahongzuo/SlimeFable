// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeAIController.h"

#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeEnemyCharacter.h"

ASlimeAIController::ASlimeAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASlimeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Combat = InPawn ? InPawn->FindComponentByClass<USlimeCombatComponent>() : nullptr;
	bUseDirectChase = false;
}

APawn* ASlimeAIController::FindPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void ASlimeAIController::ChaseDirect(APawn* MyPawn, APawn* Player)
{
	if (!MyPawn || !Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - MyPawn->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	const FVector Dir = ToPlayer.GetSafeNormal();
	MyPawn->AddMovementInput(Dir, 1.f);
}

void ASlimeAIController::UpdateChase(APawn* MyPawn, APawn* Player, float Dist)
{
	if (Dist <= ApproachAcceptRadius)
	{
		StopMovement();
		PathRefreshRemaining = 0.f;
		return;
	}

	if (bUseDirectChase)
	{
		ChaseDirect(MyPawn, Player);
		return;
	}

	PathRefreshRemaining -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (PathRefreshRemaining > 0.f && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	PathRefreshRemaining = PathRefreshInterval;

	const EPathFollowingRequestResult::Type Result = MoveToActor(Player, ApproachAcceptRadius);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		// No navmesh / blocked path: fall back to steering so the slime still approaches.
		bUseDirectChase = true;
		StopMovement();
		ChaseDirect(MyPawn, Player);
	}
}

void ASlimeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Combat)
	{
		return;
	}

	if (const ASlimeEnemyCharacter* Enemy = Cast<ASlimeEnemyCharacter>(MyPawn))
	{
		if (Enemy->bStationaryTraining)
		{
			return;
		}
	}

	APawn* Player = FindPlayerPawn();
	if (!Player)
	{
		return;
	}

	const float Dist = FVector::Dist(MyPawn->GetActorLocation(), Player->GetActorLocation());
	if (Dist > AggroRange)
	{
		StopMovement();
		PathRefreshRemaining = 0.f;
		bUseDirectChase = false;
		return;
	}

	UpdateChase(MyPawn, Player, Dist);

	FVector ToPlayer = Player->GetActorLocation() - MyPawn->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (!ToPlayer.IsNearlyZero())
	{
		MyPawn->SetActorRotation(ToPlayer.Rotation());
	}

	AttackCooldown = FMath::Max(AttackCooldown - DeltaSeconds, 0.f);
	if (Dist <= AttackRange && AttackCooldown <= 0.f)
	{
		const bool bInMelee = Dist <= FMath::Max(ApproachAcceptRadius * 1.5f, 280.f);
		if (!bInMelee)
		{
			// Mid/long range: only fire Skill1 while closing the gap.
			if (Combat->GetSkillCooldownRemaining(ESlimeSkillSlot::Skill1) <= 0.f
				&& Combat->TrySkill(ESlimeSkillSlot::Skill1))
			{
				AttackCooldown = AttackInterval;
			}
		}
		else
		{
			if (Combat->GetSkillCooldownRemaining(ESlimeSkillSlot::Skill1) <= 0.f)
			{
				Combat->TrySkill(ESlimeSkillSlot::Skill1);
			}
			else
			{
				Combat->TryComboAttack();
			}
			AttackCooldown = AttackInterval;
		}
	}
}
