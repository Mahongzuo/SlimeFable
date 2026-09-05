// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaspRagdollEnemy.h"

#include "Animation/AnimInstance.h"
#include "Combat/SlimeHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "EnemyCombatComponent.h"
#include "EnemyCombatTypes.h"
#include "Engine/CollisionProfile.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MoverDataModelTypes.h"
#include "SlimeFable.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace GaspRagdollPrivate
{
	static const FName RagdollModeName(TEXT("Ragdoll"));
	static const FName WalkingModeName(TEXT("Walking"));
	static const FName InjuryStatePropName(TEXT("Ragdoll_InjuryState"));

	/** Official ragdoll entry points, in the order we try them. */
	static const FName GetUpNames[] = {
		FName(TEXT("Ragdoll_PlayRollingGetups")),
		FName(TEXT("RagdollPlayRollingGetups")),
		FName(TEXT("PlayRollingGetups")),
	};

	static const FName PhysicsProfileNames[] = {
		FName(TEXT("SetPhysicsProfile")),
		FName(TEXT("Set_PhysicsProfile")),
	};

	static const FName SignatureDumpNames[] = {
		FName(TEXT("TriggerRagdoll")),
		FName(TEXT("Ragdoll_OnHit")),
		FName(TEXT("OnRagdollHit")),
		FName(TEXT("Ragdoll_PlayRollingGetups")),
		FName(TEXT("Ragdoll_UpdateImpactDirection")),
		FName(TEXT("Ragdoll_UpdateBehaviors")),
		FName(TEXT("SetPhysicsProfile")),
		FName(TEXT("Set_PhysicsProfile")),
		FName(TEXT("Get_PropertiesForRagdoll")),
		FName(TEXT("On_RagdollMode_Exit")),
	};

	/** Resolve a user-defined enum entry from either its authored name or its display name. */
	int64 ResolveEnumEntry(const UEnum* Enum, FName EntryName)
	{
		if (!Enum)
		{
			return INDEX_NONE;
		}
		const FString Wanted = EntryName.ToString();
		const int32 Num = Enum->NumEnums();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FString Authored = Enum->GetNameStringByIndex(Index);
			const FString Display = Enum->GetDisplayNameTextByIndex(Index).ToString();
			if (Authored.Equals(Wanted, ESearchCase::IgnoreCase)
				|| Display.Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return Enum->GetValueByIndex(Index);
			}
		}
		return INDEX_NONE;
	}
}

AGaspRagdollEnemy::AGaspRagdollEnemy()
{
	// The official ragdoll kit is the whole point of this class.
	bEnableRagdollKit = true;
}

void AGaspRagdollEnemy::BeginPlay()
{
	Super::BeginPlay();
	if (bLogRagdollSignatures)
	{
		LogOfficialRagdollSignatures();
	}
}

void AGaspRagdollEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickRagdollDeath(DeltaSeconds);
}

bool AGaspRagdollEnemy::IsInOfficialRagdollMode() const
{
	return CachedMover && CachedMover->GetMovementModeName() == GaspRagdollPrivate::RagdollModeName;
}

bool AGaspRagdollEnemy::IsSourceMeshSimulating() const
{
	return CachedSkeletalMesh
		&& (CachedSkeletalMesh->IsSimulatingPhysics() || CachedSkeletalMesh->IsAnySimulatingPhysics());
}

bool AGaspRagdollEnemy::IsAnyDeathMeshSimulating() const
{
	if (IsSourceMeshSimulating())
	{
		return true;
	}
	const USkeletalMeshComponent* Echo = GetDevourPreviewMesh();
	return Echo && (Echo->IsSimulatingPhysics() || Echo->IsAnySimulatingPhysics());
}

void AGaspRagdollEnemy::RequestOfficialRagdollMode()
{
	EnsureMoverModes();
	bPendingRagdoll = true;
	if (CachedMover && CachedMover->FindMovementModeByName(GaspRagdollPrivate::RagdollModeName))
	{
		CachedMover->QueueNextMode(GaspRagdollPrivate::RagdollModeName, true);
	}
}

