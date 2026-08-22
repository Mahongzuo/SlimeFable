// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeHitProbe.h"

#include "CombatDamageable.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeSliceable.h"
#include "SlimeSliceableComponent.h"
#include "SlimeSliceUtil.h"
#include "SlimeStatusComponent.h"
#include "EnemyCharacter.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	UClass* GetBlueprintSliceablesInterface()
	{
		static TWeakObjectPtr<UClass> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UClass>(nullptr, TEXT("/Game/Blueprints/Food/I_Sliceables.I_Sliceables_C"));
		}
		return Cached.Get();
	}

	UMaterialInterface* GetDefaultWatermelonCapMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Game/StaticMeshes/Food/Watermelon/MI_watermelon_inside.MI_watermelon_inside"));
		}
		return Cached.Get();
	}

	USoundBase* GetDefaultFruitSliceSound()
	{
		static TWeakObjectPtr<USoundBase> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/sfx_fruitslice_01.sfx_fruitslice_01"));
		}
		return Cached.Get();
	}

	bool ImplementsBlueprintSliceables(const AActor* Target)
	{
		if (!Target)
		{
			return false;
		}
		if (UClass* Iface = GetBlueprintSliceablesInterface())
		{
			return Target->GetClass()->ImplementsInterface(Iface);
		}
		return false;
	}
}

static TAutoConsoleVariable<int32> CVarSlimeHitDebug(
	TEXT("sl.HitDebug"),
	0,
	TEXT("Draw slime combat hit shapes (0=off, 1=on)."),
	ECVF_Cheat);

ESlimeTeam USlimeHitProbe::GetTeam(const AActor* Actor)
{
	if (!Actor)
	{
		return ESlimeTeam::Enemy;
	}
	if (const USlimeHealthComponent* Health = Actor->FindComponentByClass<USlimeHealthComponent>())
	{
		return Health->Team;
	}
	return ESlimeTeam::Enemy;
}

bool USlimeHitProbe::IsHostile(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	return GetTeam(A) != GetTeam(B);
}

FVector USlimeHitProbe::ResolveOrigin(AActor* Instigator, const FSlimeHitSpec& Spec, const FVector& Forward)
{
	FVector Origin = Instigator ? Instigator->GetActorLocation() : FVector::ZeroVector;
	if (const USlimeBodyComponent* Body = Instigator ? Instigator->FindComponentByClass<USlimeBodyComponent>() : nullptr)
	{
		Origin = Body->GetBlobCenter();
	}
	return Origin + Forward.GetSafeNormal() * Spec.OriginForwardOffset;
}

