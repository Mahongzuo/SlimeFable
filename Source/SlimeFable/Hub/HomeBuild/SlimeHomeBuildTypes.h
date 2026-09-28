// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeHomeBuildTypes.generated.h"

class UStaticMesh;
class UTexture2D;
class AActor;

UENUM(BlueprintType)
enum class ESlimeHomeBuildCategory : uint8
{
	Block UMETA(DisplayName = "方块"),
	Ground UMETA(DisplayName = "地表"),
	Item UMETA(DisplayName = "物品"),
	Plant UMETA(DisplayName = "植物"),
	Rock UMETA(DisplayName = "岩石"),
	Crystal UMETA(DisplayName = "水晶"),
	Sky UMETA(DisplayName = "天空"),
	Bag UMETA(DisplayName = "背包"),
	Fluid UMETA(DisplayName = "流体")
};

USTRUCT(BlueprintType)
struct FSlimeHomeBuildEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	FName EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	ESlimeHomeBuildCategory Category = ESlimeHomeBuildCategory::Block;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TSoftObjectPtr<UTexture2D> FamilyIcon;

	/** Shown on the icon when several meshes share one picture. 0 = hide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	int32 VariantIndex = 0;

	/** Same scale for every KUBIKOS mesh so a cube edge is 50 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	float UniformScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	int32 FootprintX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	int32 FootprintY = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	float HeightCm = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	bool bFluidPad = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TSoftClassPtr<AActor> FluidClass;
};

USTRUCT(BlueprintType)
struct FSlimeHomeBuildRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FName EntryId;

	UPROPERTY()
	bool bFromBag = false;

	UPROPERTY()
	int32 AnchorX = 0;

	UPROPERTY()
	int32 AnchorY = 0;

	UPROPERTY()
	float BaseZ = 0.f;

	UPROPERTY()
	int32 YawSteps = 0;

	/** Pitch in degrees. Old saves stay flat. */
	UPROPERTY()
	float PitchDegrees = 0.f;

	/** Multiplier on the authored mesh scale. Fluids ignore this. Old saves stay at 1. */
	UPROPERTY()
	float UserScale = 1.f;

	UPROPERTY()
	bool bFluid = false;
};