void AGaspRagdollEnemy::EnsureEchoFollowsSource()
{
	USkeletalMeshComponent* Source = CachedSkeletalMesh;
	if (Source)
	{
		Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Source->bEnableUpdateRateOptimizations = false;
	}
	USkeletalMeshComponent* Echo = FindChildActorVisualMesh();
	if (!Source || !Echo || Echo == Source)
	{
		return;
	}
	if (Echo->LeaderPoseComponent.Get() != Source)
	{
		Echo->SetLeaderPoseComponent(Source);
		UE_LOG(LogSlimeFable, Log, TEXT("GaspRagdollEnemy %s: Echo %s now follows %s"),
			*GetName(), *Echo->GetName(), *Source->GetName());
	}
}

void AGaspRagdollEnemy::LogRagdollRuntimeState(const TCHAR* Reason) const
{
	const FName Mode = CachedMover ? CachedMover->GetMovementModeName() : NAME_None;
	const USkeletalMeshComponent* Echo = GetDevourPreviewMesh();
	UE_LOG(LogSlimeFable, Log,
		TEXT("GaspRagdollEnemy %s: %s mode=%s srcSim=%d echoSim=%d echoTick=%d echoHidden=%d echoProfile=%s echoPhysAsset=%d"),
		*GetName(),
		Reason,
		*Mode.ToString(),
		IsSourceMeshSimulating() ? 1 : 0,
		(Echo && (Echo->IsSimulatingPhysics() || Echo->IsAnySimulatingPhysics())) ? 1 : 0,
		(Echo && Echo->IsComponentTickEnabled()) ? 1 : 0,
		(Echo && Echo->bHiddenInGame) ? 1 : 0,
		Echo ? *Echo->GetCollisionProfileName().ToString() : TEXT("none"),
		(Echo && Echo->GetPhysicsAsset()) ? 1 : 0);
}

void AGaspRagdollEnemy::TriggerSandboxRagdoll()
{
	TriggerOfficialRagdoll(KnockdownInjuryState);
}

void AGaspRagdollEnemy::TriggerOfficialRagdoll(FName InjuryEntry)
{
	if (!bEnableRagdollKit)
	{
		return;
	}
	SetInjuryState(InjuryEntry);
	// Official TriggerRagdoll is a toggle: calling it while already ragdolling starts a get-up.
	const bool bAlreadyRagdoll = IsInOfficialRagdollMode();
	RequestOfficialRagdollMode();
	if (bAlreadyRagdoll)
	{
		return;
	}
	DispatchBoolEventOnComponents(TEXT("TriggerRagdoll"));
	CallOfficialTriggerRagdoll(InjuryEntry);
}

bool AGaspRagdollEnemy::CallOfficialTriggerRagdoll(FName InjuryEntry)
{
	UFunction* Fn = FindFunction(TEXT("TriggerRagdoll"));
	if (!Fn)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspRagdollEnemy %s: no TriggerRagdoll event"), *GetName());
		return false;
	}

	uint8* Buffer = Fn->ParmsSize > 0
		? static_cast<uint8*>(FMemory_Alloca(Fn->ParmsSize))
		: nullptr;
	if (Buffer)
	{
		FMemory::Memzero(Buffer, Fn->ParmsSize);
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
			{
				BoolProp->SetPropertyValue_InContainer(Buffer, true);
			}
			else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				const int64 Value = GaspRagdollPrivate::ResolveEnumEntry(ByteProp->Enum, InjuryEntry);
				if (Value != INDEX_NONE)
				{
					ByteProp->SetPropertyValue_InContainer(Buffer, static_cast<uint8>(Value));
				}
			}
			else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = GaspRagdollPrivate::ResolveEnumEntry(EnumProp->GetEnum(), InjuryEntry);
				if (Value != INDEX_NONE)
				{
					EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
						Prop->ContainerPtrToValuePtr<void>(Buffer), Value);
				}
			}
		}
	}

	ProcessEvent(Fn, Buffer);

	if (Buffer)
	{
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer);
		}
	}
	return true;
}

