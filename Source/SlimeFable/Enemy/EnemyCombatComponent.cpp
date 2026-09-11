// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "SlimeFable.h"
#include "EnemyAllyAIController.h"
#include "EnemyCharacter.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeFallingWatermelon.h"
#include "EngineUtils.h"
#include "SlimeLockOnComponent.h"
#include "SlimeHealthComponent.h"
#include "Abilities/EnemySkillAbility.h"
#include "EnemyGameplayEffects.h"
#include "EnemyFighter.h"
#include "GaspSandboxPawn.h"
#include "PhoebeEnemy.h"
#include "PhoebeAnimSetupLibrary.h"
#include "EnemyProjectile.h"
#include "PigEnemy.h"
#include "AbilitySystemComponent.h"
#include "Animation/Skeleton.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MoverComponent.h"
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
#include "Components/SkeletalMeshComponent.h"
#include "Settings/SlimeAudioPlay.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UEnemyCombatComponent::UEnemyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(GetOwner()))
	{
		if (Target->IsDevourLocked())
		{
			if (bAttacking)
			{
				InterruptCombat();
			}
			else if (BeamRemaining > 0.f)
			{
				StopBeamLane();
			}
			if (PendingRainCount > 0)
			{
				TickWatermelonRain(DeltaTime);
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
			if (const UAnimInstance* Anim = ResolveOwnerAnimInstance())
			{
				if (!Anim->Montage_IsPlaying(Tracked))
				{
					ActiveActionMontage.Reset();
				}
			}
		}
	}

	if (DamageBuffRemaining > 0.f)
	{
		DamageBuffRemaining = FMath::Max(DamageBuffRemaining - DeltaTime, 0.f);
		if (DamageBuffRemaining <= 0.f)
		{
			OutgoingDamageMul = 1.f;
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

	if (BeamRemaining > 0.f)
	{
		TickBeamLane(DeltaTime);
	}

	if (PendingRainCount > 0)
	{
		TickWatermelonRain(DeltaTime);
	}
}

void UEnemyCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBeamLane();
	Super::EndPlay(EndPlayReason);
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
	if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(Owner))
	{
		if (Target->IsDevourLocked())
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
	else if (UAnimInstance* Anim = ResolveOwnerAnimInstance(ActiveActionMontage.Get()))
	{
		Anim->StopAllMontages(0.1f);
	}
	RestoreCostumeVisualAnim();
	ActiveActionMontage.Reset();
	RestoreAirAttackMovement();
	ClearActionState(true);
	StopBeamLane();
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

	if (Def.Exec == EEnemySkillExec::BeamLane)
	{
		SpawnBeamLaneVfx(Def, ActiveForward);
	}
	else
	{
		SpawnVfx(Def.CastNiagara, GetOwner()->GetActorLocation());
	}

	if (Def.Exec != EEnemySkillExec::Dash)
	{
		LockMovementForAttack();
	}

	ApplyPhoebeMoveLock(Def);

	if (UAnimMontage* Montage = Def.AttackMontage.LoadSynchronous())
	{
		EnemyCombat::SanitizeGaspCombatMontage(Montage);
		// Phoebe sequences need in-place root lock; never rewrite shared GASP interaction assets.
		if (Cast<AEnemyCharacter>(GetOwner()))
		{
			UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToMontage(Montage);
		}
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
		else
		{
			bActionAnimationStarted = PlayOwnerAttackMontage(Montage);
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
	if (AActor* Owner = GetOwner())
	{
		if (USoundBase* Swing = AttackSwingSound.LoadSynchronous())
		{
			SlimeAudioPlay::PlaySfxAt(this, Swing, Owner->GetActorLocation());
		}
	}
	return true;
}

bool UEnemyCombatComponent::PlayTrackedMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return false;
	}
	if (Cast<AEnemyCharacter>(GetOwner()))
	{
		UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToMontage(Montage);
	}
	const bool bPlayed = PlayOwnerAttackMontage(Montage);
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

	if (ActiveDef.Exec == EEnemySkillExec::Dash && ActiveDef.DashDistance > 0.f && !bHitFired)
	{
		TickGapCloserDash();
	}

	const float HitTime = GetHitFireTime();
	if (bActionAnimationStarted && !bHitFired && ActionElapsed >= HitTime)
	{
		bHitFired = true;
		FireHit();
	}

	float EndTime = HitTime + ActiveDef.Recovery;
	if (CostumeVisualPlayLength > 0.f)
	{
		EndTime = FMath::Max(EndTime, CostumeVisualPlayLength);
	}
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
	RestoreCostumeVisualAnim();
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
	if (Montage)
	{
		if (UAnimInstance* Anim = ResolveOwnerAnimInstance(Montage))
		{
			Anim->Montage_Stop(BlendOutTime, Montage);
		}
	}
	RestoreCostumeVisualAnim();
	ActiveActionMontage.Reset();
}

UAnimInstance* UEnemyCombatComponent::ResolveOwnerAnimInstance() const
{
	return ResolveOwnerAnimInstance(nullptr);
}

UAnimInstance* UEnemyCombatComponent::ResolveOwnerAnimInstance(const UAnimMontage* Montage) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	auto AnimFromMesh = [](USkeletalMeshComponent* Mesh) -> UAnimInstance*
	{
		return Mesh ? Mesh->GetAnimInstance() : nullptr;
	};
	auto SkeletonOf = [](const USkeletalMeshComponent* Mesh) -> const USkeleton*
	{
		const USkeletalMesh* SkelMesh = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		return SkelMesh ? SkelMesh->GetSkeleton() : nullptr;
	};

	if (AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(Owner))
	{
		USkeletalMeshComponent* Visual = Gasp->GetVisualSkeletalMesh();
		USkeletalMeshComponent* Source = Gasp->GetPrimarySkeletalMesh();
		if (Montage)
		{
			if (Visual && Montage->GetSkeleton() && Montage->GetSkeleton() == SkeletonOf(Visual))
			{
				if (UAnimInstance* Anim = AnimFromMesh(Visual))
				{
					return Anim;
				}
			}
			if (Source && Montage->GetSkeleton() && Montage->GetSkeleton() == SkeletonOf(Source))
			{
				if (UAnimInstance* Anim = AnimFromMesh(Source))
				{
					return Anim;
				}
			}
		}
		if (UAnimInstance* Anim = AnimFromMesh(Source))
		{
			return Anim;
		}
		return AnimFromMesh(Visual);
	}
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner))
	{
		return AnimFromMesh(Enemy->GetMesh());
	}
	if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(Owner))
	{
		if (USkeletalMeshComponent* Mesh = Target->GetPrimarySkeletalMesh())
		{
			return AnimFromMesh(Mesh);
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		return AnimFromMesh(Character->GetMesh());
	}
	return nullptr;
}

