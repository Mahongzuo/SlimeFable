#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "SlimePCGTypes.generated.h"

UENUM(BlueprintType)
enum class ESlimePCGBiomeType : uint8
{
	Wilderness UMETA(DisplayName = "Wilderness"),
	City UMETA(DisplayName = "City"),
	Military UMETA(DisplayName = "Military"),
	Cave UMETA(DisplayName = "Cave"),
	Interior UMETA(DisplayName = "Interior")
};

/** One HQ mesh library for a sandbox / generated PCG map. Sample lowpoly meshes must not be listed here. */
UCLASS(BlueprintType)
class SLIMEFABLE_API USlimePCGMeshLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Library",
		meta = (ToolTip = "库显示名，例如 Wilderness Trees。只放项目高模，不要放 PCGBiomeSample 的 lowpoly。"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Library",
		meta = (ToolTip = "主层 Mesh（树、建筑、机库、洞穴地板等），PCG Static Mesh Spawner 按权重抽取。"))
	TArray<TSoftObjectPtr<UStaticMesh>> PrimaryMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Library",
		meta = (ToolTip = "Child / 点缀层（灌木、碎石、家具）。官方 Biome Child Asset 换皮时用这组。"))
	TArray<TSoftObjectPtr<UStaticMesh>> SecondaryMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Library",
		meta = (ToolTip = "地面层（草、落叶、桶箱）。对应 GroundScatter 或第三 Generator。"))
	TArray<TSoftObjectPtr<UStaticMesh>> GroundMeshes;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SlimePCGMeshLibrary"), GetFName());
	}
};

/**
 * Editor recipe: a Chinese prompt maps to one of the five presets + seed.
 * Generated maps land in /Game/_Slime/SlimePCG/Generated; you place them into daily SL_* yourself.
 */
UCLASS(BlueprintType)
class SLIMEFABLE_API USlimePCGLevelRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Prompt",
		meta = (MultiLine = "true", ToolTip = "中文需求描述。Agent 根据这句话选 BiomeType / SubStyle，不在运行时跑 LLM。"))
	FText Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Prompt",
		meta = (ToolTip = "五种预设之一。Wilderness/City/Military 走 Landscape+Biome Core；Cave/Interior 走模数 PCG。"))
	ESlimePCGBiomeType BiomeType = ESlimePCGBiomeType::Wilderness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Prompt",
		meta = (ToolTip = "子风格关键字，例如 Park、Tokyo、Utopia、Facility。空则用该类型默认主包。"))
	FName SubStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Generation",
		meta = (ToolTip = "PCG 种子。同一 Prompt+Seed 必须可复现。0 表示用资产名哈希。"))
	int32 Seed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Generation",
		meta = (ClampMin = "0.05", ClampMax = "4.0", ToolTip = "相对预设的密度倍率。1 为沙盒默认。"))
	float DensityMul = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Generation",
		meta = (ClampMin = "0", ClampMax = "16", ToolTip = "预留给任务 Actor 的 POI 空地数量。沙盒阶段可先为 0。"))
	int32 PoiCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Assets",
		meta = (ToolTip = "高模库。必须指向 /Game/_Slime/SlimePCG/Data/Libraries，不要用官方 Sample Mesh。"))
	TSoftObjectPtr<USlimePCGMeshLibrary> MeshLibrary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Output",
		meta = (AllowedClasses = "/Script/Engine.World",
			ToolTip = "烤出来的关卡。默认写到 /Game/_Slime/SlimePCG/Generated。不要写进 Maps/Days。"))
	TSoftObjectPtr<UWorld> GeneratedLevel;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SlimePCGLevelRecipe"), GetFName());
	}
};
