// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "EnemyCharacter.h"
#include "Abilities/EnemySkillAbility.h"
#include "EnemyGameplayEffects.h"
#include "EnemyFighter.h"
#include "PhoebeEnemy.h"
#include "PhoebeAnimSetupLibrary.h"
#include "EnemyProjectile.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeStatusComponent.h"
#include "SlimeEnemyGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

	const bool bLockWasActive = AttackLockRemaining > 0.f;
	AttackLockRemaining = FMath::Max(AttackLockRemaining - DeltaTime, 0.f);
	if (bLockWasActive && AttackLockRemaining <= 0.f)
	{
		UnlockMovementAfterAttack();
	}

	if (!bAirAttacking)
	{
		if (UAnimMontage* Tracked = ActiveActionMontage.Get())
		{
			if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
			{
				if (const UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
				{
					if (!Anim->Montage_IsPlaying(Tracked))
					{
						ActiveActionMontage.Reset();
					}
				}
			}
		}
	}

	// Player morph path: poll combat keys instead of waiting for AI.
	if (bPlayerMorphed)
	{
		PollPlayerCombatKeys(DeltaTime);
	}

	if (bAttacking)
	{
		TickAction(DeltaTime);
	}
}

bool UEnemyCombatComponent::IsMovementLocked() const
{
	if (bAirAttacking)
	{
		return true;
	}
	if (bAttacking && ActiveDef.Exec == EEnemySkillExec::Dash)
	{
		return false;
	}
	if (AttackLockRemaining > 0.f || bLockedMovementForAttack)
	{
		return true;
	}
	if (Cast<APhoebeEnemy>(GetOwner()))
	{
		return false;
	}
	return bAttacking;
}

bool UEnemyCombatComponent::CanStartAction() const
{
	if (bAttacking || AttackLockRemaining > 0.f)
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
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = Enemy->GetEnemyAbilitySystem())
		{
			PendingGasDef = Def;
			FGameplayTagContainer Tags(SlimeEnemyTags::Ability_Skill);
			if (ASC->TryActivateAbilitiesByTag(Tags, true))
			{
				return true;
			}
		}
	}
	return StartAction(Def);
}

bool UEnemyCombatComponent::BeginGasAbility(UEnemySkillAbility* Ability)
{
	if (!Ability || !CanStartAction())
	{
		return false;
	}
	ActiveGasAbility = Ability;
	return StartAction(PendingGasDef);
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
	ActiveActionMontage.Reset();
	RestoreAirAttackMovement();
	ClearActionState(true);
}

void UEnemyCombatComponent::InterruptForMovement()
{
	if (bAirAttacking)
	{
		return;
	}

	const bool bHadAction = bAttacking || ActiveActionMontage.IsValid();
	if (!bHadAction)
	{
		return;
	}

	StopActiveActionMontage(0.2f);
	RestoreAirAttackMovement();
	ClearActionState(true);
}

bool UEnemyCombatComponent::StartAction(const FEnemySkillDef& Def)
{
	bAirAttacking = false;
	ActiveActionMontage.Reset();
	ActiveDef = Def;
	ActiveForward = GetAimForward();
	ActionElapsed = 0.f;
	bAttacking = true;
	bHitFired = false;
	AlreadyHit.Reset();

	SpawnVfx(Def.CastNiagara, GetOwner()->GetActorLocation());

	if (Def.Exec != EEnemySkillExec::Dash)
	{
		LockMovementForAttack();
	}

	ApplyPhoebeMoveLock(Def);

	if (UAnimMontage* Montage = Def.AttackMontage.LoadSynchronous())
	{
		UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToMontage(Montage);
		ActiveActionMontage = Montage;
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
		{
			if (Enemy->UsesSingleNodeAnims())
			{
				Enemy->PlayMeshAnimation(Montage, false);
				bActionAnimationStarted = true;
			}
			else if (UAnimInstance* Anim = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr)
			{
				const float Played = Anim->Montage_Play(Montage);
				if (Played <= 0.f)
				{
					Enemy->PlayMeshAnimation(Montage, false);
				}
				bActionAnimationStarted = true;
			}
		}
		else if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
			{
				bActionAnimationStarted = Anim->Montage_Play(Montage) > 0.f;
			}
		}
	}
	else
	{
		bActionAnimationStarted = true;
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

bool UEnemyCombatComponent::PlayTrackedMontage(UAnimMontage* Montage)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh()
		? Character->GetMesh()->GetAnimInstance()
		: nullptr;
	if (!Anim || !Montage)
	{
		return false;
	}
	UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToMontage(Montage);
	const bool bPlayed = Anim->Montage_Play(Montage) > 0.f;
	if (bPlayed)
	{
		ActiveActionMontage = Montage;
	}
	return bPlayed;
}