bool UEnemyCombatComponent::PlayOwnerAttackMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return false;
	}
	CostumeVisualPlayLength = 0.f;

	auto PlayOnInstance = [Montage](USkeletalMeshComponent* Mesh) -> bool
	{
		UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		return Anim && Anim->Montage_Play(Montage) > 0.f;
	};
	auto SequenceOf = [](const UAnimMontage* InMontage) -> UAnimSequenceBase*
	{
		if (!InMontage || InMontage->SlotAnimTracks.Num() == 0)
		{
			return nullptr;
		}
		const TArray<FAnimSegment>& Segs = InMontage->SlotAnimTracks[0].AnimTrack.AnimSegments;
		return Segs.Num() > 0 ? Segs[0].GetAnimReference() : nullptr;
	};

	if (AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(GetOwner()))
	{
		USkeletalMeshComponent* Visual = Gasp->GetVisualSkeletalMesh();
		USkeletalMeshComponent* Source = Gasp->GetPrimarySkeletalMesh();
		const FString MontPath = Montage->GetPathName();
		const bool bCostumeMontage = MontPath.Contains(TEXT("/_Slime/Enemies/Costume/Montages/"));
		if (bCostumeMontage && Visual)
		{
			Gasp->CancelInputRagdoll();
			RestoreCostumeVisualAnim();
			auto DisableRootMotionOnly = [](UAnimSequence* LockedSeq)
			{
				if (LockedSeq)
				{
					LockedSeq->bEnableRootMotion = false;
				}
			};
			if (UAnimSequence* First = Cast<UAnimSequence>(Montage->GetFirstAnimReference()))
			{
				DisableRootMotionOnly(First);
			}
			CostumeVisualMesh = Visual;
			CostumeVisualAnimClass = Visual->GetAnimClass();
			UAnimSequenceBase* Seq = SequenceOf(Montage);
			if (UAnimSequence* LockedSeq = Cast<UAnimSequence>(Seq))
			{
				DisableRootMotionOnly(LockedSeq);
			}
			UAnimationAsset* Playable = Seq ? static_cast<UAnimationAsset*>(Seq) : static_cast<UAnimationAsset*>(Montage);
			Visual->bPauseAnims = false;
			Visual->SetComponentTickEnabled(true);
			Visual->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Visual->SetAnimInstanceClass(nullptr);
			Visual->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Visual->PlayAnimation(Playable, false);
			bCostumeVisualSingleNode = true;
			const float FullLen = Seq ? Seq->GetPlayLength() : Montage->GetPlayLength();
			constexpr float XiguaQMaxSeconds = 1.5f;
			CostumeVisualPlayLength = MontPath.Contains(TEXT("AM_Xigua_Surprise"))
				? FMath::Min(FullLen, XiguaQMaxSeconds)
				: FullLen;
			UE_LOG(LogSlimeFable, Log,
				TEXT("EnemyCombat %s: costume skill %s on %s len=%.2f"),
				*GetNameSafe(Gasp), *GetNameSafe(Playable), *GetNameSafe(Visual), CostumeVisualPlayLength);
			return CostumeVisualPlayLength > 0.f;
		}
		if (Source && PlayOnInstance(Source))
		{
			return true;
		}
	}

	if (UAnimInstance* Anim = ResolveOwnerAnimInstance(Montage))
	{
		return Anim->Montage_Play(Montage) > 0.f;
	}
	return false;
}

