// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeHitProbe.h"

#include "CombatDamageable.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "SlimeBodyComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeStatusComponent.h"

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
	TArray<FOverlapResult>& OutOverlaps)
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
	case ESlimeHitShape::Cone:
	case ESlimeHitShape::Sphere:
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

	const bool bHit = World->OverlapMultiByObjectType(
		OutOverlaps,
		QueryOrigin,
		Rotation,
		FCollisionObjectQueryParams(ECC_Pawn),
		Shape,
		Params);

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
	const FVector Impulse = Forward.GetSafeNormal() * Skill.Knockback + FVector::UpVector * Skill.LaunchZ;

	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
	{
		Damageable->ApplyDamage(Skill.Damage, Instigator, HitLocation, Impulse);
	}
	else if (USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Health->ApplyDamage(Skill.Damage, Instigator, HitLocation, Impulse);
	}

	if (Skill.bAppliesElementAura)
	{
		if (USlimeStatusComponent* Status = Target->FindComponentByClass<USlimeStatusComponent>())
		{
			Status->ApplyAura(Skill.Element, Instigator);
		}
	}
}

int32 USlimeHitProbe::PerformHit(
	AActor* Instigator,
	const FSlimeSkillDef& Skill,
	const FVector& Origin,
	const FVector& Forward,
	TSet<TWeakObjectPtr<AActor>>& AlreadyHit,
	TArray<FSlimeHitResult>* OutHits)
{
	if (!Instigator)
	{
		return 0;
	}

	UWorld* World = Instigator->GetWorld();
	TArray<FOverlapResult> Overlaps;
	GatherOverlaps(World, Instigator, Skill.Hit, Origin, Forward, Overlaps);

	int32 Count = 0;
	const FVector Dir = Forward.GetSafeNormal();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == Instigator || AlreadyHit.Contains(Target))
		{
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
