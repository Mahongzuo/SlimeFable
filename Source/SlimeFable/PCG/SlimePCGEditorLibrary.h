#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimePCGEditorLibrary.generated.h"

class ALandscape;
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
};