bool AGaspRagdollEnemy::ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs)
{
	if (!WantsHeldRagdollMode())
	{
		return false;
	}
	Inputs.SuggestedMovementMode = GaspRagdollPrivate::RagdollModeName;
	bPendingRagdoll = false;
	return true;
}

void AGaspRagdollEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (bDeathSequence)
	{
		return;
	}
	if (Damage > 0.f)
	{
		LastDamageLocation = DamageLocation;
		LastDamageImpulse = DamageImpulse;
		PlayHitFlash();
		EnemyCombat::PlayGaspSfxAt(this, HitTakenSound, EnemyCombat::DefaultGaspHitTakenSound, GetActorLocation());
		if (bEnableRagdollKit)
		{
			ApplyOfficialHitFeedback(DamageCauser, DamageLocation, DamageImpulse);
		}
		if (bPlayHitReactMontage)
		{
			PlayHitReact(DamageLocation);
		}
	}
	if (Health)
	{
		Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
	if (Damage > 0.f && Health && Health->IsAlive() && !bCombatKnockdown)
	{
		AccrueCombatStun(Damage);
	}
}

void AGaspRagdollEnemy::ApplyOfficialHitFeedback(AActor* DamageCauser, const FVector& HitLocation, const FVector& Impulse)
{
	FVector ResolvedImpulse = Impulse;
	if (ResolvedImpulse.IsNearlyZero())
	{
		const FVector Hit = HitLocation.IsNearlyZero() ? GetActorLocation() : HitLocation;
		FVector Away = GetActorLocation() - Hit;
		Away.Z = FMath::Max(Away.Z, 20.f);
		ResolvedImpulse = Away.GetSafeNormal() * FMath::Max(HitDefaultImpulse, 1.f);
	}

	if (CallOfficialOnRagdollHit(DamageCauser, HitLocation, ResolvedImpulse))
	{
		UE_LOG(LogSlimeFable, Log, TEXT("GaspRagdollEnemy %s: OnRagdollHit impulse=%.0f"),
			*GetName(), ResolvedImpulse.Size());
	}

	EnemyCombat::CallGaspRagdollFunction(this, FName(TEXT("Ragdoll_UpdateImpactDirection")), nullptr);

	if (!HitPhysicsProfile.IsNone())
	{
		for (const FName& Name : GaspRagdollPrivate::PhysicsProfileNames)
		{
			if (CallBpFunctionWithName(Name, HitPhysicsProfile))
			{
				break;
			}
		}
	}

	ApplyHitKnockbackIfNeeded(ResolvedImpulse);
}

