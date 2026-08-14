// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlacePreview.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASlimePlacePreview::ASlimePlacePreview()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	GroundDisk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundDisk"));
	GroundDisk->SetupAttachment(RootComponent);
	GroundDisk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundDisk->SetCastShadow(false);
	GroundDisk->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.35f));
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		GroundDisk->SetStaticMesh(CylinderMesh.Object);
		// Flat disc ~1.2m diameter, 4cm thick.
		GroundDisk->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.04f));
	}
}

void ASlimePlacePreview::BeginPlay()
{
	Super::BeginPlay();
	if (GroundDisk && !DiskMID)
	{
		if (UMaterialInterface* DiskMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Materials/M_PlaceGroundDisk.M_PlaceGroundDisk")))
		{
			GroundDisk->SetMaterial(0, DiskMat);
		}
	}
}

void ASlimePlacePreview::SetPreviewMesh(UStaticMesh* InMesh)
{
	if (Mesh && InMesh)
	{
		Mesh->SetStaticMesh(InMesh);
	}
}

void ASlimePlacePreview::ApplyDiskColor(bool bValid)
{
	if (!GroundDisk)
	{
		return;
	}
	if (!DiskMID)
	{
		DiskMID = GroundDisk->CreateAndSetMaterialInstanceDynamic(0);
	}
	const FLinearColor Color = bValid
		? FLinearColor(0.15f, 0.95f, 0.35f, 0.75f)
		: FLinearColor(0.95f, 0.18f, 0.12f, 0.75f);
	if (DiskMID)
	{
		DiskMID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		DiskMID->SetVectorParameterValue(TEXT("Color"), Color);
		DiskMID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 1.5f);
		DiskMID->SetScalarParameterValue(TEXT("Opacity"), Color.A);
	}
}

void ASlimePlacePreview::SetValidPlacement(bool bValid)
{
	bLastValid = bValid;
	if (!Mesh)
	{
		return;
	}
	const FLinearColor Color = bValid
		? FLinearColor(0.25f, 0.75f, 0.35f, 0.45f)
		: FLinearColor(0.85f, 0.2f, 0.15f, 0.45f);
	Mesh->SetVisibility(true);
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Dyn = Mesh->CreateAndSetMaterialInstanceDynamic(Index))
		{
			Dyn->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Dyn->SetVectorParameterValue(TEXT("Color"), Color);
			Dyn->SetScalarParameterValue(TEXT("Opacity"), Color.A);
		}
	}
	Mesh->SetOverlayMaterial(nullptr);
	ApplyDiskColor(bValid);
}

void ASlimePlacePreview::SetGroundHit(bool bHasHit, const FVector& ImpactPoint, const FVector& ImpactNormal)
{
	if (!GroundDisk)
	{
		return;
	}
	if (!bHasHit)
	{
		GroundDisk->SetHiddenInGame(true);
		return;
	}

	GroundDisk->SetHiddenInGame(false);
	ApplyDiskColor(bLastValid);

	const FVector Normal = ImpactNormal.GetSafeNormal();
	const FVector Location = ImpactPoint + Normal * 2.f;
	const FRotator Rotation = FRotationMatrix::MakeFromZX(Normal, FVector::ForwardVector).Rotator();
	GroundDisk->SetWorldLocation(Location);
	GroundDisk->SetWorldRotation(Rotation);
}