bool UEnemyCombatComponent::TryStartAirAttack(
	const FEnemySkillDef& Def,
	UAnimMontage* StartMontage,
	UAnimMontage* LoopMontage,
	UAnimMontage* EndMontage,
	float GravityMultiplier,
	float InitialDownSpeed)
{
	if (bAirAttacking)
	{
		return true;
	}
	if (!CanStartAction())
	{
		if (bAttacking)
		{
			InterruptCombat();
		}
		if (!CanStartAction())
		{
			return false;
		}
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Move)
	{
		return false;
	}

	ActiveDef = Def;
	ActiveForward = GetAimForward();
	ActionElapsed = 0.f;
	bAttacking = true;
	bAirAttacking = true;
	bAirAttackLoopStarted = false;
	bHitFired = false;
	bActionAnimationStarted = true;
	AlreadyHit.Reset();
	bPhoebeTimedMoveLock = false;
	AttackLockRemaining = 0.f;
	AirAttackStartMontage = StartMontage;
	AirAttackLoopMontage = LoopMontage;
	AirAttackEndMontage = EndMontage;

	SavedAirAttackGravityScale = FMath::Max(Move->GravityScale, KINDA_SMALL_NUMBER);
	SavedAirAttackAirControl = Move->AirControl;
	Move->GravityScale = SavedAirAttackGravityScale * FMath::Max(GravityMultiplier, 1.f);
	Move->AirControl = 0.f;
	Move->SetMovementMode(MOVE_Falling);
	Move->Velocity.X = 0.f;
	Move->Velocity.Y = 0.f;
	Move->Velocity.Z = FMath::Min(Move->Velocity.Z, -FMath::Max(InitialDownSpeed, 1.f));
	Move->ConsumeInputVector();

	if (StartMontage && PlayTrackedMontage(StartMontage))
	{
		AirAttackStartRemaining = FMath::Max(StartMontage->GetPlayLength(), 0.05f);
	}
	else
	{
		AirAttackStartRemaining = 0.f;
		bAirAttackLoopStarted = PlayTrackedMontage(LoopMontage);
	}
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

	if (bAirAttacking)
	{
		TickAirAttack(DeltaTime);
		return;
	}

	ActionElapsed += DeltaTime;

	const float HitTime = GetHitFireTime();
	if (bActionAnimationStarted && !bHitFired && ActionElapsed >= HitTime)
	{
		bHitFired = true;
		FireHit();
	}

	const float EndTime = HitTime + ActiveDef.Recovery;
	if (ActionElapsed >= EndTime)
	{
		FinishAction();
	}
}

void UEnemyCombatComponent::TickAirAttack(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Move)
	{
		InterruptCombat();
		return;
	}

	ActionElapsed += DeltaTime;
	Move->Velocity.X = 0.f;
	Move->Velocity.Y = 0.f;
	Move->AirControl = 0.f;
	Move->ConsumeInputVector();
	if (Move->Velocity.Z > -80.f)
	{
		Move->Velocity.Z = -80.f;
	}
	if (!bAirAttackLoopStarted)
	{
		AirAttackStartRemaining = FMath::Max(0.f, AirAttackStartRemaining - DeltaTime);
		UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const bool bStartStillPlaying = Anim && AirAttackStartMontage.IsValid()
			&& Anim->Montage_IsPlaying(AirAttackStartMontage.Get());
		if (AirAttackStartRemaining <= 0.f || !bStartStillPlaying)
		{
			bAirAttackLoopStarted = PlayTrackedMontage(AirAttackLoopMontage.Get());
		}
	}
	else
	{
		// A one-sequence Montage is not section-looped by default; restart it
		// while falling so arbitrarily tall drops never freeze on its final pose.
		UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Anim && AirAttackLoopMontage.IsValid()
			&& !Anim->Montage_IsPlaying(AirAttackLoopMontage.Get()))
		{
			PlayTrackedMontage(AirAttackLoopMontage.Get());
		}
	}

	if (Move->IsMovingOnGround())
	{
		NotifyOwnerLanded();
	}
}