bool AGaspRagdollEnemy::CallOfficialOnRagdollHit(AActor* DamageCauser, const FVector& HitLocation, const FVector& Impulse)
{
	UFunction* Fn = FindFunction(TEXT("OnRagdollHit"));
	if (!Fn)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspRagdollEnemy %s: no OnRagdollHit event"), *GetName());
		return false;
	}

	USkeletalMeshComponent* HitMesh = CachedSkeletalMesh.Get();
	if (!HitMesh)
	{
		HitMesh = GetDevourPreviewMesh();
	}
	AActor* Other = DamageCauser;
	if (!Other)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			Other = PC->GetPawn();
		}
	}
	UPrimitiveComponent* OtherComp = Other ? Other->FindComponentByClass<UPrimitiveComponent>() : nullptr;

	const FVector Location = HitLocation.IsNearlyZero() ? GetActorLocation() : HitLocation;
	const FVector HitNormal = (-Impulse).GetSafeNormal();
	FHitResult Hit;
	Hit.ImpactPoint = Location;
	Hit.Location = Location;
	Hit.ImpactNormal = HitNormal.IsNearlyZero() ? FVector::UpVector : HitNormal;
	Hit.Normal = Hit.ImpactNormal;
	Hit.TraceStart = Location - Impulse.GetSafeNormal() * 32.f;
	Hit.TraceEnd = Location;
	Hit.bBlockingHit = true;
	Hit.Component = HitMesh;
	if (HitMesh)
	{
		Hit.BoneName = HitMesh->FindClosestBone(Location);
	}

	uint8* Buffer = Fn->ParmsSize > 0
		? static_cast<uint8*>(FMemory_Alloca(Fn->ParmsSize))
		: nullptr;
	if (Buffer)
	{
		FMemory::Memzero(Buffer, Fn->ParmsSize);
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}
		int32 ObjectSlot = 0;
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
			{
				if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UPrimitiveComponent::StaticClass()))
				{
					ObjProp->SetObjectPropertyValue_InContainer(Buffer, ObjectSlot == 0
						? static_cast<UObject*>(HitMesh)
						: static_cast<UObject*>(OtherComp));
					++ObjectSlot;
				}
				else if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(AActor::StaticClass()))
				{
					ObjProp->SetObjectPropertyValue_InContainer(Buffer, Other);
				}
			}
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct == TBaseStructure<FVector>::Get())
				{
					*StructProp->ContainerPtrToValuePtr<FVector>(Buffer) = Impulse;
				}
				else if (StructProp->Struct && (StructProp->Struct == TBaseStructure<FHitResult>::Get()
					|| StructProp->Struct->GetName() == TEXT("HitResult")))
				{
					*StructProp->ContainerPtrToValuePtr<FHitResult>(Buffer) = Hit;
				}
			}
		}
	}

	ProcessEvent(Fn, Buffer);

	if (Buffer)
	{
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer);
		}
	}
	return true;
}

void AGaspRagdollEnemy::ApplyHitKnockbackIfNeeded(const FVector& Impulse)
{
	if (!CachedMover || HitKnockbackScale <= 0.f)
	{
		return;
	}
	FVector Horiz = Impulse;
	Horiz.Z = 0.f;
	if (Horiz.IsNearlyZero())
	{
		return;
	}
	TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
	Effect->VelocityToApply = Horiz * HitKnockbackScale + FVector(0.f, 0.f, FMath::Clamp(Impulse.Z, 0.f, 80.f));
	Effect->bAdditiveVelocity = false;
	CachedMover->QueueInstantMovementEffect(Effect);
}

void AGaspRagdollEnemy::BeginCombatKnockdown()
{
	if (bCombatKnockdown || bDeathSequence || bDevouredDeath || !bEnableRagdollKit)
	{
		return;
	}
	bCombatKnockdown = true;
	bCombatGetUpRequested = false;
	CombatKnockdownStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	AiMoveIntent = FVector::ZeroVector;
	AiFaceIntent = FVector::ZeroVector;
	bWantChaseGait = false;
	if (Combat)
	{
		Combat->InterruptCombat();
	}

	EnsureEchoFollowsSource();
	TriggerOfficialRagdoll(KnockdownInjuryState);
	LogRagdollRuntimeState(TEXT("knockdown"));

	if (!bOfficialGetUpAfterKnockdown)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CombatGetUpTimer,
			this,
			&AGaspRagdollEnemy::PlayCombatGetUp,
			FMath::Max(GetUpDelaySeconds, 0.1f),
			false);
	}
	else
	{
		PlayCombatGetUp();
	}
}

bool AGaspRagdollEnemy::CallOfficialGetUp()
{
	for (const FName& Name : GaspRagdollPrivate::GetUpNames)
	{
		if (EnemyCombat::CallGaspRagdollFunction(this, Name, nullptr))
		{
			return true;
		}
	}
	return false;
}

