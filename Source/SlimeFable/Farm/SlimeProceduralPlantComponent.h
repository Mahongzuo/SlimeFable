// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Farm/SlimeCropTypes.h"
#include "SlimeProceduralPlantComponent.generated.h"

/** Stem, leaves, petals and fruit rebuilt every 5% of growth. */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeProceduralPlantComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	USlimeProceduralPlantComponent(const FObjectInitializer& ObjectInitializer);

	void UpdatePlant(const USlimeCropDefinition* Crop, FName VariationId, float Growth01, float HueShift, ESlimeCropQuality Quality, bool bWithered);

private:
	void Rebuild(const USlimeCropDefinition* Crop, FName VariationId, float Growth01, float HueShift, ESlimeCropQuality Quality, bool bWithered);
	void AppendPlant(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector2D>& UV, TArray<FLinearColor>& Colors,
		const USlimeCropDefinition* Crop, FRandomStream& Stream, float Growth01, float HueShift, ESlimeCropQuality Quality, bool bWithered,
		const FVector& Origin, float YawDegrees, float Scale) const;
	void ApplyPlantMaterial();
	static FLinearColor ShiftHue(FLinearColor Color, float HueShift);
	static void AddTri(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector2D>& UV, TArray<FLinearColor>& Colors,
		const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color);

	int32 BuiltBucket = INDEX_NONE;
	FName BuiltCropId = NAME_None;
	float BuiltHue = 0.f;
	ESlimeCropQuality BuiltQuality = ESlimeCropQuality::Common;
	bool bBuiltWithered = false;
	bool bLoggedMissingMaterial = false;
};
