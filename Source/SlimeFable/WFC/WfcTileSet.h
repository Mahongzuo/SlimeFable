#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WFC/WfcTypes.h"
#include "WfcTileSet.generated.h"

UCLASS(BlueprintType)
class SLIMEFABLE_API UWfcTileSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Tiles",
		meta = (ToolTip = "完备瓦片表。空则运行时用 5 拓扑×旋转默认表。"))
	TArray<FWfcTileDef> Tiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Shell",
		meta = (ToolTip = "同拓扑壳体预设权重。空则用内置 Narrow/Wide/柱/壁龛。"))
	TArray<FWfcShellPresetDef> ShellPresets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Materials",
		meta = (ToolTip = "暖土/冷石/灰泥 地板，顺序对应 EWfcTint。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> FloorByTint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Materials",
		meta = (ToolTip = "暖土/冷石/灰泥 墙，顺序对应 EWfcTint。柱和壁龛也用墙。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> WallByTint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Materials",
		meta = (ToolTip = "暖土/冷石/灰泥 天花。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> CeilingByTint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Materials",
		meta = (ToolTip = "门洞框。空则用墙材质。"))
	TSoftObjectPtr<UMaterialInterface> TrimMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Materials",
		meta = (ToolTip = "吊灯罩自发光材质。"))
	TSoftObjectPtr<UMaterialInterface> LampShadeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Dressing",
		meta = (ToolTip = "全局 Decal 池。某拓扑 RoomDressings.Decals 为空时回退这里。"))
	TArray<FWfcDecalEntry> Decals;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Dressing",
		meta = (ToolTip = "按拓扑配 Decal/Prop 数量与池。空则 EnsureDefaults 填 5 条。"))
	TArray<FWfcRoomDressing> RoomDressings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Props",
		meta = (ToolTip = "全局物品池。某拓扑 RoomDressings.Props 为空时回退这里。"))
	TArray<FWfcPropEntry> PropEntries;

	const TArray<FWfcTileDef>& ResolveTiles() const;

	UFUNCTION(BlueprintCallable, Category = "0_Config")
	void EnsureDefaults();

	const FWfcRoomDressing* FindDressing(EWfcTopology Topology) const;

	UMaterialInterface* ResolveFloor(EWfcTint Tint) const;
	UMaterialInterface* ResolveWall(EWfcTint Tint) const;
	UMaterialInterface* ResolveCeiling(EWfcTint Tint) const;
	UMaterialInterface* ResolveTrim() const;
	UMaterialInterface* ResolveLampShade() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("WfcTileSet"), GetFName());
	}

private:
	mutable TArray<FWfcTileDef> CachedDefaultTiles;
};
