// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/SlimeItemDefinition.h"
#include "SlimeElementTypes.h"

class UStaticMesh;
class UTexture2D;

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
UENUM(BlueprintType)
enum class ESlimeCropCategory : uint8
{
	Vegetable UMETA(DisplayName = "蔬菜"),
	Fruit UMETA(DisplayName = "水果"),
	Grain UMETA(DisplayName = "谷物"),
	Flower UMETA(DisplayName = "花草"),
	Cash UMETA(DisplayName = "经济作物")
};

/** One growth band. Meshes in the array are shape variants, not later stages. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeCropStage
{
	GENERATED_BODY()

	/** Growth01 where this band begins. The next band's start ends it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartGrowth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TArray<TSoftObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.05"))
	float ScaleFrom = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.05"))
	float ScaleTo = 1.f;
};

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

	/** Real seconds from sprout to mature at rate 1. Clamped to 30 minutes in the growth rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "10.0", ClampMax = "1800.0", Units = "s"))
	float GrowSeconds = 600.f;

	/** Phrase placed after 现实里, such as 大约三个月 or 要等两三年以上. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	FText RealWorldSpan;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	ESlimeCropCategory Category = ESlimeCropCategory::Vegetable;

	/** Ordered growth bands. Empty falls back to the procedural plant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TArray<FSlimeCropStage> Stages;

	/** Shown after a regrowing crop is picked, until it reaches the last stage again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TArray<TSoftObjectPtr<UStaticMesh>> HarvestedMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	bool bRegrowAfterHarvest = false;

	/** Growth01 the crop returns to when it regrows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float RegrowFromGrowth = 0.35f;

	/** How many plants to try along each side. The bed shrinks this if the soil is narrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PlantGrid = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "0.2"))
	float SizeMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = "0.2"))
	float SizeMax = 1.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Loose produce mesh, used for the harvest pop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crop")
	TSoftObjectPtr<UStaticMesh> ProduceMesh;

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

	/** 0 on old saves. The plot then derives a stable seed from PlotId. */
	UPROPERTY()
	int32 PlantSeed = 0;

	/** Regrowing crops show the harvested mesh until they reach the last stage again. */
	UPROPERTY()
	bool bHarvestedLook = false;
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
