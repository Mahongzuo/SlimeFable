// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeSliceable.h"
#include "SlimeSliceableActor.generated.h"

class UProceduralMeshComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * Placeable prop that copies a static mesh into a procedural mesh and can be
 * sliced repeatedly by slime attacks via ISlimeSliceable.
 */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimeSliceableActor : public AActor, public ISlimeSliceable
{
	GENERATED_BODY()

public:
	ASlimeSliceableActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual void SliceAt_Implementation(FVector PlanePosition, FVector PlaneNormal, UProceduralMeshComponent* MeshToSlice) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slice")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Source mesh copied into ProceduralMesh on construction / BeginPlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slice")
	TObjectPtr<UStaticMeshComponent> SourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slice")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	TObjectPtr<UMaterialInterface> CapMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	TObjectPtr<USoundBase> SliceSound;

	/** Skip slicing when the shortest local bounds axis is below this (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice", meta = (ClampMin = "1.0", Units = "cm"))
	float MinSliceExtent = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slice")
	float SliceImpulse = 220.f;

private:
	void EnsureProceduralFromSource();

	bool bMeshCopied = false;
};