void UEnemyCombatComponent::NotifyOwnerLanded()
{
	if (!bAirAttacking)
	{
		return;
	}

	if (!bHitFired)
	{
		bHitFired = true;
		FireHit();
	}

	UAnimMontage* EndMontage = AirAttackEndMontage.Get();
	StopActiveActionMontage(0.04f);
	RestoreAirAttackMovement();
	bAirAttacking = false;
	bAttacking = false;
	bActionAnimationStarted = false;
	AlreadyHit.Reset();
	UnlockMovementAfterAttack();
	BeginPhoebeLandMoveLock();

	if (!PlayTrackedMontage(EndMontage))
	{
		ActiveActionMontage.Reset();
	}
}

float UEnemyCombatComponent::GetHitFireTime() const
{
	return FMath::Max(0.f, ActiveDef.Windup + ActiveDef.HitStart + GlobalHitDelay);
}

void UEnemyCombatComponent::FinishAction()
{
	const bool bWhiffed = AlreadyHit.Num() == 0;
	if (bWhiffed)
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
		{
			if (Enemy->GetCombatRole() == EEnemyCombatRole::Chaser
				&& (ActiveDef.Exec == EEnemySkillExec::Dash || ActiveDef.Exec == EEnemySkillExec::Melee))
			{
				// A missed lunge is a readable punish window for the player.
				Enemy->ApplyTimedState(UGE_EnemyStagger::StaticClass(), 0.65f);
				Enemy->ReleaseAttackSlot();
			}
		}
	}
	bAttacking = false;
	bHitFired = false;
	bActionAnimationStarted = false;
	AlreadyHit.Reset();
	if (bPhoebeTimedMoveLock)
	{
		if (AttackLockRemaining <= 0.f)
		{
			UnlockMovementAfterAttack();
		}
	}
	else
	{
		UnlockMovementAfterAttack();
		const float IntervalMul = GetAuraAttackIntervalMul();
		AttackLockRemaining = FMath::Max(0.f, (IntervalMul - 1.f) * FMath::Max(ActiveDef.Recovery, 0.2f));
	}
	if (UEnemySkillAbility* Ability = ActiveGasAbility.Get())
	{
		ActiveGasAbility.Reset();
		Ability->EndFromCombat();
	}
}

void UEnemyCombatComponent::StopActiveActionMontage(float BlendOutTime)
{
	UAnimMontage* Montage = ActiveActionMontage.Get();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Montage && Character && Character->GetMesh())
	{
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(BlendOutTime, Montage);
		}
	}
	ActiveActionMontage.Reset();
}

void UEnemyCombatComponent::RestoreAirAttackMovement()
{
	if (!bAirAttacking)
	{
		return;
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->GravityScale = SavedAirAttackGravityScale > KINDA_SMALL_NUMBER
				? SavedAirAttackGravityScale
				: 1.f;
			Move->AirControl = SavedAirAttackAirControl;
		}
	}
	bAirAttacking = false;
	bAirAttackLoopStarted = false;
	AirAttackStartRemaining = 0.f;
	SavedAirAttackGravityScale = 1.f;
	SavedAirAttackAirControl = 0.35f;
	AirAttackStartMontage.Reset();
	AirAttackLoopMontage.Reset();
	AirAttackEndMontage.Reset();
}

void UEnemyCombatComponent::ClearActionState(bool bClearAttackLock)
{
	bAttacking = false;
	bHitFired = false;
	bActionAnimationStarted = false;
	AlreadyHit.Reset();
	UnlockMovementAfterAttack();
	if (bClearAttackLock)
	{
		AttackLockRemaining = 0.f;
		bPhoebeTimedMoveLock = false;
	}
	if (UEnemySkillAbility* Ability = ActiveGasAbility.Get())
	{
		ActiveGasAbility.Reset();
		Ability->EndFromCombat();
	}
}

