// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSliceableComponent.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USoundBase;

/**
 * Attach to actors that already own a ProceduralMesh (e.g. BP_watermelon_slice)
 * so slime combat hits can slice without reparenting the Blueprint.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeSliceableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeSliceableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	TObjectPtr<UMaterialInterface> CapMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	TObjectPtr<USoundBase> SliceSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice", meta = (ClampMin = "1.0", Units = "cm"))
	float MinSliceExtent = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	float SliceImpulse = 220.f;

	void SliceAt(FVector PlanePosition, FVector PlaneNormal, UProceduralMeshComponent* MeshToSlice);
};