void AGaspRagdollEnemy::PlayCombatGetUp()
{
	if (bDeathSequence || !bCombatKnockdown)
	{
		return;
	}
	bCombatGetUpRequested = true;
	CombatGetUpRequestedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	SetInjuryState(TEXT("None"), /*bAllowNoneEntry*/ true);
	if (CallOfficialGetUp())
	{
		return;
	}

	UE_LOG(LogSlimeFable, Warning,
		TEXT("GaspRagdollEnemy %s: Ragdoll_PlayRollingGetups missing - queueing Walking"), *GetName());
	if (CachedMover && CachedMover->FindMovementModeByName(GaspRagdollPrivate::WalkingModeName))
	{
		CachedMover->QueueNextMode(GaspRagdollPrivate::WalkingModeName, true);
	}
	bForceWalkingAfterRagdollRestore = true;
	EndCombatKnockdown();
}

void AGaspRagdollEnemy::BeginKnockdownDeath()
{
	BeginRagdollDeath();
}

void AGaspRagdollEnemy::BeginRagdollDeath()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
	}
	bCombatKnockdown = false;
	bCombatGetUpRequested = false;
	bDeathAwaitingRagdoll = false;
	StunHits.Reset();

	bDeathRagdollArmed = true;
	bRagdollDeathActive = true;
	bDeathDissolveScheduled = false;
	bRagdollSpeedPrimed = false;
	bDeathRagdollRetried = false;
	DeathSettleAccum = 0.f;
	DeathRagdollStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	EnsureEchoFollowsSource();
	TriggerOfficialRagdoll(DeathInjuryState);
	StopDeathController();
	LogRagdollRuntimeState(TEXT("death ragdoll armed"));
}