void UEnemyCombatComponent::LockMovementForAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || bLockedMovementForAttack)
	{
		return;
	}
	CachedMaxWalkSpeedBeforeAttack = Move->MaxWalkSpeed;
	Move->StopMovementImmediately();
	Move->Velocity = FVector::ZeroVector;
	Move->MaxWalkSpeed = 0.f;
	bLockedMovementForAttack = true;
}

void UEnemyCombatComponent::UnlockMovementAfterAttack()
{
	if (!bLockedMovementForAttack)
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (Move)
	{
		Move->MaxWalkSpeed = CachedMaxWalkSpeedBeforeAttack > 0.f
			? CachedMaxWalkSpeedBeforeAttack
			: 420.f;
	}
	bLockedMovementForAttack = false;
	CachedMaxWalkSpeedBeforeAttack = 0.f;
}

void UEnemyCombatComponent::ApplyPhoebeMoveLock(const FEnemySkillDef& Def)
{
	APhoebeEnemy* Phoebe = Cast<APhoebeEnemy>(GetOwner());
	if (!Phoebe || Def.Exec == EEnemySkillExec::Dash)
	{
		bPhoebeTimedMoveLock = false;
		return;
	}

	bPhoebeTimedMoveLock = true;
	AttackLockRemaining = FMath::Max(0.f, Phoebe->ResolveMoveLockSeconds(Def));
	if (Def.Exec == EEnemySkillExec::Dash)
	{
		return;
	}
	if (AttackLockRemaining <= 0.f)
	{
		UnlockMovementAfterAttack();
	}
}

void UEnemyCombatComponent::BeginPhoebeLandMoveLock()
{
	APhoebeEnemy* Phoebe = Cast<APhoebeEnemy>(GetOwner());
	if (!Phoebe)
	{
		AttackLockRemaining = 0.f;
		bPhoebeTimedMoveLock = false;
		return;
	}

	bPhoebeTimedMoveLock = true;
	AttackLockRemaining = FMath::Max(0.f, Phoebe->GetAirAttackLandMoveLockSeconds());
	if (AttackLockRemaining > 0.f)
	{
		LockMovementForAttack();
	}
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

	// Hard gate: melee / AoE / Dash must not damage when Dist2D exceeds engage + reach.
	if (ActiveDef.Exec == EEnemySkillExec::Melee
		|| ActiveDef.Exec == EEnemySkillExec::AoE
		|| ActiveDef.Exec == EEnemySkillExec::Dash)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const float Dist2D = FVector::Dist2D(Owner->GetActorLocation(), Player->GetActorLocation());
			const float Reach =
				ActiveDef.Hit.Radius + ActiveDef.Hit.Range + ActiveDef.Hit.OriginForwardOffset;
			float Cap = MaxMeleeHitDistance;
			if (ActiveDef.Exec == EEnemySkillExec::Dash)
			{
				Cap = FMath::Min(250.f, FMath::Max(MaxMeleeHitDistance, Reach));
			}
			else if (ActiveDef.Exec == EEnemySkillExec::AoE)
			{
				Cap = FMath::Min(250.f, FMath::Max(MaxMeleeHitDistance, Reach));
			}
			if (Dist2D > Cap)
			{
				return;
			}
		}
	}

	FSlimeSkillDef HitSkill = EnemyCombat::ToSlimeHitSkill(ActiveDef);
	HitSkill.Damage = ResolveDamage(ActiveDef);

	FVector Origin = Owner->GetActorLocation();
	if (ActiveDef.Exec == EEnemySkillExec::AoE)
	{
		Origin = Owner->GetActorLocation();
		if (bAirAttacking)
		{
			if (const ACharacter* Character = Cast<ACharacter>(Owner))
			{
				if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
				{
					Origin.Z -= Capsule->GetScaledCapsuleHalfHeight();
				}
			}
		}
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
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	const FVector Dir = Forward.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	const float Duration = FMath::Max(Def.HitEnd - Def.HitStart, 0.08f);
	const float DesiredSpeed = Def.DashDistance / Duration;
	const float MaxDashSpeed = FMath::Max(Move->MaxWalkSpeed * 2.5f, 900.f);
	const float Speed = FMath::Min(DesiredSpeed, MaxDashSpeed);

	if (Def.bAirDash)
	{
		Character->LaunchCharacter(Dir * Speed, true, false);
		return;
	}

	if (Move->IsFalling())
	{
		FVector Vel = Move->Velocity;
		Vel.X = Dir.X * Speed;
		Vel.Y = Dir.Y * Speed;
		Move->Velocity = Vel;
		return;
	}

	Move->SetMovementMode(MOVE_Walking);
	FVector Vel = Dir * Speed;
	Vel.Z = Move->Velocity.Z;
	Move->Velocity = Vel;
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
		UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Location, GetAimForward().Rotation(), FVector(1.f), true, true);
		if (Spawned && GetWorld())
		{
			TWeakObjectPtr<UNiagaraComponent> WeakFx(Spawned);
			FTimerHandle LifetimeHandle;
			GetWorld()->GetTimerManager().SetTimer(LifetimeHandle, [WeakFx]()
			{
				if (UNiagaraComponent* Fx = WeakFx.Get())
				{
					Fx->DeactivateImmediate();
					Fx->DestroyComponent();
				}
			}, 4.8f, false);
		}
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
	float Damage = FMath::Max(Skill.Damage + AttackPower * 0.35f, 0.f);
	if (const AActor* Owner = GetOwner())
	{
		if (const USlimeStatusComponent* Status = Owner->FindComponentByClass<USlimeStatusComponent>())
		{
			Damage *= Status->GetOutgoingDamageMul();
		}
	}
	return Damage;
}

