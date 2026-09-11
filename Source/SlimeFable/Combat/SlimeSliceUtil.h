// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimeSliceUtil.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USoundBase;

UCLASS()
class SLIMEFABLE_API USlimeSliceUtil : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool SliceProceduralMeshAt(
		UProceduralMeshComponent* TargetMesh,
		FVector PlanePosition,
		FVector PlaneNormal,
		UMaterialInterface* CapMaterial,
		USoundBase* SliceSound,
		float MinSliceExtent = 12.f,
		float SliceImpulse = 220.f);

	static void PrepareSlicedMeshPhysics(UProceduralMeshComponent* Mesh);
	static void EnableSimpleConvexCollision(UProceduralMeshComponent* Mesh);
	static bool IsMeshLargeEnoughToSlice(const UProceduralMeshComponent* Mesh, float MinSliceExtent);

	/** Repeated random plane cuts until PieceMin–PieceMax fragments remain. */
	static bool ShatterProceduralMesh(
		UProceduralMeshComponent* RootMesh,
		UMaterialInterface* CapMaterial,
		USoundBase* SliceSound,
		int32 PieceMin = 3,
		int32 PieceMax = 5,
		float MinSliceExtent = 12.f,
		float SliceImpulse = 220.f);
};
