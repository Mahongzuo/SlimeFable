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
	Fluid UMETA(DisplayName = "流体"),
	Farm UMETA(DisplayName = "农田"),
	Fence UMETA(DisplayName = "栅栏"),
	NPC UMETA(DisplayName = "NPC")
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

	/** When set, the placed record spawns this actor instead of an instanced mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TSoftClassPtr<AActor> ActorClass;

	/** Spawn an ASlimeFarmPlot. The mesh is the soil or planter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	bool bFarmPlot = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="0_Config", meta=(ToolTip="NPC种类键，普通建筑留空；运行时从NPC目录补齐。"))
	FName NpcSpeciesId;
};

USTRUCT(BlueprintType)
struct FSlimeHomeBuildRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	/** Empty for all legacy building records. NPCs store their placement anchor, not moving position. */
	UPROPERTY()
	FName NpcSpeciesId;

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

	/** Pitch in degrees. Old saves stay flat. The pool keeps this at 0. */
	UPROPERTY()
	float PitchDegrees = 0.f;

	/** Mesh scale for ordinary pieces. The pool uses it for length and width only (0.5–2). Other fluids stay at 1. */
	UPROPERTY()
	float UserScale = 1.f;

	/** Pool water depth in centimeters. 0 means the authored 80 cm. Other pieces ignore this. */
	UPROPERTY()
	float PoolDepthCm = 0.f;

	UPROPERTY()
	bool bFluid = false;
};

inline bool SlimeHomeIsPool(FName EntryId)
{
	return EntryId == TEXT("Kub_FluidPool");
}

inline float SlimeHomePoolPlanScale(float UserScale)
{
	const float Raw = UserScale > KINDA_SMALL_NUMBER ? UserScale : 1.f;
	return FMath::Clamp(Raw, 0.5f, 2.f);
}

inline float SlimeHomePoolDepthCm(float StoredCm)
{
	return StoredCm > KINDA_SMALL_NUMBER ? FMath::Clamp(StoredCm, 40.f, 160.f) : 80.f;
}

inline int32 SlimeHomePoolCells(int32 BaseCells, float UserScale)
{
	return FMath::Max(FMath::RoundToInt(FMath::Max(BaseCells, 1) * SlimeHomePoolPlanScale(UserScale)), 1);
}