void UEnemyCombatComponent::RestoreCostumeVisualAnim()
{
	if (!bCostumeVisualSingleNode)
	{
		CostumeVisualPlayLength = 0.f;
		return;
	}
	if (USkeletalMeshComponent* Visual = CostumeVisualMesh.Get())
	{
		Visual->Stop();
		Visual->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		if (CostumeVisualAnimClass)
		{
			Visual->SetAnimInstanceClass(CostumeVisualAnimClass);
		}
	}
	CostumeVisualMesh.Reset();
	CostumeVisualAnimClass = nullptr;
	bCostumeVisualSingleNode = false;
	CostumeVisualPlayLength = 0.f;
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
	if (bLockedMovementForAttack)
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (Move)
	{
		CachedMaxWalkSpeedBeforeAttack = Move->MaxWalkSpeed;
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->MaxWalkSpeed = 0.f;
	}
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
	if (ActiveDef.Exec == EEnemySkillExec::Summon)
	{
		ExecuteSummon(ActiveDef);
		return;
	}
	if (ActiveDef.Exec == EEnemySkillExec::BeamLane)
	{
		BeginBeamLane(ActiveDef, Forward);
		return;
	}
	if (ActiveDef.Exec == EEnemySkillExec::WatermelonRain)
	{
		ExecuteWatermelonRain(ActiveDef);
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
			const float Cap = FMath::Max(MaxMeleeHitDistance, Reach);
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

	const int32 HitCount = USlimeHitProbe::PerformHit(Owner, HitSkill, Origin, Forward, AlreadyHit);
	SpawnVfx(ActiveDef.HitNiagara, Origin);
	if (HitCount > 0)
	{
		if (USoundBase* Impact = AttackImpactSound.LoadSynchronous())
		{
			SlimeAudioPlay::PlaySfxAt(this, Impact, Origin);
		}
	}
}

void UEnemyCombatComponent::ExecuteDash(const FEnemySkillDef& Def, const FVector& Forward)
{
	if (Def.DashDistance <= 0.f)
	{
		return;
	}
	const FVector Dir = Forward.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	const float Duration = FMath::Max(GetHitFireTime(), 0.08f);
	const float DesiredSpeed = Def.DashDistance / Duration;
	ApplyDashVelocity(Dir, DesiredSpeed, Def.bAirDash);
}

void UEnemyCombatComponent::ApplyDashVelocity(const FVector& Forward, float Speed, bool bAirDash)
{
	const FVector Dir = Forward.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (!Move)
		{
			return;
		}
		const float MaxDashSpeed = FMath::Max(Move->MaxWalkSpeed * 2.5f, 900.f);
		const float Clamped = FMath::Min(Speed, MaxDashSpeed);

		if (bAirDash)
		{
			Character->LaunchCharacter(Dir * Clamped, true, false);
			return;
		}

		if (Move->IsFalling())
		{
			FVector Vel = Move->Velocity;
			Vel.X = Dir.X * Clamped;
			Vel.Y = Dir.Y * Clamped;
			Move->Velocity = Vel;
			return;
		}

		Move->SetMovementMode(MOVE_Walking);
		FVector Vel = Dir * Clamped;
		Vel.Z = Move->Velocity.Z;
		Move->Velocity = Vel;
		return;
	}

	AActor* Owner = GetOwner();
	if (UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr)
	{
		const float Clamped = FMath::Min(Speed, 1400.f);
		TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
		Effect->VelocityToApply = Dir * Clamped + FVector(0.f, 0.f, bAirDash ? 80.f : 0.f);
		Effect->bAdditiveVelocity = false;
		Mover->QueueInstantMovementEffect(Effect);
	}
}

