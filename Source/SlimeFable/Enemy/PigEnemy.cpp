// Copyright Epic Games, Inc. All Rights Reserved.

#include "PigEnemy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyCombatTypes.h"
#include "EnemyEncounterSubsystem.h"

APigEnemy::APigEnemy()
{
	bBiteOnlyKit = true;
	bWanderWhenIdle = true;
	bUseSingleNodeAnims = false;
	bABPDrivenLocomotion = true;
	DetectRange = 700.f;
	LeashRange = 1100.f;
	PreferredDistance = 120.f;
	WalkSpeed = 260.f;
	ChaseSpeed = 600.f;
	WanderRadius = 500.f;
	MaxHP = 90.f;
	CombatRole = EEnemyCombatRole::Chaser;
	DisplayName = FText::FromString(TEXT("猪"));
	bDevourable = true;
	bAutoFitCapsuleToMesh = true;
	PrimaryAnimClass.Reset();
}

void APigEnemy::EnsureMoveKit()
{
	const bool bAlreadyPig = Moves.Num() > 0 && Moves[0].MoveId == FName(TEXT("TuskSmash"));
	if (!bAlreadyPig)
	{
		EnemyCombat::FillPigTuskMoves(Moves);
	}
	for (FEnemyMoveDef& Move : Moves)
	{
		if (Move.MoveId == FName(TEXT("TuskLunge")))
		{
			Move.Skill.Exec = EEnemySkillExec::Dash;
			Move.Skill.DashDistance = FMath::Max(Move.Skill.DashDistance, 180.f);
			Move.bGapCloser = true;
		}
	}
}

void APigEnemy::EnterStagger(float Duration, AActor* StaggerInstigator)
{
	Super::EnterStagger(Duration, StaggerInstigator);

	if (IsInDeathSequence())
	{
		return;
	}

	if (UAnimMontage* React = HitReactMontage.LoadSynchronous())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
			{
				Anim->Montage_Play(React);
			}
		}
	}
}
