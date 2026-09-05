// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaspEnemyAIController.h"

#include "EnemyCombatComponent.h"
#include "GaspSandboxPawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/RecastNavMesh.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "SlimeFable.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"

AGaspEnemyAIController::AGaspEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGaspEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Enemy = Cast<AGaspSandboxPawn>(InPawn);
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
	State = EGaspEnemyAIState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
	UE_LOG(LogSlimeFable, Log, TEXT("GaspAI possessed %s Combat=%s Moves=%d Detect=%.0f"),
		*GetNameSafe(Enemy),
		*GetNameSafe(Combat),
		Enemy ? Enemy->GetMoves().Num() : -1,
		Enemy ? Enemy->DetectRange : -1.f);
}

void AGaspEnemyAIController::ReturnToIdle()
{
	StopMovement();
	if (Enemy)
	{
		Enemy->ClearAiMoveIntent();
		Enemy->ClearAiFaceIntent();
		Enemy->SetWantChaseGait(false);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	State = EGaspEnemyAIState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
}

APawn* AGaspEnemyAIController::FindCombatFocus() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AGaspEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DriveCombatAI(DeltaSeconds);
}

void AGaspEnemyAIController::DriveCombatAI(float DeltaSeconds)
{
	const uint64 Frame = GFrameCounter;
	if (LastDriveFrame == Frame)
	{
		return;
	}
	LastDriveFrame = Frame;

	if (!Enemy || !Combat)
	{
		return;
	}
	if (Enemy->IsDevourLocked() || Enemy->IsInDeathSequence() || Enemy->IsMorphTarget()
		|| Enemy->IsCombatKnockdown())
	{
		StopMovement();
		Enemy->ClearAiMoveIntent();
		Enemy->ClearAiFaceIntent();
		return;
	}
	if (USlimeHealthComponent* Health = Enemy->GetEnemyHealth())
	{
		if (!Health->IsAlive())
		{
			StopMovement();
			Enemy->ClearAiMoveIntent();
			Enemy->ClearAiFaceIntent();
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

	if (Dist > Enemy->LeashRange && State != EGaspEnemyAIState::Idle)
	{
		ReturnToIdle();
		return;
	}

	switch (State)
	{
	case EGaspEnemyAIState::Idle:
		TickIdle(DeltaSeconds, Dist);
		break;
	case EGaspEnemyAIState::Chase:
		TickChase(DeltaSeconds, Dist2D);
		break;
	case EGaspEnemyAIState::Telegraph:
		TickTelegraph(DeltaSeconds);
		break;
	case EGaspEnemyAIState::Execute:
		TickExecute();
		break;
	case EGaspEnemyAIState::Recover:
		TickRecover(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AGaspEnemyAIController::TickIdle(float DeltaSeconds, float Dist)
{
	APawn* Focus = FindCombatFocus();
	if (!Focus)
	{
		if (Enemy->bWanderWhenIdle)
		{
			TickWander(DeltaSeconds);
		}
		else
		{
			Enemy->ClearAiMoveIntent();
			Enemy->ClearAiFaceIntent();
			Enemy->SetWantChaseGait(false);
		}
		return;
	}
	const bool bPlayerFocus = Cast<APlayerController>(Focus->GetController()) != nullptr;
	const bool bHostile = USlimeHitProbe::IsHostile(Enemy, Focus) || bPlayerFocus;
	if (bHostile && Dist <= Enemy->DetectRange)
	{
		StopMovement();
		bWasWanderMoving = false;
		WanderPauseRemaining = 0.f;
		State = EGaspEnemyAIState::Chase;
		StateTime = 0.f;
		Enemy->SetWantChaseGait(true);
		UE_LOG(LogSlimeFable, Log, TEXT("GaspAI %s -> Chase (dist=%.0f)"), *GetNameSafe(Enemy), Dist);
		return;
	}

	Enemy->SetWantChaseGait(false);
	if (Enemy->bWanderWhenIdle)
	{
		TickWander(DeltaSeconds);
	}
	else
	{
		Enemy->ClearAiMoveIntent();
		Enemy->ClearAiFaceIntent();
	}
}

void AGaspEnemyAIController::TickWander(float DeltaSeconds)
{
	if (!Enemy)
	{
		return;
	}

	const bool bNavMoving = GetMoveStatus() == EPathFollowingStatus::Moving;
	const bool bIntentMoving = !Enemy->GetAiMoveIntent().IsNearlyZero();
	const bool bMoving = bNavMoving || bIntentMoving;

	if (bWasWanderMoving && !bMoving)
	{
		WanderPauseRemaining = FMath::FRandRange(Enemy->WanderPauseMin, Enemy->WanderPauseMax);
		WanderStuckTime = 0.f;
		Enemy->ClearAiMoveIntent();
		Enemy->ClearAiFaceIntent();
	}
	bWasWanderMoving = bMoving;

	if (bMoving)
	{
		const float MovedSq = FVector::DistSquared2D(Enemy->GetActorLocation(), WanderLastPos);
		if (MovedSq < FMath::Square(30.f))
		{
			WanderStuckTime += DeltaSeconds;
			if (WanderStuckTime >= 2.f)
			{
				StopMovement();
				Enemy->ClearAiMoveIntent();
				WanderPauseRemaining = FMath::FRandRange(Enemy->WanderPauseMin, Enemy->WanderPauseMax);
				WanderStuckTime = 0.f;
				bWasWanderMoving = false;
			}
		}
		else
		{
			WanderStuckTime = 0.f;
		}
		WanderLastPos = Enemy->GetActorLocation();

		// Direct-intent fallback: keep facing / pushing toward dest when Nav fails.
		if (!bNavMoving && !WanderDest.IsNearlyZero())
		{
			FVector To = WanderDest - Enemy->GetActorLocation();
			To.Z = 0.f;
			if (To.SizeSquared2D() < FMath::Square(80.f))
			{
				Enemy->ClearAiMoveIntent();
				bWasWanderMoving = false;
				WanderPauseRemaining = FMath::FRandRange(Enemy->WanderPauseMin, Enemy->WanderPauseMax);
			}
			else
			{
				Enemy->SetAiMoveIntent(To.GetSafeNormal());
				Enemy->SetAiFaceIntent(To.GetSafeNormal());
			}
		}
		return;
	}

	WanderPauseRemaining -= DeltaSeconds;
	if (WanderPauseRemaining > 0.f)
	{
		return;
	}

	FVector Origin = Enemy->GetSpawnOrigin();
	if (Origin.IsNearlyZero())
	{
		Origin = Enemy->GetActorLocation();
	}
	const float Radius = FMath::Max(Enemy->WanderRadius, 50.f);
	const FVector2D Offset = FMath::RandPointInCircle(Radius);
	FVector Dest = Origin + FVector(Offset.X, Offset.Y, 0.f);
	Dest.Z = Enemy->GetActorLocation().Z;
	if (FVector::DistSquared2D(Dest, Enemy->GetActorLocation()) < FMath::Square(80.f))
	{
		WanderPauseRemaining = FMath::FRandRange(Enemy->WanderPauseMin, Enemy->WanderPauseMax);
		return;
	}
	StartWanderTo(Dest);
}

bool AGaspEnemyAIController::HasRecastNavMesh() const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}
	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	return NavData && NavData->IsA<ARecastNavMesh>();
}

void AGaspEnemyAIController::StopPathIfMoving()
{
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		StopMovement();
	}
}

void AGaspEnemyAIController::StartWanderTo(const FVector& Dest)
{
	WanderDest = Dest;
	WanderLastPos = Enemy->GetActorLocation();
	WanderStuckTime = 0.f;

	FVector To = Dest - Enemy->GetActorLocation();
	To.Z = 0.f;
	const FVector Dir = To.GetSafeNormal();

	if (HasRecastNavMesh())
	{
		const EPathFollowingRequestResult::Type Result = MoveToLocation(Dest);
		if (Result == EPathFollowingRequestResult::RequestSuccessful)
		{
			bWasWanderMoving = true;
			Enemy->SetAiMoveIntent(Dir);
			Enemy->SetAiFaceIntent(Dir);
			return;
		}
	}

	// FeatureLab often has no Recast NavMesh — drive Mover via ProduceInput intent.
	StopPathIfMoving();
	Enemy->SetAiMoveIntent(Dir);
	Enemy->SetAiFaceIntent(Dir);
	bWasWanderMoving = true;
}

void AGaspEnemyAIController::DriveTowardPlayer(float Preferred)
{
	APawn* Player = FindCombatFocus();
	if (!Player || !Enemy)
	{
		return;
	}

	const float Dist2D = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
	const bool bTooFar = Dist2D > Preferred * 1.1f;
	const bool bTooClose = Dist2D < Preferred * 0.55f;
	if (!bTooFar && !bTooClose)
	{
		StopPathIfMoving();
		Enemy->ClearAiMoveIntent();
		return;
	}

	FVector To = Player->GetActorLocation() - Enemy->GetActorLocation();
	To.Z = 0.f;
	if (bTooClose)
	{
		To = -To;
	}
	if (To.IsNearlyZero())
	{
		Enemy->ClearAiMoveIntent();
		return;
	}

	const FVector Dir = To.GetSafeNormal();
	Enemy->SetAiMoveIntent(Dir);

	if (!HasRecastNavMesh())
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const FVector Goal = Player->GetActorLocation();
	const bool bNeedNewRequest = GetMoveStatus() == EPathFollowingStatus::Idle
		|| FVector::DistSquared2D(Goal, LastNavGoal) > FMath::Square(150.f)
		|| (Now - LastNavRequestTime) > 0.5f;
	if (!bNeedNewRequest)
	{
		return;
	}

	LastNavRequestTime = Now;
	LastNavGoal = Goal;
	const EPathFollowingRequestResult::Type NavResult = MoveToActor(Player, Preferred * 0.85f);
	if (NavResult == EPathFollowingRequestResult::Failed
		|| NavResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		StopPathIfMoving();
	}
}

void AGaspEnemyAIController::TickChase(float DeltaSeconds, float Dist2D)
{
	(void)DeltaSeconds;
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

void AGaspEnemyAIController::EnterTelegraph(int32 MoveIndex)
{
	if (!Enemy->GetMoves().IsValidIndex(MoveIndex))
	{
		return;
	}
	ActiveMoveIndex = MoveIndex;
	State = EGaspEnemyAIState::Telegraph;
	StateTime = 0.f;
	StopMovement();
	Enemy->ClearAiMoveIntent();
	FacePlayer();
	UE_LOG(LogSlimeFable, Verbose, TEXT("GaspAI %s -> Telegraph move=%d"), *GetNameSafe(Enemy), MoveIndex);
}

void AGaspEnemyAIController::TickTelegraph(float DeltaSeconds)
{
	FacePlayer();
	StateTime += DeltaSeconds;
	if (!Enemy->GetMoves().IsValidIndex(ActiveMoveIndex))
	{
		State = EGaspEnemyAIState::Chase;
		return;
	}
	const FEnemyMoveDef& Move = Enemy->GetMoves()[ActiveMoveIndex];
	if (StateTime >= Move.TelegraphTime)
	{
		BeginExecute();
	}
}

void AGaspEnemyAIController::BeginExecute()
{
	if (!Enemy->GetMoves().IsValidIndex(ActiveMoveIndex) || !Combat)
	{
		State = EGaspEnemyAIState::Chase;
		return;
	}
	APawn* Focus = FindCombatFocus();
	if (!Focus)
	{
		State = EGaspEnemyAIState::Chase;
		ActiveMoveIndex = INDEX_NONE;
		return;
	}

	const FEnemyMoveDef& Move = Enemy->GetMoves()[ActiveMoveIndex];
	const float Dist2D = FVector::Dist2D(Enemy->GetActorLocation(), Focus->GetActorLocation());
	if (Dist2D > Move.MaxRange * 1.15f)
	{
		ActiveMoveIndex = INDEX_NONE;
		State = EGaspEnemyAIState::Chase;
		return;
	}

	FacePlayer();
	if (!Combat->TryExecute(Move.Skill))
	{
		State = EGaspEnemyAIState::Chase;
		return;
	}
	if (MoveCooldowns.IsValidIndex(ActiveMoveIndex))
	{
		MoveCooldowns[ActiveMoveIndex] = Move.Cooldown;
	}
	State = EGaspEnemyAIState::Execute;
	StateTime = 0.f;
	UE_LOG(LogSlimeFable, Verbose, TEXT("GaspAI %s -> Execute %s"), *GetNameSafe(Enemy), *Move.MoveId.ToString());
}

void AGaspEnemyAIController::TickExecute()
{
	FacePlayer();
	if (!Combat->IsAttacking())
	{
		State = EGaspEnemyAIState::Recover;
		StateTime = 0.f;
	}
}

void AGaspEnemyAIController::TickRecover(float DeltaSeconds)
{
	StateTime += DeltaSeconds;
	const float RecoverTime = (Enemy->GetMoves().IsValidIndex(ActiveMoveIndex)
		? Enemy->GetMoves()[ActiveMoveIndex].Skill.Recovery * 0.35f
		: 0.15f);
	if (StateTime >= RecoverTime)
	{
		State = EGaspEnemyAIState::Chase;
		StateTime = 0.f;
		ActiveMoveIndex = INDEX_NONE;
	}
}

int32 AGaspEnemyAIController::SelectMove(float Dist2D) const
{
	const TArray<FEnemyMoveDef>& Moves = Enemy->GetMoves();
	TArray<int32> Candidates;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const FEnemyMoveDef& Move = Moves[Index];
		if (Move.Skill.Exec == EEnemySkillExec::Dash)
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
		TotalWeight += FMath::Max(Move.Weight, 0.01f);
		Candidates.Add(Index);
	}
	if (Candidates.Num() == 0 || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 Index : Candidates)
	{
		Roll -= FMath::Max(Moves[Index].Weight, 0.01f);
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Candidates.Last();
}

void AGaspEnemyAIController::FacePlayer()
{
	APawn* Player = FindCombatFocus();
	if (!Player || !Enemy)
	{
		return;
	}
	FVector To = Player->GetActorLocation() - Enemy->GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		// Feed OrientationIntent via ProduceInput — SetActorRotation is overwritten by Mover FinalizeFrame.
		Enemy->SetAiFaceIntent(To.GetSafeNormal());
	}
}
