// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperHeroXinAIController.h"

#include "EnemyCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeCombatTypes.h"
#include "SlimeFable.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "SuperHeroXinEnemy.h"

ASuperHeroXinAIController::ASuperHeroXinAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASuperHeroXinAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Enemy = Cast<ASuperHeroXinEnemy>(InPawn);
	Combat = InPawn ? InPawn->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
	if (Enemy)
	{
		Enemy->EnsureCombatReady();
	}
	MoveCooldowns.Reset();
	if (Enemy)
	{
		MoveCooldowns.SetNumZeroed(Enemy->GetMoves().Num());
	}
	State = ESuperHeroXinAIState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
	SetActorTickEnabled(true);
	UE_LOG(LogSlimeFable, Log, TEXT("SuperHeroXinAI possessed %s Combat=%s Moves=%d"),
		*GetNameSafe(Enemy), *GetNameSafe(Combat), Enemy ? Enemy->GetMoves().Num() : -1);
}

void ASuperHeroXinAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Enemy || !Combat)
	{
		return;
	}
	if (Enemy->IsDevourLocked() || Enemy->IsInDeathSequence() || Enemy->IsMorphTarget())
	{
		Enemy->ClearAiMoveIntent();
		return;
	}
	if (USlimeHealthComponent* Health = Enemy->GetEnemyHealth())
	{
		if (!Health->IsAlive())
		{
			Enemy->ClearAiMoveIntent();
			return;
		}
	}

	if (MoveCooldowns.Num() != Enemy->GetMoves().Num())
	{
		MoveCooldowns.SetNumZeroed(Enemy->GetMoves().Num());
	}
	for (float& Cd : MoveCooldowns)
	{
		Cd = FMath::Max(Cd - DeltaSeconds, 0.f);
	}

	APawn* Player = FindCombatFocus();
	const float Dist = Player
		? FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation())
		: TNumericLimits<float>::Max();
	const float Dist2D = Player
		? FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation())
		: TNumericLimits<float>::Max();

	if (!Player)
	{
		ReturnToIdle();
		return;
	}
	if (Dist > Enemy->LeashRange && State != ESuperHeroXinAIState::Idle)
	{
		ReturnToIdle();
		return;
	}

	switch (State)
	{
	case ESuperHeroXinAIState::Idle:
		TickIdle(Dist);
		break;
	case ESuperHeroXinAIState::Chase:
		TickChase(Dist2D);
		break;
	case ESuperHeroXinAIState::Telegraph:
		TickTelegraph(DeltaSeconds);
		break;
	case ESuperHeroXinAIState::Execute:
		TickExecute();
		break;
	case ESuperHeroXinAIState::Recover:
		TickRecover(DeltaSeconds);
		break;
	default:
		break;
	}
}

