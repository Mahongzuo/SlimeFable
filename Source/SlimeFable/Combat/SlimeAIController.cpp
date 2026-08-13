// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeAIController.h"

#include "Kismet/GameplayStatics.h"
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
}

APawn* ASlimeAIController::FindPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
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
		return;
	}

	MoveToActor(Player, AttackRange * 0.7f);

	FVector ToPlayer = Player->GetActorLocation() - MyPawn->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (!ToPlayer.IsNearlyZero())
	{
		MyPawn->SetActorRotation(ToPlayer.Rotation());
	}

	AttackCooldown = FMath::Max(AttackCooldown - DeltaSeconds, 0.f);
	if (Dist <= AttackRange && AttackCooldown <= 0.f)
	{
		if (Combat->GetSkillCooldownRemaining(ESlimeSkillSlot::Skill1) <= 0.f && Dist < 280.f)
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