float UEnemyCombatComponent::GetAuraAttackIntervalMul() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (const USlimeStatusComponent* Status = Owner->FindComponentByClass<USlimeStatusComponent>())
		{
			return Status->GetAttackIntervalMul();
		}
	}
	return 1.f;
}

void UEnemyCombatComponent::PollPlayerCombatKeys(float DeltaTime)
{
	APlayerController* PC = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const ACharacter* EnemyChar = Cast<ACharacter>(GetOwner()))
		{
			PC = Cast<APlayerController>(EnemyChar->GetController());
		}
		if (!PC)
		{
			PC = World->GetFirstPlayerController();
		}
	}
	if (!PC)
	{
		return;
	}

	const USlimeInputSettings* InputSettings = nullptr;
	if (const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		InputSettings = GI->GetSubsystem<USlimeInputSettings>();
	}

	auto WasPressed = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->WasKeyPressed(PC, Action);
		}
		return PC->WasInputKeyJustPressed(Fallback);
	};
	auto IsDown = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->IsKeyDown(PC, Action);
		}
		return PC->IsInputKeyDown(Fallback);
	};

	// Phoebe air/climb plunge must not depend on Moves being populated.
	if (APhoebeEnemy* Phoebe = Cast<APhoebeEnemy>(GetOwner()))
	{
		const bool bAttackEdge = WasPressed(ESlimeInputAction::Attack, EKeys::LeftMouseButton);
		const bool bAttackHeld = IsDown(ESlimeInputAction::Attack, EKeys::LeftMouseButton);
		if (bAttackEdge || bAttackHeld)
		{
			if (Phoebe->TryStartAirAttack())
			{
				return;
			}
		}
	}

	const AEnemyFighter* Fighter = Cast<AEnemyFighter>(GetOwner());
	if (!Fighter)
	{
		return;
	}
	const TArray<FEnemyMoveDef>& Moves = Fighter->GetMoves();
	if (Moves.Num() == 0)
	{
		return;
	}

	if (WasPressed(ESlimeInputAction::Attack, EKeys::LeftMouseButton))
	{
		TryExecute(Moves[0].Skill);
		return;
	}

	// Skill keys map to subsequent moves.
	if (Moves.Num() > 1 && WasPressed(ESlimeInputAction::Skill1, EKeys::Q))
	{
		TryExecute(Moves[1].Skill);
		return;
	}
	if (Moves.Num() > 2 && WasPressed(ESlimeInputAction::Skill2, EKeys::E))
	{
		TryExecute(Moves[2].Skill);
		return;
	}
	if (Moves.Num() > 3 && WasPressed(ESlimeInputAction::Skill3, EKeys::R))
	{
		TryExecute(Moves[3].Skill);
		return;
	}
}
