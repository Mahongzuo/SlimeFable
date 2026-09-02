// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFoliageInteractComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "FoliageInteractVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SlimeFoliageInteractSubsystem.h"

USlimeFoliageInteractComponent::USlimeFoliageInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;
}

void USlimeFoliageInteractComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	RebuildSample();

	if (UWorld* World = GetWorld())
	{
		if (USlimeFoliageInteractSubsystem* Sub = World->GetSubsystem<USlimeFoliageInteractSubsystem>())
		{
			Sub->RegisterInteractor(this);
		}
	}
}

void USlimeFoliageInteractComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (USlimeFoliageInteractSubsystem* Sub = World->GetSubsystem<USlimeFoliageInteractSubsystem>())
		{
			Sub->UnregisterInteractor(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void USlimeFoliageInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RebuildSample();
}

void USlimeFoliageInteractComponent::RebuildSample()
{
	FSlimeFoliageInteractSample Sample;
	Sample.bPlayerControlled = OwnerCharacter && OwnerCharacter->IsPlayerControlled();

	if (!bEnableFoliageInteract || !OwnerCharacter || !IsActive())
	{
		LatestSample = Sample;
		return;
	}

	if (OwnerCharacter->IsHidden())
	{
		LatestSample = Sample;
		return;
	}

	const IFoliageInteractVolume* Volume = Cast<IFoliageInteractVolume>(OwnerCharacter);
	if (Volume && Volume->ShouldSuppressFoliageInteract())
	{
		LatestSample = Sample;
		return;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.f;
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
	float SourceRadius = CapsuleRadius;
	FVector Location = OwnerCharacter->GetActorLocation();

	bool bCustomVolume = false;
	if (Volume)
	{
		FVector CustomLoc = FVector::ZeroVector;
		float CustomRadius = 0.f;
		if (Volume->GetFoliageInteractVolume(CustomLoc, CustomRadius))
		{
			Location = CustomLoc;
			SourceRadius = FMath::Max(SourceRadius, CustomRadius);
			bCustomVolume = true;
		}
	}

	if (!bCustomVolume)
	{
		if (const USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			const FBoxSphereBounds Bounds = Mesh->Bounds;
			Location = Bounds.Origin;
			SourceRadius = FMath::Max(SourceRadius, Bounds.SphereRadius);
		}
		else
		{
			Location.Z -= CapsuleHalfHeight;
		}
	}

	Sample.Location = Location;
	Sample.Radius = FMath::Max(20.f, SourceRadius * RadiusScale);

	if (const UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Sample.Velocity = Move->Velocity;
	}
	else
	{
		Sample.Velocity = OwnerCharacter->GetVelocity();
	}

	const float Speed2D = FVector(Sample.Velocity.X, Sample.Velocity.Y, 0.f).Size();
	const float Speed01 = FMath::Clamp(Speed2D / FMath::Max(1.f, FullSpeedForStrength), 0.f, 1.f);
	Sample.Strength = FMath::Lerp(IdlePartStrength, 1.f, Speed01);

	LatestSample = Sample;
}
