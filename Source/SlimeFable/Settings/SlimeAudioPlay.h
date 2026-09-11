// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SlimeAudioSettings.h"
#include "Sound/SoundBase.h"

namespace SlimeAudioPlay
{
	inline float SfxMul(const UObject* WorldContext)
	{
		if (const USlimeAudioSettings* Settings = USlimeAudioSettings::Get(WorldContext))
		{
			return Settings->GetSfxVolume();
		}
		return 1.f;
	}

	inline float MusicMul(const UObject* WorldContext)
	{
		if (const USlimeAudioSettings* Settings = USlimeAudioSettings::Get(WorldContext))
		{
			return Settings->GetMusicVolume();
		}
		return 1.f;
	}

	inline void PlaySfxAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Location)
	{
		if (!WorldContext || !Sound)
		{
			return;
		}
		UGameplayStatics::PlaySoundAtLocation(
			WorldContext, Sound, Location, SfxMul(WorldContext));
	}

	inline void PlaySfx2D(const UObject* WorldContext, USoundBase* Sound)
	{
		if (!WorldContext || !Sound)
		{
			return;
		}
		UGameplayStatics::PlaySound2D(WorldContext, Sound, SfxMul(WorldContext));
	}
}
