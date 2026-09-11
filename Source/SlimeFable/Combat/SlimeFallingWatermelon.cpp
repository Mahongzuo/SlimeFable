// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFallingWatermelon.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "ProceduralMeshComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeSliceUtil.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"

ASlimeFallingWatermelon::ASlimeFallingWatermelon()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASlimeFallingWatermelon::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(8.f);
	SetActorScale3D(FVector(FMath::Max(RainScale, 0.1f)));
	if (!ProceduralMesh)
	{
		return;
	}

	ProceduralMesh->SetMobility(EComponentMobility::Movable);
	USlimeSliceUtil::EnableSimpleConvexCollision(ProceduralMesh);
	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProceduralMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ProceduralMesh->SetGenerateOverlapEvents(true);
	ProceduralMesh->SetNotifyRigidBodyCollision(true);
	ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ProceduralMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	ProceduralMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ProceduralMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ProceduralMesh->SetSimulatePhysics(true);
	ProceduralMesh->SetEnableGravity(true);
	ProceduralMesh->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -600.f));
	ProceduralMesh->OnComponentHit.AddDynamic(this, &ASlimeFallingWatermelon::HandleMeshHit);
	ProceduralMesh->OnComponentBeginOverlap.AddDynamic(this, &ASlimeFallingWatermelon::HandleMeshOverlap);
}

void ASlimeFallingWatermelon::InitFalling(AActor* InSource, float InDamage, float InLifeAfterBreak)
{
	SourceActor = InSource;
	ImpactDamage = FMath::Max(InDamage, 0.f);
	LifeAfterBreak = FMath::Max(InLifeAfterBreak, 0.5f);
	if (InSource)
	{
		SetOwner(InSource);
		SetInstigator(Cast<APawn>(InSource));
		if (ProceduralMesh)
		{
			ProceduralMesh->IgnoreActorWhenMoving(InSource, true);
		}
		if (UPrimitiveComponent* SourcePrim = InSource->FindComponentByClass<UPrimitiveComponent>())
		{
			SourcePrim->IgnoreActorWhenMoving(this, true);
		}
	}
}

void ASlimeFallingWatermelon::HandleMeshHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)HitComp;
	(void)OtherComp;
	(void)NormalImpulse;
	(void)Hit;
	TryImpact(OtherActor);
}

void ASlimeFallingWatermelon::HandleMeshOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComp;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	TryImpact(OtherActor);
}

bool ASlimeFallingWatermelon::ShouldIgnoreActor(const AActor* Other) const
{
	if (!Other || Other == this || Other == SourceActor.Get())
	{
		return true;
	}
	return Other->IsA<ASlimeFallingWatermelon>();
}

void ASlimeFallingWatermelon::TryImpact(AActor* OtherActor)
{
	if (bShattered || ShouldIgnoreActor(OtherActor))
	{
		return;
	}
	Shatter(OtherActor);
}

void ASlimeFallingWatermelon::ApplyImpactDamage(AActor* Target)
{
	AActor* Source = SourceActor.Get();
	if (!Target || ImpactDamage <= 0.f)
	{
		return;
	}
	if (!USlimeHitProbe::IsHostile(Source ? Source : this, Target)
		|| !USlimeHitProbe::IsValidDamageTarget(Target))
	{
		return;
	}

	const FVector Location = Target->GetActorLocation();
	const FVector Impulse = FVector::DownVector * 80.f;
	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
	{
		Damageable->ApplyDamage(ImpactDamage, Source, Location, Impulse);
	}
	else if (USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Health->ApplyDamage(ImpactDamage, Source, Location, Impulse);
	}
}

void ASlimeFallingWatermelon::Shatter(AActor* HitActor)
{
	if (bShattered)
	{
		return;
	}
	bShattered = true;
	ApplyImpactDamage(HitActor);

	if (ProceduralMesh)
	{
		USlimeSliceUtil::ShatterProceduralMesh(
			ProceduralMesh,
			CapMaterial,
			SliceSound,
			ShatterPieceMin,
			ShatterPieceMax,
			MinSliceExtent,
			SliceImpulse);
	}
	SetLifeSpan(LifeAfterBreak);
}