void UEnemyCombatComponent::TickGapCloserDash()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	ActiveForward = GetAimForward();
	if (AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(Owner))
	{
		Gasp->SetAiFaceIntent(ActiveForward);
	}
	else
	{
		Owner->SetActorRotation(FRotator(0.f, ActiveForward.Rotation().Yaw, 0.f));
	}

	float Dist2D = ActiveDef.DashDistance;
	FVector Dir = ActiveForward;
	if (const AActor* Target = ResolveAimTarget())
	{
		FVector To = Target->GetActorLocation() - Owner->GetActorLocation();
		To.Z = 0.f;
		Dist2D = To.Size2D();
		if (!To.IsNearlyZero())
		{
			Dir = To.GetSafeNormal();
			ActiveForward = Dir;
		}
	}

	const float Remain = FMath::Max(GetHitFireTime() - ActionElapsed, 0.08f);
	ApplyDashVelocity(Dir, Dist2D / Remain, ActiveDef.bAirDash);
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

void UEnemyCombatComponent::ExecuteSummon(const FEnemySkillDef& Def)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	TSubclassOf<AActor> Class = Def.SummonClass;
	if (!Class)
	{
		Class = LoadClass<AActor>(nullptr, TEXT("/Game/_Slime/Enemies/Pig/BP_PigEnemy.BP_PigEnemy_C"));
	}
	if (!Class)
	{
		Class = APigEnemy::StaticClass();
	}
	if (!Class)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("EnemyCombat %s: summon class missing"), *GetNameSafe(Owner));
		return;
	}

	const FVector Forward = ActiveForward.GetSafeNormal2D();
	const float Offset = Def.SummonForwardOffset > 0.f ? Def.SummonForwardOffset : 300.f;
	const FVector Location = Owner->GetActorLocation() + Forward * Offset;
	const FRotator Rotation = Forward.IsNearlyZero() ? Owner->GetActorRotation() : Forward.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(Class, Location, Rotation, Params);
	if (!Spawned)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("EnemyCombat %s: failed to spawn %s"),
			*GetNameSafe(Owner), *GetNameSafe(Class.Get()));
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const bool bPlayerSide = (OwnerPawn && OwnerPawn->IsPlayerControlled())
		|| USlimeHitProbe::GetTeam(Owner) == ESlimeTeam::Player;
	if (bPlayerSide)
	{
		ConfigureSummonedAlly(Spawned);
	}
}

void UEnemyCombatComponent::ExecuteWatermelonRain(const FEnemySkillDef& Def)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	PendingRainDef = Def;
	PendingRainCenter = Owner->GetActorLocation();
	PendingRainCount = FMath::Max(Def.RainCount, 1);
	PendingRainDelay = 0.f;
	SpawnOneRainWatermelon();
	PendingRainCount = FMath::Max(PendingRainCount - 1, 0);
}

