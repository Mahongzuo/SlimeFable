#pragma once

#include "CoreMinimal.h"
#include "WfcTypes.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

/** +X North, +Y East, -X South, -Y West. Bit0=N, Bit1=E, Bit2=S, Bit3=W. */
UENUM(BlueprintType)
enum class EWfcTopology : uint8
{
	DeadEnd UMETA(DisplayName = "DeadEnd"),
	Corner UMETA(DisplayName = "Corner"),
	Straight UMETA(DisplayName = "Straight"),
	Tee UMETA(DisplayName = "Tee"),
	Cross UMETA(DisplayName = "Cross")
};

UENUM(BlueprintType)
enum class EWfcShellPreset : uint8
{
	Open UMETA(DisplayName = "Open"),
	Narrow UMETA(DisplayName = "Narrow"),
	Wide UMETA(DisplayName = "Wide"),
	Tight UMETA(DisplayName = "Tight"),
	ColumnTurn UMETA(DisplayName = "ColumnTurn"),
	Pillared UMETA(DisplayName = "Pillared"),
	Alcove UMETA(DisplayName = "Alcove"),
	Plain UMETA(DisplayName = "Plain"),
	Apse UMETA(DisplayName = "Apse")
};

UENUM(BlueprintType)
enum class EWfcTint : uint8
{
	Warm UMETA(DisplayName = "Warm"),
	Cool UMETA(DisplayName = "Cool"),
	Plaster UMETA(DisplayName = "Plaster")
};

UENUM(BlueprintType)
enum class EWfcHeight : uint8
{
	Standard UMETA(DisplayName = "Standard"),
	Tall UMETA(DisplayName = "Tall")
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcShellChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell")
	EWfcShellPreset Preset = EWfcShellPreset::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell")
	EWfcTint Tint = EWfcTint::Warm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell")
	EWfcHeight Height = EWfcHeight::Standard;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcTileDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tile")
	EWfcTopology Topology = EWfcTopology::Cross;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tile",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "0/90/180/270，顺时针转 Socket。"))
	int32 Rotation90 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tile",
		meta = (ToolTip = "Bit0=+X北 Open，Bit1=+Y东，Bit2=-X南，Bit3=-Y西。"))
	uint8 SocketMask = 0x0F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tile",
		meta = (ClampMin = "0.0", ToolTip = "坍缩权重。死胡同宜小，十字宜大。"))
	float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcCellResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config")
	int32 TileIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config")
	EWfcTopology Topology = EWfcTopology::Cross;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config")
	int32 Rotation90 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config")
	uint8 SocketMask = 0x0F;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcShellPresetDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ToolTip = "预设显示名，例如 Narrow。"))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell")
	EWfcShellPreset Preset = EWfcShellPreset::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ToolTip = "哪些拓扑可以抽到这个壳体。空=不限制。"))
	TArray<EWfcTopology> AllowedTopologies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ClampMin = "0.0", ToolTip = "相对权重。同拓扑内归一化。"))
	float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "Deferred Decal 材质。"))
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "true=偏贴墙（裂纹/符号），false=偏贴地（污渍）。"))
	bool bPreferWall = false;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcPropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Props",
		meta = (ToolTip = "优先刷这个 Actor 类。空则改用 StaticMesh 灰盒。"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Props",
		meta = (ToolTip = "没填 ActorClass 时刷 StaticMeshActor。例如引擎 Cube。"))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Props",
		meta = (ClampMin = "0.0", ToolTip = "同池内相对权重。"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Props",
		meta = (ToolTip = "空=该房间类型都能抽到。"))
	TArray<EWfcTopology> AllowedTopologies;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FWfcRoomDressing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "这套数量/池只作用于该拓扑。"))
	EWfcTopology Topology = EWfcTopology::Cross;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格 Decal 最少张数。再乘生成器 DecalDensity。"))
	int32 DecalCountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格 Decal 最多张数。"))
	int32 DecalCountMax = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "该拓扑 Decal 池。空则回退 TileSet.Decals。"))
	TArray<FWfcDecalEntry> Decals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "地上 Actor 最少个数。生成器关掉 SpawnProps 则不刷。"))
	int32 PropCountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "地上 Actor 最多个数。"))
	int32 PropCountMax = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "该拓扑物品池。空则回退 TileSet.PropEntries。"))
	TArray<FWfcPropEntry> Props;
};

namespace WfcTags
{
	inline const FName Generated()
	{
		return FName(TEXT("WfcGenerated"));
	}

	inline const FName GeneratedFolder()
	{
		return FName(TEXT("WFC_Generated"));
	}
}

namespace WfcMath
{
	inline constexpr int32 DirNorth = 0;
	inline constexpr int32 DirEast = 1;
	inline constexpr int32 DirSouth = 2;
	inline constexpr int32 DirWest = 3;

	inline FIntPoint DirToOffset(int32 Dir)
	{
		switch (Dir & 3)
		{
		case DirNorth: return FIntPoint(1, 0);
		case DirEast: return FIntPoint(0, 1);
		case DirSouth: return FIntPoint(-1, 0);
		default: return FIntPoint(0, -1);
		}
	}

	inline int32 OppositeDir(int32 Dir)
	{
		return (Dir + 2) & 3;
	}

	inline uint8 RotateMask(uint8 Mask, int32 Rotation90)
	{
		int32 Rot = ((Rotation90 % 4) + 4) % 4;
		uint8 Result = static_cast<uint8>(Mask & 0x0F);
		for (int32 i = 0; i < Rot; ++i)
		{
			Result = static_cast<uint8>(((Result << 1) & 0x0F) | ((Result >> 3) & 0x01));
		}
		return Result;
	}

	inline bool SocketOpen(uint8 Mask, int32 Dir)
	{
		return (Mask & (1 << (Dir & 3))) != 0;
	}

	inline uint8 CanonicalMask(EWfcTopology Topology)
	{
		switch (Topology)
		{
		case EWfcTopology::DeadEnd: return 0x01;
		case EWfcTopology::Corner: return 0x03;
		case EWfcTopology::Straight: return 0x05;
		case EWfcTopology::Tee: return 0x07;
		default: return 0x0F;
		}
	}

	inline int32 FloorDiv(int32 Value, int32 Divisor)
	{
		check(Divisor > 0);
		return FMath::FloorToInt32(static_cast<float>(Value) / static_cast<float>(Divisor));
	}

	inline uint32 CellHash(int32 Seed, int32 CellX, int32 CellY, int32 Salt = 0)
	{
		uint32 H = GetTypeHash(Seed);
		H ^= GetTypeHash(CellX) * 73856093u;
		H ^= GetTypeHash(CellY) * 19349663u;
		H ^= GetTypeHash(Salt) * 83492791u;
		return H;
	}

	SLIMEFABLE_API void BuildStandardTiles(TArray<FWfcTileDef>& OutTiles);
	SLIMEFABLE_API bool EdgeOpen(int32 WorldSeed, int32 CellX, int32 CellY, int32 Dir);
	SLIMEFABLE_API bool BorderSocketOpen(
		int32 WorldSeed,
		int32 CellX,
		int32 CellY,
		int32 Dir,
		int32 ChunkOriginX,
		int32 ChunkOriginY,
		int32 ChunkSize);
	SLIMEFABLE_API FWfcShellChoice ChooseShell(
		EWfcTopology Topology,
		int32 WorldSeed,
		int32 CellX,
		int32 CellY,
		bool bAllowTall,
		const TArray<FWfcShellPresetDef>& Presets);
	SLIMEFABLE_API void ApplyLayoutChaos(TArray<FWfcTileDef>& Tiles, float Chaos);
}