void AGaspRagdollEnemy::ApplyManualDeathPhysics()
{
	// Walking Finalize turns physics off every frame; freeze Mover first.
	SuspendMoverSim();

	auto RestoreDeathMeshTick = [](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		Mesh->SetComponentTickEnabled(true);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->bEnableUpdateRateOptimizations = false;
		Mesh->bPauseAnims = true;
	};
	RestoreDeathMeshTick(CachedSkeletalMesh);
	RestoreDeathMeshTick(GetDevourPreviewMesh());

	USkeletalMeshComponent* Echo = GetDevourPreviewMesh();
	if (Echo && CachedSkeletalMesh && Echo != CachedSkeletalMesh && Echo->LeaderPoseComponent.Get())
	{
		Echo->SetLeaderPoseComponent(nullptr);
	}

	if (Echo)
	{
		if (UAnimInstance* Anim = Echo->GetAnimInstance())
		{
			Anim->StopAllMontages(0.f);
		}
		Echo->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Echo->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Echo->SetCollisionObjectType(ECC_PhysicsBody);
		Echo->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Echo->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Echo->SetAllBodiesSimulatePhysics(true);
		Echo->SetSimulatePhysics(true);
		Echo->SetEnableGravity(true);
		Echo->WakeAllRigidBodies();
	}

	if (CachedCapsule)
	{
		CachedCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	FVector Impulse = LastDamageImpulse;
	Impulse.Z = FMath::Max(Impulse.Z, 80.f);
	if (Impulse.SizeSquared2D() < 100.f)
	{
		Impulse += GetActorForwardVector() * FMath::Max(HitDefaultImpulse, 300.f);
	}
	if (Echo)
	{
		Echo->AddImpulseToAllBodiesBelow(Impulse, NAME_None, /*bVelChange*/ true);
		Echo->WakeAllRigidBodies();
	}
}

float AGaspRagdollEnemy::MeasureRagdollSpeed(float DeltaSeconds)
{
	const USkeletalMeshComponent* Mesh = IsSourceMeshSimulating()
		? CachedSkeletalMesh.Get()
		: GetDevourPreviewMesh();
	if (Mesh && (Mesh->IsSimulatingPhysics() || Mesh->IsAnySimulatingPhysics()))
	{
		const float PhysicsSpeed = Mesh->GetPhysicsLinearVelocity().Size();
		if (PhysicsSpeed > KINDA_SMALL_NUMBER)
		{
			return PhysicsSpeed;
		}
	}
	const FVector Sample = Mesh ? Mesh->Bounds.Origin : GetActorLocation();
	if (!bRagdollSpeedPrimed)
	{
		bRagdollSpeedPrimed = true;
		PrevRagdollSample = Sample;
		return TNumericLimits<float>::Max();
	}
	const float Distance = FVector::Dist(Sample, PrevRagdollSample);
	PrevRagdollSample = Sample;
	return DeltaSeconds > KINDA_SMALL_NUMBER ? (Distance / DeltaSeconds) : 0.f;
}

void AGaspRagdollEnemy::TickRagdollDeath(float DeltaSeconds)
{
	if (!bRagdollDeathActive || bDevouredDeath || bDeathDissolveScheduled)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : DeathRagdollStartTime;
	const float Elapsed = Now - DeathRagdollStartTime;

	if (!bDeathRagdollRetried
		&& Elapsed >= 0.5f
		&& !IsInOfficialRagdollMode()
		&& !IsAnyDeathMeshSimulating())
	{
		bDeathRagdollRetried = true;
		ApplyManualDeathPhysics();
		LogRagdollRuntimeState(TEXT("manual death fallback"));
	}

	if (!IsInOfficialRagdollMode() && !IsAnyDeathMeshSimulating())
	{
		DeathSettleAccum = 0.f;
		bRagdollSpeedPrimed = false;
		if (Elapsed < FMath::Max(DeathRagdollMaxSettleSeconds, 0.5f))
		{
			return;
		}
		bDeathDissolveScheduled = true;
		LogRagdollRuntimeState(TEXT("dissolve without physics"));
	}
	else
	{
		const float Speed = MeasureRagdollSpeed(DeltaSeconds);
		if (Speed <= DeathRagdollSettleSpeed)
		{
			DeathSettleAccum += DeltaSeconds;
		}
		else
		{
			DeathSettleAccum = 0.f;
		}

		const bool bSettled = DeathSettleAccum >= FMath::Max(DeathRagdollSettleHold, 0.1f);
		const bool bTimedOut = Elapsed >= FMath::Max(DeathRagdollMaxSettleSeconds, 0.5f);
		if (!bSettled && !bTimedOut)
		{
			return;
		}

		bDeathDissolveScheduled = true;
		UE_LOG(LogSlimeFable, Log, TEXT("GaspRagdollEnemy %s: ragdoll settled (speed=%.1f timeout=%d)"),
			*GetName(), Speed, bTimedOut ? 1 : 0);
	}

	const float Delay = FMath::Max(DeathDissolveDelaySeconds, 0.f);
	UWorld* World = GetWorld();
	if (World && Delay > 0.f)
	{
		World->GetTimerManager().SetTimer(
			DeathRagdollTimer,
			this,
			&AGaspRagdollEnemy::StartDeathDissolve,
			Delay,
			false);
	}
	else
	{
		StartDeathDissolve();
	}
}

void AGaspRagdollEnemy::StartDeathDissolve()
{
	if (bDevouredDeath)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
	}

	if (UMaterialInterface* DissolveMat = DeathDissolveMaterial.LoadSynchronous())
	{
		if (!DeathDissolveMID)
		{
			DeathDissolveMID = UMaterialInstanceDynamic::Create(DissolveMat, this);
		}
		ForEachVisualMesh([this](UMeshComponent* Mesh)
		{
			if (Mesh)
			{
				Mesh->SetOverlayMaterial(DeathDissolveMID);
			}
		});
		EnemyCombat::SetGaspDeathDissolveAmount(DeathDissolveMID, 0.f);
	}

	DeathDissolveElapsed = 0.01f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathDissolveTimer,
			this,
			&AGaspRagdollEnemy::TickDeathDissolve,
			0.05f,
			true);
	}
}

