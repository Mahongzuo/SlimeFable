// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/SlimeItemDefinition.h"
#include "SlimeElementTypes.h"
#include "SlimeCropTypes.generated.h"

UENUM(BlueprintType)
enum class ESlimeFarmPlotState : uint8
{
	Empty UMETA(DisplayName = "空地"),
	Growing UMETA(DisplayName = "生长中"),
	Mature UMETA(DisplayName = "成熟"),
	Withered UMETA(DisplayName = "枯萎")
};

UENUM(BlueprintType)
enum class ESlimeCropQuality : uint8
{
	Common UMETA(DisplayName = "普通"),
	Fine UMETA(DisplayName = "优良"),
	Mutated UMETA(DisplayName = "变异")
};

/** Procedural silhouette for one crop. Units are centimeters. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimePlantShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "10.0", Units = "cm"))
	float StemHeight = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1.0", Units = "cm"))
	float StemRadius = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float StemBend = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 LeafCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "5.0", Units = "cm"))
	float LeafLength = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	int32 PetalLayers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "3"))
	int32 PetalsPerLayer = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.0"))
	float FruitScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FLinearColor StemColor = FLinearColor(0.18f, 0.45f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FLinearColor LeafColor = FLinearColor(0.12f, 0.55f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FLinearColor BloomColor = FLinearColor(0.95f, 0.75f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FLinearColor FruitColor = FLinearColor(0.85f, 0.15f, 0.1f);
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeCropDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FName CropId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	bool bFlower = false;

	/** Night-blooming crops grow faster after dusk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	bool bPrefersNight = false;

	/** Real seconds from sprout to mature at rate 1. Default is about one 10 minute day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "10.0", Units = "s"))
	float GrowSeconds = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	ESlimeElement PreferredElement = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FName YieldItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TSoftObjectPtr<USlimeConsumableDefinition> YieldItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1"))
	int32 YieldMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1"))
	int32 YieldMax = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeedReturnChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FSlimePlantShape Shape;

	FName ResolveYieldItemId() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SlimeCrop"), CropId.IsNone() ? GetFName() : CropId);
	}
};

USTRUCT()
struct FSlimeFarmPlotRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName PlotId = NAME_None;

	UPROPERTY()
	FName CropId = NAME_None;

	UPROPERTY()
	ESlimeFarmPlotState State = ESlimeFarmPlotState::Empty;

	UPROPERTY()
	ESlimeCropQuality Quality = ESlimeCropQuality::Common;

	UPROPERTY()
	float Growth01 = 0.f;

	UPROPERTY()
	float Moisture = 0.4f;

	UPROPERTY()
	float Fertility = 0.35f;

	UPROPERTY()
	float HueShift = 0.f;

	UPROPERTY()
	int64 LastUpdateUnix = 0;
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeSeedDefinition : public USlimeItemDefinition
{
	GENERATED_BODY()

public:
	USlimeSeedDefinition()
	{
		Category = ESlimeItemCategory::Seed;
		MaxStack = 99;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seed")
	FName CropId = NAME_None;
};
