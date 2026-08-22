// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyFighter.h"

#include "Components/SphereComponent.h"
#include "EnemyFighterAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyFighter::AEnemyFighter()
{
	AIControllerClass = AEnemyFighterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	MaxHP = 220.f;
	bAllowDespawn = false;
	DetectRange = 1000.f;
	LeashRange = 1500.f;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
		Move->bOrientRotationToMovement = true;
	}

	DetectRangeVisual = CreateDefaultSubobject<USphereComponent>(TEXT("DetectRangeVisual"));
	DetectRangeVisual->SetupAttachment(RootComponent);
	DetectRangeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectRangeVisual->SetHiddenInGame(true);
	DetectRangeVisual->ShapeColor = FColor(80, 180, 255);
	DetectRangeVisual->bDrawOnlyIfSelected = true;

	LeashRangeVisual = CreateDefaultSubobject<USphereComponent>(TEXT("LeashRangeVisual"));
	LeashRangeVisual->SetupAttachment(RootComponent);
	LeashRangeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeashRangeVisual->SetHiddenInGame(true);
	LeashRangeVisual->ShapeColor = FColor(255, 180, 60);
	LeashRangeVisual->bDrawOnlyIfSelected = true;

	EnemyCombat::FillDefaultFighterMoves(Moves);
}

void AEnemyFighter::EnsureMoveKit()
{
	if (bBiteOnlyKit)
	{
		const bool bAlreadyBite = Moves.Num() > 0 && Moves[0].MoveId == FName(TEXT("BiteSnap"));
		if (!bAlreadyBite)
		{
			EnemyCombat::FillWatchdogBiteMoves(Moves);
		}
		for (FEnemyMoveDef& Move : Moves)
		{
			if (Move.MoveId == FName(TEXT("BiteLunge")))
			{
				// Existing watchdog Blueprints serialize their move array. Normalize the old
				// non-moving lunge at runtime without replacing its bound montage or tuning.
				Move.Skill.Exec = EEnemySkillExec::Dash;
				Move.Skill.DashDistance = FMath::Max(Move.Skill.DashDistance, 180.f);
				Move.bGapCloser = true;
			}
		}
		return;
	}
	if (Moves.Num() == 0)
	{
		EnemyCombat::FillDefaultFighterMoves(Moves);
	}
}

void AEnemyFighter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureMoveKit();
	SyncRangeVisuals();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}
}

void AEnemyFighter::BeginPlay()
{
	if (LeashRange < DetectRange)
	{
		LeashRange = DetectRange;
	}
	Super::BeginPlay();
	EnsureMoveKit();
	SyncRangeVisuals();
	if (bUseSingleNodeAnims && IdleMontages.Num() > 0)
	{
		if (UAnimMontage* Idle = IdleMontages[0].LoadSynchronous())
		{
			PlayMeshAnimation(Idle, true);
		}
	}
}

bool AEnemyFighter::IsInCombat() const
{
	if (const AEnemyFighterAIController* AI = Cast<AEnemyFighterAIController>(GetController()))
	{
		return AI->IsEngaged();
	}
	return Super::IsInCombat();
}

void AEnemyFighter::OnRestoredToSpawn()
{
	if (AEnemyFighterAIController* AI = Cast<AEnemyFighterAIController>(GetController()))
	{
		AI->ReturnToIdle();
	}
}

void AEnemyFighter::ApplyDifficultyToCombat(float DamageMul, float IntervalMul)
{
	if (!bCombatBasesCaptured)
	{
		DifficultyBaseDamages.Reset();
		DifficultyBaseRecoveries.Reset();
		DifficultyBaseCooldowns.Reset();
		for (const FEnemyMoveDef& Move : Moves)
		{
			DifficultyBaseDamages.Add(Move.Skill.Damage);
			DifficultyBaseRecoveries.Add(Move.Skill.Recovery);
			DifficultyBaseCooldowns.Add(Move.Cooldown);
		}
		bCombatBasesCaptured = true;
	}
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		if (DifficultyBaseDamages.IsValidIndex(Index))
		{
			Moves[Index].Skill.Damage = DifficultyBaseDamages[Index] * DamageMul;
			Moves[Index].Skill.Recovery = DifficultyBaseRecoveries[Index] * IntervalMul;
			Moves[Index].Cooldown = DifficultyBaseCooldowns[Index] * IntervalMul;
		}
	}
}

void AEnemyFighter::SyncRangeVisuals()
{
	if (DetectRangeVisual)
	{
		DetectRangeVisual->SetSphereRadius(DetectRange);
	}
	if (LeashRangeVisual)
	{
		LeashRangeVisual->SetSphereRadius(LeashRange);
	}
}
