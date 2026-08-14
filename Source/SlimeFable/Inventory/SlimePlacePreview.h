// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimePlacePreview.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class SLIMEFABLE_API ASlimePlacePreview : public AActor
{
	GENERATED_BODY()

public:
	ASlimePlacePreview();

	virtual void BeginPlay() override;

	void SetPreviewMesh(UStaticMesh* InMesh);
	void SetValidPlacement(bool bValid);
	void SetGroundHit(bool bHasHit, const FVector& ImpactPoint, const FVector& ImpactNormal);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Flat disk on the ground: green valid / red invalid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> GroundDisk;

protected:
	void ApplyDiskColor(bool bValid);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiskMID;

	bool bLastValid = false;
};
