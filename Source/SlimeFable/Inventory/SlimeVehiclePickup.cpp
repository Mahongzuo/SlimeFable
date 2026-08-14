// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeVehiclePickup.h"

#include "SlimeCharacter.h"
#include "SlimeVehicleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ASlimeVehiclePickup::ASlimeVehiclePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded() && Mesh)
	{
		Mesh->SetStaticMesh(ConeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.55f, 0.55f, 0.35f));
	}
}

FText ASlimeVehiclePickup::GetInteractPromptVerb() const
{
	return FText::FromString(TEXT("使用载具"));
}

bool ASlimeVehiclePickup::CanBeUsedBy(const APawn* Picker) const
{
	if (!Picker || bDropping)
	{
		return false;
	}
	if (IgnoredPicker.Get() == Picker)
	{
		const UWorld* World = GetWorld();
		if (World && World->GetTimeSeconds() < IgnorePickerUntil)
		{
			return false;
		}
	}
	return true;
}

void ASlimeVehiclePickup::BeginDropped(APawn* InIgnoredPicker, float IgnoreSeconds)
{
	IgnoredPicker = InIgnoredPicker;
	IgnorePickerUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + IgnoreSeconds : IgnoreSeconds;
	bDropping = true;
	SettleTimer = 0.f;
	SetHighlight(false);

	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetSimulatePhysics(true);
		Mesh->SetEnableGravity(true);
		Mesh->WakeAllRigidBodies();
	}
}

void ASlimeVehiclePickup::FinishSettle()
{
	bDropping = false;
	SettleTimer = 0.f;

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetGenerateOverlapEvents(true);
	}
}

void ASlimeVehiclePickup::Tick(float DeltaSeconds)
{
	if (bDropping && Mesh)
	{
		const FVector Vel = Mesh->GetPhysicsLinearVelocity();
		if (Vel.Size() <= SettleSpeedThreshold)
		{
			SettleTimer += DeltaSeconds;
			if (SettleTimer >= SettleHoldSeconds)
			{
				FinishSettle();
			}
		}
		else
		{
			SettleTimer = 0.f;
		}
		return;
	}

	Super::Tick(DeltaSeconds);
}

bool ASlimeVehiclePickup::TryPickup(APawn* Picker)
{
	if (!CanBeUsedBy(Picker))
	{
		return false;
	}

	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(Picker);
	if (!Slime)
	{
		return false;
	}

	USlimeVehicleComponent* Vehicle = Slime->GetSlimeVehicle();
	if (!Vehicle)
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(Picker->GetActorLocation(), GetActorLocation());
	if (DistSq > FMath::Square(HighlightRadius))
	{
		return false;
	}

	if (!Vehicle->EnterVehicle(this))
	{
		return false;
	}

	Destroy();
	return true;
}
