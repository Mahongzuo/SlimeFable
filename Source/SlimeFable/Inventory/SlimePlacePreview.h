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

	/** Thin cylinder from the slime to the aim point, plus a disk on the hit. */
	void SetAimVisual(const FVector& Start, const FVector& End, const FLinearColor& Color, bool bShow);

	/** Flat green or red rectangle for a fluid footprint. Hides the schematic mesh. */
	void SetFootprintHighlight(bool bShow, float SizeXcm, float SizeYcm, bool bValid);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Flat disk on the ground: green valid / red invalid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> GroundDisk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> AimBeam;

	/** Translucent rectangle covering a fluid's placeable cells. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> Footprint;

protected:
	void ApplyDiskColor(bool bValid);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiskMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FootprintMID;

	bool bLastValid = false;
};
