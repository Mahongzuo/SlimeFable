// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "LyraAimSensitivityData.generated.h"

#define UE_API LYRAGAME_API

enum class ELyraGamepadSensitivity : uint8;

class UObject;

/** Defines a set of gamepad sensitivity to a float value. */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Lyra Aim Sensitivity Data", ShortTooltip = "Data asset used to define a map of Gamepad Sensitivty to a float value."))
class ULyraAimSensitivityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UE_API ULyraAimSensitivityData(const FObjectInitializer& ObjectInitializer);
	
	UE_API const float SensitivtyEnumToFloat(const ELyraGamepadSensitivity InSensitivity) const;
	
protected:
	/** Map of SensitivityMap settings to their corresponding float */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ELyraGamepadSensitivity, float> SensitivityMap;
};

#undef UE_API
