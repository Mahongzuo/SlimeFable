// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSpringArmComponent.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "SlimeClingComponent.h"

USlimeSpringArmComponent::USlimeSpringArmComponent()
{
	bDoCollisionTest = false;
}

void USlimeSpringArmComponent::UpdateDesiredArmLocation(
	bool bDoTrace,
	bool bDoLocationLag,
	bool bDoRotationLag,
	float DeltaTime)
{
	// Default: no boom collision (avoids pull-in / return nausea). Editor can enable Do Collision Test.
	Super::UpdateDesiredArmLocation(bDoCollisionTest && bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);

	if (IsOwnerClinging())
	{
		return;
	}

	FTransform WorldCam = GetSocketTransform(USpringArmComponent::SocketName, RTS_World);
	FVector Loc = WorldCam.GetLocation();
	ApplyFootClamp(Loc);
	WorldCam.SetLocation(Loc);

	const FTransform RelCam = WorldCam.GetRelativeTransform(GetComponentTransform());
	RelativeSocketLocation = RelCam.GetLocation();
	RelativeSocketRotation = RelCam.GetRotation();
	UpdateChildTransforms();
}

bool USlimeSpringArmComponent::IsOwnerClinging() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (const USlimeClingComponent* Cling = Owner->FindComponentByClass<USlimeClingComponent>())
		{
			return Cling->IsClinging();
		}
	}
	return false;
}

void USlimeSpringArmComponent::ApplyFootClamp(FVector& InOutLoc) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector CapsuleCenter = Owner->GetActorLocation();
	float CapsuleBottomZ = CapsuleCenter.Z;
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			CapsuleCenter = Capsule->GetComponentLocation();
			CapsuleBottomZ = CapsuleCenter.Z - Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	// Always keep camera above capsule bottom + clearance (prevents digging underground).
	const float HardMinZ = CapsuleBottomZ + MinCameraClearance;
	if (InOutLoc.Z < HardMinZ)
	{
		InOutLoc.Z = HardMinZ;
	}

	if (FVector::Dist2D(InOutLoc, CapsuleCenter) > FootClampRadius)
	{
		return;
	}

	float FloorZ = CapsuleBottomZ;
	bool bHaveFloor = false;
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeCamFootClamp), false, Owner);
		const FVector Start = CapsuleCenter + FVector(0.f, 0.f, 50.f);
		const FVector End = CapsuleCenter - FVector(0.f, 0.f, 400.f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params)
			&& Hit.bBlockingHit
			&& Hit.ImpactNormal.Z > 0.7f)
		{
			FloorZ = Hit.ImpactPoint.Z;
			bHaveFloor = true;
		}
	}

	const float BaseZ = bHaveFloor ? FMath::Max(FloorZ, CapsuleBottomZ) : CapsuleBottomZ;
	const float MinZ = BaseZ + MinCameraClearance;
	const float MaxZ = CapsuleBottomZ + MaxFootLift;
	if (InOutLoc.Z < MinZ)
	{
		InOutLoc.Z = FMath::Min(MinZ, MaxZ);
	}
}
