// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "LyraPlayerMappableKeyProfile.generated.h"

#define UE_API LYRAGAME_API

UCLASS(MinimalAPI)
class ULyraPlayerMappableKeyProfile : public UEnhancedPlayerMappableKeyProfile
{
	GENERATED_BODY()

protected:

	//~ Begin UEnhancedPlayerMappableKeyProfile interface
	UE_API virtual void EquipProfile() override;
	UE_API virtual void UnEquipProfile() override;
	//~ End UEnhancedPlayerMappableKeyProfile interface
};

#undef UE_API
