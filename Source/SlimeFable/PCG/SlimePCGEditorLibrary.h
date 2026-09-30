#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimePCGEditorLibrary.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterialInterface;

/** Editor helpers for SlimePCG sandbox maps. Python cannot spawn a real Landscape; use this instead. */
UCLASS()
class SLIMEFABLE_API USlimePCGEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawn a centered Landscape (8x8 components, 63 quads, 100uu scale) with CityPark MI_Landscape
	 * and randomly painted weight layers (grass / dirt / sand / stone).
	 */
	UFUNCTION(BlueprintCallable, Category = "0_Config|SlimePCG",
		meta = (WorldContext = "WorldContextObject",
			ToolTip = "生成 8×8 Landscape，挂 CityPark MI_Landscape，并把分层（草/土/沙/石）随机铺到权重图上。Seed 控制噪声。"))
	static ALandscape* CreateFlatLandscape(
		UObject* WorldContextObject,
		int32 ComponentCountX = 8,
		int32 ComponentCountY = 8,
		UMaterialInterface* Material = nullptr,
		int32 Seed = 7);

	/**
	 * 16x16 component landscape, about 1008m on a side, centered on the origin.
	 * The inner 150m stays flat. HoleMin/HoleMax (world cm) become a visibility hole so the
	 * museum courtyard is not covered by a second ground plane.
	 * LayerInfos order is grass, soil, rock.
	 */
	UFUNCTION(BlueprintCallable, Category = "0_Config|SlimePCG",
		meta = (WorldContext = "WorldContextObject",
			ToolTip = "在当前关卡生成约 1 公里的家园地形。中心 150 米平整，院子范围挖洞，避免和博物馆地面重叠。"))
	static ALandscape* CreateHomeLandscape(
		UObject* WorldContextObject,
		UMaterialInterface* Material,
		const TArray<ULandscapeLayerInfoObject*>& LayerInfos,
		FVector2D HoleMin,
		FVector2D HoleMax);

	/**
	 * Paint the existing HomeLandscape weight maps so Ground, Rocks and Grass
	 * each cover about a third of the surface. Height and the courtyard hole stay.
	 */
	UFUNCTION(BlueprintCallable, Category = "0_Config|SlimePCG",
		meta = (WorldContext = "WorldContextObject",
			ToolTip = "把已有家园地形的 Ground、Rocks、Grass 三层按大致相等的面积铺开。不改高度，也不动院子挖洞。"))
	static int32 PaintHomeLandscapeLayersEven(UObject* WorldContextObject);
};
