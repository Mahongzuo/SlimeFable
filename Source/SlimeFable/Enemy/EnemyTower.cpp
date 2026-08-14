// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyTower.h"

#include "CombatDamageable.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyCombatComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHealthComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AEnemyTower::AEnemyTower()
{
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;
	bAllowDespawn = false;
	MaxHP = 180.f;
	AttackRange = 1000.f;
	FireMode = EEnemyTowerFireMode::Beam;
	bShowFallbackBeamMesh = false;
	bDrawDebugBeam = true;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 0.f;
		Move->DisableMovement();
		Move->DefaultLandMovementMode = MOVE_None;
		Move->bOrientRotationToMovement = false;
	}

	AggroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AggroSphere"));
	AggroSphere->SetupAttachment(RootComponent);
	AggroSphere->SetSphereRadius(AttackRange);
	// Pawn object type so SlimeBody RefreshColliders (WorldStatic/Dynamic only) never gathers this
	// giant query sphere as a physics shell — that was stretching the slime on range entry.
	AggroSphere->SetCollisionObjectType(ECC_Pawn);
	AggroSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AggroSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AggroSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AggroSphere->SetGenerateOverlapEvents(true);
	AggroSphere->SetCanEverAffectNavigation(false);
	AggroSphere->SetHiddenInGame(true);

	EditorRangeVisual = CreateDefaultSubobject<USphereComponent>(TEXT("EditorRangeVisual"));
	EditorRangeVisual->SetupAttachment(RootComponent);
	EditorRangeVisual->SetSphereRadius(AttackRange);
	EditorRangeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorRangeVisual->SetHiddenInGame(true);
	EditorRangeVisual->ShapeColor = FColor(220, 80, 60);
	EditorRangeVisual->bDrawOnlyIfSelected = true;

	FallbackBeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackBeamMesh"));
	FallbackBeamMesh->SetupAttachment(RootComponent);
	FallbackBeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackBeamMesh->SetHiddenInGame(true);
	FallbackBeamMesh->SetVisibility(false);
	FallbackBeamMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		FallbackBeamMesh->SetStaticMesh(CylinderMesh.Object);
	}

	MissileSkill = EnemyCombat::MakeDefaultMissileSkill();
}

void AEnemyTower::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncRangeSphere();
}

void AEnemyTower::BeginPlay()
{
	Super::BeginPlay();
	SyncRangeSphere();

	if (AggroSphere)
	{
		AggroSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemyTower::HandleAggroBegin);
		AggroSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemyTower::HandleAggroEnd);
	}

	if (Combat)
	{
		Combat->MuzzleSocket = AimSocket;
	}
}

void AEnemyTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFiring();
	HideFallbackBeam();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamHideTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyTower::SyncRangeSphere()
{
	if (AggroSphere)
	{
		AggroSphere->SetSphereRadius(AttackRange);
		AggroSphere->SetCollisionObjectType(ECC_Pawn);
		AggroSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		AggroSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
	if (EditorRangeVisual)
	{
		EditorRangeVisual->SetSphereRadius(AttackRange);
	}
}

void AEnemyTower::SetEnemyPresence(EEnemyPresence NewPresence)
{
	Super::SetEnemyPresence(NewPresence);
	const bool bAwake = NewPresence == EEnemyPresence::Active || NewPresence == EEnemyPresence::Idle;
	if (AggroSphere)
	{
		AggroSphere->SetCollisionEnabled(bAwake ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (!bAwake)
	{
		StopFiring();
		bPlayerInRange = false;
		CurrentTarget.Reset();
	}
}

void AEnemyTower::HandleAggroBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || OtherActor != Player)
	{
		return;
	}
	if (GetEnemyPresence() == EEnemyPresence::Sleep || GetEnemyPresence() == EEnemyPresence::Despawned)
	{
		return;
	}
	bPlayerInRange = true;
	CurrentTarget = Player;
	StartFiring();
}

void AEnemyTower::HandleAggroEnd(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || OtherActor != Player)
	{
		return;
	}
	bPlayerInRange = false;
	CurrentTarget.Reset();
	StopFiring();
}

void AEnemyTower::StartFiring()
{
	if (!GetWorld())
	{
		return;
	}
	if (!GetWorld()->GetTimerManager().IsTimerActive(FireTimerHandle))
	{
		FireAtTarget();
		GetWorld()->GetTimerManager().SetTimer(
			FireTimerHandle, this, &AEnemyTower::FireAtTarget, FireInterval, true);
	}
}

void AEnemyTower::StopFiring()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
		World->GetTimerManager().ClearTimer(FireTelegraphTimerHandle);
	}
}

APawn* AEnemyTower::ResolvePlayerTarget() const
{
	if (CurrentTarget.IsValid())
	{
		return CurrentTarget.Get();
	}
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AEnemyTower::FaceTarget(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		SetActorRotation(To.Rotation());
	}
}

FVector AEnemyTower::GetMuzzleLocation() const
{
	if (AimSocket != NAME_None)
	{
		if (USkeletalMeshComponent* Skel = GetMesh())
		{
			if (Skel->DoesSocketExist(AimSocket))
			{
				return Skel->GetSocketLocation(AimSocket);
			}
		}
		for (const TObjectPtr<USceneComponent>& Part : GeneratedParts)
		{
			if (USkeletalMeshComponent* SkelPart = Cast<USkeletalMeshComponent>(Part.Get()))
			{
				if (SkelPart->DoesSocketExist(AimSocket))
				{
					return SkelPart->GetSocketLocation(AimSocket);
				}
			}
		}
	}
	return GetVisualBoundsCenter();
}

