// Copyright Epic Games, Inc. All Rights Reserved.

#include "PigEnemy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyCombatComponent.h"
#include "EnemyCombatTypes.h"
#include "EnemyEncounterSubsystem.h"
#include "Sound/SoundBase.h"

APigEnemy::APigEnemy()
{
	bBiteOnlyKit = true;
	bWanderWhenIdle = true;
	bUseSingleNodeAnims = false;
	bABPDrivenLocomotion = true;
	DetectRange = 700.f;
	LeashRange = 1100.f;
	PreferredDistance = 200.f;
	MeleeEngageDistance = 250.f;
	WalkSpeed = 260.f;
	ChaseSpeed = 600.f;
	WanderRadius = 500.f;
	MaxHP = 90.f;
	CombatRole = EEnemyCombatRole::Chaser;
	DisplayName = FText::FromString(TEXT("猪"));
	bDevourable = true;
	bAutoFitCapsuleToMesh = true;
	PrimaryAnimClass.Reset();

	if (UEnemyCombatComponent* CombatComp = GetEnemyCombat())
	{
		CombatComp->GlobalHitDelay = 0.f;
		CombatComp->MaxMeleeHitDistance = 550.f;
		CombatComp->AttackSwingSound = TSoftObjectPtr<USoundBase>(
			FSoftObjectPath(EnemyCombat::DefaultPigAttackSound));
		CombatComp->AttackImpactSound = TSoftObjectPtr<USoundBase>(
			FSoftObjectPath(EnemyCombat::DefaultPigImpactSound));
	}
}

void APigEnemy::EnsureMoveKit()
{
	TMap<FName, TSoftObjectPtr<UAnimMontage>> BoundMontages;
	for (const FEnemyMoveDef& Move : Moves)
	{
		if (!Move.MoveId.IsNone() && !Move.Skill.AttackMontage.IsNull())
		{
			BoundMontages.Add(Move.MoveId, Move.Skill.AttackMontage);
		}
	}

	EnemyCombat::FillPigTuskMoves(Moves);

	for (FEnemyMoveDef& Move : Moves)
	{
		if (const TSoftObjectPtr<UAnimMontage>* Found = BoundMontages.Find(Move.MoveId))
		{
			Move.Skill.AttackMontage = *Found;
		}
		if (Move.MoveId == FName(TEXT("TuskLunge")))
		{
			Move.Skill.Exec = EEnemySkillExec::Dash;
			Move.Skill.DashDistance = FMath::Max(Move.Skill.DashDistance, 200.f);
			Move.bGapCloser = true;
		}
	}

	if (UEnemyCombatComponent* CombatComp = GetEnemyCombat())
	{
		CombatComp->GlobalHitDelay = 0.f;
		float Reach = CombatComp->MaxMeleeHitDistance;
		for (const FEnemyMoveDef& Move : Moves)
		{
			Reach = FMath::Max(Reach,
				Move.Skill.Hit.Radius + Move.Skill.Hit.Range + Move.Skill.Hit.OriginForwardOffset);
		}
		CombatComp->MaxMeleeHitDistance = FMath::Max(Reach, 550.f);
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
