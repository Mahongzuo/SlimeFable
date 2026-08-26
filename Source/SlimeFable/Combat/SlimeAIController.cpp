// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeAIController.h"

#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "SlimeCombatComponent.h"
#include "SlimeEnemyCharacter.h"
#include "SlimeFable.h"

ASlimeAIController::ASlimeAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASlimeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Combat = InPawn ? InPawn->FindComponentByClass<USlimeCombatComponent>() : nullptr;
	ResetChaseFallback();
}

APawn* ASlimeAIController::FindPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void ASlimeAIController::ResetChaseFallback()
{
	ChaseStalledSeconds = 0.f;
	bDirectChaseFallback = false;
	DirectChaseActiveSeconds = 0.f;
	ChaseLastPos = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
}

bool ASlimeAIController::TryMoveToNavLocation(const FVector& Dest)
{
	APawn* MyPawn = GetPawn();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FVector Target = Dest;
	if (NavSys)
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Dest, Projected))
		{
			Target = Projected.Location;
		}
		else
		{
			UE_LOG(LogSlimeFable, Verbose, TEXT("SlimeAI: no nav projection near %s"), *Dest.ToCompactString());
			return false;
		}
	}
	if (MyPawn && FVector::DistSquared2D(Target, MyPawn->GetActorLocation()) < FMath::Square(40.f))
	{
		return false;
	}
	return MoveToLocation(Target) == EPathFollowingRequestResult::RequestSuccessful;
}

bool ASlimeAIController::RequestNavDetourTowardFocus(APawn* Player)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Player)
	{
		return false;
	}

	const FVector Self = MyPawn->GetActorLocation();
	const FVector ToPlayer = (Player->GetActorLocation() - Self).GetSafeNormal2D();
	if (ToPlayer.IsNearlyZero())
	{
		StopMovement();
		return false;
	}

	const FVector Right = FVector::CrossProduct(ToPlayer, FVector::UpVector).GetSafeNormal();
	const float Near = SideStepOffset;
	const float Far = SideStepOffset * 2.f;
	const FVector Offsets[6] = {
		ToPlayer * Far,
		(ToPlayer + Right).GetSafeNormal() * Far,
		(ToPlayer - Right).GetSafeNormal() * Far,
		(ToPlayer + Right * 0.5f).GetSafeNormal() * Near,
		(ToPlayer - Right * 0.5f).GetSafeNormal() * Near,
		Right * Near,
	};

	for (int32 Step = 0; Step < 6; ++Step)
	{
		const int32 Idx = (DetourSampleIndex + Step) % 6;
		if (TryMoveToNavLocation(Self + Offsets[Idx]))
		{
			DetourSampleIndex = (Idx + 1) % 6;
			return true;
		}
	}

	DetourSampleIndex = (DetourSampleIndex + 1) % 6;
	StopMovement();
	return false;
}

void ASlimeAIController::UpdateChase(APawn* MyPawn, APawn* Player, float Dist)
{
	if (Dist <= ApproachAcceptRadius)
	{
		StopMovement();
		PathRefreshRemaining = 0.f;
		ResetChaseFallback();
		return;
	}

	const float DeltaSec = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const FVector Here = MyPawn->GetActorLocation();
	if (FVector::DistSquared2D(Here, ChaseLastPos) < FMath::Square(20.f))
	{
		ChaseStalledSeconds += DeltaSec;
	}
	else
	{
		ChaseStalledSeconds = 0.f;
	}
	ChaseLastPos = Here;

	if (bDirectChaseFallback)
	{
		DirectChaseActiveSeconds += DeltaSec;
		if (DirectChaseActiveSeconds >= DirectChaseMaxSeconds || ChaseStalledSeconds >= 0.5f)
		{
			ResetChaseFallback();
			if (!RequestNavDetourTowardFocus(Player))
			{
				bDirectChaseFallback = true;
				DirectChaseActiveSeconds = 0.f;
			}
			PathRefreshRemaining = PathRefreshInterval;
			return;
		}
		StopMovement();
		FVector ToPlayer = Player->GetActorLocation() - MyPawn->GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			MyPawn->AddMovementInput(ToPlayer.GetSafeNormal(), 1.f);
		}
		return;
	}

	if ((GetMoveStatus() == EPathFollowingStatus::Moving && ChaseStalledSeconds >= 0.5f)
		|| ChaseStalledSeconds >= 0.75f)
	{
		StopMovement();
		if (!RequestNavDetourTowardFocus(Player))
		{
			bDirectChaseFallback = true;
			DirectChaseActiveSeconds = 0.f;
		}
		ChaseStalledSeconds = 0.f;
		PathRefreshRemaining = PathRefreshInterval;
		return;
	}

	PathRefreshRemaining -= DeltaSec;
	if (PathRefreshRemaining > 0.f && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	PathRefreshRemaining = PathRefreshInterval;

	const EPathFollowingRequestResult::Type Result = MoveToActor(Player, ApproachAcceptRadius);
	const bool bNavFailed = Result == EPathFollowingRequestResult::Failed
		|| (Result == EPathFollowingRequestResult::AlreadyAtGoal && Dist > ApproachAcceptRadius);
	if (bNavFailed)
	{
		if (!RequestNavDetourTowardFocus(Player))
		{
			bDirectChaseFallback = true;
			DirectChaseActiveSeconds = 0.f;
			StopMovement();
			FVector ToPlayer = Player->GetActorLocation() - MyPawn->GetActorLocation();
			ToPlayer.Z = 0.f;
			if (!ToPlayer.IsNearlyZero())
			{
				MyPawn->AddMovementInput(ToPlayer.GetSafeNormal(), 1.f);
			}
		}
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
		ResetChaseFallback();
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
