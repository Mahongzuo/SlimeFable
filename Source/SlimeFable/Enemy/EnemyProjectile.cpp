// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyProjectile.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 4.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(18.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);

	Niagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	Niagara->SetupAttachment(RootComponent);
	Niagara->bAutoActivate = false;
	Niagara->SetAutoDestroy(false);
}

void AEnemyProjectile::InitProjectile(AActor* InInstigator, const FEnemySkillDef& InSkill, const FVector& InVelocity)
{
	Source = InInstigator;
	Skill = InSkill;
	Velocity = InVelocity;
	LifeRemaining = FMath::Max(InSkill.ProjectileLife, 0.4f);
	Age = 0.f;
	WorldCollisionGrace = 0.1f;
	bHoming = InSkill.HomingTurnRate > KINDA_SMALL_NUMBER && InSkill.HomingRange > KINDA_SMALL_NUMBER;
	SetLifeSpan(LifeRemaining + 0.5f);

	FVector Flat = Velocity;
	Flat.Z = 0.f;
	if (Flat.IsNearlyZero())
	{
		Flat = GetActorForwardVector();
		Flat.Z = 0.f;
	}
	FallbackAimPoint = GetActorLocation() + Flat.GetSafeNormal() * 500.f;

	if (Collision)
	{
		Collision->SetSphereRadius(FMath::Max(InSkill.Hit.Radius, 12.f));
	}
	if (UNiagaraSystem* System = InSkill.ProjectileNiagara.LoadSynchronous())
	{
		Niagara->SetAsset(System);
		Niagara->SetAutoDestroy(false);
		Niagara->Activate(true);
	}
	if (InInstigator)
	{
		SetInstigator(Cast<APawn>(InInstigator));
	}
}

void AEnemyProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Niagara)
	{
		Niagara->DeactivateImmediate();
		Niagara->SetAsset(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyProjectile::ExplodeAndDestroy(bool bSpawnImpact)
{
	if (bSpawnImpact)
	{
		if (UNiagaraSystem* Impact = Skill.HitNiagara.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, Impact, GetActorLocation(), GetActorRotation(), FVector(1.f), true, true);
		}
	}
	if (Niagara)
	{
		Niagara->DeactivateImmediate();
		Niagara->SetAsset(nullptr);
	}
	Destroy();
}

AActor* AEnemyProjectile::FindHomingTarget(float MaxRange) const
{
	UWorld* World = GetWorld();
	if (!World || !Source.IsValid() || MaxRange <= 0.f)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyProjHome), false, this);
	Params.AddIgnoredActor(Source.Get());
	World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(MaxRange),
		Params);

	AActor* Best = nullptr;
	float BestDistSq = MaxRange * MaxRange;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || !USlimeHitProbe::IsHostile(Source.Get(), Actor))
		{
			continue;
		}
		if (const USlimeHealthComponent* Health = Actor->FindComponentByClass<USlimeHealthComponent>())
		{
			if (!Health->IsAlive())
			{
				continue;
			}
		}
		const float DistSq = FVector::DistSquared(GetActorLocation(), Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

void AEnemyProjectile::UpdateHoming(float DeltaSeconds)
{
	if (!bHoming)
	{
		return;
	}

	const float Speed = Velocity.Size();
	if (Speed < KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector DesiredPoint = FallbackAimPoint;
	if (AActor* Target = FindHomingTarget(Skill.HomingRange))
	{
		DesiredPoint = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	}

	const FVector DesiredDir = (DesiredPoint - GetActorLocation()).GetSafeNormal();
	if (DesiredDir.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRot = Velocity.Rotation();
	const FRotator DesiredRot = DesiredDir.Rotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaSeconds, Skill.HomingTurnRate);
	Velocity = NewRot.Vector() * Speed;
	SetActorRotation(NewRot);
}

void AEnemyProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	LifeRemaining -= DeltaSeconds;
	if (LifeRemaining <= 0.f)
	{
		ExplodeAndDestroy(false);
		return;
	}

	UpdateHoming(DeltaSeconds);

	const FVector Start = GetActorLocation();
	const FVector End = Start + Velocity * DeltaSeconds;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyProjectile), false, this);
	if (Source.IsValid())
	{
		Params.AddIgnoredActor(Source.Get());
	}

	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_Pawn);
	if (Age >= WorldCollisionGrace)
	{
		ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	}

	bool bExplode = false;
	bool bHitEnemy = false;
	if (GetWorld()->SweepSingleByObjectType(
			Hit,
			Start,
			End,
			FQuat::Identity,
			ObjQuery,
			FCollisionShape::MakeSphere(Collision->GetUnscaledSphereRadius()),
			Params))
	{
		AActor* Target = Hit.GetActor();
		if (Target && USlimeHitProbe::IsHostile(Source.Get(), Target))
		{
			AlreadyHit.Reset();
			FSlimeSkillDef Impact = EnemyCombat::ToSlimeHitSkill(Skill);
			Impact.Exec = ESlimeSkillExec::AoE;
			Impact.Hit.Shape = ESlimeHitShape::Sphere;
			Impact.Hit.Radius = FMath::Max(Skill.Hit.Radius * 2.f, 60.f);
			Impact.Hit.Range = 0.f;
			Impact.Hit.OriginForwardOffset = 0.f;
			USlimeHitProbe::PerformHit(Source.Get(), Impact, Hit.ImpactPoint, Velocity.GetSafeNormal(), AlreadyHit);
			bExplode = true;
			bHitEnemy = true;
		}
		else if (Age >= WorldCollisionGrace && Target && Target != Source)
		{
			bExplode = true;
		}
		else if (Age >= WorldCollisionGrace && !Target)
		{
			bExplode = true;
		}
	}

	SetActorLocation(bExplode ? Hit.ImpactPoint : End, false);
	if (bExplode)
	{
		ExplodeAndDestroy(bHitEnemy);
	}
}