void ASuperHeroXinAIController::ReturnToIdle()
{
	if (Enemy)
	{
		Enemy->ClearAiMoveIntent();
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	State = ESuperHeroXinAIState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
}

APawn* ASuperHeroXinAIController::FindCombatFocus() const
{
	APawn* Self = GetPawn();
	if (Self && USlimeHitProbe::GetTeam(Self) == ESlimeTeam::Player)
	{
		APawn* Best = nullptr;
		float BestDistSq = FMath::Square(2000.f);
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				APawn* Other = *It;
				if (!Other || Other == Self || !USlimeHitProbe::IsHostile(Self, Other))
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
		}
		return Best;
	}
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void ASuperHeroXinAIController::TickIdle(float Dist)
{
	APawn* Focus = FindCombatFocus();
	if (Focus && USlimeHitProbe::IsHostile(Enemy, Focus) && Dist <= Enemy->DetectRange)
	{
		State = ESuperHeroXinAIState::Chase;
		StateTime = 0.f;
		UE_LOG(LogSlimeFable, Log, TEXT("SuperHeroXinAI %s -> Chase (dist=%.0f)"), *GetNameSafe(Enemy), Dist);
		return;
	}
	Enemy->ClearAiMoveIntent();
}

void ASuperHeroXinAIController::TickChase(float Dist2D)
{
	FacePlayer();
	DriveTowardPlayer(Enemy->PreferredDistance);
	if (Combat->IsAttacking())
	{
		return;
	}
	const int32 Chosen = SelectMove(Dist2D);
	if (Chosen != INDEX_NONE)
	{
		EnterTelegraph(Chosen);
	}
}

void ASuperHeroXinAIController::EnterTelegraph(int32 MoveIndex)
{
	if (!Enemy->GetMoves().IsValidIndex(MoveIndex))
	{
		return;
	}
	ActiveMoveIndex = MoveIndex;
	State = ESuperHeroXinAIState::Telegraph;
	StateTime = 0.f;
	Enemy->ClearAiMoveIntent();
	FacePlayer();
}

void ASuperHeroXinAIController::TickTelegraph(float DeltaSeconds)
{
	FacePlayer();
	StateTime += DeltaSeconds;
	if (!Enemy->GetMoves().IsValidIndex(ActiveMoveIndex))
	{
		State = ESuperHeroXinAIState::Chase;
		return;
	}
	if (StateTime >= Enemy->GetMoves()[ActiveMoveIndex].TelegraphTime)
	{
		BeginExecute();
	}
}

void ASuperHeroXinAIController::BeginExecute()
{
	if (!Enemy->GetMoves().IsValidIndex(ActiveMoveIndex) || !Combat)
	{
		State = ESuperHeroXinAIState::Chase;
		return;
	}
	APawn* Focus = FindCombatFocus();
	if (!Focus)
	{
		State = ESuperHeroXinAIState::Chase;
		ActiveMoveIndex = INDEX_NONE;
		return;
	}
	const FEnemyMoveDef& Move = Enemy->GetMoves()[ActiveMoveIndex];
	const float Dist2D = FVector::Dist2D(Enemy->GetActorLocation(), Focus->GetActorLocation());
	if (Dist2D > Move.MaxRange * 1.15f)
	{
		ActiveMoveIndex = INDEX_NONE;
		State = ESuperHeroXinAIState::Chase;
		return;
	}
	FacePlayer();
	if (!Combat->TryExecute(Move.Skill))
	{
		State = ESuperHeroXinAIState::Chase;
		return;
	}
	if (MoveCooldowns.IsValidIndex(ActiveMoveIndex))
	{
		MoveCooldowns[ActiveMoveIndex] = Move.Cooldown;
	}
	State = ESuperHeroXinAIState::Execute;
	StateTime = 0.f;
}

void ASuperHeroXinAIController::TickExecute()
{
	FacePlayer();
	if (!Combat->IsAttacking())
	{
		State = ESuperHeroXinAIState::Recover;
		StateTime = 0.f;
	}
}

void ASuperHeroXinAIController::TickRecover(float DeltaSeconds)
{
	StateTime += DeltaSeconds;
	const float RecoverTime = (Enemy->GetMoves().IsValidIndex(ActiveMoveIndex)
		? Enemy->GetMoves()[ActiveMoveIndex].Skill.Recovery * 0.35f
		: 0.15f);
	if (StateTime >= RecoverTime)
	{
		State = ESuperHeroXinAIState::Chase;
		StateTime = 0.f;
		ActiveMoveIndex = INDEX_NONE;
	}
}

int32 ASuperHeroXinAIController::SelectMove(float Dist2D) const
{
	const TArray<FEnemyMoveDef>& Moves = Enemy->GetMoves();
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const FEnemyMoveDef& Move = Moves[Index];
		if (Move.Weight <= 0.f)
		{
			continue;
		}
		if (MoveCooldowns.IsValidIndex(Index) && MoveCooldowns[Index] > 0.f)
		{
			continue;
		}
		if (Dist2D < Move.MinRange || Dist2D > Move.MaxRange)
		{
			continue;
		}
		return Index;
	}
	return INDEX_NONE;
}

void ASuperHeroXinAIController::FacePlayer()
{
	APawn* Player = FindCombatFocus();
	if (!Player || !Enemy)
	{
		return;
	}
	FVector To = Player->GetActorLocation() - Enemy->GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero())
	{
		return;
	}
	const FRotator Face = To.Rotation();
	Enemy->SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	SetControlRotation(FRotator(0.f, Face.Yaw, 0.f));
}

void ASuperHeroXinAIController::DriveTowardPlayer(float Preferred)
{
	APawn* Player = FindCombatFocus();
	if (!Player || !Enemy)
	{
		return;
	}
	const float Dist2D = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
	if (Dist2D <= Preferred * 1.1f)
	{
		Enemy->ClearAiMoveIntent();
		return;
	}
	FVector To = Player->GetActorLocation() - Enemy->GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero())
	{
		Enemy->ClearAiMoveIntent();
		return;
	}
	Enemy->SetAiMoveIntent(To.GetSafeNormal());
}
