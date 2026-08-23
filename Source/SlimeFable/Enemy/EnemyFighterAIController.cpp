// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyFighterAIController.h"

#include "AITypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyCombatComponent.h"
#include "EnemyFighter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "Components/StateTreeComponent.h"
#include "StateTree.h"

AEnemyFighterAIController::AEnemyFighterAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("EnemyDecisionStateTree"));
	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AEnemyFighterAIController::ReturnToIdle()
{
	StopMovement();
	ClearTelegraphFx();
	StopIdleMontage();
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	State = EEnemyFighterState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
	ClearDirectWander();
	ResetChaseFallback();
	WanderPauseRemaining = 0.f;
	ApplyLocomotionMaxSpeed(false);
	TryPlayRandomIdleMontage();
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
	bUsingStateTree = false;
	if (StateTreeComponent && !DecisionStateTree.IsNull())
	{
		if (UStateTree* Tree = DecisionStateTree.LoadSynchronous())
		{
			StateTreeComponent->SetStateTree(Tree);
			StateTreeComponent->StartLogic();
			bUsingStateTree = true;
		}
	}
	State = EEnemyFighterState::Idle;
	ActiveMoveIndex = INDEX_NONE;
	StateTime = 0.f;
	ClearDirectWander();
	ResetChaseFallback();
	WanderPauseRemaining = Fighter
		? FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax)
		: 1.5f;
}

APawn* AEnemyFighterAIController::FindCombatFocus() const
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
	if (bUsingStateTree)
	{
		return;
	}
	if (Fighter->IsDevourLocked())
	{
		StopMovement();
		return;
	}
	if (Fighter->GetEnemyPresence() == EEnemyPresence::Sleep
		|| Fighter->GetEnemyPresence() == EEnemyPresence::Despawned)
	{
		StopMovement();
		ClearTelegraphFx();
		State = EEnemyFighterState::Idle;
		ResetChaseFallback();
		ApplyLocomotionMaxSpeed(false);
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

	APawn* Player = FindCombatFocus();
	const float Dist = Player
		? FVector::Dist(Fighter->GetActorLocation(), Player->GetActorLocation())
		: TNumericLimits<float>::Max();

	if (State == EEnemyFighterState::Idle)
	{
		TickIdle(DeltaSeconds, Dist);
		return;
	}

	if (!Player)
	{
		ReturnToIdle();
		return;
	}

	if (Dist > Fighter->LeashRange)
	{
		StopMovement();
		ClearTelegraphFx();
		ClearDirectWander();
		Combat->InterruptCombat();
		State = EEnemyFighterState::Idle;
		ActiveMoveIndex = INDEX_NONE;
		ResetChaseFallback();
		ApplyLocomotionMaxSpeed(false);
		return;
	}

	if (Fighter->GetEnemyPresence() == EEnemyPresence::Idle)
	{
		// Budgeted out of Active: hold position, don't chase or attack.
		StopMovement();
		ResetChaseFallback();
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

void AEnemyFighterAIController::TickIdle(float DeltaSeconds, float Dist)
{
	if (Fighter->bPassive)
	{
		if (Fighter->bWanderWhenIdle)
		{
			TickWander(DeltaSeconds);
		}
		return;
	}

	APawn* Focus = FindCombatFocus();
	const bool bHostile = Focus && USlimeHitProbe::IsHostile(Fighter, Focus);
	if (bHostile && Dist <= Fighter->DetectRange && Fighter->GetEnemyPresence() == EEnemyPresence::Active)
	{
		StopIdleMontage();
		StopMovement();
		ClearDirectWander();
		State = EEnemyFighterState::Combat;
		StateTime = 0.f;
		ResetChaseFallback();
		ApplyLocomotionMaxSpeed(true);
		return;
	}

	if (Fighter->bWanderWhenIdle)
	{
		TickWander(DeltaSeconds);
	}
}

void AEnemyFighterAIController::TickWander(float DeltaSeconds)
{
	if (bDirectWander)
	{
		TickDirectWander();
		return;
	}

	const bool bMoving = GetMoveStatus() == EPathFollowingStatus::Moving;
	if (bWasWanderMoving && !bMoving)
	{
		TryPlayRandomIdleMontage();
		WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
		WanderStuckTime = 0.f;
	}
	bWasWanderMoving = bMoving;
	if (bMoving)
	{
		// 卡住检测：导航移动中长时间没位移则中止，避免撞墙原地走。
		const float MovedSq = FVector::DistSquared2D(Fighter->GetActorLocation(), WanderLastPos);
		if (MovedSq < FMath::Square(30.f))
		{
			WanderStuckTime += DeltaSeconds;
			if (WanderStuckTime >= 2.f)
			{
				StopMovement();
				StopLocomotionAnim();
				TryPlayRandomIdleMontage();
				WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
				WanderStuckTime = 0.f;
				bWasWanderMoving = false;
			}
		}
		else
		{
			WanderStuckTime = 0.f;
		}
		WanderLastPos = Fighter->GetActorLocation();
		return;
	}

	WanderPauseRemaining -= DeltaSeconds;
	if (WanderPauseRemaining > 0.f)
	{
		if (!PlayingIdleMontage.IsValid())
		{
			TryPlayRandomIdleMontage();
		}
		return;
	}

	const FVector Origin = Fighter->GetSpawnTransform().GetLocation();
	const float Radius = FMath::Max(Fighter->WanderRadius, 50.f);
	const FVector2D Offset = FMath::RandPointInCircle(Radius);
	FVector Dest = Origin + FVector(Offset.X, Offset.Y, 0.f);
	Dest.Z = Fighter->GetActorLocation().Z;
	if (FVector::DistSquared2D(Dest, Fighter->GetActorLocation()) < FMath::Square(80.f))
	{
		TryPlayRandomIdleMontage();
		WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
		return;
	}

	StartWanderTo(Dest);
}

void AEnemyFighterAIController::StartWanderTo(const FVector& Dest)
{
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Dest);
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		bDirectWander = false;
		bWasWanderMoving = true;
		PlayWalkAnim();
		return;
	}
	if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		TryPlayRandomIdleMontage();
		WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
		return;
	}

	StopMovement();
	WanderDirectDest = Dest;
	bDirectWander = true;
	bWasWanderMoving = true;
	WanderStuckTime = 0.f;
	WanderLastPos = Fighter->GetActorLocation();
	PlayWalkAnim();
}

