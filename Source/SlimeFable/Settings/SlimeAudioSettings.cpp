// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeAudioSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

namespace SlimeAudioPrivate
{
	static const TCHAR* ConfigSection = TEXT("SlimeAudio");
	static const TCHAR* KeyMaster = TEXT("MasterVolume");
	static const TCHAR* KeyMusic = TEXT("MusicVolume");
	static const TCHAR* KeySfx = TEXT("SfxVolume");
}

void USlimeAudioSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
	Apply();
}

USlimeAudioSettings* USlimeAudioSettings::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : Cast<UGameInstance>(WorldContext);
	return GI ? GI->GetSubsystem<USlimeAudioSettings>() : nullptr;
}

void USlimeAudioSettings::SetMasterVolume(float Volume)
{
	MasterVolume = Clamp01(Volume);
	Apply();
	Save();
}

void USlimeAudioSettings::SetMusicVolume(float Volume)
{
	MusicVolume = Clamp01(Volume);
	Apply();
	Save();
}

void USlimeAudioSettings::SetSfxVolume(float Volume)
{
	SfxVolume = Clamp01(Volume);
	Apply();
	Save();
}

void USlimeAudioSettings::Apply()
{
	if (GEngine)
	{
		if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
		{
			Device->SetTransientPrimaryVolume(MasterVolume);
		}
	}
	OnVolumesChanged.Broadcast();
}

void USlimeAudioSettings::Save()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeyMaster, MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeyMusic, MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeySfx, SfxVolume, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USlimeAudioSettings::Load()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->GetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeyMaster, MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeyMusic, MusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(SlimeAudioPrivate::ConfigSection, SlimeAudioPrivate::KeySfx, SfxVolume, GGameUserSettingsIni);
	MasterVolume = Clamp01(MasterVolume);
	MusicVolume = Clamp01(MusicVolume);
	SfxVolume = Clamp01(SfxVolume);
}