bool USlimeHitProbe::GatherOverlaps(
	UWorld* World,
	AActor* Instigator,
	const FSlimeHitSpec& Spec,
	const FVector& Origin,
	const FVector& Forward,
	TArray<FOverlapResult>& OutOverlaps,
	AActor* RestrictTarget)
{
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeHitProbe), false, Instigator);
	const FVector Dir = Forward.GetSafeNormal();
	FCollisionShape Shape;
	FVector QueryOrigin = Origin;
	FQuat Rotation = FQuat::Identity;

	switch (Spec.Shape)
	{
	case ESlimeHitShape::Capsule:
		{
			const FVector End = Origin + Dir * Spec.Range;
			QueryOrigin = (Origin + End) * 0.5f;
			const FVector Axis = End - Origin;
			Rotation = FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).ToQuat();
			Shape = FCollisionShape::MakeCapsule(Spec.Radius, FMath::Max(Axis.Size() * 0.5f, Spec.Radius));
			break;
		}
	case ESlimeHitShape::Sphere:
		QueryOrigin = Origin + Dir * (Spec.Range * 0.5f);
		Shape = FCollisionShape::MakeSphere(FMath::Max(Spec.Radius + Spec.Range * 0.5f, 1.f));
		break;
	case ESlimeHitShape::Cone:
	case ESlimeHitShape::ProjectileSweep:
	default:
		Shape = FCollisionShape::MakeSphere(Spec.Shape == ESlimeHitShape::Cone
			? FMath::Max(Spec.Range, Spec.Radius)
			: FMath::Max(Spec.Radius, 1.f));
		if (Spec.Shape == ESlimeHitShape::Cone)
		{
			QueryOrigin = Origin + Dir * (Spec.Range * 0.45f);
		}
		break;
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	// Project custom Object Channel "Slicable" (see DefaultEngine.ini).
	ObjParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	const bool bHit = World->OverlapMultiByObjectType(
		OutOverlaps,
		QueryOrigin,
		Rotation,
		ObjParams,
		Shape,
		Params);

	if (RestrictTarget && bHit)
	{
		OutOverlaps.RemoveAll([RestrictTarget](const FOverlapResult& Overlap)
		{
			return Overlap.GetActor() != RestrictTarget;
		});
	}

	if (CVarSlimeHitDebug.GetValueOnGameThread() > 0)
	{
		const FColor Color = bHit ? FColor::Red : FColor::Yellow;
		if (Spec.Shape == ESlimeHitShape::Capsule)
		{
			DrawDebugCapsule(World, QueryOrigin, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, Color, false, 0.25f, 0, 1.5f);
		}
		else
		{
			DrawDebugSphere(World, QueryOrigin, Shape.GetSphereRadius(), 12, Color, false, 0.25f, 0, 1.5f);
		}
	}

	return bHit;
}

void USlimeHitProbe::ApplyToActor(
	AActor* Instigator,
	AActor* Target,
	const FSlimeSkillDef& Skill,
	const FVector& HitLocation,
	const FVector& Forward)
{
	FVector Impulse = Forward.GetSafeNormal() * Skill.Knockback + FVector::UpVector * Skill.LaunchZ;
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Target))
	{
		Impulse *= (1.f - FMath::Clamp(Enemy->KnockbackResistance, 0.f, 1.f));
	}
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (Movement->IsFalling())
			{
				Impulse.Z = 0.f;
			}
		}
	}

	float DamageAmount = Skill.Damage;
	if (Instigator)
	{
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Instigator))
		{
			if (Enemy->bHarmless)
			{
				DamageAmount = 0.f;
			}
		}
		if (DamageAmount > 0.f)
		{
			if (USlimeCombatComponent* Combat = Instigator->FindComponentByClass<USlimeCombatComponent>())
			{
				DamageAmount = Combat->ResolveOutgoingDamage(Skill);
			}
		}
	}

	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
	{
		Damageable->ApplyDamage(DamageAmount, Instigator, HitLocation, Impulse);
	}
	else if (USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Health->ApplyDamage(DamageAmount, Instigator, HitLocation, Impulse);
	}

	if (Skill.bAppliesElementAura)
	{
		if (USlimeStatusComponent* Status = Target->FindComponentByClass<USlimeStatusComponent>())
		{
			Status->ApplyAura(Skill.Element, Instigator);
		}
	}
}

