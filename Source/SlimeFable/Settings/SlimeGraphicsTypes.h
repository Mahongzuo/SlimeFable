// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeGraphicsTypes.generated.h"

UENUM(BlueprintType)
enum class ESlimeUpscaler : uint8
{
	/** Engine TSR. Default — does not init NGX. */
	Off UMETA(DisplayName = "关 (TSR)"),
	DLSS UMETA(DisplayName = "DLSS"),
	/** Reserved: enable when official UE 5.8 FSR plugin ships. Auto then maps AMD here. */
	FSR UMETA(DisplayName = "FSR")
};

UENUM(BlueprintType)
enum class ESlimeDLSSQuality : uint8
{
	Quality UMETA(DisplayName = "质量"),
	Balanced UMETA(DisplayName = "平衡"),
	Performance UMETA(DisplayName = "性能"),
	UltraPerformance UMETA(DisplayName = "超级性能"),
	DLAA UMETA(DisplayName = "DLAA")
};

UENUM(BlueprintType)
enum class ESlimePixelStreamTarget : uint8
{
	Cloud UMETA(DisplayName = "云端"),
	Lan UMETA(DisplayName = "局域网")
};

/**
 *  Slime body surface material.
 *  Classic    = original Fresnel jelly.
 *  Spectral   = per-channel refraction with an analytic ellipsoid thickness (default).
 *  Volumetric = ray march through the marching-cubes density field: real thickness, internal
 *               reflection / TIR, sky reflections. Costs a per-frame density upload.
 */
UENUM(BlueprintType)
enum class ESlimeBodySkin : uint8
{
	Classic UMETA(DisplayName = "经典果冻"),
	Spectral UMETA(DisplayName = "光谱折射"),
	Volumetric UMETA(DisplayName = "体积折射"),
	COUNT UMETA(Hidden)
};
