// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyFighterAIController.h"

#include "EnemyCombatComponent.h"
#include "EnemyFighter.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHealthComponent.h"

AEnemyFighterAIController::AEnemyFighterAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyFighterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Fighter = Cast<AEnemyFighter>(InPawn);
	Combat = InPawn ? InPawn->FindComponentByClass<UEnemyCombatComponent>() : nullptr;
	MoveCooldowns.Reset();
	if (Fighter)
	{
		MoveCooldowns.SetNumZeroed(Fighter->GetMoves().Num());
	}
	State = EEnemyFighterState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
}

APawn* AEnemyFighterAIController::FindPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AEnemyFighterAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Fighter || !Combat)
	{
		return;
	}
	if (Fighter->GetEnemyPresence() == EEnemyPresence::Sleep
		|| Fighter->GetEnemyPresence() == EEnemyPresence::Despawned)
	{
		StopMovement();
		ClearTelegraphFx();
		State = EEnemyFighterState::Idle;
		return;
	}
	if (USlimeHealthComponent* Health = Fighter->GetEnemyHealth())
	{
		if (!Health->IsAlive())
		{
			StopMovement();
			ClearTelegraphFx();
			return;
		}
	}

	for (float& Cd : MoveCooldowns)
	{
		Cd = FMath::Max(Cd - DeltaSeconds, 0.f);
	}

	APawn* Player = FindPlayerPawn();
	if (!Player)
	{
		return;
	}

	const float Dist = FVector::Dist(Fighter->GetActorLocation(), Player->GetActorLocation());

	if (State == EEnemyFighterState::Idle)
	{
		TickIdle(Dist);
		return;
	}

	if (Dist > Fighter->LeashRange)
	{
		StopMovement();
		ClearTelegraphFx();
		Combat->InterruptCombat();
		State = EEnemyFighterState::Idle;
		ActiveMoveIndex = INDEX_NONE;
		return;
	}

	if (Fighter->GetEnemyPresence() == EEnemyPresence::Idle)
	{
		// Budgeted out of Active: hold position, don't chase or attack.
		StopMovement();
		return;
	}

	switch (State)
	{
	case EEnemyFighterState::Combat:
	case EEnemyFighterState::Choose:
		TickCombat(DeltaSeconds, Dist);
		break;
	case EEnemyFighterState::Telegraph:
		TickTelegraph(DeltaSeconds);
		break;
	case EEnemyFighterState::Execute:
		TickExecute();
		break;
	case EEnemyFighterState::Recover:
		TickRecover(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AEnemyFighterAIController::TickIdle(float Dist)
{
	if (Dist <= Fighter->DetectRange && Fighter->GetEnemyPresence() == EEnemyPresence::Active)
	{
		State = EEnemyFighterState::Combat;
		StateTime = 0.f;
	}
}

void AEnemyFighterAIController::TickCombat(float DeltaSeconds, float Dist)
{
	FacePlayer();
	RequestMoveToPreferred(Dist);

	if (Combat->IsAttacking())
	{
		return;
	}

	if (ActiveMoveIndex != INDEX_NONE)
	{
		const FEnemyMoveDef& Prev = Fighter->GetMoves()[ActiveMoveIndex];
		if (Prev.NextMoveId != NAME_None)
		{
			const int32 Next = FindMoveIndexById(Prev.NextMoveId);
			if (Next != INDEX_NONE && MoveCooldowns.IsValidIndex(Next) && MoveCooldowns[Next] <= 0.f)
			{
				EnterTelegraph(Next);
				return;
			}
		}
		ActiveMoveIndex = INDEX_NONE;
	}

	const int32 Chosen = SelectMove(Dist);
	if (Chosen != INDEX_NONE)
	{
		EnterTelegraph(Chosen);
	}
}

void AEnemyFighterAIController::EnterTelegraph(int32 MoveIndex)
{
	if (!Fighter->GetMoves().IsValidIndex(MoveIndex))
	{
		return;
	}
	ActiveMoveIndex = MoveIndex;
	State = EEnemyFighterState::Telegraph;
	StateTime = 0.f;
	StopMovement();
	FacePlayer();
	ClearTelegraphFx();

	const FEnemyMoveDef& Move = Fighter->GetMoves()[MoveIndex];
	USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, Fighter);
	if (UNiagaraSystem* Fx = Move.TelegraphNiagara.LoadSynchronous())
	{
		TelegraphFx = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			Fx,
			Fighter->GetActorLocation(),
			Fighter->GetActorRotation(),
			FVector(1.f),
			true,
			true);
	}
}

void AEnemyFighterAIController::TickTelegraph(float DeltaSeconds)
{
	FacePlayer();
	StateTime += DeltaSeconds;
	if (!Fighter->GetMoves().IsValidIndex(ActiveMoveIndex))
	{
		State = EEnemyFighterState::Combat;
		return;
	}
	const FEnemyMoveDef& Move = Fighter->GetMoves()[ActiveMoveIndex];
	if (StateTime >= Move.TelegraphTime)
	{
		BeginExecute();
	}
}

