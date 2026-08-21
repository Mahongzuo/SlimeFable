/*
* Copyright (c) 2020 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#pragma once

#include "CoreMinimal.h"
#include "CustomResourcePool.h"

#define UE_API DLSS_API

struct FDLSSOptimalSettings;
class FSceneViewFamily;
class NGXRHI;

enum class EDLSSQualityMode
{
	MinValue = -2,
	UltraPerformance = -2,
	Performance = -1,
	Balanced = 0,
	Quality = 1,
	UltraQuality = 2,
	DLAA = 3,
	MaxValue = DLAA,
	NumValues = 6
};

class FDLSSUpscaler final : public ICustomResourcePool
{

	friend class FDLSSModule;
public:
	UE_NONCOPYABLE(FDLSSUpscaler)

	void SetupViewFamily(FSceneViewFamily& ViewFamily);

	UE_API float GetOptimalResolutionFractionForQuality(EDLSSQualityMode Quality) const;

	UE_API float GetMinResolutionFractionForQuality(EDLSSQualityMode Quality) const;
	UE_API float GetMaxResolutionFractionForQuality(EDLSSQualityMode Quality) const;
	UE_API bool IsFixedResolutionFraction(EDLSSQualityMode Quality) const;

	const NGXRHI* GetNGXRHI() const
	{
		return NGXRHIExtensions;
	}

	// Inherited via ICustomResourcePool
	virtual void Tick(FRHICommandListImmediate& RHICmdList) override;

	UE_API bool IsQualityModeSupported(EDLSSQualityMode InQualityMode) const;
	uint32 GetNumRuntimeQualityModes() const
	{
		return NumRuntimeQualityModes;
	}

	bool IsDLSSActive() const;

	// Give the suggested EDLSSQualityMode if one is appropriate for the given pixel count, or nothing if DLSS should be disabled
	UE_API TOptional<EDLSSQualityMode> GetAutoQualityModeFromPixels(int PixelCount) const;

	static void ReleaseStaticResources();

	static float GetMinUpsampleResolutionFraction()
	{
		return MinDynamicResolutionFraction;
	}

	static float GetMaxUpsampleResolutionFraction()
	{
		return MaxDynamicResolutionFraction;
	}

private:
	FDLSSUpscaler(NGXRHI* InNGXRHIExtensions);
	

	bool EnableDLSSInPlayInEditorViewports() const;

	// The FDLSSUpscaler(NGXRHI*) will update those once
	UE_API static NGXRHI* NGXRHIExtensions;
	UE_API static float MinDynamicResolutionFraction;
	UE_API static float MaxDynamicResolutionFraction;

	static uint32 NumRuntimeQualityModes;
	static TArray<FDLSSOptimalSettings> ResolutionSettings;
	float PreviousResolutionFraction;

	friend class FDLSSUpscalerViewExtension;
	friend class FDLSSSceneViewFamilyUpscaler;
};

#undef UE_API