bool USlimeHitProbe::TrySliceActor(
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	const FVector& Origin,
	const FVector& Forward)
{
	if (!Target)
	{
		return false;
	}

	const bool bImplementsCpp = Target->GetClass()->ImplementsInterface(USlimeSliceable::StaticClass());
	USlimeSliceableComponent* SliceComp = Target->FindComponentByClass<USlimeSliceableComponent>();
	const bool bImplementsBp = ImplementsBlueprintSliceables(Target);
	if (!bImplementsCpp && !SliceComp && !bImplementsBp)
	{
		return false;
	}

	UProceduralMeshComponent* MeshToSlice = Cast<UProceduralMeshComponent>(HitComponent);
	if (!MeshToSlice)
	{
		MeshToSlice = Target->FindComponentByClass<UProceduralMeshComponent>();
	}
	if (!MeshToSlice)
	{
		return false;
	}

	// Ensure query hits work even if the BP left a custom Object Type.
	if (MeshToSlice->GetCollisionObjectType() != ECC_WorldDynamic
		&& MeshToSlice->GetCollisionObjectType() != ECC_PhysicsBody
		&& MeshToSlice->GetCollisionObjectType() != ECC_WorldStatic)
	{
		MeshToSlice->SetCollisionObjectType(ECC_WorldDynamic);
		MeshToSlice->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshToSlice->SetGenerateOverlapEvents(true);
	}

	FVector PlanePos = Target->GetActorLocation();
	if (HitComponent)
	{
		FVector Closest = PlanePos;
		if (HitComponent->GetClosestPointOnCollision(Origin, Closest) > 0.f)
		{
			PlanePos = Closest;
		}
		else
		{
			PlanePos = MeshToSlice->Bounds.Origin;
		}
	}
	else
	{
		PlanePos = MeshToSlice->Bounds.Origin;
	}

	const FVector Dir = Forward.GetSafeNormal();
	const FVector PlaneNormal = Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;

	if (bImplementsCpp)
	{
		ISlimeSliceable::Execute_SliceAt(Target, PlanePos, PlaneNormal, MeshToSlice);
		return true;
	}
	if (SliceComp)
	{
		SliceComp->SliceAt(PlanePos, PlaneNormal, MeshToSlice);
		return true;
	}

	// Existing BP_watermelon_slice: bypass Do Once / SliceMe and cut in C++ so multi-slice works.
	UMaterialInterface* Cap = GetDefaultWatermelonCapMaterial();
	USoundBase* Sfx = GetDefaultFruitSliceSound();
	return USlimeSliceUtil::SliceProceduralMeshAt(MeshToSlice, PlanePos, PlaneNormal, Cap, Sfx);
}

int32 USlimeHitProbe::PerformHit(
	AActor* Instigator,
	const FSlimeSkillDef& Skill,
	const FVector& Origin,
	const FVector& Forward,
	TSet<TWeakObjectPtr<AActor>>& AlreadyHit,
	AActor* RestrictTarget,
	TArray<FSlimeHitResult>* OutHits)
{
	if (!Instigator)
	{
		return 0;
	}

	UWorld* World = Instigator->GetWorld();
	TArray<FOverlapResult> Overlaps;
	GatherOverlaps(World, Instigator, Skill.Hit, Origin, Forward, Overlaps, RestrictTarget);

	int32 Count = 0;
	const FVector Dir = Forward.GetSafeNormal();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == Instigator || AlreadyHit.Contains(Target))
		{
			continue;
		}
		if (RestrictTarget && Target != RestrictTarget)
		{
			continue;
		}

		const FVector HitLocation = Target->GetActorLocation();
		if (Skill.Hit.Shape == ESlimeHitShape::Cone)
		{
			const FVector ToTarget = (HitLocation - Origin).GetSafeNormal();
			const float Cos = FVector::DotProduct(Dir, ToTarget);
			const float MinCos = FMath::Cos(FMath::DegreesToRadians(Skill.Hit.ConeHalfAngle));
			if (Cos < MinCos)
			{
				continue;
			}
		}

		if (TrySliceActor(Target, Overlap.GetComponent(), Origin, Dir))
		{
			AlreadyHit.Add(Target);
			continue;
		}

		if (!IsHostile(Instigator, Target))
		{
			continue;
		}
		if (const USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
		{
			if (!Health->IsAlive())
			{
				continue;
			}
		}

		AlreadyHit.Add(Target);
		ApplyToActor(Instigator, Target, Skill, HitLocation, Dir);
		++Count;

		if (OutHits)
		{
			FSlimeHitResult Result;
			Result.Actor = Target;
			Result.Location = HitLocation;
			Result.Normal = -Dir;
			OutHits->Add(Result);
		}
	}
	return Count;
}