void UEnemyCombatComponent::TickWatermelonRain(float DeltaTime)
{
	if (PendingRainCount <= 0)
	{
		return;
	}

	PendingRainDelay -= DeltaTime;
	while (PendingRainCount > 0 && PendingRainDelay <= 0.f)
	{
		SpawnOneRainWatermelon();
		--PendingRainCount;
		PendingRainDelay += 0.02f;
	}
}

void UEnemyCombatComponent::SpawnOneRainWatermelon()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Dist = FMath::Sqrt(FMath::FRand()) * FMath::Max(PendingRainDef.RainRadius, 1.f);
	const FVector Location = PendingRainCenter + FVector(
		FMath::Cos(Angle) * Dist,
		FMath::Sin(Angle) * Dist,
		FMath::Max(PendingRainDef.RainDropHeight, 50.f));

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ASlimeFallingWatermelon* Melon = World->SpawnActor<ASlimeFallingWatermelon>(
		ASlimeFallingWatermelon::StaticClass(), Location, FRotator::ZeroRotator, Params))
	{
		Melon->InitFalling(Owner, ResolveDamage(PendingRainDef), PendingRainDef.RainLifeAfterBreak);
	}
}

void UEnemyCombatComponent::ConfigureSummonedAlly(AActor* Spawned)
{
	AActor* Master = GetOwner();
	if (!Spawned || !Master)
	{
		return;
	}

	if (AEnemyCharacter* Character = Cast<AEnemyCharacter>(Spawned))
	{
		Character->InitAsPhantom(0.f, Master);
		if (AController* Old = Character->GetController())
		{
			Old->UnPossess();
			Old->Destroy();
		}
		Character->AIControllerClass = AEnemyAllyAIController::StaticClass();
		Character->SpawnDefaultController();
		if (AEnemyAllyAIController* Ally = Cast<AEnemyAllyAIController>(Character->GetController()))
		{
			Ally->SetMaster(Master);
		}
		return;
	}

	if (AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(Spawned))
	{
		Gasp->InitAsPhantom(0.f, Master);
	}
}

void UEnemyCombatComponent::BeginBeamLane(const FEnemySkillDef& Def, const FVector& Forward)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BeamDef = Def;
	BeamForward = Forward.GetSafeNormal2D();
	if (BeamForward.IsNearlyZero())
	{
		BeamForward = Owner->GetActorForwardVector().GetSafeNormal2D();
	}
	BeamOrigin = Owner->GetActorLocation();
	BeamRemaining = Def.BeamDuration > 0.f ? Def.BeamDuration : 2.f;
	BeamTickAccum = 0.f;
	if (!ActiveBeamFx.IsValid())
	{
		SpawnBeamLaneVfx(Def, BeamForward);
	}
	FireBeamLaneHit();
}

void UEnemyCombatComponent::SpawnBeamLaneVfx(const FEnemySkillDef& Def, const FVector& Forward)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TSoftObjectPtr<UNiagaraSystem> Soft = !Def.HitNiagara.IsNull() ? Def.HitNiagara : Def.CastNiagara;
	UNiagaraSystem* System = Soft.LoadSynchronous();
	if (!System)
	{
		System = LoadObject<UNiagaraSystem>(
			nullptr, TEXT("/Game/RPGEffects/ParticlesNiagara/Priest/Beam/NS_Priest_Beam.NS_Priest_Beam"));
	}
	if (!System)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("EnemyCombat %s: missing wind R beam Niagara"), *GetNameSafe(Owner));
		return;
	}

	FVector Dir = Forward.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = Owner->GetActorForwardVector().GetSafeNormal2D();
	}
	const FRotator Rotation = Dir.Rotation();
	const FVector Location = Owner->GetActorLocation() + Dir * 80.f + FVector(0.f, 0.f, 10.f);

	if (UNiagaraComponent* Existing = ActiveBeamFx.Get())
	{
		Existing->DeactivateImmediate();
		Existing->DestroyComponent();
		ActiveBeamFx.Reset();
	}

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Owner, System, Location, Rotation, FVector(0.9f), true, true);
	if (!FX)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("EnemyCombat %s: failed to spawn wind R beam"), *GetNameSafe(Owner));
		return;
	}

	FX->SetAutoDestroy(false);
	const FLinearColor WindColor = SlimeCombat::GetElementVfxColor(ESlimeElement::Wind);
	FX->SetVariableLinearColor(TEXT("User.Color"), WindColor);
	FX->SetVariableLinearColor(TEXT("User.Tint"), WindColor);
	FX->SetVariableLinearColor(TEXT("User.ElementColor"), WindColor);
	ActiveBeamFx = FX;
}

