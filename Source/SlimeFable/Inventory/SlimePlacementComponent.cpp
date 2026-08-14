// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlacementComponent.h"

#include "SlimeItemDefinition.h"
#include "SlimePlacePreview.h"
#include "SlimePlacedActor.h"
#include "SlimeInventorySubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "InputCoreTypes.h"
#include "Components/StaticMeshComponent.h"

USlimePlacementComponent::USlimePlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool USlimePlacementComponent::BeginPlacement(USlimePlaceableDefinition* Definition)
{
	if (!Definition)
	{
		return false;
	}

	CancelPlacement();
	ActiveDefinition = Definition;

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = World->SpawnActor<ASlimePlacePreview>(ASlimePlacePreview::StaticClass(), GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (PreviewActor)
		{
			if (UStaticMesh* Mesh = Definition->PreviewMesh.LoadSynchronous())
			{
				PreviewActor->SetPreviewMesh(Mesh);
			}
		}
	}
	return true;
}

void USlimePlacementComponent::CancelPlacement()
{
	ActiveDefinition = nullptr;
	bPlacementValid = false;
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

bool USlimePlacementComponent::ConfirmPlacement()
{
	if (!ActiveDefinition || !bPlacementValid)
	{
		return false;
	}

	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!World || !Pawn)
	{
		return false;
	}

	USlimeInventorySubsystem* Inv = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<USlimeInventorySubsystem>()
		: nullptr;
	if (!Inv || !Inv->RemoveItem(ActiveDefinition->ItemId, 1))
	{
		return false;
	}

	UClass* SpawnClass = ActiveDefinition->PlacedActorClass.LoadSynchronous();
	if (!SpawnClass)
	{
		SpawnClass = ASlimePlacedActor::StaticClass();
	}

	const FVector Location = LastHit.ImpactPoint;
	const FRotator Rotation = FRotationMatrix::MakeFromZX(LastHit.ImpactNormal, Pawn->GetActorForwardVector()).Rotator();

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* Spawned = World->SpawnActor<AActor>(SpawnClass, Location, Rotation, Params);
	if (ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(Spawned))
	{
		Placed->ConfigureFromItem(ActiveDefinition->ItemId, ActiveDefinition);
	}

	CancelPlacement();
	return true;
}

void USlimePlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (ActiveDefinition)
	{
		UpdatePreview();

		APawn* Pawn = Cast<APawn>(GetOwner());
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (PC)
		{
			if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) || PC->WasInputKeyJustPressed(EKeys::Enter))
			{
				ConfirmPlacement();
			}
			else if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton) || PC->WasInputKeyJustPressed(EKeys::Escape))
			{
				CancelPlacement();
			}
		}
	}
}

bool USlimePlacementComponent::IsFlatGround(const FHitResult& Hit, float MaxSlopeDegrees) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}
	const float CosMax = FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
	return FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), FVector::UpVector) >= CosMax;
}

void USlimePlacementComponent::UpdatePreview()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !PC || !World || !ActiveDefinition)
	{
		bPlacementValid = false;
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector End = CamLoc + CamRot.Vector() * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimePlaceTrace), false, GetOwner());
	if (PreviewActor)
	{
		Params.AddIgnoredActor(PreviewActor);
	}

	const bool bHit = World->LineTraceSingleByChannel(LastHit, CamLoc, End, ECC_Visibility, Params);
	bPlacementValid = bHit && IsFlatGround(LastHit, ActiveDefinition->MaxSlopeDegrees);

	if (PreviewActor)
	{
		if (bHit)
		{
			PreviewActor->SetActorLocation(LastHit.ImpactPoint);
			const FRotator Rot = FRotationMatrix::MakeFromZX(LastHit.ImpactNormal, Pawn->GetActorForwardVector()).Rotator();
			PreviewActor->SetActorRotation(Rot);
			PreviewActor->SetGroundHit(true, LastHit.ImpactPoint, LastHit.ImpactNormal);
		}
		else
		{
			PreviewActor->SetGroundHit(false, FVector::ZeroVector, FVector::UpVector);
		}
		PreviewActor->SetValidPlacement(bPlacementValid);
		PreviewActor->SetActorHiddenInGame(!bHit);
	}
}