void AEnemyFighterAIController::TickDirectWander()
{
	if (!Fighter)
	{
		ClearDirectWander();
		return;
	}

	FVector To = WanderDirectDest - Fighter->GetActorLocation();
	To.Z = 0.f;
	if (To.SizeSquared() < FMath::Square(80.f))
	{
		ClearDirectWander();
		TryPlayRandomIdleMontage();
		WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
		return;
	}

	// 卡住检测：如果 1.5 秒内位移不足 30 单位，视为撞墙，放弃当前目标。
	const float DeltaSec = GetWorld()->GetDeltaSeconds();
	const float MovedSq = FVector::DistSquared2D(Fighter->GetActorLocation(), WanderLastPos);
	if (MovedSq < FMath::Square(30.f))
	{
		WanderStuckTime += DeltaSec;
		if (WanderStuckTime >= 1.5f)
		{
			ClearDirectWander();
			StopLocomotionAnim();
			TryPlayRandomIdleMontage();
			WanderPauseRemaining = FMath::FRandRange(Fighter->WanderPauseMin, Fighter->WanderPauseMax);
			return;
		}
	}
	else
	{
		WanderStuckTime = 0.f;
	}
	WanderLastPos = Fighter->GetActorLocation();

	const FVector Dir = To.GetSafeNormal();
	Fighter->AddMovementInput(Dir, 1.f);
	Fighter->SetActorRotation(Dir.Rotation());
}

void AEnemyFighterAIController::ClearDirectWander()
{
	bDirectWander = false;
	bWasWanderMoving = false;
	bPlayingWalk = false;
	bPlayingRun = false;
}

void AEnemyFighterAIController::ApplyLocomotionMaxSpeed(bool bChasing)
{
	if (!Fighter || !Fighter->bABPDrivenLocomotion)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Fighter->GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bChasing ? Fighter->ChaseSpeed : Fighter->WalkSpeed;
	}
	// 追逐时切 Run 蒙太奇，闲逛时切 Walk 蒙太奇。
	if (bChasing)
	{
		PlayRunAnim();
	}
	else if (bPlayingWalk && bPlayingRun)
	{
		// 从 Run 切回 Walk
		PlayWalkAnim();
	}
}