void AEnemyFighterAIController::BeginExecute()
{
	ClearTelegraphFx();
	if (!Fighter->GetMoves().IsValidIndex(ActiveMoveIndex) || !Combat)
	{
		State = EEnemyFighterState::Combat;
		return;
	}
	const FEnemyMoveDef& Move = Fighter->GetMoves()[ActiveMoveIndex];
	FacePlayer();
	if (!Combat->TryExecute(Move.Skill))
	{
		State = EEnemyFighterState::Combat;
		return;
	}
	if (MoveCooldowns.IsValidIndex(ActiveMoveIndex))
	{
		MoveCooldowns[ActiveMoveIndex] = Move.Cooldown;
	}
	USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, Fighter);
	State = EEnemyFighterState::Execute;
	StateTime = 0.f;
}

void AEnemyFighterAIController::TickExecute()
{
	FacePlayer();
	if (!Combat->IsAttacking())
	{
		State = EEnemyFighterState::Recover;
		StateTime = 0.f;
	}
}

void AEnemyFighterAIController::TickRecover(float DeltaSeconds)
{
	StateTime += DeltaSeconds;
	const float RecoverTime = Fighter->GetMoves().IsValidIndex(ActiveMoveIndex)
		? Fighter->GetMoves()[ActiveMoveIndex].Skill.Recovery * 0.35f
		: 0.15f;
	if (StateTime >= RecoverTime)
	{
		State = EEnemyFighterState::Choose;
		StateTime = 0.f;
	}
}

int32 AEnemyFighterAIController::SelectMove(float Dist) const
{
	const TArray<FEnemyMoveDef>& Moves = Fighter->GetMoves();
	float TotalWeight = 0.f;
	TArray<int32> Candidates;
	Candidates.Reserve(Moves.Num());

	const bool bNeedGapClose = Dist > Fighter->PreferredDistance * 1.6f;

	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const FEnemyMoveDef& Move = Moves[Index];
		if (MoveCooldowns.IsValidIndex(Index) && MoveCooldowns[Index] > 0.f)
		{
			continue;
		}
		if (Dist < Move.MinRange || Dist > Move.MaxRange)
		{
			continue;
		}
		float W = FMath::Max(Move.Weight, 0.01f);
		if (bNeedGapClose && Move.bGapCloser)
		{
			W *= 2.5f;
		}
		TotalWeight += W;
		Candidates.Add(Index);
	}

	if (Candidates.Num() == 0 || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 Index : Candidates)
	{
		float W = FMath::Max(Moves[Index].Weight, 0.01f);
		if (bNeedGapClose && Moves[Index].bGapCloser)
		{
			W *= 2.5f;
		}
		Roll -= W;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Candidates.Last();
}

int32 AEnemyFighterAIController::FindMoveIndexById(FName MoveId) const
{
	if (MoveId.IsNone() || !Fighter)
	{
		return INDEX_NONE;
	}
	const TArray<FEnemyMoveDef>& Moves = Fighter->GetMoves();
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		if (Moves[Index].MoveId == MoveId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AEnemyFighterAIController::FacePlayer()
{
	APawn* Player = FindPlayerPawn();
	APawn* MyPawn = GetPawn();
	if (!Player || !MyPawn)
	{
		return;
	}
	FVector To = Player->GetActorLocation() - MyPawn->GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		MyPawn->SetActorRotation(To.Rotation());
	}
}

void AEnemyFighterAIController::RequestMoveToPreferred(float Dist)
{
	APawn* Player = FindPlayerPawn();
	if (!Player || !Fighter)
	{
		return;
	}

	PathRefreshRemaining -= GetWorld()->GetDeltaSeconds();
	const float Preferred = Fighter->PreferredDistance;
	const bool bTooFar = Dist > Preferred * 1.25f;
	const bool bTooClose = Dist < Preferred * 0.55f;

	if (!bTooFar && !bTooClose)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			StopMovement();
		}
		return;
	}

	if (PathRefreshRemaining > 0.f && GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	PathRefreshRemaining = PathRefreshInterval;

	if (bTooFar)
	{
		MoveToActor(Player, Preferred * 0.85f);
	}
	else
	{
		// Step away slightly when too close.
		const FVector Away = (Fighter->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		const FVector Dest = Player->GetActorLocation() + Away * Preferred;
		MoveToLocation(Dest);
	}
}

void AEnemyFighterAIController::ClearTelegraphFx()
{
	if (UNiagaraComponent* Fx = TelegraphFx.Get())
	{
		Fx->DeactivateImmediate();
		Fx->DestroyComponent();
	}
	TelegraphFx.Reset();
}