void UEnemyCombatComponent::TickBeamLane(float DeltaTime)
{
	if (BeamRemaining <= 0.f)
	{
		return;
	}

	BeamRemaining = FMath::Max(0.f, BeamRemaining - DeltaTime);
	const float Interval = BeamDef.BeamTickInterval > 0.f ? BeamDef.BeamTickInterval : 0.5f;
	BeamTickAccum += DeltaTime;
	while (BeamRemaining > 0.f && BeamTickAccum >= Interval)
	{
		BeamTickAccum -= Interval;
		FireBeamLaneHit();
	}

	if (BeamRemaining <= 0.f)
	{
		StopBeamLane();
	}
}

void UEnemyCombatComponent::FireBeamLaneHit()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FSlimeSkillDef HitSkill = EnemyCombat::ToSlimeHitSkill(BeamDef);
	HitSkill.Damage = ResolveDamage(BeamDef);
	HitSkill.Hit.Shape = ESlimeHitShape::Capsule;
	HitSkill.Hit.Range = BeamDef.Hit.Range > 0.f ? BeamDef.Hit.Range : 1000.f;
	HitSkill.Hit.Radius = BeamDef.Hit.Radius > 0.f ? BeamDef.Hit.Radius : 100.f;
	HitSkill.Hit.OriginForwardOffset = 0.f;

	TSet<TWeakObjectPtr<AActor>> WaveHits;
	const int32 HitCount = USlimeHitProbe::PerformHit(Owner, HitSkill, BeamOrigin, BeamForward, WaveHits);
	if (HitCount > 0)
	{
		if (USoundBase* Impact = AttackImpactSound.LoadSynchronous())
		{
			SlimeAudioPlay::PlaySfxAt(this, Impact, BeamOrigin);
		}
	}
}

