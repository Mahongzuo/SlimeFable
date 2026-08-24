// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "EnemyCharacter.h"
#include "Abilities/EnemySkillAbility.h"
#include "EnemyGameplayEffects.h"
#include "EnemyFighter.h"
#include "EnemyProjectile.h"
#include "AbilitySystemComponent.h"
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
#include "SlimeEnemyGameplayTags.h"
#include "Engine/GameInstance.h"
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
	bAttacking = false;
	bHitFired = false;
	bActionAnimationStarted = false;
	AlreadyHit.Reset();
	if (UEnemySkillAbility* Ability = ActiveGasAbility.Get())
	{
		ActiveGasAbility.Reset();
		Ability->EndFromCombat();
	}
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

	if (bActionAnimationStarted && !bHitFired && ActionElapsed >= ActiveDef.HitStart)
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
	if (UEnemySkillAbility* Ability = ActiveGasAbility.Get())
	{
		ActiveGasAbility.Reset();
		Ability->EndFromCombat();
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
	return FMath::Max(Skill.Damage + AttackPower * 0.35f, 0.f);
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

	// Resolve the move list from an EnemyFighter. Non-fighter enemies have no Moves array
	// and simply cannot attack while morphed — that is acceptable for now.
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

	// Left mouse = first move (basic attack).
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