void AEnemyFighterAIController::UpdateWalkPlayRate()
{
	if (!Fighter)
	{
		return;
	}
	USkeletalMeshComponent* Skel = Fighter->GetMesh();
	if (!Skel || !Fighter->bABPDrivenLocomotion)
	{
		return;
	}
	const float CurrentMaxSpeed = Fighter->GetCharacterMovement()
		? Fighter->GetCharacterMovement()->MaxWalkSpeed
		: Fighter->WalkSpeed;
	const float BaseSpeed = FMath::Max(Fighter->WalkSpeed, 1.f);
	const float Rate = FMath::Clamp(CurrentMaxSpeed / BaseSpeed, 0.1f, 5.f);
	Skel->SetPlayRate(Rate);
}

void AEnemyFighterAIController::PlayWalkAnim()
{
	if (!Fighter)
	{
		return;
	}
	// ABP 驱动模式：闲逛播 Walk，追逐播 Run（由 ApplyLocomotionMaxSpeed 切换）。
	if (Fighter->bABPDrivenLocomotion)
	{
		if (bPlayingWalk && !bPlayingRun)
		{
			return; // 已在播 Walk，不重复播放
		}
		if (UAnimMontage* Walk = Fighter->WalkMontage.LoadSynchronous())
		{
			Fighter->PlayMeshAnimation(Walk, true);
			bPlayingWalk = true;
			bPlayingRun = false;
			PlayingIdleMontage.Reset();
		}
		return;
	}
	if (bPlayingWalk)
	{
		return;
	}
	if (UAnimMontage* Walk = Fighter->WalkMontage.LoadSynchronous())
	{
		Fighter->PlayMeshAnimation(Walk, true);
		bPlayingWalk = true;
		bPlayingRun = false;
		PlayingIdleMontage.Reset();
	}
}

void AEnemyFighterAIController::PlayRunAnim()
{
	if (!Fighter)
	{
		return;
	}
	// 非 ABP 驱动模式不切 Run（走旧 PlayRate 加速路径）。
	if (!Fighter->bABPDrivenLocomotion)
	{
		if (bPlayingWalk)
		{
			UpdateWalkPlayRate();
		}
		else
		{
			PlayWalkAnim();
			UpdateWalkPlayRate();
		}
		return;
	}
	// ABP 驱动模式：有 Run 蒙太奇就播 Run，没有就回退 Walk+PlayRate。
	if (UAnimMontage* Run = Fighter->RunMontage.LoadSynchronous())
	{
		if (bPlayingWalk && bPlayingRun)
		{
			return; // 已在播 Run，不重复播放
		}
		Fighter->PlayMeshAnimation(Run, true);
		bPlayingWalk = true;
		bPlayingRun = true;
		PlayingIdleMontage.Reset();
	}
	else
	{
		PlayWalkAnim();
		UpdateWalkPlayRate();
	}
}

void AEnemyFighterAIController::StopLocomotionAnim()
{
	if (!Fighter)
	{
		return;
	}
	if (bPlayingWalk || bPlayingRun)
	{
		if (Fighter->UsesSingleNodeAnims())
		{
			Fighter->StopMeshAnimation();
		}
		bPlayingWalk = false;
		bPlayingRun = false;
	}
}

void AEnemyFighterAIController::TryPlayRandomIdleMontage()
{
	if (!Fighter || Fighter->IdleMontages.Num() == 0)
	{
		return;
	}
	bPlayingWalk = false;
	bPlayingRun = false;
	if (Fighter->UsesSingleNodeAnims())
	{
		const int32 Index = FMath::RandRange(0, Fighter->IdleMontages.Num() - 1);
		if (UAnimMontage* Montage = Fighter->IdleMontages[Index].LoadSynchronous())
		{
			Fighter->PlayMeshAnimation(Montage, false);
			PlayingIdleMontage = Montage;
		}
		return;
	}

	USkeletalMeshComponent* Mesh = Fighter->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	if (UAnimMontage* Playing = PlayingIdleMontage.Get())
	{
		if (Anim->Montage_IsPlaying(Playing))
		{
			return;
		}
	}

	const int32 Index = FMath::RandRange(0, Fighter->IdleMontages.Num() - 1);
	if (UAnimMontage* Montage = Fighter->IdleMontages[Index].LoadSynchronous())
	{
		Anim->Montage_Play(Montage);
		PlayingIdleMontage = Montage;
	}
}

