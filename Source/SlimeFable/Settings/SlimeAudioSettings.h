// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeAudioSettings.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlimeAudioVolumesChanged);

/** Persists Master / Music / SFX volume (0..1) to GameUserSettings.ini. */
UCLASS()
class SLIMEFABLE_API USlimeAudioSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSfxVolume() const { return SfxVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetEffectiveMusicVolume() const { return MasterVolume * MusicVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetEffectiveSfxVolume() const { return MasterVolume * SfxVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetSfxVolume(float Volume);

	/** Apply master to audio device; broadcast so menu BGM can refresh. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void Apply();

	void Save();
	void Load();

	UPROPERTY(BlueprintAssignable, Category = "Audio")
	FOnSlimeAudioVolumesChanged OnVolumesChanged;

	static USlimeAudioSettings* Get(const UObject* WorldContext);

protected:
	float MasterVolume = 1.f;
	float MusicVolume = 0.85f;
	float SfxVolume = 1.f;

	static float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }
};
