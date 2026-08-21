// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "EnemyCharacter.h"
#include "EnemyProjectile.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHitProbe.h"

UEnemyCombatComponent::UEnemyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (Enemy->IsDevourLocked())
		{
			if (bAttacking)
			{
				InterruptCombat();
			}
			return;
		}
	}
	if (bAttacking)
	{
		TickAction(DeltaTime);
	}
}

bool UEnemyCombatComponent::CanStartAction() const
{
	if (bAttacking)
	{
		return false;
	}
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner))
	{
		if (Enemy->IsDevourLocked())
		{
			return false;
		}
	}
	return true;
}

bool UEnemyCombatComponent::TryExecute(const FEnemySkillDef& Def)
{
	if (!CanStartAction())
	{
		return false;
	}
	return StartAction(Def);
}

void UEnemyCombatComponent::InterruptCombat()
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->StopMeshAnimation();
	}
	else if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.1f);
		}
	}
	bAttacking = false;
	bHitFired = false;
	AlreadyHit.Reset();
}

bool UEnemyCombatComponent::StartAction(const FEnemySkillDef& Def)
{
	ActiveDef = Def;
	ActiveForward = GetAimForward();
	ActionElapsed = 0.f;
	bAttacking = true;
	bHitFired = false;
	AlreadyHit.Reset();

	SpawnVfx(Def.CastNiagara, GetOwner()->GetActorLocation());

	if (UAnimMontage* Montage = Def.AttackMontage.LoadSynchronous())
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
		{
			if (Enemy->UsesSingleNodeAnims())
			{
				Enemy->PlayMeshAnimation(Montage, false);
			}
			else if (UAnimInstance* Anim = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr)
			{
				const float Played = Anim->Montage_Play(Montage);
				if (Played <= 0.f)
				{
					Enemy->PlayMeshAnimation(Montage, false);
				}
			}
		}
		else if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
			{
				Anim->Montage_Play(Montage);
			}
		}
	}

	if (Def.Exec == EEnemySkillExec::Dash)
	{
		ExecuteDash(Def, ActiveForward);
	}
	else if (Def.Exec == EEnemySkillExec::Projectile)
	{
		// Projectiles fire at HitStart via FireHit path below.
	}

	USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, GetOwner());
	return true;
}

void UEnemyCombatComponent::TickAction(float DeltaTime)
{
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (Enemy->IsDevourLocked())
		{
			InterruptCombat();
			return;
		}
	}
	ActionElapsed += DeltaTime;

	if (!bHitFired && ActionElapsed >= ActiveDef.HitStart)
	{
		bHitFired = true;
		FireHit();
	}

	const float EndTime = ActiveDef.HitStart + ActiveDef.Recovery;
	if (ActionElapsed >= EndTime)
	{
		FinishAction();
	}
}

void UEnemyCombatComponent::FinishAction()
{
	bAttacking = false;
	bHitFired = false;
	AlreadyHit.Reset();
}

void UEnemyCombatComponent::FireHit()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner))
	{
		if (Enemy->IsDevourLocked())
		{
			return;
		}
	}

	const FVector Forward = ActiveForward.GetSafeNormal();
	if (ActiveDef.Exec == EEnemySkillExec::Projectile)
	{
		ExecuteProjectile(ActiveDef, Forward);
		return;
	}

	FSlimeSkillDef HitSkill = EnemyCombat::ToSlimeHitSkill(ActiveDef);
	HitSkill.Damage = ResolveDamage(ActiveDef);

	FVector Origin = Owner->GetActorLocation();
	if (ActiveDef.Exec == EEnemySkillExec::AoE)
	{
		Origin = Owner->GetActorLocation();
		HitSkill.Hit.OriginForwardOffset = 0.f;
	}
	else
	{
		Origin = USlimeHitProbe::ResolveOrigin(Owner, HitSkill.Hit, Forward);
	}

	USlimeHitProbe::PerformHit(Owner, HitSkill, Origin, Forward, AlreadyHit);
	SpawnVfx(ActiveDef.HitNiagara, Origin);
}

void UEnemyCombatComponent::ExecuteDash(const FEnemySkillDef& Def, const FVector& Forward)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Def.DashDistance <= 0.f)
	{
		return;
	}
	const FVector Dir = Forward.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}
	Character->LaunchCharacter(Dir * (Def.DashDistance / FMath::Max(Def.HitEnd - Def.HitStart, 0.08f)), true, true);
}

void UEnemyCombatComponent::ExecuteProjectile(const FEnemySkillDef& Def, const FVector& Forward)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FVector Origin = GetMuzzleLocation();
	const FVector Dir = Forward.GetSafeNormal();
	const FVector Velocity = Dir * FMath::Max(Def.ProjectileSpeed, 100.f);

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(Origin, Dir.Rotation(), Params))
	{
		FEnemySkillDef Shot = Def;
		Shot.Damage = ResolveDamage(Def);
		Projectile->InitProjectile(Owner, Shot, Velocity);
	}
	SpawnVfx(Def.CastNiagara, Origin);
}

void UEnemyCombatComponent::SpawnVfx(const TSoftObjectPtr<UNiagaraSystem>& SoftSystem, const FVector& Location) const
{
	if (SoftSystem.IsNull())
	{
		return;
	}
	if (UNiagaraSystem* System = SoftSystem.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Location, GetAimForward().Rotation(), FVector(1.f), true, true);
	}
}

FVector UEnemyCombatComponent::GetAimForward() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ForwardVector;
	}
	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.f;
	if (Forward.IsNearlyZero())
	{
		Forward = Owner->GetActorForwardVector();
	}
	return Forward.GetSafeNormal();
}

FVector UEnemyCombatComponent::GetMuzzleLocation() const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && MuzzleSocket != NAME_None)
	{
		return Character->GetMesh()->GetSocketLocation(MuzzleSocket);
	}
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() + GetAimForward() * 60.f + FVector(0.f, 0.f, 40.f)
				 : FVector::ZeroVector;
}

float UEnemyCombatComponent::ResolveDamage(const FEnemySkillDef& Skill) const
{
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (Enemy->bHarmless)
		{
			return 0.f;
		}
	}
	return FMath::Max(Skill.Damage + AttackPower * 0.35f, 0.f);
}