void AEnemyFighterAIController::StopIdleMontage()
{
	if (!Fighter)
	{
		PlayingIdleMontage.Reset();
		bPlayingWalk = false;
		bPlayingRun = false;
		return;
	}
	if (Fighter->UsesSingleNodeAnims())
	{
		Fighter->StopMeshAnimation();
		PlayingIdleMontage.Reset();
		bPlayingWalk = false;
		bPlayingRun = false;
		return;
	}
	USkeletalMeshComponent* Mesh = Fighter->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (Anim)
	{
		if (UAnimMontage* Playing = PlayingIdleMontage.Get())
		{
			Anim->Montage_Stop(0.15f, Playing);
		}
	}
	PlayingIdleMontage.Reset();
	bPlayingWalk = false;
	bPlayingRun = false;
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
	StopLocomotionAnim();
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
	bPlayingWalk = false;
	bPlayingRun = false;
	if (!Fighter->RequestAttackSlot(Move.Skill.GetTotalDuration() + Move.TelegraphTime + 0.5f))
	{
		State = EEnemyFighterState::Combat;
		ActiveMoveIndex = INDEX_NONE;
		RequestMoveToPreferred(FVector::Dist(Fighter->GetActorLocation(), FindCombatFocus()->GetActorLocation()) + 1.f);
		return;
	}
	if (!Combat->TryExecute(Move.Skill))
	{
		Fighter->ReleaseAttackSlot();
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
		Fighter->ReleaseAttackSlot();
		State = EEnemyFighterState::Recover;
		StateTime = 0.f;
	}
}

void AEnemyFighterAIController::TickRecover(float DeltaSeconds)
{
	StateTime += DeltaSeconds;
	// 恢复期间播 idle，避免攻击结束后画面凝滞。
	if (!PlayingIdleMontage.IsValid())
	{
		TryPlayRandomIdleMontage();
	}
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
	APawn* Player = FindCombatFocus();
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
	APawn* Player = FindCombatFocus();
	if (!Player || !Fighter)
	{
		return;
	}

	PathRefreshRemaining -= GetWorld()->GetDeltaSeconds();
	const float Preferred = Fighter->PreferredDistance;
	const bool bTooFar = Dist > Preferred * 1.1f;
	const bool bTooClose = Dist < Preferred * 0.55f;

	if (!bTooFar && !bTooClose)
	{
		ResetChaseFallback();
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			StopMovement();
		}
		// 在合适距离停下：停 locomotion 动画，播 idle。
		StopLocomotionAnim();
		if (!PlayingIdleMontage.IsValid())
		{
			TryPlayRandomIdleMontage();
		}
		return;
	}

	if (!bTooFar)
	{
		ResetChaseFallback();
	}
	else if (!bDirectChaseFallback)
	{
		const FVector CurrentLocation = Fighter->GetActorLocation();
		if (FVector::DistSquared2D(CurrentLocation, ChaseLastPos) < FMath::Square(1.f))
		{
			ChaseStalledSeconds += GetWorld()->GetDeltaSeconds();
		}
		else
		{
			ChaseStalledSeconds = 0.f;
		}
		ChaseLastPos = CurrentLocation;
		bDirectChaseFallback = ChaseStalledSeconds >= 0.5f;
	}

	if (!bDirectChaseFallback
		&& PathRefreshRemaining > 0.f
		&& GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}
	PathRefreshRemaining = PathRefreshInterval;

	if (bTooFar)
	{
		if (!bDirectChaseFallback)
		{
			const EPathFollowingRequestResult::Type Result = MoveToActor(Player, Preferred * 0.85f);
			bDirectChaseFallback = Result != EPathFollowingRequestResult::RequestSuccessful;
		}
		if (bDirectChaseFallback)
		{
			StopMovement();
			FVector ToPlayer = Player->GetActorLocation() - Fighter->GetActorLocation();
			ToPlayer.Z = 0.f;
			Fighter->AddMovementInput(ToPlayer.GetSafeNormal(), 1.f);
		}
		// 追逐时播 Run（ABP 驱动）或 Walk+PlayRate（旧模式）。
		if (Fighter->bABPDrivenLocomotion)
		{
			PlayRunAnim();
		}
		else
		{
			PlayWalkAnim();
		}
	}
	else
	{
		const FVector Away = (Fighter->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		const FVector Dest = Player->GetActorLocation() + Away * Preferred;
		const EPathFollowingRequestResult::Type Result = MoveToLocation(Dest);
		if (Result != EPathFollowingRequestResult::RequestSuccessful)
		{
			Fighter->AddMovementInput(Away, 1.f);
		}
		// 后退也播 Walk（不算追逐跑）。
		PlayWalkAnim();
	}
}

void AEnemyFighterAIController::ResetChaseFallback()
{
	ChaseStalledSeconds = 0.f;
	bDirectChaseFallback = false;
	ChaseLastPos = Fighter ? Fighter->GetActorLocation() : FVector::ZeroVector;
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