void UEnemyCombatComponent::StopBeamLane()
{
	BeamRemaining = 0.f;
	BeamTickAccum = 0.f;
	if (UNiagaraComponent* Fx = ActiveBeamFx.Get())
	{
		Fx->DeactivateImmediate();
		Fx->DestroyComponent();
	}
	ActiveBeamFx.Reset();
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

AActor* UEnemyCombatComponent::FindNearestHostile(float MaxRange) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(MaxRange);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Other = *It;
		if (!Other || Other == Owner)
		{
			continue;
		}
		if (!USlimeHitProbe::IsHostile(Owner, Other) || !USlimeHitProbe::IsValidDamageTarget(Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

AActor* UEnemyCombatComponent::ResolveAimTarget() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const bool bPlayerSide = (OwnerPawn && OwnerPawn->IsPlayerControlled())
		|| USlimeHitProbe::GetTeam(Owner) == ESlimeTeam::Player;
	if (bPlayerSide)
	{
		if (const USlimeLockOnComponent* Lock = Owner->FindComponentByClass<USlimeLockOnComponent>())
		{
			if (AActor* Locked = Lock->GetLockedTarget())
			{
				if (USlimeHitProbe::IsHostile(Owner, Locked) && USlimeHitProbe::IsValidDamageTarget(Locked))
				{
					return Locked;
				}
			}
		}
		return FindNearestHostile(2500.f);
	}

	return UGameplayStatics::GetPlayerPawn(this, 0);
}

FVector UEnemyCombatComponent::GetAimForward() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ForwardVector;
	}
	if (const AActor* Target = ResolveAimTarget())
	{
		FVector To = Target->GetActorLocation() - Owner->GetActorLocation();
		To.Z = 0.f;
		if (!To.IsNearlyZero())
		{
			return To.GetSafeNormal();
		}
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
	return Damage * FMath::Max(OutgoingDamageMul, 0.f);
}

void UEnemyCombatComponent::ApplyOutgoingDamageMul(float Mul, float DurationSeconds)
{
	OutgoingDamageMul = FMath::Max(Mul, 0.f);
	DamageBuffRemaining = FMath::Max(DurationSeconds, 0.f);
	if (DamageBuffRemaining <= 0.f)
	{
		OutgoingDamageMul = 1.f;
	}
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

	const TArray<FEnemyMoveDef>* MovesPtr = nullptr;
	if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(GetOwner()))
	{
		MovesPtr = &Target->GetEnemyMoves();
	}
	else if (const AEnemyFighter* Fighter = Cast<AEnemyFighter>(GetOwner()))
	{
		MovesPtr = &Fighter->GetMoves();
	}
	if (!MovesPtr || MovesPtr->Num() == 0)
	{
		return;
	}
	const TArray<FEnemyMoveDef>& Moves = *MovesPtr;

	// Prefer official dual-character Interaction when BP still has it (Gasp Sandbox).
	if (WasPressed(ESlimeInputAction::Attack, EKeys::LeftMouseButton))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			auto TryNamed = [OwnerActor](FName Name) -> bool
			{
				if (UFunction* Fn = OwnerActor->FindFunction(Name))
				{
					if (Fn->NumParms == 1 && Fn->GetReturnProperty() && Fn->GetReturnProperty()->IsA<FBoolProperty>())
					{
						bool bOk = false;
						OwnerActor->ProcessEvent(Fn, &bOk);
						return bOk;
					}
				}
				return false;
			};
			if (TryNamed(TEXT("TryCharacterInteraction")) || TryNamed(TEXT("TryTakedown")))
			{
				return;
			}
		}

		// LMB cycles unslotted combo moves only; Q/E/R own PlayerSkillSlot entries.
		TArray<int32> ComboIndices;
		ComboIndices.Reserve(Moves.Num());
		for (int32 MoveIndex = 0; MoveIndex < Moves.Num(); ++MoveIndex)
		{
			if (Moves[MoveIndex].PlayerSkillSlot == EEnemyPlayerSkillSlot::None)
			{
				ComboIndices.Add(MoveIndex);
			}
		}
		if (ComboIndices.Num() == 0)
		{
			return;
		}
		const int32 Idx = ComboIndices[PlayerAttackCycleIndex % ComboIndices.Num()];
		PlayerAttackCycleIndex = (PlayerAttackCycleIndex + 1) % ComboIndices.Num();
		TryExecute(Moves[Idx].Skill);
		return;
	}

	if (EnemyCombat::HasPlayerSkillSlots(Moves))
	{
		if (WasPressed(ESlimeInputAction::Skill1, EKeys::Q))
		{
			if (const FEnemyMoveDef* Move = EnemyCombat::FindMoveByPlayerSlot(Moves, EEnemyPlayerSkillSlot::SkillQ))
			{
				TryExecute(Move->Skill);
			}
			return;
		}
		if (WasPressed(ESlimeInputAction::Skill2, EKeys::E))
		{
			if (const FEnemyMoveDef* Move = EnemyCombat::FindMoveByPlayerSlot(Moves, EEnemyPlayerSkillSlot::SkillE))
			{
				TryExecute(Move->Skill);
			}
			return;
		}
		if (WasPressed(ESlimeInputAction::Skill3, EKeys::R))
		{
			if (const FEnemyMoveDef* Move = EnemyCombat::FindMoveByPlayerSlot(Moves, EEnemyPlayerSkillSlot::SkillR))
			{
				TryExecute(Move->Skill);
			}
			return;
		}
		if (WasPressed(ESlimeInputAction::ResetBody, EKeys::T))
		{
			if (const FEnemyMoveDef* Move = EnemyCombat::FindMoveByPlayerSlot(Moves, EEnemyPlayerSkillSlot::SkillT))
			{
				TryExecute(Move->Skill);
			}
			return;
		}
		return;
	}

	// GASP uses LMB to cycle all moves so E stays free for SmartObject sit.
	if (Cast<AGaspSandboxPawn>(GetOwner()))
	{
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
