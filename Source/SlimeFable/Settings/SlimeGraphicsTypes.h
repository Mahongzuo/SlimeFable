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
