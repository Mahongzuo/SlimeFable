// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSliceableComponent.h"

#include "ProceduralMeshComponent.h"
#include "SlimeSliceUtil.h"

USlimeSliceableComponent::USlimeSliceableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeSliceableComponent::SliceAt(
	FVector PlanePosition,
	FVector PlaneNormal,
	UProceduralMeshComponent* MeshToSlice)
{
	AActor* Owner = GetOwner();
	UProceduralMeshComponent* TargetMesh = MeshToSlice;
	if (!TargetMesh && Owner)
	{
		TargetMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
	}
	USlimeSliceUtil::SliceProceduralMeshAt(
		TargetMesh,
		PlanePosition,
		PlaneNormal,
		CapMaterial,
		SliceSound,
		MinSliceExtent,
		SliceImpulse);
}