bool AGaspRagdollEnemy::SetInjuryState(FName EntryName, bool bAllowNoneEntry)
{
	if (EntryName.IsNone() && !bAllowNoneEntry)
	{
		return false;
	}
	FProperty* Prop = GetClass()->FindPropertyByName(GaspRagdollPrivate::InjuryStatePropName);
	if (!Prop)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("GaspRagdollEnemy %s: Blueprint has no %s variable - injury state skipped"),
			*GetName(), *GaspRagdollPrivate::InjuryStatePropName.ToString());
		return false;
	}

	UEnum* Enum = nullptr;
	FNumericProperty* Underlying = nullptr;
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		Enum = EnumProp->GetEnum();
		Underlying = EnumProp->GetUnderlyingProperty();
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		Enum = ByteProp->Enum;
		Underlying = ByteProp;
	}
	if (!Enum || !Underlying)
	{
		return false;
	}

	const int64 Value = GaspRagdollPrivate::ResolveEnumEntry(Enum, EntryName);
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspRagdollEnemy %s: unknown %s entry '%s'"),
			*GetName(), *Enum->GetName(), *EntryName.ToString());
		return false;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(this);
	Underlying->SetIntPropertyValue(ValuePtr, Value);
	return true;
}

bool AGaspRagdollEnemy::CallBpFunctionWithName(FName FunctionName, FName Value)
{
	UFunction* Fn = FindFunction(FunctionName);
	if (!Fn)
	{
		return false;
	}
	uint8* Buffer = Fn->ParmsSize > 0
		? static_cast<uint8*>(FMemory_Alloca(Fn->ParmsSize))
		: nullptr;
	if (Buffer)
	{
		FMemory::Memzero(Buffer, Fn->ParmsSize);
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				NameProp->SetPropertyValue_InContainer(Buffer, Value);
			}
			else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
			{
				StrProp->SetPropertyValue_InContainer(Buffer, Value.ToString());
			}
			else if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				TextProp->SetPropertyValue_InContainer(Buffer, FText::FromName(Value));
			}
		}
	}
	ProcessEvent(Fn, Buffer);
	return true;
}

void AGaspRagdollEnemy::LogOfficialRagdollSignatures() const
{
	UE_LOG(LogSlimeFable, Log, TEXT("GaspRagdollEnemy %s: official ragdoll surface on %s"),
		*GetName(), *GetClass()->GetName());

	for (const FName& Name : GaspRagdollPrivate::SignatureDumpNames)
	{
		UFunction* Fn = FindFunction(Name);
		if (!Fn)
		{
			UE_LOG(LogSlimeFable, Log, TEXT("  fn %s: MISSING"), *Name.ToString());
			continue;
		}
		FString Params;
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			Params += FString::Printf(TEXT("%s%s %s"),
				Params.IsEmpty() ? TEXT("") : TEXT(", "),
				*It->GetCPPType(),
				*It->GetName());
		}
		UE_LOG(LogSlimeFable, Log, TEXT("  fn %s(%s)"), *Name.ToString(), *Params);
	}

	if (FProperty* Prop = GetClass()->FindPropertyByName(GaspRagdollPrivate::InjuryStatePropName))
	{
		UEnum* Enum = nullptr;
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			Enum = EnumProp->GetEnum();
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			Enum = ByteProp->Enum;
		}
		if (Enum)
		{
			for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
			{
				UE_LOG(LogSlimeFable, Log, TEXT("  injury[%lld] authored=%s display=%s"),
					Enum->GetValueByIndex(Index),
					*Enum->GetNameStringByIndex(Index),
					*Enum->GetDisplayNameTextByIndex(Index).ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogSlimeFable, Log, TEXT("  var Ragdoll_InjuryState: MISSING"));
	}
}
