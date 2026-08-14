// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSliceUtil.h"

#include "Kismet/GameplayStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"

bool USlimeSliceUtil::IsMeshLargeEnoughToSlice(const UProceduralMeshComponent* Mesh, float MinSliceExtent)
{
	if (!Mesh)
	{
		return false;
	}
	const FBoxSphereBounds Bounds = Mesh->CalcLocalBounds();
	const FVector Extent = Bounds.BoxExtent;
	const float Shortest = FMath::Min3(Extent.X, Extent.Y, Extent.Z) * 2.f;
	return Shortest >= MinSliceExtent;
}

void USlimeSliceUtil::PrepareSlicedMeshPhysics(UProceduralMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
}

bool USlimeSliceUtil::SliceProceduralMeshAt(
	UProceduralMeshComponent* TargetMesh,
	FVector PlanePosition,
	FVector PlaneNormal,
	UMaterialInterface* CapMaterial,
	USoundBase* SliceSound,
	float MinSliceExtent,
	float SliceImpulse)
{
	if (!TargetMesh || !IsMeshLargeEnoughToSlice(TargetMesh, MinSliceExtent))
	{
		return false;
	}

	FVector Normal = PlaneNormal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		AActor* Owner = TargetMesh->GetOwner();
		Normal = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	}

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		TargetMesh,
		PlanePosition,
		Normal,
		true,
		OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		CapMaterial);

	PrepareSlicedMeshPhysics(TargetMesh);
	if (OtherHalf)
	{
		PrepareSlicedMeshPhysics(OtherHalf);
		const FVector Impulse = Normal * SliceImpulse + FVector::UpVector * (SliceImpulse * 0.35f);
		OtherHalf->AddImpulseAtLocation(Impulse, PlanePosition);
		TargetMesh->AddImpulseAtLocation(-Impulse * 0.6f, PlanePosition);
	}

	if (SliceSound)
	{
		if (AActor* Owner = TargetMesh->GetOwner())
		{
			UGameplayStatics::PlaySoundAtLocation(Owner, SliceSound, Owner->GetActorLocation());
		}
	}
	return true;
}
