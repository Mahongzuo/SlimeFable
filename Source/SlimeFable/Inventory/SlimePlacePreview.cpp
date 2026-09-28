// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlacePreview.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
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

	AimBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimBeam"));
	AimBeam->SetupAttachment(RootComponent);
	AimBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimBeam->SetCastShadow(false);
	AimBeam->SetHiddenInGame(true);
	if (CylinderMesh.Succeeded())
	{
		AimBeam->SetStaticMesh(CylinderMesh.Object);
	}

	Footprint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Footprint"));
	Footprint->SetupAttachment(RootComponent);
	Footprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Footprint->SetCastShadow(false);
	Footprint->SetHiddenInGame(true);
	if (CubeMesh.Succeeded())
	{
		Footprint->SetStaticMesh(CubeMesh.Object);
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
	if (!Mesh || Mesh->GetStaticMesh() == InMesh)
	{
		return;
	}
	// SetStaticMesh keeps override materials. A tint from the previous block would hide the new one.
	Mesh->EmptyOverrideMaterials();
	Mesh->SetOverlayMaterial(nullptr);
	Mesh->SetStaticMesh(InMesh);
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
	Mesh->SetVisibility(true);
	if (bValid)
	{
		Mesh->SetOverlayMaterial(nullptr);
	}
	else if (UMaterialInterface* Overlay = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Materials/M_PickupOutline.M_PickupOutline")))
	{
		Mesh->SetOverlayMaterial(Overlay);
	}
	ApplyDiskColor(bValid);
}

void ASlimePlacePreview::SetAimVisual(const FVector& Start, const FVector& End, const FLinearColor& Color, bool bShow)
{
	if (!AimBeam)
	{
		return;
	}
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (!bShow || Length < 1.f)
	{
		AimBeam->SetHiddenInGame(true);
		if (GroundDisk)
		{
			GroundDisk->SetHiddenInGame(true);
		}
		return;
	}

	AimBeam->SetHiddenInGame(false);
	AimBeam->SetWorldLocation((Start + End) * 0.5f);
	AimBeam->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
	// Engine cylinder is 100cm tall and 50cm in radius. Keep the beam about 3cm across.
	AimBeam->SetWorldScale3D(FVector(0.03f, 0.03f, Length / 100.f));
	if (!BeamMID)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Materials/M_BuildAimBeam.M_BuildAimBeam")))
		{
			BeamMID = UMaterialInstanceDynamic::Create(Mat, this);
			AimBeam->SetMaterial(0, BeamMID);
		}
	}
	if (BeamMID)
	{
		BeamMID->SetVectorParameterValue(TEXT("Color"), Color);
		BeamMID->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
	}
	if (GroundDisk)
	{
		GroundDisk->SetHiddenInGame(false);
		GroundDisk->SetWorldLocation(End + FVector(0.f, 0.f, 2.f));
		GroundDisk->SetWorldRotation(FRotator::ZeroRotator);
		GroundDisk->SetWorldScale3D(FVector(0.45f, 0.45f, 0.04f));
		ApplyDiskColor(Color.R < 0.5f);
	}
}

void ASlimePlacePreview::SetFootprintHighlight(bool bShow, float SizeXcm, float SizeYcm, bool bValid)
{
	if (!Footprint)
	{
		return;
	}
	if (!bShow)
	{
		Footprint->SetHiddenInGame(true);
		return;
	}
	if (Mesh)
	{
		Mesh->SetVisibility(false, false);
	}
	if (GroundDisk)
	{
		GroundDisk->SetHiddenInGame(true);
	}
	Footprint->SetHiddenInGame(false);
	Footprint->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 4.f));
	Footprint->SetWorldRotation(FRotator::ZeroRotator);
	Footprint->SetWorldScale3D(FVector(
		FMath::Max(SizeXcm, 10.f) / 100.f,
		FMath::Max(SizeYcm, 10.f) / 100.f,
		0.06f));
	if (!FootprintMID)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Materials/M_PlaceGroundDisk.M_PlaceGroundDisk")))
		{
			Footprint->SetMaterial(0, Mat);
		}
		FootprintMID = Footprint->CreateAndSetMaterialInstanceDynamic(0);
	}
	const FLinearColor Color = bValid
		? FLinearColor(0.15f, 0.95f, 0.35f, 0.55f)
		: FLinearColor(0.95f, 0.18f, 0.12f, 0.55f);
	if (FootprintMID)
	{
		FootprintMID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		FootprintMID->SetVectorParameterValue(TEXT("Color"), Color);
		FootprintMID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 1.5f);
		FootprintMID->SetScalarParameterValue(TEXT("Opacity"), Color.A);
	}
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