void AEnemyTower::FireAtTarget()
{
	if (!bPlayerInRange)
	{
		return;
	}
	if (Health && !Health->IsAlive())
	{
		StopFiring();
		return;
	}
	if (GetEnemyPresence() != EEnemyPresence::Active && GetEnemyPresence() != EEnemyPresence::Idle)
	{
		return;
	}

	APawn* Target = ResolvePlayerTarget();
	if (!Target)
	{
		return;
	}
	if (const USlimeHealthComponent* TargetHealth = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		if (!TargetHealth->IsAlive())
		{
			return;
		}
	}

	const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
	if (DistSq > FMath::Square(AttackRange * 1.05f))
	{
		return;
	}

	FaceTarget(Target);
	USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, this);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FireTelegraphTimerHandle);
	if (FireTelegraphTime <= KINDA_SMALL_NUMBER)
	{
		CommitFire();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			FireTelegraphTimerHandle, this, &AEnemyTower::CommitFire, FireTelegraphTime, false);
	}
}

void AEnemyTower::CommitFire()
{
	if (!bPlayerInRange)
	{
		return;
	}
	if (Health && !Health->IsAlive())
	{
		StopFiring();
		return;
	}
	if (GetEnemyPresence() != EEnemyPresence::Active && GetEnemyPresence() != EEnemyPresence::Idle)
	{
		return;
	}

	APawn* Target = ResolvePlayerTarget();
	if (!Target)
	{
		return;
	}
	if (const USlimeHealthComponent* TargetHealth = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		if (!TargetHealth->IsAlive())
		{
			return;
		}
	}

	const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
	if (DistSq > FMath::Square(AttackRange * 1.05f))
	{
		return;
	}

	FaceTarget(Target);

	if (FireMode == EEnemyTowerFireMode::Beam)
	{
		FireBeam(Target);
	}
	else
	{
		FireProjectile(Target);
	}
}

void AEnemyTower::FireBeam(AActor* Target)
{
	UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return;
	}

	const FVector Start = GetMuzzleLocation();
	const FVector EndAim = Target->GetActorLocation();
	const float TargetDist = FVector::Dist(Start, EndAim);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyTowerBeam), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);

	FVector BeamEnd = EndAim;
	bool bBlockedByWorld = false;
	if (World->LineTraceSingleByChannel(Hit, Start, EndAim, ECC_Visibility, Params) && Hit.bBlockingHit)
	{
		if (Hit.Distance + 5.f < TargetDist)
		{
			BeamEnd = Hit.ImpactPoint;
			bBlockedByWorld = true;
		}
	}

	if (!bBlockedByWorld)
	{
		BeamEnd = EndAim;
		const FVector Impulse = (EndAim - Start).GetSafeNormal() * 180.f;
		if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
		{
			Damageable->ApplyDamage(BeamDamage, this, BeamEnd, Impulse);
		}
		else if (USlimeHealthComponent* TargetHealth = Target->FindComponentByClass<USlimeHealthComponent>())
		{
			TargetHealth->ApplyDamage(BeamDamage, this, BeamEnd, Impulse);
		}
	}

	const FVector Dir = (BeamEnd - Start);
	const float Length = FMath::Max(Dir.Size(), 1.f);
	const FRotator BeamRot = Dir.GetSafeNormal().Rotation();

	if (UNiagaraSystem* BeamFx = BeamNiagara.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, BeamFx, Start, BeamRot, FVector(1.f), true, true);
	}

	if (bDrawDebugBeam)
	{
		DrawDebugLine(World, Start, BeamEnd, FColor::Red, false, BeamDuration, 0, 2.f);
		DrawDebugPoint(World, BeamEnd, 8.f, FColor::Red, false, BeamDuration, 0);
	}

	if (bShowFallbackBeamMesh && FallbackBeamMesh)
	{
		// Engine cylinder is 100cm tall along Z; scale Z to beam length, XY to thickness.
		const float RadiusScale = BeamThickness / 50.f;
		const float LengthScale = Length / 100.f;
		FallbackBeamMesh->SetWorldLocation(Start + Dir * 0.5f);
		FallbackBeamMesh->SetWorldRotation(BeamRot + FRotator(90.f, 0.f, 0.f));
		FallbackBeamMesh->SetWorldScale3D(FVector(RadiusScale, RadiusScale, LengthScale));
		FallbackBeamMesh->SetVisibility(true);
		FallbackBeamMesh->SetHiddenInGame(false);

		World->GetTimerManager().ClearTimer(BeamHideTimerHandle);
		World->GetTimerManager().SetTimer(
			BeamHideTimerHandle, this, &AEnemyTower::HideFallbackBeam, BeamDuration, false);
	}
}

void AEnemyTower::HideFallbackBeam()
{
	if (FallbackBeamMesh)
	{
		FallbackBeamMesh->SetVisibility(false);
		FallbackBeamMesh->SetHiddenInGame(true);
	}
}

void AEnemyTower::FireProjectile(AActor* Target)
{
	if (!Combat || Combat->IsAttacking())
	{
		return;
	}

	if (UNiagaraSystem* MuzzleFx = MuzzleNiagara.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, MuzzleFx, GetMuzzleLocation(), GetActorRotation(), FVector(1.f), true, true);
	}

	Combat->TryExecute(MissileSkill);
}
